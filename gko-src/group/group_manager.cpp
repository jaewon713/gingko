/***************************************************************************
 *
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/
#include "group_manager.h"

#include <deque>
#include <string>
#include <fstream>

#include <thrift/config.h>
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/PosixThreadFactory.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/protocol/TCompactProtocol.h>

#include <libtorrent/bencode.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/extensions/metadata_transfer.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/torrent_info.hpp>

#include "bbts/host_info_util.h"
#include "bbts/number_util.h"
#include "bbts/path.h"
#include "bbts/util.h"
#include "conf_range.h"
#include "GroupConf.pb.h"
#include "group_manager_service_handler.h"
#include "group_params.h"
#include "utils.h"
#include "bbts/log.h"

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;
using namespace apache::thrift::concurrency;

using boost::posix_time::seconds;
using boost::shared_ptr;
using boost::system::error_code;

using libtorrent::session;
using libtorrent::alert;
using libtorrent::session_settings;
using libtorrent::torrent_info;
using libtorrent::libtorrent_exception;

extern ::bbts::GroupConfig kGroupConfig;
extern ::bbts::group::GroupParams g_params;

static const char *kFinishedResumeFileName = "finished.resume";

namespace bbts {
namespace group {

GroupManager::GroupManager() 
    : thrift_tracker_(io_service_),
      total_check_timer_(io_service_),
      last_check_timeout_(0),
      last_conf_file_modify_time_(0),
      finished_infohash_list_changed_(true),
      current_cluster_thread_num_(0),
      cluster_should_stop_(false),
      last_total_downloaded_(0),
      conf_failed_times_(0) {
  infohash_map_.clear();
  torrent_map_.clear();
  finished_infohash_list_.clear();

  // set cred info
  cred_.uid = static_cast<int32_t>(getuid());
  cred_.pid = static_cast<int32_t>(getpid());
  cred_.gid = static_cast<int32_t>(getgid());
}

GroupManager::~GroupManager() {
  DEBUG_LOG("group manager delete");
}

void GroupManager::Join() {
  thrift_thread_.join();
  cluster_thread_.join();
  DEBUG_LOG("begin to reset...");
  session_.reset();
  DEBUG_LOG("end to reset...");
  usleep(100000);  //  wait for other thread..
  DEBUG_LOG("group manager exit");
}

int GroupManager::StartThriftThread() {
  boost::thread tmp_thread(boost::bind(&GroupManager::GroupManagerServer, this));
  thrift_thread_.swap(tmp_thread);
  return 0;
}

int GroupManager::StartClusterThread() {
  boost::thread tmp_thread(boost::bind(&GroupManager::ClusterDownload, this));
  cluster_thread_.swap(tmp_thread);
  return 0;
}

void GroupManager::GroupManagerServer(void) {
  shared_ptr<GroupManagerServiceHandler> handler(new GroupManagerServiceHandler());
  shared_ptr<TProcessor> processor(new GroupManagerServiceProcessor(handler));
  shared_ptr<TProtocolFactory> protocolFactory(new TCompactProtocolFactory());
  shared_ptr<TTransportFactory> transportFactory(new TFramedTransportFactory());

  shared_ptr<ThreadManager> threadManager = ThreadManager::newSimpleThreadManager(10);
  shared_ptr<PosixThreadFactory> threadFactory = shared_ptr<PosixThreadFactory>(new PosixThreadFactory());
  threadManager->threadFactory(threadFactory);
  threadManager->start();

  remove(kGroupConfig.unix_socket().c_str());
  shared_ptr <TServerSocket> serverSocket(new TServerSocket(kGroupConfig.unix_socket().c_str()));
  thrift_server_.reset(new TThreadPoolServer(processor, serverSocket, transportFactory, protocolFactory, threadManager));

  try {
    thrift_server_->serve();
  } catch (TException &ex) {
    WARNING_LOG("server serve error:%s\n", ex.what());
  }
}

void GroupManager::GenerateFinishedListResumeFile() {
  if (!finished_infohash_list_changed_) {
    // no changed, return directry
    // so every cycle don't need to write disk
    return;
  }

  finished_infohash_list_changed_ = false;
  NOTICE_LOG("finished_infohash_list changed, begin to resume...");

  std::string serialized_str;
  libtorrent::entry resume_data;
  std::string finished_resume_file_name = kGroupConfig.resume_dir() + "/";
  finished_resume_file_name += kFinishedResumeFileName;

  {
    boost::mutex::scoped_lock lock(tasks_lock_);
    if (finished_infohash_list_.empty()) {
      // finished_infohash_list_ is empty, so delete it
      // if file doesn't exists, ignore ret code
      unlink(finished_resume_file_name.c_str());
      NOTICE_LOG("finished_infohash_list is empty, delete resume file");
      return;
    }

    // for each finished task, general serialized string to persist
    FinishedInfohashList::iterator it = finished_infohash_list_.begin();
    for (; it != finished_infohash_list_.end(); ++it) {
      shared_ptr<GroupTask> task = it->second;
      if (task.get() == NULL) {
        continue;
      }
      task->GenerateResumeTaskString(&serialized_str);
      resume_data[it->first] = serialized_str;
    }
  }

  std::ofstream ofs(finished_resume_file_name.c_str(), std::ios::binary);
  if (!ofs) {
    WARNING_LOG("open fininshed resume file failed: %s.", finished_resume_file_name.c_str());
    return;
  }

  libtorrent::bencode(std::ostream_iterator<char>(ofs), resume_data);
  ofs.close();
  NOTICE_LOG("generate finished resume data to file: %s success.", finished_resume_file_name.c_str());
}

void GroupManager::Shutdown() {
  // remove all torrent
  {
    boost::mutex::scoped_lock lock(tasks_lock_);
    for (TorrentTaskMap::iterator it = torrent_map_.begin();
         it != torrent_map_.end(); ++it) {
      session_->remove_torrent(it->first);
    }
  }

  // resume finished_infohash_list_ info
  GenerateFinishedListResumeFile();

  // clear all map, so task in it will be destructed, and all thread will be wake up
  {
    boost::mutex::scoped_lock lock(tasks_lock_);
    finished_infohash_list_.clear();
    infohash_map_.clear();
    torrent_map_.clear();
  }

  NOTICE_LOG("begin to wake up all cluster thread...");
  // wake up all cluster down thread
  WakeUpAllClusterDownThread();
  NOTICE_LOG("end to wake up all cluster thread...");

  thrift_server_->stop();
  io_service_.stop();
}

int GroupManager::LoadFromFinishedResume(
    const std::string &resume_file) {
  if (resume_file.empty()) {
    return -1;
  }

  // get file size
  struct stat statbuf;
  if (0 != stat(const_cast<char *>(resume_file.c_str()), &statbuf)) {
    WARNING_LOG("stat failed: %s", resume_file.c_str());
    return -1;
  }
  if (!(statbuf.st_mode & S_IFREG)) {
    return -1;
  }
  int file_size = statbuf.st_size;

  // open resume file
  std::ifstream ifs(resume_file.c_str(), std::ios::binary);
  if (!ifs) {
    WARNING_LOG("can't open resume file: %s", resume_file.c_str());
    return -1;
  }

  boost::scoped_array<char> buffer(new char[file_size]);
  ifs.read(buffer.get(), file_size);
  ifs.close();
  libtorrent::entry resume_data = libtorrent::bdecode(buffer.get(), buffer.get() + file_size);
  if (resume_data.type() != libtorrent::entry::dictionary_t) {
    WARNING_LOG("bdecode resume file failed: %s, type: %d", resume_file.c_str(), resume_data.type());
    return -1;
  }

  // load from resume file
  {
    boost::mutex::scoped_lock lock(tasks_lock_);
    finished_infohash_list_.clear();
    std::string serialized_str;
    libtorrent::entry::dictionary_type &dict = resume_data.dict();
    for (libtorrent::entry::dictionary_type::iterator it = dict.begin();
         it != dict.end(); ++it) {
      shared_ptr<GroupTask> task;
      task.reset(new GroupTask());
      TaskStatus status;
      UnSerializedTaskStatus(resume_data[it->first].string(), &status);
      task->set_task_status(status);
      finished_infohash_list_.push_back(std::make_pair(it->first, task));
    }
  }
  return 0;
}

int GroupManager::Run() {
  if (!peer_stat_.Open(kGroupConfig.peer_stat_log_file())) {
    WARNING_LOG("open peer stat file %s failed", kGroupConfig.peer_stat_log_file().c_str());
  }

  std::string root_path;
  if (!bbts::get_root_path(&root_path)) {
    fprintf(stderr, "Get program root path failed.\n");
    return false;
  }
  if (!thrift_tracker_.LoadThriftTrackers(root_path + "/conf/tracker.conf")) {
    FATAL_LOG("load thrift trackers fail\n");
    return false;
  }

  session_.reset(new libtorrent::session(
      libtorrent::fingerprint("GK", 3, 0, 0, 0),
      std::make_pair(kGroupConfig.listen_port(), kGroupConfig.listen_port()),
      "0.0.0.0",
      0,
      alert::error_notification | alert::peer_notification |
      alert::tracker_notification | alert::storage_notification |
      alert::status_notification | alert::performance_warning |
      alert::ip_block_notification | alert::debug_notification));

  error_code ec;
  session_->listen_on(std::make_pair(kGroupConfig.listen_port(), kGroupConfig.listen_port()),
                      ec,
                      "0.0.0.0",
                      session::listen_no_system_port);
  if (ec) {
    FATAL_LOG("listen on port %d failed.", kGroupConfig.listen_port());
    return false;
  }

  // session settings
  SetSessionSettings();

  session_->add_extension(&libtorrent::create_metadata_plugin);
  session_->add_extension(&create_bbts_torrent_plugin);
  listen_endpoint_ = libtorrent::tcp::endpoint(
          libtorrent::address::from_string(GetLocalHostIPString(), ec), session_->listen_port());
  thrift_tracker_.SetAttribute(session_->id().to_string(),
                               listen_endpoint_.address().to_string(ec));
  session_->add_ex_announce_func(
      boost::bind(&ThriftTracker::ThriftTrackerAnnounce, &thrift_tracker_, _1, _2));

  // start serve thread
  StartThriftThread();
  // start cluster thread
  StartClusterThread();

  sleep(1);  // sleep for thread to run 

  // chmod bbts-group.sock file
  // so other user can communicate with this group
  // if falied, just ignore it
  if (chmod(kGroupConfig.unix_socket().c_str(), 0777) != 0) {
    WARNING_LOG("chmod unix socket file[%s] error[%d]:%s\n",
                 kGroupConfig.unix_socket().c_str(),
                 errno,
                 strerror(errno));
  }

  io_service_.post(boost::bind(&GroupManager::ProcessAlert, this));

  // start hangle check
  total_check_timer_.expires_from_now(seconds(kTimerCheckCycle));
  total_check_timer_.async_wait(boost::bind(&GroupManager::TimerCallback, this, _1));

  // AddResumeTask
  if (AddResumeTask() != 0) {
    WARNING_LOG("add resume failed!");
  }

  // load resume finished tasks
  std::string finished_resume_file_name = kGroupConfig.resume_dir() +  + "/" + kFinishedResumeFileName;
  LoadFromFinishedResume(finished_resume_file_name);

  try {
    io_service_.run();
  } catch (libtorrent_exception &e) {
    FATAL_LOG("exception: %s", e.what());
  } catch (std::exception &e) {
    FATAL_LOG("exception: %s", e.what());
  }

  thrift_server_->stop();
  TRACE_LOG("stop running group server");

  return 0;
}

void GroupManager::TimerCallback(const boost::system::error_code &ec) {
  if (ec) {
    DEBUG_LOG("hang check timer canceled.");
    return;
  }
  DEBUG_LOG("in hang check timer");

  // hang check for each infohash_map_
  std::vector<shared_ptr<GroupTask> > task_list;
  {
    boost::mutex::scoped_lock lock(tasks_lock_);
    InfohashTaskMap::iterator it = infohash_map_.begin();
    for (; it != infohash_map_.end(); ++it) {
      shared_ptr<GroupTask> task = it->second;
      if (task.get() == NULL) {
        continue;
      }
      task_list.push_back(task);
    }
  }

  // we need copy all tasks first because HangCheckCallback may need lock if hang-timeout
  // thus can cause deadlock
  for (std::vector<shared_ptr<GroupTask> >::iterator it = task_list.begin();
       it != task_list.end(); ++it) {
    (*it)->HangCheckCallback();
  }

  // check finished_infohash_list_
  {
    int64_t current_mtime = static_cast<int64_t>(time(NULL));
    boost::mutex::scoped_lock lock(tasks_lock_);
    FinishedInfohashList::iterator it = finished_infohash_list_.begin();
    for (; it != finished_infohash_list_.end();) {
      shared_ptr<GroupTask> task = it->second;
      if (task.get() == NULL) {
        continue;
      }

      if ((current_mtime - task->end_time()) > kGroupConfig.finished_timeout()
          && kGroupConfig.finished_timeout() != -1) {
        // timeout, remove it
        it = finished_infohash_list_.erase(it);
        NOTICE_LOG("infohash:%s finished timeout, remove it", it->first.c_str());
        finished_infohash_list_changed_ = true;
      } else {
        ++it;
      }
    }
  }
  GenerateFinishedListResumeFile();

  // check if this group timeout
  if (kGroupConfig.timeout() > 0) {
    last_check_timeout_ += kTimerCheckCycle;
    if (last_check_timeout_ >= kGroupConfig.timeout()) {
      NOTICE_LOG("this group timeout, quit");
      Shutdown();
    }
  }

  // check if group.conf has changed
  struct stat statbuf;
  ::stat(g_params.conf_file.c_str(), &statbuf);
  time_t current_mtime = statbuf.st_mtime;
  if (current_mtime > last_conf_file_modify_time_ ||
      current_mtime == -1/* -1 means file delete*/) {
    // conf file changed, reload it
    bbts::GroupConfig config;
    if (LoadGroupConfig(g_params.conf_file, &config) == 0) {
      kGroupConfig = config;
      last_conf_file_modify_time_ = current_mtime;
      SetSessionSettings();
      conf_failed_times_ = 0;
      NOTICE_LOG("load group config success");
    } else {
      ++conf_failed_times_;
      NOTICE_LOG("load group config failed, times = %d", conf_failed_times_);
      if (conf_failed_times_ >= kConfMaxFailed) {
        WARNING_LOG("load group confi failed more than %d times, quit", kConfMaxFailed);
        Shutdown();
      }
    }
  }

  total_check_timer_.expires_from_now(seconds(kTimerCheckCycle));
  total_check_timer_.async_wait(boost::bind(&GroupManager::TimerCallback, this, _1));
}

void GroupManager::SetSessionSettings() {
  session_settings settings = libtorrent::high_performance_seed();
  settings.use_php_tracker = false;
  settings.use_c_tracker = true;
  settings.stop_tracker_timeout = 1;
  settings.no_connect_privileged_ports = false;
  settings.listen_queue_size = 200;
  settings.alert_queue_size = 50000;
  settings.num_want = kGroupConfig.peers_num_want();
  settings.disk_io_read_mode = kGroupConfig.disable_os_cache() ?
      session_settings::disable_os_cache : session_settings::enable_os_cache;
  settings.disk_io_write_mode = kGroupConfig.disable_os_cache() ?
      session_settings::disable_os_cache : session_settings::enable_os_cache;
  settings.max_metadata_size = kGroupConfig.max_metadata_size();
  settings.min_announce_interval = kGroupConfig.seed_announce_interval();
  settings.min_reconnect_time = kGroupConfig.min_reconnect_time();
  settings.peer_connect_timeout = kGroupConfig.peer_connection_timeout();
  settings.upload_rate_limit = kGroupConfig.upload_rate_limit();
  settings.download_rate_limit = kGroupConfig.download_rate_limit();
  settings.max_queued_disk_bytes = kGroupConfig.max_queued_disk_bytes();
  settings.max_out_request_queue = kGroupConfig.max_out_request_queue();
  settings.max_allowed_in_request_queue = kGroupConfig.max_allowed_in_request_queue();
  settings.whole_pieces_threshold = kGroupConfig.whole_pieces_threshold();
  settings.request_queue_time = kGroupConfig.request_queue_time();
  settings.cache_size = kGroupConfig.cache_size();
  settings.cache_expiry = kGroupConfig.cache_expiry();
  settings.read_cache_line_size = kGroupConfig.read_cache_line_size();
  settings.write_cache_line_size = kGroupConfig.write_cache_line_size();
  settings.file_pool_size = kGroupConfig.file_pool_size();
  settings.send_buffer_watermark = kGroupConfig.send_buffer_watermark();
  settings.send_buffer_low_watermark = kGroupConfig.send_buffer_low_watermark();
  settings.send_socket_buffer_size = kGroupConfig.send_socket_buffer_size();
  settings.recv_socket_buffer_size = kGroupConfig.recv_socket_buffer_size();
  settings.dont_count_slow_torrents = true;
  settings.active_seeds = kGroupConfig.active_seeds();
  settings.active_limit = kGroupConfig.active_seeds();
  settings.active_downloads = kGroupConfig.active_downloads();
  settings.connections_limit = kGroupConfig.connection_limit();
  settings.ignore_limits_on_local_network = false;
  settings.enable_outgoing_utp = false;
  settings.enable_incoming_utp = false;
  settings.incoming_starts_queued_torrents = true;
  settings.seeding_outgoing_connections = false;
  settings.choking_algorithm = session_settings::auto_expand_choker;
  settings.seed_choking_algorithm = session_settings::round_robin;
  settings.unchoke_slots_limit = 1000;
  if (kGroupConfig.suggest_mode()) {
    settings.explicit_read_cache = true;
    settings.suggest_mode = session_settings::suggest_read_cache;
  }
  session_->set_settings(settings);
}

ClientRpcStatus GroupManager::AddTaskToMap(
    shared_ptr<GroupTask> &task) {
  if (task.get() == NULL) {
    return kClientRpcErrorParams;
  }

  // create success, add task to map
  bool is_add_success = false;
  if (!task->infohash().empty() &&
      (task->infohash().length() == 40 || task->is_torrent_url())) {
    if (infohash_map_.find(task->infohash()) == infohash_map_.end()) {
      // new infohash, so add it in map
      infohash_map_[task->infohash()] = task;
      torrent_map_[task->torrent_handle()] = task;
      is_add_success = true;
      NOTICE_LOG("add task[%s] to group success.", task->infohash().c_str());
    } else {
      NOTICE_LOG("infohash[%s[ has already in this group", task->infohash().c_str());
    }
  }

  if (!is_add_success) {
    return kClientRpcAddTaskExist;
  }

  return kClientRpcSuccess;
}

ClientRpcStatus GroupManager::AddTask(
    const DownloadParam &down_params,
    const UserCred &cred) {
  if (!kGroupConfig.is_allow_all_add()) {
    // cred check
    if (cred.uid != 0 && cred.uid != cred_.uid && cred.uid != (int32_t)get_self_uid()) {
      NOTICE_LOG("cred check failed, cred.uid is %d, group.uid is %d", cred.uid, cred_.uid);
      return kClientRpcCredFailed;
    }
  }

  // add task
  shared_ptr<GroupTask> task;
  task.reset(new GroupTask());

  // lock before create, thus any alert before add to map will not be process before add finish
  boost::mutex::scoped_lock lock(tasks_lock_);
  if (task->Create(down_params, cred) != 0) {
    FATAL_LOG("create task:%s failed", GeneralParamString(down_params).c_str());
    return kClientRpcAddTaskCreateFailed;
  }

  return AddTaskToMap(task);
}

int GroupManager::AddResumeTask() {
  DIR* dir = opendir(const_cast<char *>(kGroupConfig.resume_dir().c_str()));
  if (NULL == dir) {
    WARNING_LOG("can not open dir: %s", kGroupConfig.resume_dir().c_str());
    return -1;
  }

  NOTICE_LOG("will add resume task...");
  struct dirent entry, *entptr;
  for (readdir_r(dir, &entry, &entptr); entptr != NULL; readdir_r(dir, &entry, &entptr)) {
    if ((strcmp(entptr->d_name, ".") == 0) ||
        (strcmp(entptr->d_name, "..") == 0)||
        (strcmp(entptr->d_name, kFinishedResumeFileName) == 0)) {
      continue;
    }
    std::string file_name = kGroupConfig.resume_dir() + "/" + entptr->d_name;
    struct stat statbuf;
    if (0 != stat(const_cast<char *>(file_name.c_str()), &statbuf)) {
      WARNING_LOG("stat failed: %s", file_name.c_str());
      continue;
    }
    if (!(statbuf.st_mode & S_IFREG)) {
      continue;
    }

    // lock before create, thus any alert before add to map will not be process before add finish
    boost::mutex::scoped_lock lock(tasks_lock_);
    shared_ptr<GroupTask> task;
    task.reset(new GroupTask());
    if (task->CreateFromResumeFile(file_name, statbuf.st_size) != 0) {
      WARNING_LOG("load from resume file:%s failed, ignore it!", file_name.c_str());
      unlink(file_name.c_str());
      continue;
    }

    if (AddTaskToMap(task) == 0) {
      NOTICE_LOG("Add resume task[%s] success.", task->infohash().c_str());
    }
  }
  closedir(dir);

  return 0;
}

ClientRpcStatus GroupManager::ControlTask(
    const std::string &infohash,
    const bool is_all,
    const ControlTaskType::type type,
    const UserCred &cred) {
  if (is_all) {
    return ControlAllTasks(type, cred);
  }

  if (infohash.empty()) {
    WARNING_LOG("cancel task, infohash is empty, but is_all is false");
    return kClientRpcErrorParams;
  }
  
  return ControlTaskByInfohash(infohash, type, cred);
}

ClientRpcStatus GroupManager::ControlAllTasks(
    const ControlTaskType::type type,
    const UserCred &cred) {
  // move infohash_map_ to finished_infohash_list_
  // copy first
  std::vector<shared_ptr<GroupTask> > task_list;
  {
    boost::mutex::scoped_lock lock(tasks_lock_);
    for (InfohashTaskMap::iterator it = infohash_map_.begin();
         it != infohash_map_.end(); ++it) {
      shared_ptr<GroupTask> task = it->second;
      if (task.get() == NULL) {
        WARNING_LOG("infohash:%s get task is null, strange!", it->first.c_str());
        continue;
      }

      if (task->CheckCred(cred) != 0) {
        // can't cancel, skip
        NOTICE_LOG("DEBUG  user cred.uid is %d", cred.uid);
        continue;
      }

      task_list.push_back(task);
    }
  }

  // remove....
  // if in this loop, some new task add, we don't cancel it, because in this
  // time, we think all task before has been canceled 
  std::vector<shared_ptr<GroupTask> >::iterator it = task_list.begin();
  for (; it != task_list.end(); ++it) {
    shared_ptr<GroupTask> task = *it;
    task->ControlTask(type);
  }
  TRACE_LOG("control all infohash, infohash_map size is %d, torrent_handle size is %d",
             infohash_map_.size(), torrent_map_.size());

  return kClientRpcSuccess;
}

ClientRpcStatus GroupManager::ControlTaskByInfohash(
    const std::string &infohash,
    const ControlTaskType::type type,
    const UserCred &cred) {
  InfohashTaskMap::iterator it = infohash_map_.find(infohash);
  if (it == infohash_map_.end()) {
    WARNING_LOG("infohash:%s can't find task!", infohash.c_str());
    return kClientRpcTaskNotExist;
  }
  shared_ptr<GroupTask> task = it->second;
  if (task.get() == NULL) {
    WARNING_LOG("infohash:%s get task is null!", infohash.c_str());
    return kClientRpcTaskNotExist;
  }

  if (task->CheckCred(cred) != 0) {
    NOTICE_LOG("infohash:%s check cred failed, cred.uid is %d", infohash.c_str(), cred.uid);
    return kClientRpcCredFailed;
  }

  task->ControlTask(type);
  return kClientRpcSuccess;
}

int GroupManager::GetTaskStatus(
    const std::string &infohash,
    bool is_full,
    std::vector<TaskStatus> &task_status_list) {

  task_status_list.clear();

  if (is_full) {
    return GetAllTaskStatus(task_status_list);
  }

  if (infohash.empty()) {
    WARNING_LOG("get task status, but infohash is empty!");
    return -1;
  }

  return GetTaskStatusByInfohash(infohash, task_status_list);
}

int GroupManager::GetAllTaskStatus(
    std::vector<TaskStatus> &task_status_list) {
  // find finished tasks
  boost::mutex::scoped_lock lock(tasks_lock_);
  for (FinishedInfohashList::iterator it = finished_infohash_list_.begin();
       it != finished_infohash_list_.end(); ++it) {
    shared_ptr<GroupTask> task = it->second;
    if (task.get() == NULL) {
      WARNING_LOG("get from finished_infohash_list[%s] success, but task is null!", it->first.c_str());
      continue;
    }

    task_status_list.push_back(task->task_status());
  }

  // running tasks
  for (InfohashTaskMap::iterator it = infohash_map_.begin();
      it != infohash_map_.end(); ++it) {
    shared_ptr<GroupTask> task = it->second;
    if (task.get() == NULL) {
      WARNING_LOG("get from infohash_map[%s] success, but task is null!", it->first.c_str());
      continue;
    }

    task_status_list.push_back(task->task_status());
  }
  return 0;
}

int GroupManager::GetTaskStatusByInfohash(
    const std::string &infohash,
    std::vector<TaskStatus> &task_status_list) {
  boost::mutex::scoped_lock lock(tasks_lock_);
  // first, check if finished_infohash_list_ has this infohash status
  FinishedInfohashList::iterator it = finished_infohash_list_.begin();
  for (; it != finished_infohash_list_.end(); ++it) {
    if (it->first == infohash && it->second.get() != NULL) {
      task_status_list.push_back(it->second->task_status());
    }
  }

  // second, get infohash_map_ status
  shared_ptr<GroupTask> task;
  InfohashTaskMap::iterator it_finder = infohash_map_.find(infohash);
  if (it_finder != infohash_map_.end()) {
    task = it_finder->second;
  }

  if (task.get() != NULL) {
    task_status_list.push_back(task->task_status());
  }

  return 0;
}

shared_ptr<GroupTask> GroupManager::GetTaskByTorrentHanlde(
    const libtorrent::torrent_handle &handle) {
  shared_ptr<GroupTask> task;
  {
    boost::mutex::scoped_lock lock(tasks_lock_);
    TorrentTaskMap::iterator it = torrent_map_.find(handle);
    if (it == torrent_map_.end()) {
      WARNING_LOG("Get task by torrent handle failed!");
      return task;
    } else {
      task = it->second;
    }

    if (task.get() == NULL) {
      WARNING_LOG("Get taskby torrent handle success but handle is null!");
    }
  }

  return task;
}

void GroupManager::OnTorrentFinished(const libtorrent::torrent_finished_alert *alert) {
  NOTICE_LOG("%s", alert->message().c_str());

  shared_ptr<GroupTask> task = GetTaskByTorrentHanlde(alert->handle);
  if(task.get() != NULL && task->OnTorrentFinished() != 0) {
    WARNING_LOG("finished torrent failed");
  }
}

void GroupManager::OnStateChanged(const libtorrent::state_changed_alert *alert) {
  NOTICE_LOG("%s", alert->message().c_str());

  shared_ptr<GroupTask> task = GetTaskByTorrentHanlde(alert->handle);
  if (task.get() != NULL && task->OnStateChanged(alert) != 0) {
    WARNING_LOG("on state changed alert failed");
  }
}

void GroupManager::OnSaveResumeData(const libtorrent::save_resume_data_alert *alert) {
  DEBUG_LOG("%s", alert->message().c_str());

  shared_ptr<GroupTask> task = GetTaskByTorrentHanlde(alert->handle);
  if (task.get() != NULL && task->OnSaveResumeData(alert) != 0) {
    WARNING_LOG("on save resume data alert failed");
  }
}

void GroupManager::OnMetadataReceived(const libtorrent::metadata_received_alert *alert) {
  NOTICE_LOG("%s", alert->message().c_str());

  shared_ptr<GroupTask> task = GetTaskByTorrentHanlde(alert->handle);
  if (task.get() != NULL && task->OnMetadataReceived() != 0) {
    WARNING_LOG("on metadata received alert failed");
  }
}

void GroupManager::OnHashFailed(const libtorrent::hash_failed_alert *alert) {
  NOTICE_LOG("%s", alert->message().c_str());

  shared_ptr<GroupTask> task = GetTaskByTorrentHanlde(alert->handle);
  if (task.get() != NULL && task->OnHashFailed(alert) != 0) {
    WARNING_LOG("on hash failed alert failed");
  }
}

void GroupManager::OnStorageMoved(const libtorrent::storage_moved_alert *alert) {
  NOTICE_LOG("%s", alert->message().c_str());

  shared_ptr<GroupTask> task = GetTaskByTorrentHanlde(alert->handle);
  if (task.get() != NULL && task->OnStorageMoved() != 0) {
    WARNING_LOG("on hash failed alert failed");
  }
}

void GroupManager::OnListenFailed(const libtorrent::listen_failed_alert *alert) {
  const libtorrent::tcp::endpoint &ep = alert->endpoint;
  if (alert->sock_type == libtorrent::listen_failed_alert::tcp &&
          ep.address().is_v4() && ep.port() == kGroupConfig.listen_port()) {
    FATAL_LOG("%s.", alert->message().c_str());
    Shutdown();
  }
}

void GroupManager::RemoveTorrentTmp(
    const libtorrent::torrent_handle &torrent_handle,
    const std::string &infohash) {
  // remove torrent first
  session_->remove_torrent(torrent_handle);
  shared_ptr<GroupTask> task;
  // move from infohash_map_ and torrent_map_ to finished_map_
  {
    boost::mutex::scoped_lock lock(tasks_lock_);
    InfohashTaskMap::iterator it = infohash_map_.find(infohash);
    task = it->second;
    if (it == infohash_map_.end()) {
      WARNING_LOG("infohash %s finished, but can't find in infohash_map_!", infohash.c_str());
      return;
    }

    task = it->second;
    if (task.get() == NULL) {
      WARNING_LOG("infohash %s finished, but get task is NULL!", infohash.c_str());
      return;
    }
    finished_infohash_list_.push_back(std::make_pair(infohash, task));
    finished_infohash_list_changed_ = true;
    infohash_map_.erase(infohash);
    torrent_map_.erase(torrent_handle);
  }

  TRACE_LOG("remove infohash[%s] into finished_infohash_list", infohash.c_str());
  
}

void GroupManager::OnTorrentError(const libtorrent::torrent_error_alert *alert) {
  NOTICE_LOG("%s", alert->message().c_str());
}

void GroupManager::ProcessAlert() {
  if (NULL == session_->wait_for_alert(libtorrent::milliseconds(200))) {
    io_service_.post(boost::bind(&GroupManager::ProcessAlert, this));
    return;
  }

  {
    // lock first, because if someone is in adding task or finished task,
    // torrent_handle may hasn't been added into torrent_map_, so we must wait
    boost::mutex::scoped_lock lock(tasks_lock_);
  }

  std::deque<alert *> alerts;
  session_->pop_alerts(&alerts);
  for (std::deque<alert *>::iterator it = alerts.begin(); it != alerts.end(); ++it) {
    switch ((*it)->type()) {
      case libtorrent::listen_failed_alert::alert_type:
        OnListenFailed((libtorrent::listen_failed_alert *)(*it));
        break;

      case libtorrent::save_resume_data_alert::alert_type:
        OnSaveResumeData((libtorrent::save_resume_data_alert *)(*it));
        break;

      case libtorrent::torrent_finished_alert::alert_type:
        OnTorrentFinished((libtorrent::torrent_finished_alert *)(*it));
        break;

      case libtorrent::torrent_error_alert::alert_type:
        OnTorrentError((libtorrent::torrent_error_alert *)(*it));
        break;

      case peer_stat_alert::alert_type:
        peer_stat_.Print(*static_cast<peer_stat_alert *>(*it));
        break;

      case libtorrent::state_changed_alert::alert_type:
        OnStateChanged((libtorrent::state_changed_alert *)(*it));
        break;

      case libtorrent::hash_failed_alert::alert_type:
        OnHashFailed((libtorrent::hash_failed_alert *)(*it));
        break;

      case libtorrent::metadata_received_alert::alert_type:
        OnMetadataReceived((libtorrent::metadata_received_alert *)(*it));
        break;

      case libtorrent::storage_moved_alert::alert_type:
        OnStorageMoved((libtorrent::storage_moved_alert *)(*it));
        break;

      case libtorrent::torrent_checked_alert::alert_type:
      case libtorrent::listen_succeeded_alert::alert_type:
      case libtorrent::add_torrent_alert::alert_type:
      case libtorrent::torrent_added_alert::alert_type:
      case libtorrent::torrent_removed_alert::alert_type:
      case libtorrent::torrent_deleted_alert::alert_type:
      case libtorrent::url_seed_alert::alert_type:
      case libtorrent::peer_blocked_alert::alert_type:
      case libtorrent::peer_ban_alert::alert_type:
      case libtorrent::torrent_resumed_alert::alert_type:
      case libtorrent::torrent_paused_alert::alert_type:
        NOTICE_LOG("alert: %s.", (*it)->message().c_str());
        break;

      case libtorrent::peer_error_alert::alert_type:
      case libtorrent::torrent_delete_failed_alert::alert_type:
      case libtorrent::metadata_failed_alert::alert_type:
      case libtorrent::tracker_warning_alert::alert_type:
      case libtorrent::tracker_error_alert::alert_type:
      case libtorrent::file_error_alert::alert_type:
        WARNING_LOG("alert: %s.", (*it)->message().c_str());
        break;

      case libtorrent::tracker_reply_alert::alert_type:
      case libtorrent::tracker_announce_alert::alert_type:
        TRACE_LOG("alert: %s.", (*it)->message().c_str());
        break;

      case libtorrent::performance_alert::alert_type:
      case libtorrent::peer_connect_alert::alert_type:
      case libtorrent::peer_disconnected_alert::alert_type:
      case libtorrent::incoming_connection_alert::alert_type:
      default:
        DEBUG_LOG("alert: %s.", (*it)->message().c_str());
        break;
    }

    delete *it;
  }
  alerts.clear();
  io_service_.post(boost::bind(&GroupManager::ProcessAlert, this));
}

void GroupManager::PushClusterThreadQueue(
    boost::shared_ptr<GroupClusterTask> task) {
  boost::mutex::scoped_lock lock(queue_lock_);
  cluster_queue_tasks_.push(task);
  DEBUG_LOG("add a task to queue...");
}

shared_ptr<GroupTask> GroupManager::GetFinishedTaskByInfohash(
    const std::string &infohash) {
  shared_ptr<GroupTask> task;
  if (infohash.empty()) {
    return task;
  }

  FinishedInfohashList::iterator it = finished_infohash_list_.begin();
  for (; it != finished_infohash_list_.end(); ++it) {
    if (it->first == infohash) {
      task = it->second;
      return task;
    }
  }

  return task;
}

int GroupManager::DeleteFinishedTaskByInfohash(
    const std::string &infohash) {
  if (infohash.empty()) {
    return -1;
  }
  
  FinishedInfohashList::iterator it = finished_infohash_list_.begin();
  for (; it != finished_infohash_list_.end(); ++it) {
    if (it->first == infohash) {
      // found and delete it
      NOTICE_LOG("delete infohash:%s from finished_infohash_list", infohash.c_str());
      finished_infohash_list_.erase(it);
      finished_infohash_list_changed_ = true;
      return 0;
    }
  }

  return -1;
}

void GroupManager::UpdateInfohashByTorrentUrl(
    const std::string &torrent_url,
    const std::string &infohash) {
  if (torrent_url.empty() ||
      infohash.empty() ||
      infohash.length() != 40) {
    return;
  }
  
  NOTICE_LOG("begin to update torrent_url:%s to infohash:%s", torrent_url.c_str(), infohash.c_str());
  boost::mutex::scoped_lock lock(tasks_lock_);
  bool is_found_in_infohash_map = true;
  InfohashTaskMap::iterator it_infohash = infohash_map_.find(torrent_url);
  shared_ptr<GroupTask> task;
  if (it_infohash == infohash_map_.end()) {
    NOTICE_LOG("get torrent_url:%s from infohash_map failed, try to find in finished_infohash_list", torrent_url.c_str());
    task = GetFinishedTaskByInfohash(torrent_url);
    if (task.get() == NULL) {
      WARNING_LOG("get torrent_url:%s from both infohash_map and finished_infohash_list failed!", torrent_url.c_str());
      return;
    }
    is_found_in_infohash_map = false;
  }

  task = it_infohash->second;
  if (task.get() == NULL) {
    WARNING_LOG("found task torrent_url:%s, but task is null!", torrent_url.c_str());
  }

  if (is_found_in_infohash_map) {
    infohash_map_.insert(InfohashTaskMap::value_type(infohash, task));
    infohash_map_.erase(torrent_url);
  } else {
    if (DeleteFinishedTaskByInfohash(torrent_url) != 0) {
      WARNING_LOG("found torrent_url:%s in finished_infohash_list, but delete it failed!", torrent_url.c_str());
      return;
    }
    finished_infohash_list_.push_back(std::make_pair(infohash, task));
    finished_infohash_list_changed_ = true;
  }

  NOTICE_LOG("update torrent_url:%s into infohash:%s success", torrent_url.c_str(), infohash.c_str());
}

void GroupManager::WakeUpAllClusterDownThread() {
  cluster_should_stop_ = true;
}

void GroupManager::DeleteClusterThreadNum() {
  boost::mutex::scoped_lock lock(queue_lock_);
  if (current_cluster_thread_num_ > 0) {
    --current_cluster_thread_num_;
  }
}

void GroupManager::ClusterDownload() {
  DEBUG_LOG("Cluster download start...");
  bool is_normal_sleep = false;
  while(!cluster_should_stop_) {
    // sleep first
    if (is_normal_sleep) {
      sleep(kClusterRunCycye);
    }

    shared_ptr<GroupClusterTask> task;
    {
      boost::mutex::scoped_lock lock(queue_lock_);
      if (current_cluster_thread_num_ < kGroupConfig.max_cluster_thread_num()) {
        if (cluster_queue_tasks_.empty()) {
          DEBUG_LOG("queue is empty, wait for next try");
          is_normal_sleep = true;
          continue;
        }
        task = cluster_queue_tasks_.front();
        cluster_queue_tasks_.pop();
        DEBUG_LOG("task is not null, current_cluster_thread_num_ is %d", current_cluster_thread_num_);
      } else {
        // thread num too many, wait
        DEBUG_LOG("current_cluster_thread_num is %d, wait for next try", current_cluster_thread_num_);
        is_normal_sleep = true;
        continue;
      }
    }

    if (task.get() == NULL) {
      DEBUG_LOG("task is null, not new cluster task or some error..");
      is_normal_sleep = true;
      continue;
    }

    // start a thread
    // check if task's thread num has full
    if (task->CheckIfThreadnumOut() || task->IsStoped()) {
      DEBUG_LOG("task thread num has all start....");
      is_normal_sleep = true;
      continue;
    }

    for (int i = task->current_start_thread_num(); i < task->threads_num(); ++i) {
      DEBUG_LOG("start a download thread....");
      thread_group_.create_thread(boost::bind(&GroupClusterTask::Download, task.get()));
      {
        boost::mutex::scoped_lock lock(queue_lock_);
        ++current_cluster_thread_num_;
        if (current_cluster_thread_num_ > kGroupConfig.max_cluster_thread_num()) {
          is_normal_sleep = true;
          cluster_queue_tasks_.push(task);
          break;
        } else {
          is_normal_sleep = false;
        }
      }
    }  // for
  }  // while

  DEBUG_LOG("cluster download thread exit");
}

void GroupManager::SessionDownloadLimit() {
  {
    boost::mutex::scoped_lock  lock(queue_lock_);
    if (current_cluster_thread_num_ == 0) {
      return;
    }
  }

  int64_t  download_total = 0;
  {
    boost::mutex::scoped_lock lock(tasks_lock_);
    TorrentTaskMap::iterator it = torrent_map_.begin();
    for (; it != torrent_map_.end(); ++it) {
      download_total += it->first.status(0).total_download;
    }
  }

  int64_t downloaded_since_last = download_total - last_total_downloaded_;
  last_total_downloaded_ = download_total;
  download_limit_.BandwidthLimit(downloaded_since_last, kGroupConfig.download_rate_limit());
}

ClientRpcStatus GroupManager::SetTaskOptions(
    const TaskOptions &options) {
  if (options.infohash.empty()) {
    // means set session options
    session_settings settings = session_->settings();

    if (options.connections_limit > 0) {
      int connections_limit = SetLimitIntValue(kMinConnectionLimit, kMaxConnectionLimit, options.connections_limit);
      settings.connections_limit = connections_limit;
      kGroupConfig.set_connection_limit(connections_limit);
    }
    if (options.download_limit > 0) {
      int download_limit = SetLimitIntValue(kMinDownloadLimit*1024*1024, kMaxDownloadLimit*1024*1024, options.download_limit);
      settings.download_rate_limit = download_limit;
      kGroupConfig.set_download_rate_limit(download_limit);
    }
    if (options.upload_limit > 0) {
      int upload_limit = SetLimitIntValue(kMinUploadLimit*1024*1024, kMaxUploadLimit*1024*1024, options.upload_limit);
      settings.upload_rate_limit = upload_limit;
      kGroupConfig.set_upload_rate_limit(upload_limit);
    }
    session_->set_settings(settings);

    std::string download_limit, upload_limit;
    BytesToReadable(settings.download_rate_limit, &download_limit);
    BytesToReadable(settings.upload_rate_limit, &upload_limit);
    TRACE_LOG("group setopt downlimit: %s/s, uplimit: %s/s, maxconn:%d",
               download_limit.c_str(), upload_limit.c_str(), settings.connections_limit);

    return kClientRpcSuccess;
  }  // if infohash empty

  // set special task
  shared_ptr<GroupTask> task;
  {
    boost::mutex::scoped_lock lock(tasks_lock_);
    task = infohash_map_[options.infohash];
  }

  if (task.get() == NULL) {
    WARNING_LOG("get infohash[%s] task options, but task is null", options.infohash.c_str());
    return kClientRpcTaskNotExist;
  }

  if (task->SetOptions(options) != 0) {
    WARNING_LOG("set infohash[%s] task options failed, download_limit:%d,\
                  upload_limit:%d, connections_limit:%d",
                  options.infohash.c_str(), options.download_limit,
                  options.upload_limit, options.connections_limit);
    return kClientRpcSetOptFailed;
  }

  return kClientRpcSuccess;
}

int GroupManager::GetTaskOptions(
    const std::string &infohash,
    std::vector<TaskOptions> &task_options_list) {
  TaskOptions task_option;
  task_options_list.clear();

  if (infohash.empty()) {
    // means get group task options 
    session_settings settings = session_->settings();
    task_option.__set_infohash("");
    task_option.__set_download_limit(settings.download_rate_limit);
    task_option.__set_upload_limit(settings.upload_rate_limit);
    task_option.__set_connections_limit(settings.connections_limit);
    task_options_list.push_back(task_option);
    return 0;
  }  // if infohash.empty
  
  // get special task
  shared_ptr<GroupTask> task;
  {
    boost::mutex::scoped_lock lock(tasks_lock_);
    InfohashTaskMap::iterator it = infohash_map_.find(infohash);
    if (it != infohash_map_.end()) {
      task = infohash_map_[infohash];
    }
  }

  if (task.get() == NULL) {
    WARNING_LOG("get infohash[%s] task options, but task is null", infohash.c_str());
    return -1;
  }

  if (task->GetOptions(&task_option) != 0) {
    WARNING_LOG("infohash[%s] get task options failed", infohash.c_str());
  }
  task_options_list.push_back(task_option);
  return 0;
}

}  // namespace group
}  // namespace bbts

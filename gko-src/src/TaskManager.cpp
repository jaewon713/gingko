/***************************************************************************
 *
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file   TaskManager.cpp
 *
 * @author liuming03
 * @date   2013-1-14
 */

#include "bbts/agent/TaskManager.h"

#include <libtorrent/bencode.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/escape_string.hpp>
#include <libtorrent/extensions/metadata_transfer.hpp>
#include <libtorrent/session.hpp>

#include "bbts/agent/Task.h"
#include "bbts/agent/TaskDB.h"
#include "bbts/BBTSTorrentPlugin.h"
#include "bbts/configure.pb.h"
#include "bbts/constant_def.h"
#include "bbts/ErrorCategory.h"
#include "bbts/host_info_util.h"
#include "bbts/LazySingleton.hpp"
#include "bbts/log.h"
#include "bbts/number_util.h"
#include "bbts/path.h"
#include "bbts/socket_util.h"
#include "bbts/encode.h"

using std::string;
using std::deque;
using boost::asio::io_service;
using boost::scoped_ptr;
using boost::shared_ptr;
using boost::scoped_array;
using boost::system::error_code;
using boost::asio::local::stream_protocol;
using libtorrent::session;
using libtorrent::session_settings;
using libtorrent::torrent_handle;
using libtorrent::alert;
using libtorrent::torrent_info;
using libtorrent::alert;
using bbts::message::AgentConfigure;
typedef boost::mutex::scoped_lock scoped_lock;

namespace bbts {
namespace agent {

#define ADD_TASK_TO_MAP(task) \
do { \
    tasks_map_[(task)->get_id()] = task; \
    infohash_map_[(task)->get_infohash()] = task; \
    data_path_map_[(task)->get_data_path()] = task; \
    torrent_map_[(task)->get_torrent()] = task; \
} while (0)

#define ERASE_TASK_FROM_MAP(task) \
do { \
    torrent_map_.erase((task)->get_torrent()); \
    infohash_map_.erase((task)->get_infohash()); \
    data_path_map_.erase((task)->get_data_path()); \
    tasks_map_.erase((task)->get_id()); \
} while (0)

static void WriteBaseMessage(const shared_ptr<UnixSocketConnection> &connection,
                             const error_code &ec) {
  bbts::message::BaseRes response;
  response.set_fail_msg(ec.message());
  response.set_ret_code(ec.value());
  WriteMessage(connection, RES_BASE, response);
}

static int get_agent_config_cb(void *arg, int argc, char **argv, char **col_name) {
  message::AgentOptions *options = static_cast<message::AgentOptions *>(arg);
  options->set_upload_limit(atoi(argv[1]));
  options->set_max_conections(atoi(argv[2]));
  return 0;
}

TaskManager::TaskManager() :
    worker_pool_("BBTS AGENT WORKER THREAD"),
    thrift_tracker_(worker_pool_.get_io_service()),
    current_metas_total_size_(0) {
  control_server_.set_heartbeat_recv_cycle(120);
  control_server_.set_accept_callback(boost::bind(&TaskManager::OnAccept, this, _1));
  control_server_.set_read_callback(boost::bind(&TaskManager::OnReadMessage, this, _1, _2));
}

TaskManager::~TaskManager() {}

shared_ptr<TaskManager::metadata_t> TaskManager::GetMetaByInfohash(const string &infohash) {
  shared_ptr<metadata_t> meta;
  scoped_lock lock(metas_lock_);
  for (metadata_list_t::iterator it = metas_.begin(); it != metas_.end(); ++it) {
    if ((*it)->infohash == infohash) {
      meta = *it;
      current_metas_total_size_ -= meta->metadata.length();
      metas_.erase(it);
      break;
    }
  }
  return meta;
}

void TaskManager::PersistTasks() {
  shared_ptr<Task> task;
  scoped_lock lock(tasks_lock_);
  tasks_map_t::iterator it;
  for (it = tasks_map_.begin(); it != tasks_map_.end();) {
    task = it->second;
    ++it;
    ERASE_TASK_FROM_MAP(task);
    task->Persist();
  }
}

void TaskManager::AddResumeTask() {
  string path = LazySingleton<AgentConfigure>::instance()->resume_dir();
  DIR* dir = opendir(const_cast<char *>(path.c_str()));
  if (NULL == dir) {
    WARNING_LOG("can not open dir: %s", path.c_str());
    return;
  }

  NOTICE_LOG("will add resume task...");
  struct dirent entry, *entptr;
  scoped_lock lock(tasks_lock_);
  for (readdir_r(dir, &entry, &entptr); entptr != NULL; readdir_r(dir, &entry, &entptr)) {
    if ((strcmp(entptr->d_name, ".") == 0) || (strcmp(entptr->d_name, "..") == 0)) {
      continue;
    }
    string file_name = path + "/" + entptr->d_name;
    struct stat statbuf;
    if (0 != stat(const_cast<char *>(file_name.c_str()), &statbuf)) {
      WARNING_LOG("stat failed: %s", file_name.c_str());
      continue;
    }
    if (!(statbuf.st_mode & S_IFREG)) {
      continue;
    }

    shared_ptr<Task> task = Task::CreateFromResumeFile(file_name, statbuf.st_size);
    if (!task) {
      unlink(file_name.c_str());
      continue;
    }
    ADD_TASK_TO_MAP(task);
  }
  closedir(dir);
}

bool TaskManager::GetAgentOptionFromDb(message::AgentOptions *options) {
  assert(options);
  options->set_upload_limit(-1);

  bool if_has_agent_config = false;
  string sql = "select bind_port, upload_limit, connection_limit from agent_config limit 1";
  for (int i = 0; i < 3; ++i) {
    if (!task_db_->Excute(sql, &get_agent_config_cb, options)) {
      task_db_->Reconnect();
      WARNING_LOG("execute sql(%s) failed:%d.", sql.c_str(), i);
      continue;
    }
    if_has_agent_config = true;
    break;
  }

  if (options->upload_limit() == -1) {
    // this means empty table or some error happens
    if_has_agent_config = false;
    AgentConfigure *configure = LazySingleton<AgentConfigure>::instance();
    std::stringstream s;
    s << "insert into agent_config(bind_port, upload_limit, connection_limit) values("
      << configure->listen_port() << "," << configure->upload_limit() << ","
      << configure->connection_limit() << ")";
    for (int i = 0; i < 3; ++i) {
      if (!task_db_->Excute(s.str())) {
        task_db_->Reconnect();
        WARNING_LOG("execute sql(%s) failed:%d", s.str().c_str(), i);
        continue;
      }
      NOTICE_LOG("insert one record into agent_config:%s", s.str().c_str());
      break;
    }
  }

  return if_has_agent_config;
}

bool TaskManager::Start() {
  AgentConfigure *configure = LazySingleton<AgentConfigure>::instance();
  if (!peer_stat_.Open(configure->peer_stat_file())) {
    WARNING_LOG("Open peer stat file %s failed", configure->peer_stat_file().c_str());
  }

  if (!thrift_tracker_.LoadThriftTrackers(configure->tracker_conf_file())) {
    NOTICE_LOG("Can't load dynamic trackers conf, will try load static tracker conf");
    // get running root path
    string root_path;
    if (!bbts::get_root_path(&root_path)) {
      WARNING_LOG("Get program root path failed.\n");
      return false;
    }
    if (!thrift_tracker_.LoadThriftTrackers(root_path + "/conf/tracker.conf")) {
      FATAL_LOG("load thrift trackers fail\n");
      return false;
    }
  }

  control_server_.set_endpoint(UnixSocketConnection::EndPoint(configure->socket_file()));
  if (!control_server_.Start(0777)) {
    FATAL_LOG("control server start failed.");
    return false;
  }

  task_db_.reset(new TaskDB(configure->database_file(),
                            worker_pool_.get_io_service(),
                            configure->db_del_interval()));
  session_.reset(new session(
      libtorrent::fingerprint("GK", 3, 0, 0, 0),
      std::make_pair(configure->listen_port(), configure->listen_port()),
      "0.0.0.0",
      0,
      alert::error_notification | alert::peer_notification | alert::tracker_notification |
      alert::storage_notification | alert::status_notification | alert::performance_warning |
      alert::ip_block_notification | alert::debug_notification));

  error_code ec;
  session_->listen_on(std::make_pair(configure->listen_port(), configure->listen_port()),
                      ec,
                      "0.0.0.0",
                      session::listen_no_system_port);
  if (ec) {
    FATAL_LOG("listen on port %d failed.", configure->listen_port());
    return false;
  }
  {
    // get agent config from sqlite or configure file
    int upload_rate_limit = -1;
    int connections_limit = -1;
    message::AgentOptions agent_options;
    if (GetAgentOptionFromDb(&agent_options)) {
      upload_rate_limit = agent_options.upload_limit() * 1024 * 1024;
      connections_limit = agent_options.max_conections();
    } else {
      upload_rate_limit = configure->upload_limit() * 1024 * 1024;
      connections_limit = configure->connection_limit();
    }
    if (upload_rate_limit > 500 * 1024 * 1024) {
      upload_rate_limit = 500 * 1024 * 1024;
    }
    NOTICE_LOG("agent upload rate limit:%d, connection limit:%d",  upload_rate_limit, connections_limit);

    session_settings settings = libtorrent::high_performance_seed();
    settings.use_php_tracker = false;
    settings.use_c_tracker = true;
    settings.stop_tracker_timeout = 1;
    settings.no_connect_privileged_ports = false;
    settings.listen_queue_size = 200;
    settings.alert_queue_size = 50000;
    settings.num_want = configure->peers_num_want();
    if (configure->disable_os_cache()) {
      settings.disk_io_read_mode = session_settings::disable_os_cache;
      settings.disk_io_write_mode = session_settings::disable_os_cache;
    } else {
      settings.disk_io_read_mode = session_settings::enable_os_cache;
      settings.disk_io_write_mode = session_settings::enable_os_cache;
    }
    settings.max_metadata_size = configure->max_metadata_size() * 1024 * 1024;
    settings.min_announce_interval = configure->seeding_announce_interval();
    settings.min_reconnect_time = configure->min_reconnect_time();
    settings.peer_connect_timeout = configure->peer_connect_timeout();
    settings.upload_rate_limit = upload_rate_limit; 
    settings.download_rate_limit = configure->download_limit() * 1024 * 1024;
    settings.max_queued_disk_bytes = configure->max_queued_disk_bytes() * 1024 * 1024;
    settings.max_out_request_queue = configure->max_out_request_queue();
    settings.max_allowed_in_request_queue = configure->max_allowed_in_request_queue();
    settings.whole_pieces_threshold = configure->whole_pieces_threshold();
    settings.request_queue_time = configure->request_queue_time();
    settings.cache_size = configure->cache_size() * 64;
    settings.cache_expiry = configure->cache_expiry();
    settings.read_cache_line_size = configure->read_cache_line_size();
    settings.write_cache_line_size = configure->write_cache_line_size();
    settings.file_pool_size = configure->file_pool_size();
    settings.send_buffer_watermark = configure->send_buffer_watermark() * 1024 * 1024;
    settings.send_buffer_low_watermark = configure->send_buffer_low_watermark() * 1024;
    settings.send_socket_buffer_size = configure->send_socket_buffer_size() * 1024;
    settings.recv_socket_buffer_size = configure->recv_socket_buffer_size() * 1024;
    settings.dont_count_slow_torrents = true;
    settings.active_seeds = configure->active_seeds();
    settings.active_limit = configure->active_seeds();
    settings.connections_limit = connections_limit;
    settings.max_peerlist_size = connections_limit;
    settings.ignore_limits_on_local_network = false;
    settings.enable_outgoing_utp = false;
    settings.enable_incoming_utp = false;
    settings.auto_manage_prefer_seeds = true;
    settings.incoming_starts_queued_torrents = true;
    settings.seeding_outgoing_connections = true;
    settings.choking_algorithm = session_settings::auto_expand_choker;
    settings.seed_choking_algorithm = session_settings::round_robin;
    settings.unchoke_slots_limit = 1000;
    settings.anonymous_mode = false;
    settings.handshake_timeout = 10;
    if (configure->suggest_mode()) {
      settings.explicit_read_cache = true;
      settings.suggest_mode = session_settings::suggest_read_cache;
    } else {
      settings.explicit_read_cache = false;
      settings.suggest_mode = session_settings::no_piece_suggestions;
    }
    session_->set_settings(settings);
  }

  session_->add_extension(&libtorrent::create_metadata_plugin);
  session_->add_extension(&create_bbts_torrent_plugin);
  listen_endpoint_ = libtorrent::tcp::endpoint(
          libtorrent::address::from_string(GetLocalHostIPString(), ec), session_->listen_port());
  thrift_tracker_.SetAttribute(session_->id().to_string(),
                               listen_endpoint_.address().to_string(ec));
  session_->add_ex_announce_func(
      boost::bind(&ThriftTracker::ThriftTrackerAnnounce, &thrift_tracker_, _1, _2));

  worker_pool_.get_io_service().post(boost::bind(&TaskManager::ProcessAlert, this));
  AddResumeTask();
  worker_pool_.Start(configure->worker_thread_num());
  return true;
}

void TaskManager::Join() {
  control_server_.Join();
  worker_pool_.JoinAll();
  session_.reset();
}

void TaskManager::Stop() {
  control_server_.Stop();
  PersistTasks();
  usleep(100000);  //  wait for task quit announce 100ms
  worker_pool_.Stop();
}

void TaskManager::AddTask(const shared_ptr<UnixSocketConnection> &connection,
                          message::AddTask &message,
                          error_code &ec) {
  message::Task *task_info = message.mutable_task();
  {
    struct ucred cred = boost::any_cast<struct ucred>(connection->get_user_argument());
    task_info->set_uid(cred.uid);
    task_info->set_gid(cred.gid);
  }

  boost::intrusive_ptr<torrent_info> ti;
  if (task_info->has_torrent_path()) {
    int64_t size = libtorrent::file_size(task_info->torrent_path().c_str());
    if (size <= 0) {
      WARNING_LOG("torrent file(%s) size not correct.", task_info->torrent_path().c_str());
      ec.assign(bbts::errors::TORRENT_FILE_ERROR, get_error_category());
      return;
    }
    ti.reset(new torrent_info(task_info->torrent_path(), ec, 0, task_info->new_name()));
    if (ec) {
      WARNING_LOG("parse torrent file(%s) failed, ret(%d): %s.",
                   task_info->torrent_path().c_str(), ec.value(), ec.message().c_str());
      return;
    }
    task_info->set_infohash(libtorrent::to_hex(ti->info_hash().to_string()));
  } else if (task_info->has_data()) {
    const string &data = task_info->data();
    ti.reset(new torrent_info(data.data(), data.length(), ec, 0, task_info->new_name()));
    if (ec) {
      WARNING_LOG("parse torrent data failed, ret(%d): %s.", ec.value(), ec.message().c_str());
      return;
    }
  } else if (task_info->has_infohash()) {
    string bytes;
    if (task_info->infohash().length() != 40 || !HexstrToBytes(task_info->infohash(), &bytes)) {
      WARNING_LOG("infohash(%s) not correct.", task_info->infohash().c_str());
      ec.assign(bbts::errors::INFOHASH_NOT_CORRECT, get_error_category());
      return;
    }
    ti.reset(new torrent_info(libtorrent::sha1_hash(bytes), 0, task_info->new_name()));
    shared_ptr<metadata_t> meta = GetMetaByInfohash(task_info->infohash());
    if (meta) {
      libtorrent::lazy_entry metadata;
      libtorrent::lazy_bdecode(meta->metadata.c_str(),
                               meta->metadata.c_str() + meta->metadata.length(), metadata, ec);
      if (ec) {
        WARNING_LOG("bdecode metadata(%s) failed: %s.",
                     task_info->infohash().c_str(), ec.message().c_str());
        return;
      }
      if (!ti->parse_info_section(metadata, ec, 0)) {
        WARNING_LOG("parse metadata(%s) failed: %s",
                     task_info->infohash().c_str(), ec.message().c_str());
        return;
      }
      NOTICE_LOG("get metadata(%s) success", task_info->infohash().c_str());
    } else {
      WARNING_LOG("%s: no metadata to find", task_info->infohash().c_str());
      ec.assign(bbts::errors::NO_METADATA, get_error_category());
      return;
    }
  } else {
    WARNING_LOG("not set info_hash or torrent_path or torrent_url");
    ec.assign(bbts::errors::MSG_ARGS_ERROR, get_error_category());
    return;
  }

  if (task_info->new_name().empty()) {
    task_info->set_new_name(ti->name());
  }

  {
    shared_ptr<Task> dup_task;
    string data_path = task_info->save_path() + '/' + task_info->new_name();
    scoped_lock lock(tasks_lock_);
    infohash_map_t::iterator infohash_it = infohash_map_.find(task_info->infohash());
    data_path_map_t::iterator data_path_it = data_path_map_.find(data_path);
    if (infohash_it != infohash_map_.end()) {
      // TODO 区分data path是否相同
      dup_task = infohash_it->second.lock();
    } else if (data_path_it != data_path_map_.end()) {
      dup_task = data_path_it->second.lock();
    }
    if (dup_task) {
      DEBUG_LOG("dup task type: %d, new task type: %d", dup_task->get_type(), task_info->type());
      if (dup_task->get_type() == message::NOCHECK_TASK
          || task_info->type() == message::SEEDING_TASK) {
        WARNING_LOG("new task conflict with task(%ld), it will quit.", dup_task->get_id());
        ERASE_TASK_FROM_MAP(dup_task);
        dup_task->RemoveTorrent();
        dup_task->Cancel();
      } else {
        WARNING_LOG("add torrent(%s) failed: new nocheck task(%s:%s) conflict with seed task.",
                     task_info->infohash().c_str(), data_path.c_str());
        ec.assign(bbts::errors::DUP_TASK_ERROR, get_error_category());
        return;
      }
    }
  }

  shared_ptr<Task> task;
  {
    scoped_lock lock(tasks_lock_);
    task = Task::Create(message, ti, ec);
    if (ec) {
      return;
    }
    ADD_TASK_TO_MAP(task);
  }

  NOTICE_LOG("add task(%ld) success", task->get_id());
  message::TaskRes response;
  response.set_taskid(task->get_id());
  WriteMessage(connection, RES_TASK, response);
  return;
}

void TaskManager::CtrlTask(int64_t taskid,
                           const tasks_map_t::iterator &it,
                           const struct ucred &cred,
                           message::BatchCtrl::ctrl_t type,
                           message::BatchCtrlRes &response,
                           bool skip) {
  message::TaskRes task_res;
  message::BaseRes *base_res = task_res.mutable_res();
  task_res.set_taskid(taskid);
  if (it == tasks_map_.end()) {
    base_res->set_ret_code(bbts::errors::TASK_NOT_FOUND);
    base_res->set_fail_msg("task not found");
    response.add_tasks()->CopyFrom(task_res);
    return;
  }

  shared_ptr<Task> task = it->second;
  if (!task->CheckCred(cred)) {
    if (!skip) {
      base_res->set_ret_code(bbts::errors::CHECK_CRED_FAILED);
      base_res->set_fail_msg("check cred fail");
      response.add_tasks()->CopyFrom(task_res);
    }
    return;
  }

  switch (type) {
    case message::BatchCtrl::CANCEL:
      ERASE_TASK_FROM_MAP(task);
      task->Cancel();
      break;
    case message::BatchCtrl::PAUSE:
      task->Pause();
      break;
    case message::BatchCtrl::RESUME:
      task->Resume();
      break;
    default:
      break;        //not reach here
  }
  base_res->set_ret_code(bbts::errors::NO_ERROR);
  base_res->set_fail_msg("No error");
  response.add_tasks()->CopyFrom(task_res);
}

void TaskManager::BatchCtrlTasks(const shared_ptr<UnixSocketConnection> &connection,
                                 const message::BatchCtrl &message) {
  struct ucred cred = boost::any_cast<struct ucred>(connection->get_user_argument());
  message::BatchCtrlRes response;
  tasks_map_t::iterator it;
  if (message.ctrl_all()) {
    scoped_lock lock(tasks_lock_);
    for (it = tasks_map_.begin(); it != tasks_map_.end();) {
      tasks_map_t::iterator curit = it++;
      CtrlTask(curit->first, curit, cred, message.ctrl_type(), response, true);
    }
  } else {
    scoped_lock lock(tasks_lock_);
    for (int i = 0; i < message.taskids_size(); ++i) {
      int64_t taskid = message.taskids(i);
      it = tasks_map_.find(taskid);
      CtrlTask(taskid, it, cred, message.ctrl_type(), response);
    }
  }
  WriteMessage(connection, RES_BATCH_CTRL, response);
}

void TaskManager::ListTask(int64_t taskid,
                           const tasks_map_t::iterator &it,
                           message::BatchListRes &response) {
  shared_ptr<Task> task;
  if (it == tasks_map_.end()) {
    task = Task::Create(taskid);
  } else {
    task = it->second;
  }
  message::TaskStatus *task_status = response.add_status();
  task->GetStatus(task_status);
}

void TaskManager::ListTasks(const shared_ptr<UnixSocketConnection> &connection,
                            const message::BatchCtrl &message) {
  message::BatchListRes response;
  tasks_map_t::iterator it;
  if (message.ctrl_all()) {
    scoped_lock lock(tasks_lock_);
    for (it = tasks_map_.begin(); it != tasks_map_.end(); ++it) {
      ListTask(it->first, it, response);
    }
  } else {
    scoped_lock lock(tasks_lock_);
    for (int i = 0; i < message.taskids_size(); ++i) {
      int64_t taskid = message.taskids(i);
      it = tasks_map_.find(taskid);
      ListTask(taskid, it, response);
    }
  }
  WriteMessage(connection, RES_BATCH_LIST, response);
}

void TaskManager::TaskSetopt(const shared_ptr<UnixSocketConnection> &connection,
                             const message::TaskOptions &options,
                             error_code &ec) {
  shared_ptr<Task> task;
  {
    scoped_lock lock(tasks_lock_);
    tasks_map_t::iterator it = tasks_map_.find(options.taskid());
    if (it == tasks_map_.end()) {
      WARNING_LOG("task(%ld) not found in running tasks", options.taskid());
      ec.assign(bbts::errors::TASK_NOT_FOUND, get_error_category());
      return;
    }
    task = it->second;
    if (!task->CheckCred(boost::any_cast<struct ucred>(connection->get_user_argument()))) {
      ec.assign(bbts::errors::CHECK_CRED_FAILED, get_error_category());
      return;
    }
  }
  task->SetOptions(options);
  WriteBaseMessage(connection, ec);
}

void TaskManager::TaskGetopt(const shared_ptr<UnixSocketConnection> &connection,
                             message::TaskOptions *options,
                             error_code &ec) {
  shared_ptr<Task> task;
  {
    scoped_lock lock(tasks_lock_);
    tasks_map_t::iterator it = tasks_map_.find(options->taskid());
    if (it == tasks_map_.end()) {
      WARNING_LOG("task(%ld) not found in running task", options->taskid());
      ec.assign(bbts::errors::TASK_NOT_FOUND, get_error_category());
      return;
    }
    task = it->second;
  }
  task->GetOptions(options);
  WriteMessage(connection, RES_TASK_GETOPT, *options);
}
void TaskManager::AgentSetopt(const shared_ptr<UnixSocketConnection> &connection,
                              const message::AgentOptions &options,
                              error_code &ec) {
  struct ucred cred = boost::any_cast<struct ucred>(connection->get_user_argument());
  if (cred.uid != 0 && cred.uid != get_self_uid()) {
    NOTICE_LOG("uid %d have no cred for change agent opt.", cred.uid);
    ec.assign(bbts::errors::CHECK_CRED_FAILED, get_error_category());
    return;
  }
  session_settings settings = session_->settings();
  if (options.has_max_conections()) {
    settings.connections_limit = options.max_conections();
  }
  if (options.has_upload_limit()) {
    settings.upload_rate_limit = options.upload_limit();
  }
  session_->set_settings(settings);

  // update sqlite db
  int upload_rate_limit = static_cast<int>(settings.upload_rate_limit / 1024 / 1024);
  std::stringstream sql;
  sql << "update agent_config set connection_limit = " << settings.connections_limit
      << ", upload_limit = " << upload_rate_limit;
  bool is_update_success = false;
  for (int i = 0; i < 3; ++i) {
    if (!task_db_->Excute(sql.str())) {
      WARNING_LOG("update agent options in sqlite(%s) failed:%d", sql.str().c_str(), i);
      task_db_->Reconnect();
      continue;
    }
    is_update_success = true;
  }

  if (!is_update_success) {
    WARNING_LOG("update agent options failed!");
  }

  string upload_limit;
  BytesToReadable(settings.upload_rate_limit, &upload_limit);
  TRACE_LOG("Agent setopt uplimit: %s/s, maxconn: %d",
             upload_limit.c_str(), settings.connections_limit);
  WriteBaseMessage(connection, ec);
}

void TaskManager::AgentGetopt(const shared_ptr<UnixSocketConnection> &connection,
                              message::AgentOptions *options) {
  session_settings settings = session_->settings();
  options->set_bind_port(session_->listen_port());
  if (options->has_max_conections()) {
    options->set_max_conections(settings.connections_limit);
  }
  if (options->has_upload_limit()) {
    options->set_upload_limit(settings.upload_rate_limit);
  }
  WriteMessage(connection, RES_AGENT_GETOPT, *options);
}

void TaskManager::AddMetadata(const message::Metadata &message) {
	AgentConfigure *configure = LazySingleton<AgentConfigure>::instance();
  int32_t metadata_length = message.data().length();

  int max_total_meta_size = configure->max_total_meta_size() * 1024 * 1024;
  // meta too big or small
  if (metadata_length < 1 || metadata_length > max_total_meta_size) {
    return;
  }

  scoped_lock lock(metas_lock_);
  int metas_size = metas_.size();
  while (metas_size > 0) {
    if ((metadata_length + current_metas_total_size_ > max_total_meta_size) ||
        (metas_size + 1 > configure->max_total_meta_num())) {
      current_metas_total_size_ -= metas_.front()->metadata.length();
      metas_.pop_front();
      --metas_size;
    } else {
      break;
    }
  }

  shared_ptr<metadata_t> meta(new metadata_t());
  meta->infohash = message.infohash();
  meta->metadata = message.data();
  meta->add_time = time(NULL);
  metas_.push_back(meta);
  current_metas_total_size_ += metadata_length;
  NOTICE_LOG("add metadata success: %s", meta->infohash.c_str());
}

#define PARSE_MSG(msg) \
do {\
    if (!message.ParseFromArray(ptr + sizeof(type), data->size() - sizeof(type))) {\
      WARNING_LOG("parse message " msg " failed, type(%u)", type);\
      ec.assign(bbts::errors::PROTOBUF_PARSE_ERROR, get_error_category());\
      break;\
    }\
} while(0)

void TaskManager::ProcessMessage(const shared_ptr<UnixSocketConnection> &connection,
                                 const shared_ptr<const std::vector<char> > &data) {
  const char *ptr = &(*data)[0];
  uint32_t type = *reinterpret_cast<const uint32_t *>(ptr);
  DEBUG_LOG("control handler, type: %u", type);
  error_code ec;
  switch (type) {
    case REQ_BATCH_CTRL: {
      message::BatchCtrl message;
      PARSE_MSG("REQ_BATCH_CTRL");
      if (message.ctrl_type() == message.LIST) {
        ListTasks(connection, message);
      } else {
        BatchCtrlTasks(connection, message);
      }
      break;
    }

    case REQ_ADD_TASK: {
      message::AddTask message;
      PARSE_MSG("REQ_ADD_TASK");
      AddTask(connection, message, ec);
      break;
    }

    case REQ_ADD_METADATA: {
      message::Metadata message;
      PARSE_MSG("REQ_ADD_METADATA");
      AddMetadata(message);
      WriteBaseMessage(connection, ec);
      break;
    }

    case REQ_TASK_SETOPT: {
      message::TaskOptions message;
      PARSE_MSG("REQ_TASK_GETOPT");
      TaskSetopt(connection, message, ec);
      break;
    }

    case REQ_TASK_GETOPT: {
      message::TaskOptions message;
      PARSE_MSG("REQ_TASK_GETOPT");
      TaskGetopt(connection, &message, ec);
      break;
    }

    case REQ_AGENT_SETOPT: {
      message::AgentOptions message;
      PARSE_MSG("REQ_AGENT_SETOPT");
      AgentSetopt(connection, message, ec);
      break;
    }

    case REQ_AGENT_GETOPT: {
      message::AgentOptions message;
      PARSE_MSG("REQ_AGENT_GETOPT");
      AgentGetopt(connection, &message);
      break;
    }

    default:
      WARNING_LOG("message type (%d) not support.", type);
      ec.assign(bbts::errors::MSG_ARGS_ERROR, get_error_category());
      break;
  }

  if (ec) {
    WriteBaseMessage(connection, ec);
  }
}

void TaskManager::OnReadMessage(const shared_ptr<UnixSocketConnection> &connection,
                                const shared_ptr<const std::vector<char> > &data) {
  worker_pool_.get_io_service().post(
      boost::bind(&TaskManager::ProcessMessage, this, connection, data));
}

void TaskManager::OnAccept(const boost::shared_ptr<UnixSocketConnection> &connection) {
  int fd = connection->get_socket().native_handle();
  DEBUG_LOG("process message fd: %d", fd);
  struct ucred cred;
  if (!bbts::recv_cred(fd, &cred)) {
    WARNING_LOG("read cred failed.");
    WriteBaseMessage(connection, error_code(bbts::errors::READ_CRED_FAILED, get_error_category()));
    return;
  }
  connection->set_user_argument(cred);
}

void TaskManager::SeedingTimerCallback(int taskid, const error_code &ec) {
  if (ec) {
    TRACE_LOG("seeding timer callback error: %s, taskid: %ld", ec.message().c_str(), taskid);
    return;
  }
  tasks_map_t::iterator it;
  scoped_lock lock(tasks_lock_);
  it = tasks_map_.find(taskid);
  if (it != tasks_map_.end()) {
    shared_ptr<Task> task = it->second;
    NOTICE_LOG("task(%ld) finished, will quit.", task->get_id());
    ERASE_TASK_FROM_MAP(task);
  }
}

void TaskManager::RemoveTaskByError(shared_ptr<Task> task, const string &error) {
  task->set_error(error);
  WARNING_LOG("task(%ld) failed, will quit. error: %s.", task->get_id(), error.c_str());
  ERASE_TASK_FROM_MAP(task);
}

void TaskManager::ProcessTaskByTorrent(torrent_handle torrent,
                                       boost::function<void(shared_ptr<Task>)> cb) {
  scoped_lock lock(tasks_lock_);
  if (!torrent.is_valid()) {
    return;
  }
  torrent_map_t::iterator it = torrent_map_.find(torrent);
  if (it != torrent_map_.end()) {
    shared_ptr<Task> task = it->second.lock();
    if (task) {
      cb(task);
    }
  }
}

void TaskManager::OnTorrentFinished(const libtorrent::torrent_finished_alert *alert) {
  NOTICE_LOG("%s", alert->message().c_str());
  ProcessTaskByTorrent(alert->handle, boost::bind(&Task::ToSeeding, _1));
}

void TaskManager::OnListenFailed(const libtorrent::listen_failed_alert *alert) {
  const libtorrent::tcp::endpoint &ep = alert->endpoint;
  if (alert->sock_type == libtorrent::listen_failed_alert::tcp
      && ep.address().is_v4()
      && ep.port() == LazySingleton<AgentConfigure>::instance()->listen_port()) {
    FATAL_LOG("%s.", alert->message().c_str());
    Stop();
  }
}

void TaskManager::OnPeerDisconnect(const libtorrent::peer_disconnected_alert *alert) {
  error_code ec = alert->error;
  if (ec.value() == boost::asio::error::eof ||  // End of file
      ec.value() == libtorrent::errors::upload_upload_connection ||
      ec.value() == libtorrent::errors::torrent_aborted) {
    return;
  }

  DEBUG_LOG("Peer disconnect alert:%s", alert->message().c_str());
}

void TaskManager::OnTorrentError(const libtorrent::torrent_error_alert *alert) {
  ProcessTaskByTorrent(alert->handle, boost::bind(
      &TaskManager::RemoveTaskByError, this, _1, alert->error.message()));
}

void TaskManager::ProcessAlert() {
  if (NULL == session_->wait_for_alert(libtorrent::milliseconds(200))) {
    worker_pool_.get_io_service().post(boost::bind(&TaskManager::ProcessAlert, this));
    return;
  }

  deque<alert *> alerts;
  session_->pop_alerts(&alerts);
  for (deque<alert *>::iterator it = alerts.begin(); it != alerts.end(); ++it) {
    switch ((*it)->type()) {
      case libtorrent::listen_failed_alert::alert_type:
        OnListenFailed((libtorrent::listen_failed_alert *)(*it));
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

      case libtorrent::listen_succeeded_alert::alert_type:
      case libtorrent::add_torrent_alert::alert_type:
      case libtorrent::torrent_added_alert::alert_type:
      case libtorrent::torrent_removed_alert::alert_type:
      case libtorrent::torrent_deleted_alert::alert_type:
      case libtorrent::url_seed_alert::alert_type:
      case libtorrent::peer_blocked_alert::alert_type:
      case libtorrent::peer_ban_alert::alert_type:
      case libtorrent::state_changed_alert::alert_type:
      case libtorrent::torrent_resumed_alert::alert_type:
      case libtorrent::torrent_paused_alert::alert_type:
        NOTICE_LOG("alert: %s.", (*it)->message().c_str());
        break;

      case libtorrent::peer_error_alert::alert_type:
      case libtorrent::hash_failed_alert::alert_type:
      case libtorrent::torrent_delete_failed_alert::alert_type:
      case libtorrent::metadata_failed_alert::alert_type:
      case libtorrent::tracker_warning_alert::alert_type:
      case libtorrent::tracker_error_alert::alert_type:
      case libtorrent::file_error_alert::alert_type:
      case libtorrent::metadata_received_alert::alert_type:
        WARNING_LOG("alert: %s.", (*it)->message().c_str());
        break;

      case libtorrent::tracker_reply_alert::alert_type:
      case libtorrent::tracker_announce_alert::alert_type:
        TRACE_LOG("alert: %s.", (*it)->message().c_str());
        break;

      case libtorrent::peer_disconnected_alert::alert_type:
        OnPeerDisconnect(static_cast<libtorrent::peer_disconnected_alert *>(*it));
        break;

      case libtorrent::performance_alert::alert_type:
      case libtorrent::peer_connect_alert::alert_type:
      case libtorrent::incoming_connection_alert::alert_type:
      case peer_handshake_alert::alert_type:
      default:
        DEBUG_LOG("alert: %s.", (*it)->message().c_str());
        break;
    }

    delete *it;
  }
  alerts.clear();
  worker_pool_.get_io_service().post(boost::bind(&TaskManager::ProcessAlert, this));
}

} // namespace agent
} // namespace bbts

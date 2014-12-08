/***************************************************************************
 *
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file   Task.cpp
 *
 * @author liuming03
 * @date   2013-1-16
 */

#include <fstream>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/system/error_code.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/session.hpp>

#include "bbts/agent/Task.h"
#include "bbts/agent/TaskDB.h"
#include "bbts/agent/TaskManager.h"
#include "bbts/configure.pb.h"
#include "bbts/ErrorCategory.h"
#include "bbts/host_info_util.h"
#include "bbts/LazySingleton.hpp"
#include "bbts/log.h"
#include "bbts/number_util.h"
#include "bbts/path.h"
#include "bbts/encode.h"

using std::pair;
using std::string;
using std::stringstream;
using std::vector;
using boost::scoped_ptr;
using boost::scoped_array;
using boost::shared_ptr;
using boost::shared_array;
using boost::intrusive_ptr;
using boost::system::error_code;
using boost::posix_time::seconds;
using libtorrent::torrent_info;
using libtorrent::add_torrent_params;
using libtorrent::torrent_handle;
using libtorrent::torrent_status;
using libtorrent::entry;
using libtorrent::lazy_entry;
using libtorrent::sha1_hash;
using bbts::message::AgentConfigure;

namespace bbts {
namespace agent {

Task::Task(int64_t id)
  : flags_(0),
    id_(id),
    type_(message::NOCHECK_TASK),
    seeding_time_(0),
    seeding_timer_(TaskManager::instance().get_io_service()) {
  stat_.hostname = GetLocalHostname();
  libtorrent::tcp::endpoint& ep = TaskManager::instance().get_listen_endpoint();
  error_code ec;
  stat_.ip = ep.address().to_string(ec);
  stat_.port = ep.port();
  stat_.start_time = time(NULL);
  stat_.progress_ppm = 1000000;
}

Task::~Task() {
  if (flags_ & TASK_FLAG_NOT_NEW) {
    UpdateStatus(true);
    if (flags_ & TASK_FLAG_PERSIST) {
      GenerateResumeFile();
      NOTICE_LOG("task(%ld) has been persisted to filesystem.", id_);
    } else {
      DeleteResumeFile();
      NOTICE_LOG("task(%ld) has been delete.", id_);
    }
    RemoveTorrent();
  }
}

void Task::RemoveTorrent() {
  if (torrent_.is_valid()) {
    stat_.payload_uploaded = torrent_.status(0).total_payload_upload;
    stat_.end_time = time(NULL);
    stat_.time_for_seeding = stat_.end_time - stat_.start_time;
    PrintTaskStatistics(stat_, LazySingleton<AgentConfigure>::instance()->task_stat_file());
    try {
      TaskManager::instance().get_session()->remove_torrent(torrent_);
    } catch (libtorrent::libtorrent_exception &e) {
      WARNING_LOG("task(%ld) %s", id_, e.what());
    }
  }
}

void Task::Cancel() {
  flags_ |= TASK_FLAG_CANCELED;
}

void Task::Persist() {
  flags_ |= TASK_FLAG_PERSIST;
}

shared_ptr<Task> Task::Create(int64_t taskid) {
  return shared_ptr<Task>(new Task(taskid));
}

shared_ptr<Task> Task::Create(const message::AddTask &message,
                              const intrusive_ptr<torrent_info> &ti,
                              error_code &ec) {
  shared_ptr<Task> task(new Task(-1));
  const message::Task &task_info = message.task();
  task->infohash_ = task_info.infohash();
  task->cred_.uid = task_info.uid();
  task->cred_.gid = task_info.gid();
  task->cmd_ = task_info.cmd();
  task->save_path_ = task_info.save_path();
  task->new_name_ = task_info.new_name();
  task->seeding_time_ = task_info.seeding_time();
  task->type_ = task_info.type();

  if (!task->GenTaskId()) {
    ec.assign(bbts::errors::GENERATE_TASK_ID_FAILED, get_error_category());
    return shared_ptr<Task>();
  }

  add_torrent_params params;
  params.ti = ti;
  params.save_path = task_info.save_path();
  if (task_info.trackers_size() != 0) {
    for (int i = 0; i < task_info.trackers_size(); ++i) {
      const message::Host &tracker = task_info.trackers(i);
      task->trackers_.push_back(std::make_pair(tracker.ip(), tracker.port()));
    }
    params.thrift_trackers = task->trackers_;
  } else {
    TaskManager::instance().get_thrift_tracker().GetTrackersByInfohash(task->infohash_,
                                                                       &params.thrift_trackers);
  }

  params.flags |= add_torrent_params::flag_paused | add_torrent_params::flag_duplicate_is_error;
  params.flags &= ~add_torrent_params::flag_auto_managed;
  switch (task_info.type()) {
    case message::SEEDING_TASK:
      params.flags |= add_torrent_params::flag_real_seed;
      /* no break */
    case message::NOCHECK_TASK:
      params.flags |= add_torrent_params::flag_seed_mode;
      break;
  }

  const message::TaskOptions& options = message.options();
  if (options.has_upload_limit()) {
    params.upload_limit = options.upload_limit();
    task->stat_.upload_limit = options.upload_limit();
  }
  if (options.has_max_conections()) {
    params.max_connections = options.max_conections();
  }
  torrent_handle th = TaskManager::instance().get_session()->add_torrent(params, ec);
  if (ec) {
    WARNING_LOG("add torrent(%s) failed, %s.", task->infohash_.c_str(), ec.message().c_str());
    return shared_ptr<Task>();
  }
  task->torrent_ = th;

  task->flags_ |= TASK_FLAG_NOT_NEW;
  task->GenerateResumeFile();  //生成恢复数据文件失败，应不影响正常任务执行

  task->stat_.infohash = task->infohash_;
  task->stat_.total_size = ti->total_size();
  task->stat_.num_files = ti->num_files();
  task->stat_.num_paths = ti->get_paths().size();
  task->stat_.num_symlinks = ti->files().get_symlink_file().size();
  task->stat_.num_pieces = ti->num_pieces();
  task->Resume();  //创建和启动的cmd可分离
  return task;
}

shared_ptr<Task> Task::CreateFromResumeFile(const string &resume_file, int64_t file_size) {
  std::ifstream ifs(resume_file.c_str(), std::ios::binary);
  if (!ifs) {
    WARNING_LOG("can't open resume file: %s", resume_file.c_str());
    return shared_ptr<Task>();
  }
  scoped_array<char> buffer(new char[file_size]);
  ifs.read(buffer.get(), file_size);
  ifs.close();
  entry resume_data = libtorrent::bdecode(buffer.get(), buffer.get() + file_size);
  if (resume_data.type() != entry::dictionary_t) {
    WARNING_LOG("bdecode resume file failed: %s, type: %d", resume_file.c_str(), resume_data.type());
    return shared_ptr<Task>();
  }

  shared_ptr<Task> task(new Task(-1));
  task->id_ = resume_data["id"].integer();
  entry &cred_e = resume_data["cred"];
  task->cred_.pid = cred_e["pid"].integer();
  task->cred_.uid = cred_e["uid"].integer();
  task->cred_.gid = cred_e["gid"].integer();
  task->cmd_ = resume_data["cmd"].string();
  task->seeding_time_ = resume_data["seeding_time"].integer();
  task->new_name_ = resume_data["new_name"].string();
  task->save_path_ = resume_data["save_path"].string();
  task->type_ = static_cast<message::TaskType>(resume_data["type"].integer());

  error_code ec;
  add_torrent_params params;
  if (resume_data.find_key("info_hash") != NULL) {
    task->infohash_ = resume_data["info_hash"].string();
    string bytes;
    if (task->infohash_.length() != 40 || !HexstrToBytes(task->infohash_, &bytes)) {
      WARNING_LOG("task(%ld) info hash(%s) not correct.", task->id_, task->infohash_.c_str());
      return shared_ptr<Task>();
    }
    params.ti = new torrent_info(sha1_hash(bytes), 0, task->new_name_);
    entry *meta_entry = resume_data.find_key("metadata");
    if (meta_entry) {
      string &meta_buf = meta_entry->string();
      lazy_entry metadata;
      int ret = libtorrent::lazy_bdecode(meta_buf.c_str(), meta_buf.c_str() + meta_buf.length(), metadata, ec);
      if (ret != 0 || !params.ti->parse_info_section(metadata, ec, 0)) {
        WARNING_LOG("Parse metadata from resume file(%) failed: %s.", resume_file.c_str(), ec.message().c_str());
        return shared_ptr<Task>();
      }
    } else {
      WARNING_LOG("Resume file(%s) not found metadata!", resume_file.c_str());
      return shared_ptr<Task>();
    }
  } else {
    WARNING_LOG("Resume file(%s) not found infohash!", resume_file.c_str());
    return shared_ptr<Task>();
  }

  if (resume_data.find_key("trackers") != NULL) {
    entry::list_type &trackers_e = resume_data["trackers"].list();
    for (entry::list_type::iterator it = trackers_e.begin(); it != trackers_e.end(); ++it) {
      task->trackers_.push_back(std::make_pair((*it)["host"].string(), (*it)["port"].integer()));
    }
    params.thrift_trackers = task->trackers_;
  } else {
    TaskManager::instance().get_thrift_tracker().GetTrackersByInfohash(task->infohash_,
                                                                       &params.thrift_trackers);
  }

  params.save_path = task->save_path_;
  params.flags |= add_torrent_params::flag_paused | add_torrent_params::flag_duplicate_is_error;
  params.flags &= ~add_torrent_params::flag_auto_managed;
  switch (task->type_) {
    case message::SEEDING_TASK:
      params.flags |= add_torrent_params::flag_real_seed;
      /* no break */
    case message::NOCHECK_TASK:
      params.flags |= add_torrent_params::flag_seed_mode;
      break;
  }

  torrent_handle th = TaskManager::instance().get_session()->add_torrent(params, ec);
  if (ec) {
    WARNING_LOG("add torrent(%s) failed, %s.", task->infohash_.c_str(), ec.message().c_str());
    return shared_ptr<Task>();
  }
  task->torrent_ = th;
  task->torrent_.set_max_connections(resume_data["max_connections"].integer());
  task->torrent_.set_upload_limit(resume_data["upload_limit"].integer());
  task->flags_ |= TASK_FLAG_NOT_NEW;
  task->stat_.infohash = task->infohash_;
  task->stat_.total_size = params.ti->total_size();
  task->stat_.piece_length = params.ti->piece_length();
  task->stat_.num_files = params.ti->num_files();
  task->stat_.num_paths = params.ti->get_paths().size();
  task->stat_.num_symlinks = params.ti->files().get_symlink_file().size();
  task->stat_.num_pieces = params.ti->num_pieces();
  task->stat_.upload_limit = resume_data["upload_limit"].integer();
  task->Resume();
  return task;
}

static int insert_task_cb(void *arg, int argc, char **argv, char **col_name) {
  *static_cast<int64_t *>(arg) = atol(argv[0]);
  return 0;
}

bool Task::GenTaskId() {
  stringstream sql;
  // 原子操作，为了获取自增id
  sql << "begin transaction; insert into task(infohash, save_path, status, uid, gid, cmd, start_time) values('"
      << infohash_ << "', '" << get_data_path() << "', '"
      << message::TaskStatus_status_t_Name(message::TaskStatus_status_t_DOWNLOAD) << "', '"
      << cred_.uid << "', '" << cred_.gid << "', '" << cmd_ << "', datetime('now', 'localtime'))"
      << "; select last_insert_rowid(); commit transaction";

  for (int i = 0; i < 3; ++i) {
    TaskDB *db = TaskManager::instance().get_task_db();
    if (!db->Excute(sql.str(), &insert_task_cb, &id_) || id_ < 0) {
      WARNING_LOG("excute sql failed(%s), reconnect and try again(%d)", sql.str().c_str(), i);
      db->Reconnect();
      continue;
    }
    break;
  }
  return id_ >= 0;
}

bool Task::CheckCred(const struct ucred &cred) const {
  //root用户或启动agent的用户或任务发起者
  if (!(flags_ & TASK_FLAG_NOT_NEW) || cred.uid == 0 || cred.uid == get_self_uid() || cred.uid == cred_.uid) {
    return true;
  }
  return false;
}

void Task::Resume() const {
  torrent_.resume();
  torrent_.auto_managed(true);
}

void Task::Pause() const {
  torrent_.auto_managed(false);
  torrent_.pause();
}

void Task::SetOptions(const message::TaskOptions &task_opt) const {
  if (task_opt.has_upload_limit()) {
    torrent_.set_upload_limit(task_opt.upload_limit());
  }
  if (task_opt.has_max_conections()) {
    torrent_.set_max_connections(task_opt.max_conections());
  }

  string upload_limit;
  BytesToReadable(torrent_.upload_limit(), &upload_limit);
  NOTICE_LOG("Task(%ld) setopt uplimit: %s/s, maxconn: %d", id_, upload_limit.c_str(), torrent_.max_connections());
}

void Task::GetOptions(message::TaskOptions *task_opt) const {
  if (task_opt->has_upload_limit()) {
    task_opt->set_upload_limit(torrent_.upload_limit());
  }
  if (task_opt->has_max_conections()) {
    task_opt->set_max_conections(torrent_.max_connections());
  }
}

message::TaskStatus::status_t Task::GetStatusCode(const torrent_status &ts, bool is_finish) const {
  if (!ts.error.empty() || !error_.empty()) {
    return message::TaskStatus_status_t_ERROR;
  }

  if (flags_ & TASK_FLAG_CANCELED) {
    return message::TaskStatus_status_t_CANCELED;
  } else if (ts.paused) {
    return message::TaskStatus_status_t_PAUSED;
  } else if (ts.is_finished) {
    if (!is_finish) {
      return (message::TaskStatus_status_t_SEEDING);
    }
    return (message::TaskStatus_status_t_FINISHED);
  } else {
    switch (ts.state) {
      case torrent_status::downloading_metadata:
        return message::TaskStatus_status_t_DTORRENT;
      case torrent_status::checking_resume_data:
        case torrent_status::queued_for_checking:
        return message::TaskStatus_status_t_CHECKQ;
      case torrent_status::checking_files:
        return message::TaskStatus_status_t_CHECKING;
      case torrent_status::downloading:
        default:
        return message::TaskStatus_status_t_DOWNLOAD;
    }
  }
}

static int get_status_cb(void *arg, int argc, char **argv, char **col_name) {
  message::TaskStatus *task_status = (message::TaskStatus *)(arg);
  message::Task *task_info = task_status->mutable_task();
  task_info->set_infohash(string(argv[1]));
  string data_path = string(argv[2]);
  task_info->set_save_path(parent_path(data_path));
  task_info->set_new_name(file_name(data_path));
  task_info->set_uid(atoi(argv[6]));
  task_info->set_gid(atoi(argv[7]));
  task_info->set_cmd(string(argv[8]));

  message::TaskStatus::status_t status;
  if (!message::TaskStatus::status_t_Parse(string(argv[3]), &status)) {
    status = message::TaskStatus_status_t_UNKNOW;
  }
  task_status->set_status(status);
  task_status->set_progress(atoi(argv[4]));
  task_status->set_total_upload(atol(argv[5]));
  task_status->set_error(string(argv[9]));
  return 0;
}

void Task::GetStatus(message::TaskStatus *task_status) const {
  message::Task *task_info = task_status->mutable_task();
  task_info->set_taskid(id_);
  if (flags_ & TASK_FLAG_NOT_NEW) {  //从本地获取状态信息
    task_info->set_cmd(cmd_);
    task_info->set_uid(cred_.uid);
    task_info->set_gid(cred_.gid);
    task_info->set_infohash(infohash_);
    task_info->set_save_path(save_path_);
    task_info->set_new_name(new_name_);

    torrent_status ts = torrent_.status(0);
    task_status->set_status(GetStatusCode(ts));
    task_status->set_error(ts.error.empty() ? error_ : ts.error);
    task_status->set_progress(ts.progress_ppm);
    task_status->set_num_peers(ts.num_peers);
    task_status->set_num_seeds(ts.num_seeds);
    task_status->set_upload_rate(ts.upload_rate);
    task_status->set_total_upload(ts.total_payload_upload);
    return;
  } else {  // 从taskdb获取task状态信息
    stringstream sql;
    sql << "select id, infohash, save_path, status, progress, upload, uid, gid, cmd, error from task where id = "
        << id_;
    bool excute_success = false;
    TaskDB *db = TaskManager::instance().get_task_db();
    for (int i = 0; i < 3; ++i) {
      if (!db->Excute(sql.str(), get_status_cb, task_status)) {
        db->Reconnect();
        WARNING_LOG("excute sql(%s) failed %d.", sql.str().c_str(), i);
        continue;
      }
      excute_success = true;
      break;
    }

    if (!excute_success) {
      task_status->set_status(message::TaskStatus::UNKNOW);
      task_status->set_error("select from db error");
      return;
    }

    if (!task_info->has_infohash()) {
      task_status->set_status(message::TaskStatus::UNKNOW);
      task_status->set_error("task not found");
      WARNING_LOG("not find task %ld", id_);
      return;
    }
  }
}

void Task::ToSeeding() {
  UpdateStatus();
  if (!error_.empty()) {
    seeding_timer_.expires_from_now(seconds(0));
    seeding_timer_.async_wait(boost::bind(
        &TaskManager::SeedingTimerCallback, &TaskManager::instance(), id_, _1));
    return;
  }

  if (seeding_time_ == -1) {
    TRACE_LOG("task(%ld) seeding for infinite", id_, seeding_time_);
    return;
  } else {
    NOTICE_LOG("Task(%ld) seeding for %d seconds", id_, seeding_time_);
    seeding_timer_.expires_from_now(seconds(seeding_time_));
    seeding_timer_.async_wait(boost::bind(
        &TaskManager::SeedingTimerCallback, &TaskManager::instance(), id_, _1));
  }
}

void Task::UpdateStatus(bool is_finish) const {
  if (!(flags_ & TASK_FLAG_NOT_NEW)) {
    return;
  }

  /*
   * status字段在数据库上直接用字符串表示比数字表示的好处是：
   * agent如果改变了相应数字，只要对应的字符串不变就可以，这样数据库中的数据无须跟着进行调整
   */
  string status;
  torrent_status ts = torrent_.status(0);
  status = message::TaskStatus_status_t_Name(GetStatusCode(ts, is_finish));
  string error = ts.error.empty() ? error_ : ts.error;

  stringstream sql;
  sql << "update task set infohash = '" << infohash_.c_str() << "', status = '" << status << "', upload = "
      << ts.total_payload_upload << ", download = " << ts.total_payload_download << ", progress = " << ts.progress_ppm
      << ", error = '" << error << "'";
  if (is_finish) {
    sql << ", end_time = datetime('now', 'localtime')";
  }
  sql << " where id = " << id_;
  TaskDB *db = TaskManager::instance().get_task_db();
  if (!db->Excute(sql.str())) {
    WARNING_LOG("excute sql(%s) failed.", sql.str().c_str());
    db->Reconnect();
    return;
  }

  NOTICE_LOG("update task(%ld) status success.", id_);
}

void Task::GenerateResumeFile() const {
  if (!(flags_ & TASK_FLAG_NOT_NEW)) {
    return;
  }

  entry resume_data;
  entry &cred_e = resume_data["cred"];
  cred_e["pid"] = cred_.pid;
  cred_e["uid"] = cred_.uid;
  cred_e["gid"] = cred_.gid;
  resume_data["id"] = id_;
  resume_data["cmd"] = cmd_;
  resume_data["seeding_time"] = seeding_time_;
  resume_data["save_path"] = save_path_;
  resume_data["new_name"] = new_name_;
  resume_data["upload_limit"] = torrent_.upload_limit();
  resume_data["download_limit"] = torrent_.download_limit();
  resume_data["max_connections"] = torrent_.max_connections();
  resume_data["type"] = type_;
  resume_data["info_hash"] = infohash_;
  if (!trackers_.empty()) {
    entry &trackers_e = resume_data["trackers"];
    for (vector<pair<string, int> >::const_iterator it = trackers_.begin();
        it != trackers_.end(); ++it) {
      entry tracker_e;
      tracker_e["host"] = it->first;
      tracker_e["port"] = it->second;
      trackers_e.list().push_back(tracker_e);
    }
  }

  boost::intrusive_ptr<torrent_info const> ti = torrent_.torrent_file();
  if (ti->info_hash() == sha1_hash(0)) {
    WARNING_LOG("task(%ld) have no metadata to resume file, skip.", id_);
    return;
  }
  shared_array<char> metadata = ti->metadata();
  int metadata_size = ti->metadata_size();
  resume_data["metadata"] = string(metadata.get(), metadata.get() + metadata_size);

  stringstream strm;
  strm << LazySingleton<AgentConfigure>::instance()->resume_dir() << "/" << id_ << "." << "resume";
  std::ofstream ofs(strm.str().c_str(), std::ios::binary);
  if (!ofs) {
    WARNING_LOG("open file failed: %s.", strm.str().c_str());
    return;
  }

  libtorrent::bencode(std::ostream_iterator<char>(ofs), resume_data);
  ofs.close();
  NOTICE_LOG("generate resume data to file: %s", strm.str().c_str());
  return;
}

void Task::DeleteResumeFile() const {
  if (!(flags_ & TASK_FLAG_NOT_NEW)) {
    return;
  }
  stringstream strm;
  strm << LazySingleton<AgentConfigure>::instance()->resume_dir() << "/" << id_ << "." << "resume";
  unlink(strm.str().c_str());
}

} // namespace agent
} // namespace bbts

/***************************************************************************
 *
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/
#include "group_task.h"

#include <fstream>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/system/error_code.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/session.hpp>

#include "bbts/ErrorCategory.h"
#include "bbts/host_info_util.h"
#include "bbts/log.h"
#include "bbts/number_util.h"
#include "bbts/path.h"
#include "bbts/Syms.h"
#include "bbts/encode.h"

#include "GroupConf.pb.h"
#include "group_manager.h"
#include "utils.h"

using boost::posix_time::seconds;
using boost::shared_ptr;
using boost::system::error_code;

using libtorrent::add_torrent_params;
using libtorrent::cluster_config_entry;
using libtorrent::torrent_info;
using libtorrent::torrent_status;
using libtorrent::symlink_file;

using bbts::group::GroupManager;

extern ::bbts::GroupConfig kGroupConfig;

extern void SetSignalAction();

namespace bbts {
namespace group {

GroupTask::GroupTask()
    : control_status_(CONTROL_STATUS_NORMAL),
      seeding_timer_(GroupManager::GetInstance().io_service()), 
      timeout_timer_(GroupManager::GetInstance().io_service()), 
      has_pre_allocate_thread_start_(false),
      last_check_time_(time(NULL)),
      last_check_downloaded_(0),
      is_timeout_(false),
      is_mem_exceed_(false),
      low_downrate_times_(0),
      is_torrent_url_(false) {
  // do nothing
}

GroupTask::~GroupTask() {
  StopClusterTask();
  Persist();
  DEBUG_LOG("task delete");
}

void GroupTask::Persist() {
  if (control_status_ == CONTROL_STATUS_CANCELED ||
      control_status_ == CONTROL_STATUS_FINISHED) {
    // canceled or finished, delete resume file
    DeleteResumeFile();
  } else {
    GenerateResumeFile();
  }
}

int GroupTask::Create(
    const DownloadParam &param,
    const UserCred &cred) {
  params_ = param;
  cred_   = cred;
  task_status_.uid = cred.uid;

  // set signal action first
  SetSignalAction();

  if (params_.save_path.empty() || !check_and_mkdir_r(params_.save_path, 0755)) {
    FATAL_LOG("save path(%s) not correct or can not create.", params_.save_path.c_str());
    return -1;
  }
  task_status_.save_path = params_.save_path;

  error_code ec;
  add_torrent_params torrent_params;
  if (!params_.torrent_path.empty()) {
    int64_t size = libtorrent::file_size(params_.torrent_path.c_str());
    if (size <= 0 || size > kGroupConfig.max_metadata_size()) {
      FATAL_LOG("file(%s) size(%d)invalid.", params_.torrent_path.c_str(), size);
      return -1;
    }

    torrent_params.ti = new torrent_info(params_.torrent_path, ec, 0, params_.new_name);
    if (ec) {
      FATAL_LOG("pares torrent file(%s) failed, ret(%d): %s.",
                 params_.torrent_path.c_str(), ec.value(), ec.message().c_str());
      return -1;
    }
    if (params_.patition_download) {
      ParseNeedDownloadFiles<uint8_t>(params_.include_regex,
                                      params_.exclude_regex,
                                      *(torrent_params.ti),
                                      &torrent_params.file_priorities);
       pre_file_priorities_.resize(torrent_params.file_priorities.size(), 0);
       std::copy(torrent_params.file_priorities.begin(), torrent_params.file_priorities.end(), pre_file_priorities_.begin());
    } else {
      // all files will be downloaded
      pre_file_priorities_.resize(torrent_params.ti->num_files(), 1);
    }
    SetOrgPieces(torrent_params.ti);
  } else if (!params_.torrent_url.empty()) {
    torrent_params.url = params_.torrent_url;
    params_.need_save_resume = false;
    params_.need_down_to_tmp_first = false;
    is_torrent_url_ = true;
  } else if (!params_.infohash.empty()) {
    if (!params_.save_torrent_path.empty()) {
      std::ofstream ofs(params_.save_torrent_path.c_str());
      if (!ofs) {
        FATAL_LOG("can't create torrent file(%s) for save.", params_.save_torrent_path.c_str());
        return -1;
      }
      ofs.close();
    }
    std::string bytes;
    if (params_.infohash.length() != 40 || !bbts::HexstrToBytes(params_.infohash, &bytes)) {
      FATAL_LOG("infohash(%s) not correct.", params_.infohash.c_str());
      return -1;
    }
    torrent_params.info_hash.assign(bytes);
    torrent_params.ti = new torrent_info(torrent_params.info_hash, 0, params_.new_name);
  } else {
    FATAL_LOG("not set infohash or torrent file or torrent_url");
    return -1;
  }
  if (!is_torrent_url_) {
    infohash_ = libtorrent::to_hex(torrent_params.ti->info_hash().to_string());
  }

  std::vector<std::pair<std::string, int> > trackers;
  TurnTrackersListToStdList(params_.trackers, &trackers);
  if (trackers.empty()) {
    GroupManager::GetInstance().thrift_tracker().GetTrackersByInfohash(infohash_, &trackers);
  }
  torrent_params.thrift_trackers = trackers;

  if (is_torrent_url_) {
    // use torrent url path as infohash first
    infohash_ = params_.torrent_url;
  }
  params_.infohash = infohash_;

  if (params_.need_down_to_tmp_first) {
    tmp_save_path_ = params_.save_path + "/.gko3_" + params_.infohash + "_" + params_.new_name + ".tmp";
    if (!check_and_mkdir(tmp_save_path_, 0755)) {
      FATAL_LOG("[%s] tmp save path(%s) can not create.", infohash_.c_str(), tmp_save_path_.c_str());
      return -1;
    }
  } else {
    tmp_save_path_ = params_.save_path;
  }
  torrent_params.save_path = tmp_save_path_;

  std::vector<char> buffer;
  if (params_.need_save_resume) {
    resume_file_ = tmp_save_path_ + "/.gko3_" + params_.infohash + "_" + params_.new_name + ".resume";
    struct stat statbuf;
    if (stat(resume_file_.c_str(), &statbuf) == 0 && (statbuf.st_mode & S_IFREG)) {
      std::ifstream ifs(resume_file_.c_str(), std::ios::binary);
      if (ifs) {
        buffer.resize(statbuf.st_size);
        ifs.read(&buffer[0], statbuf.st_size);
        ifs.close();
        torrent_params.resume_data.swap(buffer);
      } else {
        WARNING_LOG("[%s] can't open resume file: %s, ignore", infohash_.c_str(), resume_file_.c_str());
      }
    }
  }

  if (params_.storage_pre_allocate) {
    torrent_params.storage_mode = libtorrent::storage_mode_allocate;
  }

  torrent_params.flags |= add_torrent_params::flag_duplicate_is_error;
  torrent_params.flags |= add_torrent_params::flag_override_resume_data;
  torrent_params.flags |= add_torrent_params::flag_paused;
  torrent_params.flags &= ~add_torrent_params::flag_auto_managed;

  torrent_params.download_limit = params_.download_limit;
  torrent_params.upload_limit = params_.upload_limit;
  torrent_params.max_connections = params_.connections_limit;
  torrent_params.url_seeds = params_.web_seeds;
  torrent_handle_ = GroupManager::GetInstance().session()->add_torrent(torrent_params, ec);

  if (ec) {
    FATAL_LOG("[%s] add torrent failed: %s.", infohash_.c_str(), ec.message().c_str());
    return -1;
  }

  if (params_.dynamic_allocate >= 0) {
    // set share mode first, so all pieces won't be downloaded before 
    // unset share mode 
    torrent_handle_.set_share_mode(true);
  }

  if (!params_.cluster_config.prefix_path.empty()) {
    if (!ParseSyms()) {
      NOTICE_LOG("[%s] Not support cluster(hadoop) download.", infohash_.c_str());
      return -1;
    }
    cluster_config_entry cluster_config;
    TurnClusterParamToClusterConfigEntry(params_.cluster_config, &cluster_config);
    NOTICE_LOG("[%s] support cluster(hadoop) download.", infohash_.c_str());
    DEBUG_LOG("[%s] cluster path: hdfs://%s@%s:%d%s", infohash_.c_str(),
               cluster_config.user.c_str(),cluster_config.host.c_str(), cluster_config.port,
               cluster_config.prefix_path.c_str());
    cluster_task_.reset(new GroupClusterTask(cluster_config,
                                              torrent_handle_,
                                              params_.cluster_thread_num,
                                              params_.ignore_hdfs_error));
  }

  std::string down_limit, up_limit;
  BytesToReadable(params_.download_limit, &down_limit);
  BytesToReadable(params_.upload_limit, &up_limit);
  TRACE_LOG("[%s] downlimit: %s, uplimit: %s, maxconn: %d",
             infohash_.c_str(), down_limit.c_str(), up_limit.c_str(), torrent_handle_.max_connections());

  // set task_status_ info
  task_status_.start_time = static_cast<int64_t>(time(NULL));
  task_status_.infohash = infohash_;
  task_status_.download_limit = params_.download_limit;
  task_status_.upload_limit = params_.upload_limit;
  if (torrent_params.ti.get() != NULL) {
    // if ti(torrent_info) is NULL, means torrent_url assign
    task_status_.num_files = torrent_params.ti->num_files();
    task_status_.num_paths = torrent_params.ti->get_paths().size();
    task_status_.num_symlinks = torrent_params.ti->files().get_symlink_file().size();
    task_status_.num_pieces = torrent_params.ti->num_pieces();
    task_status_.piece_length = torrent_params.ti->piece_length();
  }

  // Resume file
  GenerateResumeFile();

  // resume task
  Resume();
  
  // set timer
  if (params_.timeout > 0) {
    timeout_timer_.expires_from_now(seconds(params_.timeout));
    timeout_timer_.async_wait(boost::bind(&GroupTask::TimeoutTimerCallback, this, _1));
  }

  return 0;
}

int GroupTask::CreateFromResumeFile(
    const std::string &resume_file,
    int64_t file_size) {
  // load data from resume_file
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

  libtorrent::entry &cred_e = resume_data["cred"];
  UserCred cred;
  cred.pid = cred_e["pid"].integer();
  cred.uid = cred_e["uid"].integer();
  cred.gid = cred_e["gid"].integer();

  // check if task has been canceled or finished
  control_status_ = static_cast<ControlTaskStatus>(resume_data["control_status"].integer());
  if (control_status_ == CONTROL_STATUS_CANCELED ||
      control_status_ == CONTROL_STATUS_FINISHED) {
    WARNING_LOG("task has been canceled or finished!");
    return -1;
  }

  // modify status
  seeding_time_ = resume_data["seeding_time"].integer();
  last_check_downloaded_ = resume_data["last_check_downloaded"].integer();
  last_check_time_ = static_cast<time_t>(resume_data["last_check_time"].integer());
  UnSerializedTaskStatus(resume_data["task_status"].string(), &task_status_);

  DownloadParam param;
  UnSerializedDownParam(resume_data["down_param"].string(), &param);

  // create at last because this will general resume file again
  if (Create(param, cred) != 0) {
    WARNING_LOG("[%s] load from resume file failed.", params_.infohash.c_str());
    return -1;
  }

  return 0;
}

int GroupTask::SetOptions(
    const TaskOptions &options) {
  if (!options.infohash.empty() &&
      options.infohash != infohash_) {
    WARNING_LOG("[%s] options is not empty, but not match this task!",
                 infohash_.c_str(), options.infohash.c_str());
    return -1;
  }

  if (options.download_limit > 0) {
    torrent_handle_.set_download_limit(options.download_limit);
    params_.__set_download_limit(options.download_limit);
  }

  if (options.upload_limit > 0) {
    torrent_handle_.set_upload_limit(options.upload_limit);
    params_.__set_upload_limit(options.upload_limit);
  }

  if (options.connections_limit > 0) {
    torrent_handle_.set_max_connections(options.connections_limit);
    params_.__set_connections_limit(options.connections_limit);
  }
  return 0;
}

int GroupTask::GetOptions(
    TaskOptions *options) {
  if (options == NULL) {
    return -1;
  }

  options->__set_infohash(infohash_);
  options->__set_download_limit(torrent_handle_.download_limit());
  options->__set_upload_limit(torrent_handle_.upload_limit());
  options->__set_connections_limit(torrent_handle_.max_connections());

  return 0;
}

void GroupTask::GenerateResumeTaskString(
    std::string *serialized_str) const {
  assert(serialized_str != NULL);
  SerializedTaskStatus(task_status_, serialized_str);
}

void GroupTask::GenerateResumeFile() const {
  if (infohash_.empty()) {
    return;
  }

  libtorrent::entry resume_data;
  libtorrent::entry &cred_e = resume_data["cred"];
  cred_e["pid"] = cred_.pid;
  cred_e["uid"] = cred_.uid;
  cred_e["gid"] = cred_.gid;
  resume_data["infohash"] = infohash_;
  resume_data["seeding_time"] = seeding_time_;
  resume_data["last_check_downloaded"] = last_check_downloaded_;
  resume_data["last_check_time"] = static_cast<int64_t>(last_check_time_);
  resume_data["control_status"] = control_status_;

  if (!trackers_.empty()) {
    libtorrent::entry &trackers_e = resume_data["trackers"];
    std::vector<std::pair<std::string, int> >::const_iterator it = trackers_.begin();
    for (; it != trackers_.end(); ++it) {
      libtorrent::entry tracker_e;
      tracker_e["host"] = it->first;
      tracker_e["port"] = it->second;
      trackers_e.list().push_back(tracker_e);
    }
  }

  std::string serialized_str;
  SerializedDownParam(params_, &serialized_str);
  resume_data["down_param"] = serialized_str;

  // task status
  GenerateResumeTaskString(&serialized_str);
  resume_data["task_status"] = serialized_str;

  std::string task_resume_file_name = kGroupConfig.resume_dir();
  task_resume_file_name += "/" + infohash_ + ".resume";
  std::ofstream ofs(task_resume_file_name.c_str(), std::ios::binary);
  if (!ofs) {
    WARNING_LOG("[%s] open file failed: %s.", infohash_.c_str(), task_resume_file_name.c_str());
    return;
  }

  libtorrent::bencode(std::ostream_iterator<char>(ofs), resume_data);
  ofs.close();
  NOTICE_LOG("[%s] generate resume data to file: %s", infohash_.c_str(), task_resume_file_name.c_str());
  return;
}

void GroupTask::DeleteResumeFile() const {
  std::string task_resume_file_name = kGroupConfig.resume_dir();
  task_resume_file_name += "/" + infohash_ + ".resume";
  unlink(task_resume_file_name.c_str());
  NOTICE_LOG("[%s] delete resume data file:%s", infohash_.c_str(), task_resume_file_name.c_str());

  if (!resume_file_.empty()) {
    unlink(resume_file_.c_str());
  }

  if (!tmp_save_path_.empty()) {
    rmdir(tmp_save_path_.c_str());
  }
}

int GroupTask::CheckCred(
    const UserCred &cred) const {
  // if operated by root or task's owner
  if (cred.uid == 0 || cred.uid == cred_.uid) {
    return 0;
  }
  return -1;
}

void GroupTask::Resume() {
  torrent_handle_.resume();
  torrent_handle_.auto_managed(true);
}

void GroupTask::Pause() {
  torrent_handle_.auto_managed(false);
  torrent_handle_.pause();
}

void GroupTask::ControlTask(ControlTaskType::type type) {
  switch (type) {
    case ControlTaskType::CANCEL:
      control_status_ = CONTROL_STATUS_CANCELED;
      RemoveTorrent(true);    // remove by user
      break;

    case ControlTaskType::PAUSE:
      control_status_ = CONTROL_STATUS_PAUSED;
      Pause();
      break;

    case ControlTaskType::RESUME:
      control_status_ = CONTROL_STATUS_RESUME;
      Resume();
      break;

    default:
      WARNING_LOG("[%s] control task ,but type[%d] is unknown.", infohash_.c_str(), type);
  }
}

void GroupTask::StopClusterTask() {
  if (cluster_task_) {
    torrent_status ts = torrent_handle_.status(0);
    cluster_task_->StopDownload();
    if (torrent_handle_.is_valid()) {
      stat_.downloaded_from_hdfs = cluster_task_->cluster_total_download();
      NOTICE_LOG("[%s] hadoop download: %lld, p2p download: %lld",
                  infohash_.c_str(), stat_.downloaded_from_hdfs,
                  ts.total_payload_download - stat_.downloaded_from_hdfs);
    }
  }
}

void GroupTask::RemoveTorrent(bool is_remove_by_user) {
  StopClusterTask();
  torrent_status ts = torrent_handle_.status(0);
  if (torrent_handle_.is_valid()) {
    UpdateTaskStatus();
    stat_.payload_downloaded = ts.total_payload_download;
    stat_.payload_uploaded = ts.total_payload_upload;
    stat_.progress_ppm = ts.progress_ppm;
    if (stat_.time_for_download) {
      stat_.time_for_seeding  = time(NULL) - stat_.start_time - stat_.time_for_download_metadata
          - stat_.time_for_check_files - stat_.time_for_download;
    }

    // if comes here, means seeding end, we can assert seeding finished
    GroupManager::GetInstance().RemoveTorrentTmp(torrent_handle_, infohash_);
    task_status_.is_task_finished = true;
    task_status_.time_for_seeding = stat_.time_for_seeding;
    task_status_.end_time = static_cast<int64_t>(time(NULL));

    if (!is_remove_by_user) {
      // normal removed
      task_status_.state = torrent_status::finished;
      task_status_.state_string = "FINISHED";
      control_status_ = CONTROL_STATUS_FINISHED;
    } else {
      task_status_.state = static_cast<int32_t>(CONTROL_STATUS_CANCELED);
      task_status_.state_string = "CANCELED";
      control_status_ = CONTROL_STATUS_CANCELED;
    }

    if (is_timeout_) {
      task_status_.state = static_cast<int32_t>(CONTROL_STATUS_TIMEOUT);
      task_status_.state_string = "TIMEOUT";
    }

    // persit it, so if finished but terminate, resume will keep the latest state
    Persist();
  }
}

void GroupTask::SeedingTimerCallback(const error_code &ec) {
  if (ec) {
    DEBUG_LOG("[%s] seeding timer canceled, %s.", infohash_.c_str(), ec.message().c_str());
    return;
  }
  NOTICE_LOG("[%s] download finished, will quit.", infohash_.c_str());
  RemoveTorrent(false);
}

void GroupTask::UpdateTaskStatus() {
  torrent_status ts = torrent_handle_.status(0);
  task_status_.payload_downloaded = ts.total_payload_download;
  task_status_.payload_uploaded = ts.total_payload_upload;
  task_status_.state = ts.state;
  task_status_.download_rate = ts.download_rate;
  task_status_.upload_rate = ts.upload_rate;
  task_status_.progress = ts.progress;
  task_status_.progress_ppm = ts.progress_ppm;

  // update state string
  switch (ts.state) {
    case torrent_status::queued_for_checking:
      task_status_.state_string = "CHECKQ";
      break;
    case torrent_status::checking_files:
      task_status_.state_string = "CHECKING";
      break;
    case torrent_status::downloading_metadata:
      task_status_.state_string = "DTORRENT";
      break;
    case torrent_status::downloading:
      task_status_.state_string = "DOWNLOAD";
      break;
    case torrent_status::finished:
      task_status_.state_string = "FINISHED";
      break;
    case torrent_status::seeding:
      task_status_.state_string = "SEEDING";
      break;
    case torrent_status::allocating:
      task_status_.state_string = "ALLOCATING";
      break;
    case torrent_status::checking_resume_data:
      task_status_.state_string = "CHECKRESUME";
      break;
    default:
      task_status_.state_string = "UNKNOWN";
      break;
  }

  if (control_status_ == CONTROL_STATUS_PAUSED) {
    task_status_.state = static_cast<int32_t>(CONTROL_STATUS_PAUSED);
    task_status_.state_string = "PAUSED";
  }

}

void GroupTask::ToSeeding() {
  torrent_status ts = torrent_handle_.status(0);
  LogProgress(ts);
  UpdateTaskStatus();
  if (params_.seeding_time == -1) {
    NOTICE_LOG("[%s] seeding for infinite", infohash_.c_str());
    return;
  }

  int seeding_time = 0;
  if (params_.seeding_time >= 0) {
    seeding_time = params_.seeding_time;
  } else {
    int upload_limit = ts.uploads_limit > 0 ? ts.uploads_limit : kGroupConfig.upload_rate_limit();
    seeding_time = (ts.num_peers + 1) * ts.total_payload_download / (1 + ts.num_seeds) / (upload_limit + 1) / 2;
    int tmp = ts.total_download / (ts.download_rate + 1);
    if (tmp < seeding_time) {
      seeding_time = tmp;
    }
  }
  NOTICE_LOG("[%s] seeding for %d seconds", infohash_.c_str(), seeding_time);
  seeding_timer_.expires_from_now(seconds(seeding_time));
  seeding_timer_.async_wait(boost::bind(&GroupTask::SeedingTimerCallback, this, _1));
}

void GroupTask::TimeoutTimerCallback(const error_code &ec) {
  if (ec) {
    DEBUG_LOG("[%s] timeout timer canceled.", infohash_.c_str());
    return;
  }
  WARNING_LOG("[%s] download timeout, will quit.", infohash_.c_str());
  if (!torrent_handle_.status(0).is_finished) {
    is_timeout_ = true;
  }
  RemoveTorrent(false);
  error_code timer_ec;
  timeout_timer_.cancel(timer_ec);
}

void GroupTask::HangCheckCallback() {
  torrent_status ts = torrent_handle_.status(0);
  LogProgress(ts);
  UpdateTaskStatus();
  if (params_.need_save_resume && ts.has_metadata && ts.need_save_resume) {
    torrent_handle_.save_resume_data();
  }
  if (ts.progress_ppm == 1000000) {
    return;
  }

  time_t now = time(NULL);
  if ((ts.state == torrent_status::downloading && ts.total_payload_download == last_check_downloaded_)
      || (ts.state == torrent_status::downloading_metadata &&
          !GroupManager::GetInstance().thrift_tracker().HaveSeed())) {
    if (last_check_time_ + params_.hang_timeout - now <= 0) {
      FATAL_LOG("download timeout, maybe noseeder.");
      is_timeout_ = true;
      RemoveTorrent(false);
      return;
    }
  } else {
    last_check_time_ = now;
    last_check_downloaded_ = ts.total_payload_download;
  }

  // if download limit more than 2M, and down_rate less than 800K in 10 times, reconnect
  if (params_.download_limit > 2048 * 1024 &&
      ts.download_rate < 800 * 1024 &&
      control_status_ != CONTROL_STATUS_PAUSED) {
    ++low_downrate_times_;
    if (low_downrate_times_ > 9) {
      torrent_handle_.pause();
      torrent_handle_.resume();
      low_downrate_times_ = 0;
    }
  }

  // check mem limit
  if (params_.mem_limit > 0) {
    static const long PAGE_SIZE = sysconf(_SC_PAGESIZE);
    long virt, rss;
    FILE *fp = fopen("/proc/self/statm", "r");
    fscanf(fp, "%ld%ld", &virt, &rss);
    fclose(fp);
    rss = rss * PAGE_SIZE / 1024 / 1024;
    if (rss > params_.mem_limit) {
      FATAL_LOG("rss(%ldM) exceed memery limit(%d)\n", rss, params_.mem_limit);
      is_mem_exceed_ = true;
      RemoveTorrent(false);
      return;
    }
  }
}

int GroupTask::OnTorrentFinished() {
  if (params_.dynamic_allocate >= 0) {
    // check if is really finished
    libtorrent::bitfield pieces = torrent_handle_.status(0).pieces;
    DEBUG_LOG("pieces.count() = %d, org.count() = %d", pieces.count(), org_pieces_.count());
    boost::intrusive_ptr<torrent_info const> ti = torrent_handle_.torrent_file();
    int num_pieces = ti->num_pieces();  // defend for empty file
    if (pieces.count() < org_pieces_.count() && num_pieces > 0) {
      // not really finished
      return 0;
    }
  }

  stat_.time_for_download = time(NULL) - stat_.start_time
      - stat_.time_for_download_metadata - stat_.time_for_check_files;
  task_status_.time_for_download = stat_.time_for_download;
  if (params_.need_down_to_tmp_first) {
    NOTICE_LOG("mv data from %s to %s", tmp_save_path_.c_str(), params_.save_path.c_str());
    torrent_handle_.move_storage(params_.save_path);
    return 0;
  }
  return OnStorageMoved();
}

int GroupTask::OnStorageMoved() {
  if (CorrectModeAndSymlinks() != 0) {
    return -1;
  }
  ToSeeding();
  return 0;
}

int GroupTask::OnHashFailed(const libtorrent::hash_failed_alert *alert) {
  WARNING_LOG("alert: %s.", alert->message().c_str());
  if (cluster_task_) {
    cluster_task_->AddHashFailedPiece(alert->piece_index);
  }

  return 0;
}

int GroupTask::OnSaveResumeData(
    const libtorrent::save_resume_data_alert *alert) {
  std::ofstream ofs(resume_file_.c_str(), std::ios::binary);
  if (!ofs) {
    WARNING_LOG("open file failed: %s.", resume_file_.c_str());
    return -1;
  }
  libtorrent::bencode(std::ostream_iterator<char>(ofs), *(alert->resume_data));
  ofs.close();
  return 0;
}

int GroupTask::OnStateChanged(const libtorrent::state_changed_alert *alert) {
  boost::intrusive_ptr<torrent_info const> ti = torrent_handle_.torrent_file();
  if (alert->state != torrent_status::downloading_metadata && ti &&
      params_.dynamic_allocate >= 0 && !has_pre_allocate_thread_start_) {
    boost::thread tmp_thread(boost::bind(&GroupTask::PreAllocateThread, this));
    allocate_threads_.swap(tmp_thread);
    has_pre_allocate_thread_start_ = true;
  } 

  if (alert->state != torrent_status::downloading) {
    return 0;
  }

  stat_.time_for_check_files = time(NULL) - stat_.start_time - stat_.time_for_download_metadata;
  task_status_.time_for_check_files = stat_.time_for_check_files;

  if (params_.debug && params_.patition_download) {
    std::vector<int> file_priorities = torrent_handle_.file_priorities();
    for (unsigned i = 0; i < file_priorities.size(); ++i) {
      DEBUG_LOG("[%s] file %d priorities: %d", infohash_.c_str(), i, file_priorities[i]);
    }
  }

  cluster_config_entry cluster_config;
  if (ti) {
      BytesToHex(ti->info_hash().to_string(), &stat_.infohash);
      stat_.total_size = ti->total_size();
      stat_.piece_length = ti->piece_length();
      stat_.num_files = ti->num_files();
      stat_.num_paths = ti->get_paths().size();
      stat_.num_symlinks = ti->files().get_symlink_file().size();
      stat_.num_pieces = ti->num_pieces();
      cluster_config = ti->cluster_config();
  }

  if (cluster_task_.get() == NULL) {
    if (cluster_config.port != 0 && !cluster_config.host.empty() &&
        !cluster_config.user.empty() && !cluster_config.passwd.empty()) {
      if (!ParseSyms()) {
        NOTICE_LOG("[%s] Not support cluster(hadoop) download.", infohash_.c_str());
        return false;
      }
      NOTICE_LOG("[%s] Support hadoop download, will download from hadoop.", infohash_.c_str());
      stat_.is_hdfs_download = true;
      std::stringstream strm;
      strm << "hdfs://" << cluster_config.user << ':' << cluster_config.passwd << '@'
           << cluster_config.host << ':' << cluster_config.port << cluster_config.prefix_path;
      stat_.hdfs_address = strm.str();
      cluster_task_.reset(new GroupClusterTask(cluster_config,
                                               torrent_handle_,
                                               params_.cluster_thread_num,
                                               params_.ignore_hdfs_error));
    }
  }

  if (cluster_task_.get() != NULL) {
    NOTICE_LOG("[%s] will start cluster thread.", infohash_.c_str());
    if (!cluster_task_->StartDownload()) {
      FATAL_LOG("[%s] start cluster download failed.", infohash_.c_str());
      RemoveTorrent(false);
      return false;
    }

    // push it into queue here, so cluster_task_ will not be delete until task finished
    GroupManager::GetInstance().PushClusterThreadQueue(cluster_task_);
    // need set signal action again, because hdfs will set itself
    SetSignalAction();
  }

  return 0;
}

int GroupTask::OnMetadataReceived() {
  stat_.time_for_download_metadata = time(NULL) - stat_.start_time;
  boost::intrusive_ptr<torrent_info const> ti = torrent_handle_.torrent_file();
  
  // update infohash_ for web url
  if (is_torrent_url_) {
    infohash_ = libtorrent::to_hex(ti->info_hash().to_string());
    params_.infohash = infohash_;
    task_status_.infohash = infohash_;
    // TODO need update group manager
    GroupManager::GetInstance().UpdateInfohashByTorrentUrl(params_.torrent_url, infohash_);
  }
  pre_file_priorities_.resize(ti->num_files(), 1);
  if (params_.patition_download) {
    std::vector<int> file_priorities;
    ParseNeedDownloadFiles<int>(params_.include_regex, params_.exclude_regex, *ti, &file_priorities);
    torrent_handle_.prioritize_files(file_priorities);
    pre_file_priorities_ = file_priorities;
  }
  SetOrgPieces(ti);

  if (!params_.save_torrent_path.empty()) {
    libtorrent::create_torrent ct(*ti);
    libtorrent::entry te = ct.generate();
    if (!params_.save_torrent_path.empty()) {
      std::ofstream ofs(params_.save_torrent_path.c_str());
      if (!ofs) {
        WARNING_LOG("open file(%s) to save torrent failed", params_.save_torrent_path.c_str());
      } else {
        libtorrent::bencode(std::ostream_iterator<char>(ofs), te);
        ofs.close();
      }
    }
  }
  return 0;
}

void GroupTask::SetOrgPieces(const boost::intrusive_ptr<torrent_info const> ti) {
    // all files should be pre-allocate before download
    // store org_pieces
    int num_pieces = ti->num_pieces();
    if (num_pieces <= 0) {
      num_pieces = 1;  // empty files, num_pieces is 0, we should protect this situation
    }
    org_pieces_.resize(num_pieces, false);

    for (unsigned i = 0; i < pre_file_priorities_.size(); ++i) {
      libtorrent::file_entry fe = ti->file_at(i);
      int first_piece = ti->map_file(i, 0, 0).piece;
      int last_piece = ti->map_file(i, (std::max)(fe.size - 1, libtorrent::size_type(0)), 0).piece;
      // because this state is not downloading, if we just use torrent_.status(0),pieces
      // all bitmask will be set to 0, and this is not true, so we have to set manually
      for (int j = first_piece; j <= last_piece; ++j) {
        if (pre_file_priorities_[i] > 0 || org_pieces_.get_bit(j)) {
          org_pieces_.set_bit(j);
        } else {
          org_pieces_.clear_bit(j);
        }  // if pre_file_priorities_[i] > 0
      }  // for j
    }  // for i
}

int GroupTask::CorrectModeAndSymlinks() {
  std::string save_path = params_.save_path;
  boost::intrusive_ptr<torrent_info const> ti = torrent_handle_.torrent_file();
  if (!ti) {
      FATAL_LOG("[%s] can't get torrent info", infohash_.c_str());
      return -1;
  }
  // create and change directory mode
  if (MakeCorrectDir(*ti) != 0) {
    FATAL_LOG("[%s] make dir failed", infohash_.c_str());
    return -1;
  }

  // file chmod
  std::vector<int> file_priorities;
  if (params_.dynamic_allocate >= 0) {
    file_priorities = pre_file_priorities_;
  } else {
    file_priorities = torrent_handle_.file_priorities();
  }

  for (int i = 0; i < ti->num_files(); ++i) {
    libtorrent::file_entry fe = ti->file_at(i);
    std::string file_name = save_path + "/" + fe.path;
    if (file_priorities[i] == 0) {
      unlink(file_name.c_str());  // delete over files, ignore fail
      DEBUG_LOG("unlink file %s", file_name.c_str());
      continue;
    }
    if (chmod(file_name.c_str(), fe.mode) != 0) {
      FATAL_LOG("[%s] file(%s) chmod to %o failed, error[%d]:%s", infohash_.c_str(),
              file_name.c_str(), fe.mode, errno, strerror(errno));
      return -1;
    }
  }

  // create symlinks files
  const std::vector<symlink_file> &symlinks = ti->files().get_symlink_file();

  std::vector<boost::regex> include_regex, exclude_regex;
  TurnStdListToRegexList(params_.include_regex, &include_regex);
  TurnStdListToRegexList(params_.exclude_regex, &exclude_regex);

  for (std::vector<symlink_file>::const_iterator it = symlinks.begin(); it != symlinks.end(); ++it) {
    bool need = false;
    if (params_.patition_download) {
      RegexMatch regex_m(it->path);
      if (include_regex.empty() ||
          std::find_if(include_regex.begin(), include_regex.end(), regex_m) != include_regex.end()) {
        need = true;
      }
      if (std::find_if(exclude_regex.begin(), exclude_regex.end(), regex_m) !=exclude_regex.end()) {
        need = false;
      }
    } else {
      need = true;
    }
    if (!need) {
      continue;
    }

    std::string path = save_path + "/" + it->path;
    if (symlink(it->symlink_path.c_str(), path.c_str()) < 0) {
      FATAL_LOG("[%s] create symlink file %s -> %s failed.", infohash_.c_str(), path.c_str(), it->symlink_path.c_str());
      return -1;
    }
    TRACE_LOG("[%s] create symlink file %s -> %s success.", infohash_.c_str(), path.c_str(), it->symlink_path.c_str());
  }  // for std::vector

  return 0;
}

// is_before_download: if true, means mkdir before download,
// else means mkdir after download finished
int GroupTask::MakeCorrectDir(const torrent_info &ti, bool is_before_download) {
  std::string save_path = params_.save_path;
  const std::vector<std::string>& paths = ti.get_paths();
  const std::vector<mode_t>& modes = ti.get_paths_mode();
  int path_len = paths.size();

  std::vector<boost::regex> include_regex, exclude_regex;
  TurnStdListToRegexList(params_.include_regex, &include_regex);
  TurnStdListToRegexList(params_.exclude_regex, &exclude_regex);

  for (int i = 0; i < path_len; ++i) {
    std::string path = save_path + "/" + paths[i];
    mode_t mode = modes[i];
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) != 0) {
      bool need = false;
      if (params_.patition_download) {
        RegexMatch regex_m(paths[i]);
        if (include_regex.empty() ||
            std::find_if(include_regex.begin(), include_regex.end(), regex_m) != include_regex.end()) {
          need = true;
        }
        if (std::find_if(exclude_regex.begin(), exclude_regex.end(), regex_m) != exclude_regex.end()) {
          need = false;
        }
      } else {
        need = true;
      }
      if (is_before_download && !need) {
        continue;
      }
      if (!check_and_mkdir_r(path.c_str(), mode) != 0) {
        WARNING_LOG("[%s] mkdir %s(%o) failed.", infohash_.c_str(), path.c_str(), mode);
        return -1;
      }
    } else if (statbuf.st_mode & S_IFDIR) {
      if (chmod(path.c_str(), mode) != 0) {
        WARNING_LOG("[%s] path(%s) chmod to %o failed.", infohash_.c_str(), path.c_str(), mode);
        return -1;
      }
    } else {
      WARNING_LOG("[%s] path %s is not a dir, can't change mode.", infohash_.c_str(), path.c_str());
      return -1;
    }
  }

  return 0;
}

void GroupTask::PreAllocateThread() {
  DEBUG_LOG("[%s] start PreAllocateThread...", infohash_.c_str());
  const int kDefaultMultiple = 50;
  boost::intrusive_ptr<torrent_info const> ti = torrent_handle_.torrent_file();

  std::string save_path = params_.save_path;
  int num_pieces = ti->num_pieces();
  int piece_length = ti->piece_length();

  // if dynamic_allocate is 0 or error, use default
  int multiple = params_.dynamic_allocate > 0 ? params_.dynamic_allocate : kDefaultMultiple;
  const off_t kEachLength = multiple * piece_length;
  int last_download_index = 0;

  std::vector<int> piece_priority(num_pieces, 0);
  torrent_handle_.set_share_mode(false);
  // after set_share_mode called, all file_priorities will be set to 1, so we should set again
  torrent_handle_.prioritize_files(pre_file_priorities_);
  // set all piece's priority to 0, thus all piece won't been download before set 1 again.
  torrent_handle_.prioritize_pieces(piece_priority);

  // first, mkdir
  MakeCorrectDir(*ti, true);

  // allocate each file
  for (int i = 0; i < ti->num_files(); ++i) {
    libtorrent::file_entry fe = ti->file_at(i);
    if (pre_file_priorities_[i] == 0 || fe.pad_file) {
      continue;
    }

    std::string file_name = save_path + "/" + fe.path;
    int first_piece = ti->map_file(i, 0, 0).piece;
    int last_piece = ti->map_file(i, (std::max)(fe.size - 1, libtorrent::size_type(0)), 0).piece;
    // check if file exists and file size. if file has correct size, then we just continue
    // because if we allocate again, libtorrent may think all pieces of this file has been downloaded,
    // so after allocate this file, libtorrent won't download again, thus we may get an empty file.
    struct stat file_buf;
    if (stat(file_name.c_str(), &file_buf) == 0 && file_buf.st_size == fe.size) {
      NOTICE_LOG("[%s] file %s has size:%ld, and goat size is %ld, so don't allocate again",
                  infohash_.c_str(), file_name.c_str(), file_buf.st_size, fe.size);
      // set piece correct priority
      for (int i = first_piece; i <= last_piece; ++i) {
        piece_priority[i] = 1;
      }
      torrent_handle_.prioritize_pieces(piece_priority);
      last_download_index = last_piece;
      continue;
    }

    int fd = open(file_name.c_str(), O_RDWR | O_CREAT | O_TRUNC, fe.mode);
    if (fd < 0) {
      // if one file allocate failed, all_allocate_size will be error if continue,
      // so, just break and don't use dynamic pre-allocate mode
      WARNING_LOG("[%s] open file %s failed, don't use dynamic pre-allocate mode, use normal mode:errno[%d]:%s",
                   infohash_.c_str(), file_name.c_str(), errno, strerror(errno));
      close(fd);
      break;
    }
    DEBUG_LOG("[%s] open file %s success, size is %ld", infohash_.c_str(), file_name.c_str(), fe.size);

    int file_download_index = first_piece;
    off_t current_offset = 0;
    while (current_offset < fe.size) {
      off_t each_len = kEachLength;
      if ((current_offset + kEachLength) > fe.size) {
        each_len = fe.size - current_offset;
      }
      int fallocate_ret = posix_fallocate(fd, current_offset, each_len);
      if (fallocate_ret != 0 && fallocate_ret != EINVAL) {
          WARNING_LOG("[%s] posix_fallocate failed:ret=%d, offset=%ld, len=%ld, file=%s\n",
                 infohash_.c_str(), fallocate_ret, current_offset, each_len, file_name.c_str());
          break;
      }
      DEBUG_LOG("[%s] allocate current_offset:%ld", infohash_.c_str(), current_offset);

      current_offset += each_len;
      if (current_offset >= fe.size) {
        // this file has been success allocate, goto next one
        DEBUG_LOG("[%s] current_offset size is %ld", infohash_.c_str(), current_offset);
        break;
      }

      int current_index = current_offset / piece_length;
      for (int i = file_download_index; i <= (current_index + first_piece); ++i) {
        piece_priority[i] = 1;
      }
      torrent_handle_.prioritize_pieces(piece_priority);
      file_download_index = current_index + first_piece;
    }  // while
    
    close(fd);

    // ensure all pieces of this file can be download
    if (file_download_index < last_piece) {
      for (int i = file_download_index; i <= last_piece; ++i) {
        piece_priority[i] = 1;
      }
      torrent_handle_.prioritize_pieces(piece_priority);
      file_download_index = last_piece;
    }
    last_download_index = last_piece;
  }  // for

  // set pieces again, because pre allocate may be failed,
  // so set pieces again, ensure all piece can be download
  if (last_download_index < num_pieces) {
    for (int i = last_download_index; i < num_pieces; ++i) {
      if (org_pieces_.get_bit(i)) {
        piece_priority[i] = 1;
      }
    }
    torrent_handle_.prioritize_pieces(piece_priority);
  }

  NOTICE_LOG("[%s] pre allocate thread exit", infohash_.c_str());
}


}  // namespace group
}  // namespace bbts

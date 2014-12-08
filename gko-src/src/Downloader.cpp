/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   Downloader.cpp
 *
 * @author liuming03
 * @date   2013-7-26
 * @brief 
 */

#include "bbts/tool/Downloader.h"

#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <deque>
#include <fstream>

#include <boost/bind.hpp>
#include <boost/system/error_code.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/extensions/metadata_transfer.hpp>
#include <libtorrent/ip_filter.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/torrent_info.hpp>

#include "bbts/BBTSTorrentPlugin.h"
#include "bbts/common_def.h"
#include "bbts/configure.pb.h"
#include "bbts/constant_def.h"
#include "bbts/ErrorCategory.h"
#include "bbts/host_info_util.h"
#include "bbts/LazySingleton.hpp"
#include "bbts/log.h"
#include "bbts/message.pb.h"
#include "bbts/number_util.h"
#include "bbts/path.h"
#include "bbts/RegexMatch.hpp"
#include "bbts/socket_util.h"
#include "bbts/Syms.h"
#include "bbts/tool/BBTSClient.h"
#include "bbts/encode.h"
#include "bbts/tool/ClusterDownloader.h"

using std::set;
using std::string;
using std::stringstream;
using std::vector;
using boost::scoped_array;
using boost::scoped_ptr;
using boost::shared_ptr;
using boost::posix_time::seconds;
using boost::regex;
using boost::system::error_code;
using libtorrent::add_torrent_params;
using libtorrent::alert;
using libtorrent::cluster_config_entry;
using libtorrent::entry;
using libtorrent::ex_announce_response;
using libtorrent::libtorrent_exception;
using libtorrent::session_settings;
using libtorrent::symlink_file;
using libtorrent::torrent_info;
using libtorrent::torrent_status;

namespace bbts {
namespace tool {

static bool catch_stop_sigal = false;

static void HandleStopSigal(int sig) {
  fprintf(stderr, "catch sigal %d\n", sig);
  catch_stop_sigal = true;
}

static void SetSignalAction() {
  struct sigaction sa;
  sa.sa_flags = SA_RESETHAND;
  sa.sa_handler = HandleStopSigal;
  sigemptyset(&sa.sa_mask);
  //sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
}

static void LogProgress(const torrent_status &ts) {
  string downrate, uprate, download, upload;
  BytesToReadable(ts.download_rate, &downrate);
  BytesToReadable(ts.upload_rate, &uprate);
  BytesToReadable(ts.total_payload_download, &download);
  BytesToReadable(ts.total_payload_upload, &upload);
  TRACE_LOG("status: %d, progress: %5.2f%%, downrate: %8s/s, uprate: %8s/s, download: %12s, upload: %12s",
             ts.state, (ts).progress * 100, downrate.c_str(), uprate.c_str(), download.c_str(), upload.c_str());
}

static void StdoutProgress(const torrent_status &ts) {
  string downrate, uprate, download, upload;
  BytesToReadable(ts.download_rate, &downrate);
  BytesToReadable(ts.upload_rate, &uprate);
  BytesToReadable(ts.total_payload_download, &download);
  BytesToReadable(ts.total_payload_upload, &upload);
  printf("\rstatus: %d, progress: %5.2f%%, downrate: %8s/s, uprate: %8s/s, download: %12s, upload: %12s",
         ts.state, (ts).progress * 100, downrate.c_str(), uprate.c_str(), download.c_str(), upload.c_str());
  fflush(stdout);
}

template <typename IntegerType>
static void ParseNeedDownloadFiles(const vector<regex> &include, const vector<regex> &exclude,
                                   const torrent_info &ti, vector<IntegerType> *file_priorities) {
  int num_files = ti.num_files();
  file_priorities->resize(num_files, 0);
  for (int i = 0; i < num_files; ++i) {
    string filepath = ti.files().file_path(i);
    RegexMatch regex_m(filepath);
    if (include.empty() || std::find_if(include.begin(), include.end(), regex_m) != include.end()) {
      file_priorities->at(i) = 1;
    }
    if (std::find_if(exclude.begin(), exclude.end(), regex_m) != exclude.end()) {
      file_priorities->at(i) = 0;
    }
    if (file_priorities->at(i)) {
      NOTICE_LOG("will download file: %s", filepath.c_str());
    }
  }
}

down_params_t::down_params_t()
  : seeding_time(-2),
    timeout(0),
    hang_timeout(0),
    mem_limit(0),
    debug(false),
    print_progress(false),
    patition_download(false),
    need_save_resume(false),
    need_down_to_tmp_first(false),
    quit_by_tracker_failed(false),
    storage_pre_allocate(false),
    dynamic_allocate(-1),
    use_dio_read(false),
    use_dio_write(false),
    ignore_hdfs_error(false),
    listen_port_range(45000 + get_random_num() % 500, 65535) {}

Downloader::Downloader(const down_params_t &params) :
    configure_(LazySingleton<message::DownloadConfigure>::instance()),
    params_(params),
    should_stop_(false),
    thrift_tracker_(io_service_),
    seeding_timer_(io_service_),
    timeout_timer_(io_service_),
    is_timeout_(false),
    is_mem_exceed_(false),
    hang_check_timer_(io_service_),
    last_check_time_(time(NULL)),
    last_check_downloaded_(0),
    low_downrate_times_(0),
    progress_timer_(io_service_),
    has_pre_allocate_thread_start_(false),
    control_server_(io_service_) {
  control_server_.set_endpoint(UnixSocketConnection::EndPoint(params_.control_path));
  control_server_.set_heartbeat_recv_cycle(60);
  control_server_.set_read_callback(boost::bind(&Downloader::ControlHandler, this, _1, _2));

  string download_log_dir;
  string download_log_name;
  path_slipt(configure_->download_log_file(), &download_log_dir, &download_log_name);
  InitLog(download_log_dir, download_log_name, params.debug);
}

Downloader::~Downloader() {
  CLOSE_LOG();
}

void Downloader::RemoveTorrent() {
  torrent_status ts = torrent_.status(0);
  if (cluster_downloader_) {
    cluster_downloader_->StopDownload();
    if (torrent_.is_valid()) {
      stat_.downloaded_from_hdfs = cluster_downloader_->get_cluster_total_download();
      NOTICE_LOG("hadoop download: %lld, p2p download: %lld", stat_.downloaded_from_hdfs,
                  ts.total_payload_download - stat_.downloaded_from_hdfs);
      peer_stat_.Print(peer_stat_alert(ts.info_hash,
                                       cluster_downloader_->get_cluster_config().host,
                                       cluster_downloader_->get_start_time(),
                                       time(NULL),
                                       0,
                                       stat_.downloaded_from_hdfs,
                                       1));
    }
  }
  if (torrent_.is_valid()) {
    stat_.payload_downloaded = ts.total_payload_download;
    stat_.payload_uploaded = ts.total_payload_upload;
    stat_.progress_ppm = ts.progress_ppm;
    if (stat_.time_for_download) {
      stat_.time_for_seeding  = time(NULL) - stat_.start_time - stat_.time_for_download_metadata
          - stat_.time_for_check_files - stat_.time_for_download;
    }
    error_code ec;
    progress_timer_.cancel(ec);
    session_->remove_torrent(torrent_);
  }
}

void Downloader::TimeoutTimerCallback(const error_code &ec) {
  if (ec) {
    DEBUG_LOG("timeout timer canceled.");
    return;
  }
  WARNING_LOG("download timeout, will quit.");
  if (!torrent_.status(0).is_finished) {
    is_timeout_ = true;
  }
  RemoveTorrent();
}

void Downloader::HangCheckCallback(const error_code &ec) {
  if (ec) {
    DEBUG_LOG("hang check timer canceled.");
    return;
  }

  torrent_status ts = torrent_.status(0);
  LogProgress(ts);
  if (params_.need_save_resume && ts.has_metadata && ts.need_save_resume) {
    torrent_.save_resume_data();
  }
  if (ts.progress_ppm == 1000000) {
    return;
  }

  time_t now = time(NULL);
  if ((ts.state == torrent_status::downloading && ts.total_payload_download == last_check_downloaded_)
      || (ts.state == torrent_status::downloading_metadata && !thrift_tracker_.HaveSeed())) {
    if (last_check_time_ + params_.hang_timeout - now <= 0) {
      FATAL_LOG("download timeout, maybe noseeder.");
      is_timeout_ = true;
      RemoveTorrent();
      return;
    }
  } else {
    last_check_time_ = now;
    last_check_downloaded_ = ts.total_payload_download;
  }

  //如果下载限速超过2M，且下载率连续10次(150s)都不超过800K，则断开连接并重连
  if (configure_->download_limit() > 2048 * 1024 && ts.download_rate < 800 * 1024) {
    ++low_downrate_times_;
    if (low_downrate_times_ > 9) {
      torrent_.pause();
      torrent_.resume();
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
      RemoveTorrent();
      return;
    }
  }
  hang_check_timer_.expires_from_now(seconds(15));
  hang_check_timer_.async_wait(boost::bind(&Downloader::HangCheckCallback, this, _1));
}

void Downloader::ProgressCallback(const error_code &ec) {
  if (ec) {
    DEBUG_LOG("progress timer canceled.");
    printf("\n");
    return;
  }

  torrent_status ts = torrent_.status(0);
  StdoutProgress(ts);
  progress_timer_.expires_from_now(seconds(2));
  progress_timer_.async_wait(boost::bind(&Downloader::ProgressCallback, this, _1));
}

void Downloader::SeedingTimerCallback(const error_code &ec) {
  if (ec) {
    DEBUG_LOG("seeding timer canceled, %s.", ec.message().c_str());
    return;
  }
  NOTICE_LOG("download finished, will quit.");
  RemoveTorrent();
}

void Downloader::AnnounceCallback(const shared_ptr<ex_announce_response> &response, int *retval) {
  switch (response->ret) {
    case 26234:
      NOTICE_LOG("tracker said we should stop downloading.");
      RemoveTorrent();
      *retval = 50;
      break;

    case 10000:
      if (params_.quit_by_tracker_failed) {
        NOTICE_LOG("connect with tracker failed, we should stop downloading.");
        RemoveTorrent();
        *retval = 51;
      }
      break;

    default:
      break;
  }
}

void Downloader::ToSeeding() {
  torrent_status ts = torrent_.status(0);
  LogProgress(ts);
  if (params_.seeding_time == -1) {
    NOTICE_LOG("seeding for infinite");
    return;
  }

  int seeding_time = 0;
  if (params_.seeding_time >= 0) {
    seeding_time = params_.seeding_time;
  } else {
    int upload_limit = ts.uploads_limit > 0 ? ts.uploads_limit : configure_->upload_limit();
    seeding_time = (ts.num_peers + 1) * ts.total_payload_download / (1 + ts.num_seeds) / (upload_limit + 1) / 2;
    int tmp = ts.total_download / (ts.download_rate + 1);
    if (tmp < seeding_time) {
      seeding_time = tmp;
    }
  }
  NOTICE_LOG("seeding for %d seconds", seeding_time);
  seeding_timer_.expires_from_now(seconds(seeding_time));
  seeding_timer_.async_wait(boost::bind(&Downloader::SeedingTimerCallback, this, _1));
}

bool Downloader::CorrectModeAndSymlinks() {
  string save_path = params_.save_path;
  boost::intrusive_ptr<torrent_info const> ti = torrent_.torrent_file();
  if (!ti) {
      WARNING_LOG("can't get torrent info");
      return false;
  }
  // 创建&更改目录mode
  MakeCorrectDir(*ti);

  {  // 更改文件权限
    vector<int> file_priorities;
    if (params_.dynamic_allocate >= 0) {
      file_priorities = pre_file_priorities_;
    } else {
      file_priorities = torrent_.file_priorities();
    }
    for (int i = 0; i < ti->num_files(); ++i) {
      libtorrent::file_entry fe = ti->file_at(i);
      string file_name = save_path + "/" + fe.path;
      if (file_priorities[i] == 0 || fe.pad_file) {
        unlink(file_name.c_str());  // delete over files, ignore fail
        DEBUG_LOG("unlink file %s", file_name.c_str());
        continue;
      }
      if (chmod(file_name.c_str(), fe.mode) != 0) {
        FATAL_LOG("file(%s) chmod to %o failed.", file_name.c_str(), fe.mode);
        return false;
      }
    }
  }

  {  // 建立符号连接
    const vector<symlink_file> &symlinks = ti->files().get_symlink_file();
    for (vector<symlink_file>::const_iterator it = symlinks.begin(); it != symlinks.end(); ++it) {
      bool need = false;
      if (params_.patition_download) {
        RegexMatch regex_m(it->path);
        if (params_.include_regex.empty() || std::find_if(
            params_.include_regex.begin(), params_.include_regex.end(), regex_m) != params_.include_regex.end()) {
          need = true;
        }
        if (std::find_if(params_.exclude_regex.begin(), params_.exclude_regex.end(), regex_m) !=
            params_.exclude_regex.end()) {
          need = false;
        }
      } else {
        need = true;
      }
      if (!need) {
        continue;
      }

      string path = save_path + "/" + it->path;
      if (symlink(it->symlink_path.c_str(), path.c_str()) < 0) {
        FATAL_LOG("create symlink file %s -> %s failed.", path.c_str(), it->symlink_path.c_str());
        return false;
      }
      TRACE_LOG("create symlink file %s -> %s success.", path.c_str(), it->symlink_path.c_str());
    }
  }
  return true;
}

bool Downloader::OnTorrentFinished() {
  if (params_.dynamic_allocate >= 0) {
    // check if is really finished
    // if not in downloading or seeding mode, torrent_.status(0).pieces will be set to 0
    // TODO: this may be a problem
    
    libtorrent::bitfield pieces = torrent_.status(0).pieces;
    DEBUG_LOG("pieces.count() = %d, org.count() = %d", pieces.count(), org_pieces_.count());
    boost::intrusive_ptr<torrent_info const> ti = torrent_.torrent_file();
    int num_pieces = ti->num_pieces();  // defend for empty file
    if (pieces.count() < org_pieces_.count() && num_pieces > 0) {
      // not really finished
      return true;
    }
  }

  stat_.time_for_download = time(NULL) - stat_.start_time
      - stat_.time_for_download_metadata - stat_.time_for_check_files;
  error_code ec;
  hang_check_timer_.cancel(ec);
  if (params_.need_down_to_tmp_first) {
    NOTICE_LOG("mv data from %s to %s", tmp_save_path_.c_str(), params_.save_path.c_str());
    torrent_.move_storage(params_.save_path);
    return true;
  }
  return OnStorageMoved();
}

bool Downloader::OnStorageMoved() {
  error_code ec;
  if (!CorrectModeAndSymlinks()) {
    return false;
  }
  ToSeeding();
  return true;
}

bool Downloader::OnStateChanged(const libtorrent::state_changed_alert *alert) {
  NOTICE_LOG("%s", alert->message().c_str());

  boost::intrusive_ptr<torrent_info const> ti = torrent_.torrent_file();
  if (alert->state != torrent_status::downloading_metadata && ti &&
      params_.dynamic_allocate >= 0 && !has_pre_allocate_thread_start_) {
    boost::thread tmp_thread(boost::bind(&Downloader::PreAllocateThread, this));
    allocate_threads_.swap(tmp_thread);
    has_pre_allocate_thread_start_ = true;
  } 

  if (alert->state != torrent_status::downloading) {
    return true;
  }

  stat_.time_for_check_files = time(NULL) - stat_.start_time - stat_.time_for_download_metadata;
  if (params_.debug && params_.patition_download) {
    vector<int> file_priorities = torrent_.file_priorities();
    for (unsigned i = 0; i < file_priorities.size(); ++i) {
      DEBUG_LOG("file %d priorities: %d", i, file_priorities[i]);
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

  if (!cluster_downloader_) {
    if (cluster_config.port != 0 && !cluster_config.host.empty() &&
        !cluster_config.user.empty() && !cluster_config.passwd.empty()) {
      if (!ParseSyms()) {
        NOTICE_LOG("Not support cluster(hadoop) download.");
        return false;
      }
      NOTICE_LOG("Support hadoop download, will download from hadoop.");
      stat_.is_hdfs_download = true;
      stringstream strm;
      strm << "hdfs://" << cluster_config.user << ':' << cluster_config.passwd << '@'
           << cluster_config.host << ':' << cluster_config.port << cluster_config.prefix_path;
      stat_.hdfs_address = strm.str();
      cluster_downloader_.reset(new ClusterDownloader(cluster_config,
                                                      torrent_,
                                                      configure_->cluster_thread_num(),
                                                      params_.ignore_hdfs_error));
    }
  }

  if (cluster_downloader_) {
    NOTICE_LOG("will start cluster thread.");
    if (!cluster_downloader_->StartDownload()) {
      FATAL_LOG("start cluster download failed.");
      return false;
    }
    SetSignalAction();
  }
  return true;
}

void Downloader::SetOrgPieces(const boost::intrusive_ptr<torrent_info const> ti) {
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

void Downloader::OnMetadataReceived() {
  stat_.time_for_download_metadata = time(NULL) - stat_.start_time;
  boost::intrusive_ptr<torrent_info const> ti = torrent_.torrent_file();
  pre_file_priorities_.resize(ti->num_files(), 1);
  if (params_.patition_download) {
    vector<int> file_priorities;
    ParseNeedDownloadFiles<int>(params_.include_regex, params_.exclude_regex, *ti, &file_priorities);
    torrent_.prioritize_files(file_priorities);
    pre_file_priorities_ = file_priorities;
  }
  SetOrgPieces(ti);

  if (meta_msg_) {
    if (!params_.save_torrent_path.empty()) {
      libtorrent::create_torrent ct(*ti);
      entry te = ct.generate();
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
    if (ti->metadata_size() > configure_->max_metadata_size()) {
      meta_msg_.reset();
    } else {
      meta_msg_->set_data(ti->metadata().get(), ti->metadata_size());
    }
  }
}

void Downloader::OnHashFailed(const libtorrent::hash_failed_alert *alert) {
  WARNING_LOG("alert: %s.", alert->message().c_str());
  if (cluster_downloader_) {
    cluster_downloader_->add_hash_failed_piece(alert->piece_index);
  }
}

void Downloader::OnSaveResumeData(const libtorrent::save_resume_data_alert *alert) {
  std::ofstream ofs(resume_file_.c_str(), std::ios::binary);
  if (!ofs) {
    WARNING_LOG("open file failed: %s.", resume_file_.c_str());
    return;
  }
  libtorrent::bencode(std::ostream_iterator<char>(ofs), *(alert->resume_data));
  ofs.close();
}

void Downloader::ProcessAlert(int *retval, bool loop) {
  int &ret = *retval;
  if (catch_stop_sigal) {
    RemoveTorrent();
    ret = 20;
  }

  session_->wait_for_alert(libtorrent::milliseconds(100));
  std::deque<alert *> alerts;
  session_->pop_alerts(&alerts);
  for (std::deque<alert *>::iterator it = alerts.begin(); it != alerts.end(); ++it) {
    switch ((*it)->type()) {
      case libtorrent::save_resume_data_alert::alert_type:
        OnSaveResumeData((libtorrent::save_resume_data_alert *)(*it));
        break;

      case libtorrent::listen_failed_alert::alert_type: {
        libtorrent::listen_failed_alert *lf_alert = (libtorrent::listen_failed_alert *)(*it);
        const libtorrent::tcp::endpoint &ep = lf_alert->endpoint;
        if (lf_alert->sock_type == libtorrent::listen_failed_alert::tcp
            && ep.address().is_v4() && ep.port() != 0) {
          FATAL_LOG("%s.", lf_alert->message().c_str());
          RemoveTorrent();
          ret = -2;
        }
        break;
      }

      case libtorrent::state_changed_alert::alert_type:
        if (!OnStateChanged((libtorrent::state_changed_alert *)(*it))) {
          RemoveTorrent();
          ret = -3;
        }
        break;

      case libtorrent::metadata_received_alert::alert_type:
        TRACE_LOG("%s", (*it)->message().c_str());
        OnMetadataReceived();
        break;

      case libtorrent::torrent_finished_alert::alert_type: {
        NOTICE_LOG("alert: %s.", (*it)->message().c_str());
        if (!OnTorrentFinished()) {
          RemoveTorrent();
          ret = -4;
        }
        break;
      }

      case libtorrent::storage_moved_alert::alert_type:
        NOTICE_LOG("alert: %s.", (*it)->message().c_str());
        if (!OnStorageMoved()) {
          RemoveTorrent();
          ret = -5;
        }
        break;

      case libtorrent::torrent_error_alert::alert_type:
        FATAL_LOG("download failed, error: %s.", ((libtorrent::torrent_error_alert * )(*it))->error.message().c_str());
        RemoveTorrent();
        ret = -6;
        break;

      case libtorrent::hash_failed_alert::alert_type:
        WARNING_LOG("alert: %s.", (*it)->message().c_str());
        OnHashFailed((libtorrent::hash_failed_alert *)(*it));
        break;

      case libtorrent::torrent_removed_alert::alert_type: {
        NOTICE_LOG("alert: %s.", (*it)->message().c_str());
        if (is_timeout_) {
          ret = -7;
        } else if (is_mem_exceed_) {
          ret = -9;
        }
        should_stop_ = true;
        break;
      }

      case libtorrent::storage_moved_failed_alert::alert_type:
        FATAL_LOG("%s", ((libtorrent::storage_moved_failed_alert * )(*it))->error.message().c_str());
        RemoveTorrent();
        ret = -8;
        break;

      case peer_stat_alert::alert_type:
        peer_stat_.Print(*static_cast<peer_stat_alert *>(*it));
        break;

      case libtorrent::listen_succeeded_alert::alert_type:
      case libtorrent::add_torrent_alert::alert_type:
      case libtorrent::torrent_added_alert::alert_type:
      case libtorrent::torrent_deleted_alert::alert_type:
      case libtorrent::lsd_peer_alert::alert_type:
      case libtorrent::url_seed_alert::alert_type:
      case libtorrent::peer_blocked_alert::alert_type:
      case libtorrent::peer_ban_alert::alert_type:
      case libtorrent::torrent_resumed_alert::alert_type:
      case libtorrent::torrent_paused_alert::alert_type:
      case libtorrent::tracker_reply_alert::alert_type:
      case libtorrent::tracker_announce_alert::alert_type:
      case libtorrent::torrent_checked_alert::alert_type:
        NOTICE_LOG("alert: %s.", (*it)->message().c_str());
        break;

      case libtorrent::incoming_connection_alert::alert_type:
      case peer_handshake_alert::alert_type:
        DEBUG_LOG("alert: %s.", (*it)->message().c_str());
        break;

      case libtorrent::peer_error_alert::alert_type:
      case libtorrent::torrent_delete_failed_alert::alert_type:
      case libtorrent::metadata_failed_alert::alert_type:
      case libtorrent::tracker_warning_alert::alert_type:
      case libtorrent::tracker_error_alert::alert_type:
      case libtorrent::file_error_alert::alert_type:
      case libtorrent::save_resume_data_failed_alert::alert_type:
      case libtorrent::fastresume_rejected_alert::alert_type:
        WARNING_LOG("alert: %s.", (*it)->message().c_str());
        break;

      case libtorrent::performance_alert::alert_type:
      case libtorrent::peer_connect_alert::alert_type:
      case libtorrent::peer_disconnected_alert::alert_type:
      default:
        DEBUG_LOG("alert: %s.", (*it)->message().c_str());
        break;
    }
    delete *it;
  }

  if (cluster_downloader_ && cluster_downloader_->IsStoped() && cluster_downloader_->get_retval() != 0) {
    if (!torrent_.status(0).is_finished) {
      ret = cluster_downloader_->get_retval();
      RemoveTorrent();
    } else {
      cluster_downloader_->set_retval(0);
    }
  }

  if (should_stop_) {
    error_code ec;
    seeding_timer_.cancel(ec);
    timeout_timer_.cancel(ec);
    hang_check_timer_.cancel(ec);
    progress_timer_.cancel(ec);
    control_server_.Close();
  }

  if (loop) {
    io_service_.post(boost::bind(&Downloader::ProcessAlert, this, retval, !should_stop_));
  }
}

void Downloader::StartDownload() {
  stat_.start_time = time(NULL);
  torrent_.resume();
  torrent_.auto_managed(true);

  hang_check_timer_.expires_from_now(seconds(15));
  hang_check_timer_.async_wait(boost::bind(&Downloader::HangCheckCallback, this, _1));
  if (params_.timeout > 0) {
    timeout_timer_.expires_from_now(seconds(params_.timeout));
    timeout_timer_.async_wait(boost::bind(&Downloader::TimeoutTimerCallback, this, _1));
  }
  if (params_.print_progress) {
    progress_timer_.expires_from_now(seconds(2));
    progress_timer_.async_wait(boost::bind(&Downloader::ProgressCallback, this, _1));
  }
}

bool Downloader::AddDownloadTask() {
  if (params_.save_path.empty() || !check_and_mkdir_r(params_.save_path, 0755)) {
    FATAL_LOG("save path(%s) not correct or can not create.", params_.save_path.c_str());
    return false;
  }

  error_code ec;
  add_torrent_params torrent_params;
  if (!params_.torrent_path.empty()) {
    int64_t size = libtorrent::file_size(params_.torrent_path.c_str());
    if (size <= 0 || size > configure_->max_metadata_size()) {
      FATAL_LOG("file(%s) size invalid.", params_.torrent_path.c_str());
      return false;
    }
    torrent_params.ti = new torrent_info(params_.torrent_path, ec, 0, params_.new_name);
    if (ec) {
      FATAL_LOG("pares torrent file(%s) failed, ret(%d): %s.",
                 params_.torrent_path.c_str(), ec.value(), ec.message().c_str());
      return false;
    }
    params_.infohash = libtorrent::to_hex(torrent_params.ti->info_hash().to_string());
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
  } else if (!params_.infohash.empty()) {
    if (!params_.save_torrent_path.empty()) {
      std::ofstream ofs(params_.save_torrent_path.c_str());
      if (!ofs) {
        FATAL_LOG("can't create torrent file(%s) for save.", params_.save_torrent_path.c_str());
        return false;
      }
      ofs.close();
    }
    string bytes;
    if (params_.infohash.length() != 40 || !bbts::HexstrToBytes(params_.infohash, &bytes)) {
      FATAL_LOG("infohash(%s) not correct.", params_.infohash.c_str());
      return false;
    }
    torrent_params.info_hash.assign(bytes);
    torrent_params.ti = new torrent_info(torrent_params.info_hash, 0, params_.new_name);
    meta_msg_.reset(new message::Metadata());
    meta_msg_->set_infohash(params_.infohash);
  } else {
    FATAL_LOG("not set infohash or torrent file or torrent_url");
    return false;
  }

  if (params_.trackers.empty()) {
    thrift_tracker_.GetTrackersByInfohash(params_.infohash, &params_.trackers);
  }
  torrent_params.thrift_trackers = params_.trackers;
  if (params_.need_down_to_tmp_first) {
    tmp_save_path_ = params_.save_path + "/.gko3_" + params_.infohash + "_" + params_.new_name + ".tmp";
    if (!check_and_mkdir(tmp_save_path_, 0755)) {
      FATAL_LOG("tmp save path(%s) can not create.", tmp_save_path_.c_str());
      return false;
    }
  } else {
    tmp_save_path_ = params_.save_path;
  }
  torrent_params.save_path = tmp_save_path_;

  vector<char> buffer;
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
        WARNING_LOG("can't open resume file: %s, ignore", resume_file_.c_str());
      }
    }
  }

  if (params_.storage_pre_allocate) {
    torrent_params.storage_mode = libtorrent::storage_mode_allocate;
  }

  torrent_params.flags |= add_torrent_params::flag_paused;
  torrent_params.flags |= add_torrent_params::flag_duplicate_is_error;
  torrent_params.flags |= add_torrent_params::flag_override_resume_data;
  torrent_params.flags &= ~add_torrent_params::flag_auto_managed;
  torrent_params.download_limit = configure_->download_limit();
  torrent_params.upload_limit = configure_->upload_limit();
  torrent_params.max_connections = configure_->connection_limit();
  torrent_params.url_seeds = params_.web_seeds;
  torrent_ = session_->add_torrent(torrent_params, ec);
  if (ec) {
    FATAL_LOG("add torrent failed: %s.", ec.message().c_str());
    return false;
  }

  if (params_.dynamic_allocate >= 0) {
    // set share mode first, so all pieces won't be downloaded before 
    // unset share mode 
    torrent_.set_share_mode(true);
  }

  if (!params_.cluster_config.prefix_path.empty()) {
    if (!ParseSyms()) {
      NOTICE_LOG("Not support cluster(hadoop) download.");
      return false;
    }
    NOTICE_LOG("support cluster(hadoop) download.");
    DEBUG_LOG("cluster path: hdfs://%s@%s:%d%s", params_.cluster_config.user.c_str(),
               params_.cluster_config.host.c_str(), params_.cluster_config.port,
               params_.cluster_config.prefix_path.c_str());
    cluster_downloader_.reset(new ClusterDownloader(params_.cluster_config,
                                                    torrent_,
                                                    configure_->cluster_thread_num(),
                                                    params_.ignore_hdfs_error));
  }

  string down_limit, up_limit;
  BytesToReadable(configure_->download_limit(), &down_limit);
  BytesToReadable(configure_->upload_limit(), &up_limit);
  TRACE_LOG("downlimit: %s, uplimit: %s, maxconn: %d", down_limit.c_str(),
             up_limit.c_str(), torrent_.max_connections());
  return true;
}

bool Downloader::Init() {
  SetSignalAction();
  if (!peer_stat_.Open(configure_->peer_stat_file())) {
    WARNING_LOG("Open peer stat file %s failed", configure_->peer_stat_file().c_str());
  }

  if (params_.trackers.empty()) {
    if (!thrift_tracker_.LoadThriftTrackers(configure_->tracker_conf_file())) {
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
  }

  session_.reset(new libtorrent::session(libtorrent::fingerprint("GK", 3, 0, 0, 0),
                                         params_.listen_port_range, "0.0.0.0", 0,
                                         alert::error_notification | alert::peer_notification |
                                         alert::tracker_notification | alert::storage_notification |
                                         alert::status_notification | alert::performance_warning |
                                         alert::ip_block_notification | alert::debug_notification));
  session_settings settings = libtorrent::high_performance_seed();
  settings.use_php_tracker = false;
  settings.use_c_tracker = true;
  settings.stop_tracker_timeout = 1;
  settings.no_connect_privileged_ports = false;
  settings.allow_multiple_connections_per_ip = true;
  settings.alert_queue_size = 50000;
  settings.num_want = configure_->peers_num_want();
  settings.low_prio_disk = false;
  settings.lock_disk_cache = configure_->lock_disk_cache();
  if (configure_->suggest_mode()) {
    settings.explicit_read_cache = true;
    settings.suggest_mode = session_settings::suggest_read_cache;
  }
  settings.disk_io_read_mode = params_.use_dio_read ?
      session_settings::disable_os_cache : session_settings::enable_os_cache;
  settings.disk_io_write_mode = params_.use_dio_write ?
      session_settings::disable_os_cache : session_settings::enable_os_cache;
  settings.max_metadata_size = configure_->max_metadata_size();
  settings.min_announce_interval = configure_->seeding_announce_interval();
  settings.max_announce_interval = configure_->max_announce_interval();
  settings.min_reconnect_time = configure_->min_reconnect_time();
  settings.peer_connect_timeout = configure_->peer_connect_timeout();
  settings.max_queued_disk_bytes = configure_->max_queued_disk_bytes();
  settings.max_out_request_queue = configure_->max_out_request_queue();
  settings.max_allowed_in_request_queue = configure_->max_allowed_in_request_queue();
  settings.whole_pieces_threshold = configure_->whole_pieces_threshold();
  settings.request_queue_time = configure_->request_queue_time();
  settings.cache_size = configure_->cache_size();
  settings.cache_expiry = configure_->cache_expiry();
  settings.read_cache_line_size = configure_->read_cache_line_size();
  settings.write_cache_line_size = configure_->write_cache_line_size();
  settings.file_pool_size = configure_->file_pool_size();
  settings.send_buffer_watermark = configure_->send_buffer_watermark();
  settings.send_buffer_low_watermark = configure_->send_buffer_low_watermark();
  settings.send_socket_buffer_size = configure_->send_socket_buffer_size();
  settings.recv_socket_buffer_size = configure_->recv_socket_buffer_size();
  settings.dont_count_slow_torrents = true;
  settings.active_seeds = 1;
  settings.active_downloads = 1;
  settings.active_limit = 1;
  settings.ignore_limits_on_local_network = false;
  settings.enable_outgoing_utp = false;
  settings.enable_incoming_utp = false;
  settings.choking_algorithm = session_settings::auto_expand_choker;
  settings.seed_choking_algorithm = session_settings::round_robin;
  settings.unchoke_slots_limit = 1000;
  settings.ignore_resume_timestamps = true;
  settings.anonymous_mode = false;
  //settings.no_recheck_incomplete_resume = true;
  session_->set_settings(settings);
  session_->add_extension(&libtorrent::create_metadata_plugin);
  session_->add_extension(&create_bbts_torrent_plugin);
  session_->set_ip_filter(params_.filter);
  session_->add_ex_announce_func(
      boost::bind(&ThriftTracker::ThriftTrackerAnnounce, &thrift_tracker_, _1, _2));

  error_code ec;
  stat_.ip = GetLocalHostIPString();
  stat_.port = session_->listen_port();
  stat_.hostname = GetLocalHostname();
  stat_.download_limit = configure_->download_limit();
  stat_.upload_limit = configure_->upload_limit();
  thrift_tracker_.SetAttribute(session_->id().to_string(), stat_.ip);
  NOTICE_LOG("[version: %s][build date: %s][pid: %s]",
              GINGKO_VERSION, BUILD_DATE, libtorrent::to_hex(session_->id().to_string()).c_str());
  return true;
}

int Downloader::Download() {
  if (!Init()) {
    return -1;
  }

  if (!AddDownloadTask()) {
    FATAL_LOG("add download task fail.");
    return -1;
  }

  if (!params_.control_path.empty() && !control_server_.Serve(0700)) {
    FATAL_LOG("start control server failed.");
    return -1;
  }

  int ret = 0;
  thrift_tracker_.set_announce_callback(boost::bind(&Downloader::AnnounceCallback, this, _1, &ret));
  StartDownload();
  error_code ec;
  io_service_.post(boost::bind(&Downloader::ProcessAlert, this, &ret, true));
  try {
    io_service_.run(ec);
  } catch (libtorrent_exception &e) {
    FATAL_LOG("exception: %s, torrent_ maybe removed", e.what());
    ret = -1;
  } catch (std::exception &e) {
    FATAL_LOG("exception: %s, torrent_ maybe removed", e.what());
    ret = -1;
  }

  if (ret != 0) {
    FATAL_LOG("download fail: %d", ret);
  } else {
    NOTICE_LOG("download success.");
    unlink(resume_file_.c_str());
    rmdir(tmp_save_path_.c_str());
    if (meta_msg_) {
      BBTSClient client;
      client.AddMetadata(meta_msg_.get());    //忽略返回值
    }
  }
  stat_.retval = ret;
  stat_.end_time = time(NULL);
  PrintTaskStatistics(stat_, configure_->task_stat_file());
  return ret;
}

static void WriteBaseMessage(const shared_ptr<UnixSocketConnection> &connection,
                             const error_code &ec) {
  bbts::message::BaseRes response;
  response.set_fail_msg(ec.message());
  response.set_ret_code(ec.value());
  WriteMessage(connection, RES_BASE, response);
}

static message::TaskStatus::status_t GetStatusCode(const torrent_status &ts) {
  if (!ts.error.empty()) {
    return message::TaskStatus_status_t_ERROR;
  }

  if (ts.paused) {
    return message::TaskStatus_status_t_PAUSED;
  } else if (ts.is_finished) {
    return message::TaskStatus_status_t_SEEDING;
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

void Downloader::ListTask(const shared_ptr<UnixSocketConnection> &connection) {
  message::BatchListRes response;
  message::TaskStatus *task_status = response.add_status();
  message::Task *task_info = task_status->mutable_task();
  task_info->set_taskid(-1);
  task_info->set_cmd(params_.cmd);
  task_info->set_uid(getuid());
  task_info->set_gid(getgid());
  task_info->set_infohash(params_.infohash);
  task_info->set_save_path(params_.save_path);
  task_info->set_new_name(params_.new_name);
  torrent_status ts = torrent_.status(0);
  task_status->set_status(GetStatusCode(ts));
  task_status->set_error(ts.error);
  task_status->set_progress(ts.progress_ppm);
  task_status->set_num_peers(ts.num_peers);
  task_status->set_num_seeds(ts.num_seeds);
  task_status->set_upload_rate(ts.upload_rate);
  task_status->set_download_rate(ts.download_rate);
  task_status->set_total_upload(ts.total_payload_upload);
  task_status->set_total_download(ts.total_payload_download);
  WriteMessage(connection, RES_BATCH_LIST, response);
}

#define PARSE_MSG(msg) \
do {\
    if (!message.ParseFromArray(ptr + sizeof(type), data->size() - sizeof(type))) {\
      WARNING_LOG("parse message " msg " failed, type(%u)", type);\
      ec.assign(bbts::errors::PROTOBUF_PARSE_ERROR, get_error_category());\
      break;\
    }\
} while(0)

void Downloader::ControlHandler(const shared_ptr<UnixSocketConnection> &connection,
                                const shared_ptr<const vector<char> > &data) {
  const char *ptr = &(*data)[0];
  uint32_t type = *reinterpret_cast<const uint32_t *>(ptr);
  DEBUG_LOG("control handler, type: %u", type);
  error_code ec;
  switch (type) {
    case REQ_TASK_SETOPT: {
      message::TaskOptions message;
      PARSE_MSG("REQ_TASK_GETOPT");
      if (ec) {
        break;
      }
      if (message.has_download_limit()) {
        configure_->set_download_limit(message.download_limit());
        torrent_.set_download_limit(configure_->download_limit());
      }
      if (message.has_upload_limit()) {
        configure_->set_upload_limit(message.upload_limit());
        torrent_.set_upload_limit(configure_->upload_limit());
      }
      if (message.has_max_conections()) {
        configure_->set_connection_limit(message.max_conections());
        torrent_.set_max_connections(configure_->connection_limit());
      }
      WriteBaseMessage(connection, ec);
      break;
    }

    case REQ_TASK_GETOPT: {
      message::TaskOptions message;
      PARSE_MSG("REQ_TASK_GETOPT");
      if (ec) {
        break;
      }
      if (message.has_download_limit()) {
        message.set_download_limit(configure_->download_limit());
      }
      if (message.has_upload_limit()) {
        message.set_upload_limit(configure_->upload_limit());
      }
      if (message.has_max_conections()) {
        message.set_max_conections(configure_->connection_limit());
      }
      WriteMessage(connection, RES_TASK_GETOPT, message);
      break;
    }

    case REQ_BATCH_CTRL: {
      message::BatchCtrl message;
      PARSE_MSG("REQ_BATCH_CTRL");
      if (message.ctrl_type() == message.LIST) {
        ListTask(connection);
      } else {
        WARNING_LOG("ctrl type type (%u) not support.", message.ctrl_type());
        ec.assign(bbts::errors::MSG_ARGS_ERROR, get_error_category());
      }
      break;
    }

    default:
      WARNING_LOG("message type (%u) not support.", type);
      ec.assign(bbts::errors::MSG_ARGS_ERROR, get_error_category());
      break;
  }

  if (ec) {
    WriteBaseMessage(connection, ec);
  }
}

// is_before_download: if true, means mkdir before download,
// else means mkdir after download finished
bool Downloader::MakeCorrectDir(const torrent_info &ti, bool is_before_download) {
  string save_path = params_.save_path;
  const vector<string>& paths = ti.get_paths();
  const vector<mode_t>& modes = ti.get_paths_mode();
  int path_len = paths.size();
  for (int i = 0; i < path_len; ++i) {
    string path = save_path + "/" + paths[i];
    mode_t mode = modes[i];
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) != 0) {
      bool need = false;
      if (params_.patition_download) {
        RegexMatch regex_m(paths[i]);
        if (params_.include_regex.empty() || std::find_if(
            params_.include_regex.begin(), params_.include_regex.end(), regex_m) != params_.include_regex.end()) {
          need = true;
        }
        if (std::find_if(params_.exclude_regex.begin(), params_.exclude_regex.end(), regex_m) !=
            params_.exclude_regex.end()) {
          need = false;
        }
      } else {
        need = true;
      }
      if (is_before_download && !need) {
        continue;
      }
      if (!check_and_mkdir_r(path.c_str(), mode) != 0) {
        WARNING_LOG("mkdir %s(%o) failed.", path.c_str(), mode);
        return false;
      }
    } else if (statbuf.st_mode & S_IFDIR) {
      if (chmod(path.c_str(), mode) != 0) {
        WARNING_LOG("path(%s) chmod to %o failed.", path.c_str(), mode);
        return false;
      }
    } else {
      WARNING_LOG("path %s is not a dir, can't change mode.", path.c_str());
      return false;
    }
  }

  return true;
}

void Downloader::PreAllocateThread() {
  DEBUG_LOG("start PreAllocateThread...");
  const int kDefaultMultiple = 50;
  boost::intrusive_ptr<torrent_info const> ti = torrent_.torrent_file();

  string save_path = params_.save_path;
  int num_pieces = ti->num_pieces();
  int piece_length = ti->piece_length();

  // if dynamic_allocate is 0 or error, use default
  int multiple = params_.dynamic_allocate > 0 ? params_.dynamic_allocate : kDefaultMultiple;
  const off_t kEachLength = multiple * piece_length;
  int last_download_index = 0;

  vector<int> piece_priority(num_pieces, 0);
  torrent_.set_share_mode(false);
  // after set_share_mode called, all file_priorities will be set to 1, so we should set again
  torrent_.prioritize_files(pre_file_priorities_);
  // set all piece's priority to 0, thus all piece won't been download before set 1 again.
  torrent_.prioritize_pieces(piece_priority);

  // first, mkdir
  MakeCorrectDir(*ti, true);

  // allocate each file
  for (int i = 0; i < ti->num_files(); ++i) {
    libtorrent::file_entry fe = ti->file_at(i);
    if (pre_file_priorities_[i] == 0 || fe.pad_file) {
      continue;
    }

    string file_name = save_path + "/" + fe.path;
    int first_piece = ti->map_file(i, 0, 0).piece;
    int last_piece = ti->map_file(i, (std::max)(fe.size - 1, libtorrent::size_type(0)), 0).piece;
    // check if file exists and file size. if file has correct size, then we just continue
    // because if we allocate again, libtorrent may think all pieces of this file has been downloaded,
    // so after allocate this file, libtorrent won't download again, thus we may get an empty file.
    struct stat file_buf;
    if (stat(file_name.c_str(), &file_buf) == 0 && file_buf.st_size == fe.size) {
      NOTICE_LOG("file %s has size:%ld, and goat size is %ld, so don't allocate again",
                  file_name.c_str(), file_buf.st_size, fe.size);
      // set piece correct priority
      for (int i = first_piece; i <= last_piece; ++i) {
        piece_priority[i] = 1;
      }
      torrent_.prioritize_pieces(piece_priority);
      last_download_index = last_piece;
      continue;
    }

    int fd = open(file_name.c_str(), O_RDWR | O_CREAT | O_TRUNC, fe.mode);
    if (fd < 0) {
      // if one file allocate failed, all_allocate_size will be error if continue,
      // so, just break and don't use dynamic pre-allocate mode
      WARNING_LOG("open file %s failed, don't use dynamic pre-allocate mode, use normal mode:errno[%d]:%s",
                   file_name.c_str(), errno, strerror(errno));
      close(fd);
      break;
    }
    DEBUG_LOG("open file %s success, size is %ld", file_name.c_str(), fe.size);

    int file_download_index = first_piece;
    off_t current_offset = 0;
    while (current_offset < fe.size) {
      off_t each_len = kEachLength;
      if ((current_offset + kEachLength) > fe.size) {
        each_len = fe.size - current_offset;
      }
      int fallocate_ret = posix_fallocate(fd, current_offset, each_len);
      if (fallocate_ret != 0 && fallocate_ret != EINVAL) {
          WARNING_LOG("posix_fallocate failed:ret=%d, offset=%ld, len=%ld, file=%s\n",
                 fallocate_ret, current_offset, each_len, file_name.c_str());
          break;
      }
      DEBUG_LOG("allocate current_offset:%ld", current_offset);

      current_offset += each_len;
      if (current_offset >= fe.size) {
        // this file has been success allocate, goto next one
        DEBUG_LOG("current_offset size is %ld", current_offset);
        break;
      }

      int current_index = current_offset / piece_length;
      for (int i = file_download_index; i <= (current_index + first_piece); ++i) {
        piece_priority[i] = 1;
      }
      torrent_.prioritize_pieces(piece_priority);
      file_download_index = current_index + first_piece;
    }  // while
    
    close(fd);

    // ensure all pieces of this file can be download
    if (file_download_index < last_piece) {
      for (int i = file_download_index; i <= last_piece; ++i) {
        piece_priority[i] = 1;
      }
      torrent_.prioritize_pieces(piece_priority);
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
    torrent_.prioritize_pieces(piece_priority);
  }

  NOTICE_LOG("pre allocate thread exit");
}


} // namespace tool
} // namespace bbts

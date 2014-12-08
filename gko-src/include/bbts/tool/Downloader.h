/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   Downloader.h
 *
 * @author liuming03
 * @date   2013-7-26
 * @brief 
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_TOOL_DOWNLOADER_H_
#define OP_OPED_NOAH_TOOLS_BBTS_TOOL_DOWNLOADER_H_

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/regex.hpp>
#include <boost/thread.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/ip_filter.hpp>

#include "bbts/statistics.h"
#include "bbts/ThriftTracker.h"
#include "bbts/UnixSocketServer.h"

namespace libtorrent {
class session;
class ex_announce_response;
class torrent_info;
}

namespace bbts {
namespace message {
class DownloadConfigure;
class Metadata;
}

namespace tool {

class ClusterDownloader;

struct down_params_t {
  down_params_t();

  int seeding_time;
  int timeout;
  int hang_timeout;
  int mem_limit;
  bool debug:1;
  bool print_progress:1;
  bool patition_download:1;
  bool need_save_resume:1;
  bool need_down_to_tmp_first:1;

  // download task quit if tracker can't connection to
  bool quit_by_tracker_failed:1;
  // allocate storage before download
  bool storage_pre_allocate:1;
  // dynamic allocate multiple of piece length
  int dynamic_allocate;

  // use direct io mode
  bool use_dio_read:1;
  bool use_dio_write:1;

  bool ignore_hdfs_error:1;
  std::string torrent_path;
  std::string infohash;
  std::string torrent_url;
  std::string new_name;
  std::string save_path;
  libtorrent::cluster_config_entry cluster_config;
  std::vector<std::string> web_seeds;
  std::pair<int, int> listen_port_range;
  std::vector<boost::regex> include_regex;
  std::vector<boost::regex> exclude_regex;
  std::string control_path;
  std::string save_torrent_path;
  std::string cmd;
  libtorrent::ip_filter filter;

  // fixed tracker while user assign
  std::vector<std::pair<std::string, int> > trackers;
};

/**
 * @brief
 */
class Downloader : private boost::noncopyable {
 public:
  Downloader(const down_params_t &params);
  virtual ~Downloader();

  int Download();

 private:
  bool Init();
  bool AddDownloadTask();
  void StartDownload();
  void SeedingTimerCallback(const boost::system::error_code &ec);
  void TimeoutTimerCallback(const boost::system::error_code &ec);
  void HangCheckCallback(const boost::system::error_code &ec);
  void ProgressCallback(const boost::system::error_code &ec);
  bool CorrectModeAndSymlinks();
  void ToSeeding();
  void ProcessAlert(int *retval, bool loop);
  bool OnTorrentFinished();
  bool OnStateChanged(const libtorrent::state_changed_alert *alert);
  void OnMetadataReceived();
  void OnHashFailed(const libtorrent::hash_failed_alert *alert);
  void OnSaveResumeData(const libtorrent::save_resume_data_alert *alert);
  bool OnStorageMoved();
  void RemoveTorrent();
  void ControlHandler(const boost::shared_ptr<UnixSocketConnection> &connection,
                      const boost::shared_ptr<const std::vector<char> > &data);
  void AnnounceCallback(const boost::shared_ptr<libtorrent::ex_announce_response> &response,
                        int *retval);
  void ListTask(const boost::shared_ptr<UnixSocketConnection> &connection);

  void PreAllocateThread();

  bool MakeCorrectDir(const libtorrent::torrent_info &ti, bool is_before_download = false);

  void SetOrgPieces(const boost::intrusive_ptr<libtorrent::torrent_info const> ti);

  message::DownloadConfigure *configure_;
  down_params_t params_;
  boost::scoped_ptr<libtorrent::session> session_;
  libtorrent::torrent_handle torrent_;
  volatile bool should_stop_;
  boost::asio::io_service io_service_;
  ThriftTracker thrift_tracker_;
  boost::asio::deadline_timer seeding_timer_;
  boost::asio::deadline_timer timeout_timer_;
  bool is_timeout_:1;
  bool is_mem_exceed_:1;
  boost::scoped_ptr<ClusterDownloader> cluster_downloader_;

  // hang住检测以及下载速率效率800KB/s
  boost::asio::deadline_timer hang_check_timer_;
  time_t last_check_time_;
  int64_t last_check_downloaded_;
  int low_downrate_times_;

  // print progress
  boost::asio::deadline_timer progress_timer_;

  // tmp save path
  std::string tmp_save_path_;

  // resume file name
  std::string resume_file_;

  // Tell Agent
  boost::scoped_ptr<message::Metadata> meta_msg_;

  // allocate thread
  boost::thread allocate_threads_;
  // store org_pieces before dynamic allocate thread start 
  libtorrent::bitfield org_pieces_;
  // store pre_file_prorities
  std::vector<int> pre_file_priorities_;
  // this flag ensure pre allocate thread start only once
  bool has_pre_allocate_thread_start_;

  UnixSocketServer control_server_;

  task_statistics_t stat_;
  PeerStatFile peer_stat_;

};

} // namespace tool
} // namespace bbts
#endif // OP_OPED_NOAH_TOOLS_BBTS_TOOL_DOWNLOADER_H_

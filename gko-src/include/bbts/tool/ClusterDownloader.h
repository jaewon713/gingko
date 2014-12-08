/**
 * @file ClusterDownloader.h
 *
 * @author liuming03
 * @date 2013-7-29
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_DOWNLOAD_CLUSTER_DOWNLOADER_H_
#define OP_OPED_NOAH_TOOLS_BBTS_DOWNLOAD_CLUSTER_DOWNLOADER_H_

#include <queue>

#include <boost/noncopyable.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>
#include <libtorrent/torrent_handle.hpp>

#include "bbts/SpeedLimit.h"

namespace bbts {

class BTInfo;

namespace tool {

class ClusterDownloader : private boost::noncopyable {
 public:
  ClusterDownloader(const libtorrent::cluster_config_entry &config,
                    libtorrent::torrent_handle handle,
                    int threads_num,
                    bool ignore_error);

  virtual ~ClusterDownloader();

  bool StartDownload();
  void Join();
  void StopDownload();
  bool IsStoped();

  inline int get_retval() {
    return retval_;
  }

  inline void set_retval(int retval) {
    retval_ = retval;
  }

  inline void add_hash_failed_piece(int piece_index) {
    {
      boost::mutex::scoped_lock lock(mutex_);
      hash_failed_pieces_.push(piece_index);
    }
    cond_.notify_one();
  }

  inline int64_t get_cluster_total_download() {
    return cluster_total_download_;
  }

  inline time_t get_start_time() {
    return start_time_;
  }

  inline const libtorrent::cluster_config_entry& get_cluster_config() {
    return config_;
  }

 private:
  void Download();
  int GetUndownloadedPiece();
  int GetPieceToDownload(int last_piece_downloaded, bool &get_from_hash_failed);

  libtorrent::cluster_config_entry config_;
  libtorrent::torrent_handle torrent_;
  int threads_num_;                          // 集群下载线程数
  int complete_threads_num_;
  boost::thread_group thread_group_;         // 集群下载线程池
  boost::scoped_ptr<BTInfo> btinfo_;
  void *fs_;
  libtorrent::bitfield pieces_have_;         // 已经有的pieces位域
  libtorrent::bitfield pieces_downloading_;  // 正在下载的pieces
  int64_t last_p2p_total_downloaded_;
  std::queue<int> hash_failed_pieces_;       // libtorrent hash failed piece vec
  boost::mutex mutex_;
  boost::condition_variable cond_;
  volatile bool should_stop_;                // 下载线程是否需要停止下载
  int retval_;
  SpeedLimit download_limit_;
  int64_t cluster_total_download_;

  time_t start_time_;
  bool ignore_error_;
};

} // namespace tool
} // namespace bbts
#endif // OP_OPED_NOAH_TOOLS_BBTS_DOWNLOAD_CLUSTER_DOWNLOADER_H_

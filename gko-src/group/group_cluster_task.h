/***************************************************************************
 *
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

#ifndef OP_OPED_NOAH_TOOLS_BBTS_GROUP_CLUSTER_TASK_H_
#define OP_OPED_NOAH_TOOLS_BBTS_GROUP_CLUSTER_TASK_H_

#include <queue>

#include <boost/noncopyable.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>
#include <libtorrent/torrent_handle.hpp>

#include "bbts/SpeedLimit.h"

#include "group_manager.h"

namespace bbts {

class BTInfo;

namespace group {

class GroupClusterTask : private boost::noncopyable {
 public:
  GroupClusterTask(const libtorrent::cluster_config_entry &config,
                   const libtorrent::torrent_handle &handle,
                   int threads_num,
                   bool ignore_error);

  virtual ~GroupClusterTask();

  // tell group_manager that we need start some threads to start hdfs download
  bool StartDownload();

  // tell all this cluster task's  downloading thread to wake up and exit
  void StopDownload();

  // judge if need stopped
  bool IsStoped();

  // download process, called by threads
  void Download();

  inline int retval() {
    return retval_;
  }

  inline void set_retval(int retval) {
    retval_ = retval;
  }

  inline void AddHashFailedPiece(int piece_index) {
    {
      boost::mutex::scoped_lock lock(mutex_);
      hash_failed_pieces_.push(piece_index);
    }
    cond_.notify_one();
  }

  inline int64_t cluster_total_download() {
    return cluster_total_download_;
  }

  inline time_t start_time() {
    return start_time_;
  }

  inline const libtorrent::cluster_config_entry& cluster_config() {
    return config_;
  }

  int inline threads_num() {
    return threads_num_;
  }

  // if current_start_thread_num_ > threads_num_, will not create new download sthread
  inline bool CheckIfThreadnumOut() {
    return (current_start_thread_num_ >= threads_num_) ? true : false;
  }

  // need lock, so don't inline here
  int current_start_thread_num();

  // if StopDownload, we need to notify all supspend thread belongs to this task
  void AddThreadToGroup(boost::thread *thread);

 private:
  int GetUndownloadedPiece();
  int GetPieceToDownload(int last_piece_downloaded, bool &get_from_hash_failed);

  libtorrent::cluster_config_entry config_;
  libtorrent::torrent_handle torrent_;
  int threads_num_;                          // thread number of cluster task
  int current_start_thread_num_;             // num of therad of this task that has started
  int complete_threads_num_;                 // num of complete threads num
  boost::shared_ptr<BTInfo> btinfo_;
  boost::thread_group thread_group_;         // thread for this task, only use for wait all thread exit
  void *fs_;
  libtorrent::bitfield pieces_have_;         // bitmap of pieces which have downloaded
  libtorrent::bitfield pieces_downloading_;  // bitmap of pieces which in downloading
  int64_t last_p2p_total_downloaded_;
  std::queue<int> hash_failed_pieces_;       // libtorrent hash failed piece vec
  boost::mutex mutex_;
  boost::condition_variable cond_;
  volatile bool should_stop_;                // weather need stop
  int retval_;
  SpeedLimit download_limit_;
  int64_t cluster_total_download_;

  time_t start_time_;
  bool ignore_error_;
  bool is_stoped_;
};

} // namespace group
} // namespace bbts

#endif  // OP_OPED_NOAH_TOOLS_BBTS_GROUP_CLUSTER_TASK_H_

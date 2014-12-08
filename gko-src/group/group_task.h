/***************************************************************************
 *
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

#ifndef OP_OPED_NOAH_TOOLS_BBTS_GROUP_TASK_H_
#define OP_OPED_NOAH_TOOLS_BBTS_GROUP_TASK_H_

#include <boost/asio/deadline_timer.hpp>
#include <boost/noncopyable.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>

#include "bbts/statistics.h"
#include "bbts/ThriftTracker.h"

#include "group_cluster_task.h"
#include "gen-cpp/GroupManagerService.h"

namespace bbts {
namespace group {

class GroupClusterTask;

/**
 * @brief group task
 * each task map a torrent_handle and a infohash
 */
class GroupTask : private boost::noncopyable {
 public:
  GroupTask();
  ~GroupTask();

  inline std::string infohash() const {
    return infohash_;
  }

  inline std::string get_data_path() const {
    return save_path_ + '/' + new_name_;
  }

  inline libtorrent::torrent_handle torrent_handle() const {
    return torrent_handle_;
  }

  inline TaskStatus& task_status() {
    return task_status_;
  }

  inline int64_t end_time() {
    return task_status_.end_time;
  }

  inline void set_error(const std::string &error) {
    error_ = error;
  }

  inline void set_task_status(const TaskStatus &task_status) {
    task_status_ = task_status;
  }

  inline int32_t download_limit() {
    return params_.download_limit;
  }

  inline int32_t upload_limit() {
    return params_.upload_limit;
  }

  inline int32_t connections_limit() {
    return params_.connections_limit;
  }

  inline bool is_torrent_url() {
    return is_torrent_url_;
  }
  
  // create a task by download parm and cred
  int Create(const DownloadParam &params, const UserCred &cred);

  // resume last task according to resume_file
  int CreateFromResumeFile(const std::string &resume_file, int64_t file_size);

  // check if user has permit to operate this task
  int CheckCred(const UserCred &cred) const;

  // pause current task
  void Pause();

  // resume current task
  void Resume();

  // cancel current task
  int Cancel();

  // this task turns to seeding mode
  void ToSeeding();

  // persist this task into resume file, if task finished, delete resume file
  void Persist();

  // remove torrent_handle of this task
  void RemoveTorrent(bool is_removed_by_user);

  // control current task
  void ControlTask(ControlTaskType::type type);

  // generate/delete task resume file
  void GenerateResumeFile() const;
  void DeleteResumeFile() const;

  // generate task serialized string 
  void GenerateResumeTaskString(std::string *serialized_str) const;

  // set task options
  int SetOptions(const TaskOptions &options);

  // get task options
  int GetOptions(TaskOptions *options);

  // get task options

  // libtorrent on_calls
  int OnTorrentFinished();
  int OnStorageMoved();
  int OnStateChanged(const libtorrent::state_changed_alert *alert);
  int OnHashFailed(const libtorrent::hash_failed_alert *alert);
  int OnSaveResumeData(const libtorrent::save_resume_data_alert *alert);
  int OnMetadataReceived();

  // storage functions
  int CorrectModeAndSymlinks();
  int MakeCorrectDir(const libtorrent::torrent_info &ti, bool is_before_download = true);

  void SetOrgPieces(const boost::intrusive_ptr<libtorrent::torrent_info const> ti);

  void UpdateTaskStatus();
  void HangCheckCallback();
  void StopClusterTask();
  void SeedingTimerCallback(const boost::system::error_code &ec);
  void TimeoutTimerCallback(const boost::system::error_code &ec);

 private:
  void UpdateStatus(bool is_finish = false) const;
  void PreAllocateThread();

  enum ControlTaskStatus{
    CONTROL_STATUS_NORMAL   = 0,
    CONTROL_STATUS_CANCELED = 11,
    CONTROL_STATUS_PAUSED   = 12,
    CONTROL_STATUS_RESUME   = 13,
    CONTROL_STATUS_PERSIST  = 14,
    CONTROL_STATUS_FINISHED = 15,
    CONTROL_STATUS_TIMEOUT  = 16,
  };

  ControlTaskStatus control_status_;            // inner control task status
  std::string infohash_;
  UserCred cred_;                               // uid and pid of this task
  std::string save_path_;
  std::string new_name_;                        // file new_name, hasn't use in this version

  std::vector<std::pair<std::string, int> > trackers_;
  DownloadParam params_;
  int seeding_time_;                           // specify seeding time
  boost::asio::deadline_timer seeding_timer_;
  boost::asio::deadline_timer timeout_timer_;
  libtorrent::torrent_handle torrent_handle_;
  std::string error_;
  task_statistics_t stat_;
  std::string tmp_save_path_;
  std::string resume_file_;

  // allocate thread
  boost::thread allocate_threads_;
  // store org_pieces before dynamic allocate thread start 
  libtorrent::bitfield org_pieces_;
  // store pre_file_prorities
  std::vector<int> pre_file_priorities_;
  // this flag ensure pre allocate thread start only once
  bool has_pre_allocate_thread_start_;

  TaskStatus task_status_;
  time_t last_check_time_;
  int64_t last_check_downloaded_;
  bool is_timeout_:1;
  bool is_mem_exceed_:1;
  int low_downrate_times_;
  bool is_torrent_url_:1;     // if true, means torrent param use weburl, else not

  boost::shared_ptr<GroupClusterTask> cluster_task_;
};

} // namespace agent
} // namespace bbts
#endif  // OP_OPED_NOAH_TOOLS_BBTS_GROUP_TASK_H_

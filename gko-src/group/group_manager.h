/***************************************************************************
 *
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

#ifndef OP_OPED_NOAH_TOOLS_BBTS_GROUP_MANAGER_H_
#define OP_OPED_NOAH_TOOLS_BBTS_GROUP_MANAGER_H_

#include <queue>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/unordered_map.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/session.hpp>
#include <thrift/server/TThreadPoolServer.h>

#include "bbts/message.pb.h"
#include "bbts/statistics.h"
#include "bbts/ThriftTracker.h"
#include "bbts/SpeedLimit.h"

#include "define.h"
#include "gen-cpp/GroupManagerService.h"
#include "group_cluster_task.h"
#include "group_task.h"
#include "group_manager_service_handler.h"

namespace bbts {
namespace group {

class GroupTask;
class GroupClusterTask;

class GroupManager : private boost::noncopyable {
 public:
  // infohash=>task
  typedef boost::unordered_map<std::string, boost::shared_ptr<GroupTask> > InfohashTaskMap;

  // torrent_handle=>task
  typedef boost::unordered_map<libtorrent::torrent_handle, boost::shared_ptr<GroupTask> > TorrentTaskMap;

  // finished task lists, infohash=>task
  typedef std::vector<std::pair<std::string, boost::shared_ptr<GroupTask> > > FinishedInfohashList;

  static GroupManager &GetInstance() {
    static GroupManager manager;
    return manager;
  }

  // run task
  int Run();

  // shutdown when group exit
  void Shutdown();

  // join to wait thread exit
  void Join();

  // add a task to this group
  ClientRpcStatus AddTask(const DownloadParam &down_params,
                          const UserCred &cred);

  // get a special task or all tasks' status
  int GetTaskStatus(const std::string &infohash,
                    bool is_full,
                    std::vector<TaskStatus> &task_status_list);

  // control task, like cancel/pause/resume
  ClientRpcStatus ControlTask(const std::string &infohash,
                              const bool is_all,
                              const ControlTaskType::type type,
                              const UserCred &cred);

  // set task options
  ClientRpcStatus SetTaskOptions(const TaskOptions &options);

  // get task options
  int GetTaskOptions(const std::string &infohash,
                     std::vector<TaskOptions> &task_options_list);

  // delete cluster_thread_num, called by GroupClusterTask when thread completed
  void DeleteClusterThreadNum();

  // remove torrent, but keep it in this group(finished_task_list) for a time
  void RemoveTorrentTmp(const libtorrent::torrent_handle &torrent_handle,
                        const std::string &infohash);

  // push cluster task into thread queue, thread will start later
  void PushClusterThreadQueue(boost::shared_ptr<GroupClusterTask> task);

  // control session download limit
  void SessionDownloadLimit();


  // update torrent url's infohash when metadata is received
  void UpdateInfohashByTorrentUrl(const std::string &torrent_url,
                                  const std::string &infohash);

  boost::asio::io_service &io_service() {
    return io_service_;
  }

  libtorrent::session *session() {
    return session_.get();
  }

  inline const ThriftTracker& thrift_tracker() const {
    return  thrift_tracker_;
  }

 private:
  GroupManager();
  ~GroupManager();

  // timer check cycle, include if group timeout, if finished tasks timeout, etc.
  static const int kTimerCheckCycle = 3;      // 3 seconds

  // cluster task start thread check cycle
  static const int kClusterRunCycye = 5;      // 5 seconds

  // max conf failed times
  static const int kConfMaxFailed = 5;      // 5 times, total 5*3 = 15s

  // start serve thread for thrift server
  int StartThriftThread();
  // group server thread function
  void GroupManagerServer();

  // start cluster thread
  int StartClusterThread();
  // cluster thread function
  void ClusterDownload();

  // process alert on all torrent
  void ProcessAlert();
  // hang check, also check if timeout
  void TimerCallback(const boost::system::error_code &ec);

  // some libtorrent session on_calls
  void OnTorrentFinished(const libtorrent::torrent_finished_alert *alert);
  void OnListenFailed(const libtorrent::listen_failed_alert *alert);
  void OnTorrentError(const libtorrent::torrent_error_alert *alert);
  void OnStateChanged(const libtorrent::state_changed_alert *alert);
  void OnHashFailed(const libtorrent::hash_failed_alert *alert);
  void OnMetadataReceived(const libtorrent::metadata_received_alert *alert);
  void OnSaveResumeData(const libtorrent::save_resume_data_alert *alert);
  void OnStorageMoved(const libtorrent::storage_moved_alert *alert);

  // set session settings, if conf file changed, will called this function to reload
  void SetSessionSettings();

  // get task by torrent handle
  boost::shared_ptr<GroupTask> GetTaskByTorrentHanlde(const libtorrent::torrent_handle &handle);

  // get all tasks' status
  int GetAllTaskStatus(std::vector<TaskStatus> &task_status_list);

  // get task status by infohash
  int GetTaskStatusByInfohash(const std::string &infohash,
                              std::vector<TaskStatus> &task_status_list);

  // control task
  ClientRpcStatus ControlAllTasks(const ControlTaskType::type type,
                                  const UserCred &cred);
  ClientRpcStatus ControlTaskByInfohash(const std::string &infohash,
                                        const ControlTaskType::type type,
                                        const UserCred &cred);

  // add task to map
  ClientRpcStatus AddTaskToMap(boost::shared_ptr<GroupTask> &task);

  // add from resume file
  int AddResumeTask();

  // when finished_infohash_list_ changed, write persists info into file
  void GenerateFinishedListResumeFile();

  // load from finished resume file
  int LoadFromFinishedResume(const std::string &resume_file);

  // wake up all cluster task threads to exit
  void WakeUpAllClusterDownThread();

  // FinishedInfohashList operator
  boost::shared_ptr<GroupTask> GetFinishedTaskByInfohash(const std::string &infohash);
  int DeleteFinishedTaskByInfohash(const std::string &infohash);

 private:
  InfohashTaskMap infohash_map_;                    // infohash=>task
  TorrentTaskMap torrent_map_;                      // torrent_handle=>task
  FinishedInfohashList finished_infohash_list_;     // finished_infohash=>task

  boost::mutex tasks_lock_;                         // lock for inner maps
  boost::mutex queue_lock_;                         // lock for cluster task

  boost::asio::io_service io_service_;
  boost::scoped_ptr<libtorrent::session> session_;

  boost::thread thrift_thread_;
  boost::thread cluster_thread_;
  boost::scoped_ptr<apache::thrift::server::TThreadPoolServer> thrift_server_;
  libtorrent::tcp::endpoint listen_endpoint_;

  ThriftTracker thrift_tracker_;
  PeerStatFile peer_stat_;
  boost::asio::deadline_timer total_check_timer_;

  int32_t last_check_timeout_;                        // last check timeout timestamp
  time_t  last_conf_file_modify_time_;                // last conf file modify timestamp

  UserCred cred_;                                     // user cred of this group
  bool finished_infohash_list_changed_:1;             // if finished_infohash_list_ changed

  boost::thread_group thread_group_;                  // thread group of cluster task
  int32_t current_cluster_thread_num_;                // current cluster thread num

  std::queue<boost::shared_ptr<GroupClusterTask> > cluster_queue_tasks_;
  bool cluster_should_stop_:1;

  // cluster download limit
  bbts::SpeedLimit download_limit_;
  int64_t last_total_downloaded_;

  int32_t conf_failed_times_;                         // conf failed times, if failed more than kConfMaxFailed, quit
};  // class GroupManager

}  // namespace group
}  // namespace bbts

#endif  // OP_OPED_NOAH_TOOLS_BBTS_GROUP_MANAGER_H_

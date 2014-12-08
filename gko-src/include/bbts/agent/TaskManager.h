/***************************************************************************
 *
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file   TaskManager.h
 *
 * @author liuming03
 * @date   2013-1-14
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_AGENT_TASK_MANAGER_H_
#define OP_OPED_NOAH_TOOLS_BBTS_AGENT_TASK_MANAGER_H_

#include <list>
#include <map>

#include <boost/noncopyable.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/unordered_map.hpp>
#include <libtorrent/alert_types.hpp>

#include "bbts/message.pb.h"
#include "bbts/statistics.h"
#include "bbts/ThriftTracker.h"
#include "bbts/UnixSocketServer.h"
#include "bbts/WorkerPool.h"

namespace bbts {

namespace agent {

class TaskDB;
class Task;

/**
 * @brief 任务管理类
 *
 * 任务管理类是单实例的类，管理agent中的所有任务；其他线程的处理，均通过任务管理类的单实例进行处理
 */
class TaskManager : private boost::noncopyable {
 public:
  struct metadata_t {
    std::string infohash;
    std::string metadata;
    time_t add_time;
  };

  typedef boost::asio::local::stream_protocol::socket local_socket;
  typedef std::map<int64_t, boost::shared_ptr<Task> > tasks_map_t;
  typedef boost::unordered_map<libtorrent::torrent_handle, boost::weak_ptr<Task> > torrent_map_t;
  typedef boost::unordered_map<std::string, boost::weak_ptr<Task> > infohash_map_t;
  typedef boost::unordered_map<std::string, boost::weak_ptr<Task> > data_path_map_t;
  typedef std::list<boost::shared_ptr<metadata_t> > metadata_list_t;

  virtual ~TaskManager();

  // singal object
  static TaskManager& instance() {
    static TaskManager manager;
    return manager;
  }

  /**
   * @brief 任务管理器正式启动运行
   *
   * @return 成功返回true，否则返回false
   */
  bool Start();

  /**
   * @brief 等待TaskManager处理结束
   */
  void Join();

  /**
   * @brief 停止任务
   */
  void Stop();

  inline boost::asio::io_service& get_io_service() {
    return worker_pool_.get_io_service();
  }

  inline libtorrent::session* get_session() {
    return session_.get();
  }

  inline TaskDB* get_task_db() {
    return task_db_.get();
  }

  inline const ThriftTracker& get_thrift_tracker() const {
    return  thrift_tracker_;
  }

  inline libtorrent::tcp::endpoint& get_listen_endpoint() {
    return listen_endpoint_;
  }

  /**
   * @brief 做种完成后的回调函数
   *
   * @param taskid(in) 任务id
   * @param ec(in)     错误码
   */
  void SeedingTimerCallback(int taskid, const boost::system::error_code &ec);

 private:
  TaskManager();

  /**
   * @brief CmdDispatcher绑定的处理函数，交由ProcessMessage()进行处理
   *
   * @param peer_sock(in) 对端socket
   */
  void OnReadMessage(const boost::shared_ptr<UnixSocketConnection> &connection,
                     const boost::shared_ptr<const std::vector<char> > &data);

  void OnAccept(const boost::shared_ptr<UnixSocketConnection> &);

  /**
   * @brief 处理cmd发来的命令
   *
   * 从fd中读取命令解析后处理，处理完成后，写消息告知cmd。
   * @param peer_sock(in) cmd请求的socket
   */
  void ProcessMessage(const boost::shared_ptr<UnixSocketConnection> &connection,
                      const boost::shared_ptr<const std::vector<char> > &data);


  /**
   * @brief 持久化并删除TaskManager中的所有task
   */
  void PersistTasks();

  /**
   * @brief 获取infohash
   *
   * @param infohash(in)
   */
  boost::shared_ptr<metadata_t> GetMetaByInfohash(const std::string &infohash);
  /**
   * @brief 处理一次session中的alerts
   */
  void ProcessAlert();

  /**
   * @brief 删除（下载出错的）任务
   *
   * @param taskid
   * @param error 出错原因
   */
  void RemoveTaskByError(boost::shared_ptr<Task> task, const std::string &error);

  /**
   * @brief 根据torrent_handle找到对应的task，并对task调用指定的回调函数
   * @param torrent task(in) 对应的torrent_handle
   * @param cb(in)           回调函数
   */
  void ProcessTaskByTorrent(libtorrent::torrent_handle torrent,
                            boost::function<void(boost::shared_ptr<Task>)> cb);

  /**
   * @brief 启动时将持久化存储的任务恢复到任务管理器中
   */
  void AddResumeTask();

  /* 以下为处理各种用户请求消息的函数 */
  /**
   * @brief 处理添加任务的message
   * @param fd(in)
   * @param message(in)  message会被修改infohash,new_name字段
   * @param ae(in)
   */
  void AddTask(const boost::shared_ptr<UnixSocketConnection> &connection,
               message::AddTask &message,
               boost::system::error_code &ec);

  /**
   * @brief 处理设置任务参数的消息
   */
  void TaskSetopt(const boost::shared_ptr<UnixSocketConnection> &connection,
                  const message::TaskOptions &options,
                  boost::system::error_code &ec);

  /**
   * @brief 从task.db中获得agent的配置
   * */
  bool GetAgentOptionFromDb(message::AgentOptions *options);

  /**
   * @brief 处理获取任务参数的消息
   */
  void TaskGetopt(const boost::shared_ptr<UnixSocketConnection> &connection,
                  message::TaskOptions *options,
                  boost::system::error_code &ec);

  /**
   * @brief 处理设置agent参数的消息
   */
  void AgentSetopt(const boost::shared_ptr<UnixSocketConnection> &connection,
                   const message::AgentOptions &options,
                   boost::system::error_code &ec);

  /**
   * @brief 处理获取agent参数的消息
   */
  void AgentGetopt(const boost::shared_ptr<UnixSocketConnection> &connection,
                   message::AgentOptions *options);

  // 处理获取某个任务状态的消息
  void ListTask(int64_t taskid, const tasks_map_t::iterator &it, message::BatchListRes &response);

  /**
   * @brief 处理取得本机当前所有任务状态的消息
   */
  void ListTasks(const boost::shared_ptr<UnixSocketConnection> &connection,
                 const message::BatchCtrl &message);

  //处理单个任务的控制消息
  void CtrlTask(int64_t taskid,
                const tasks_map_t::iterator &it,
                const struct ucred &cred,
                message::BatchCtrl::ctrl_t type,
                message::BatchCtrlRes &response,
                bool skip = false);

  /**
   * @brief 处理批处理消息
   */
  void BatchCtrlTasks(const boost::shared_ptr<UnixSocketConnection> &connection,
                      const message::BatchCtrl &message);

  /**
   * @brief 处理添加metadata的消息
   */
  void AddMetadata(const message::Metadata &message);

  /*  以下为处理相关alert消息的回调函数  */
  void OnTorrentFinished(const libtorrent::torrent_finished_alert *alert);
  void OnListenFailed(const libtorrent::listen_failed_alert *alert);
  void OnTorrentError(const libtorrent::torrent_error_alert *alert);
  void OnPeerDisconnect(const libtorrent::peer_disconnected_alert *alert);

  // libtorrent中的session对象，管理所有任务
  boost::scoped_ptr<libtorrent::session> session_;
  // endpoint for session listen
  libtorrent::tcp::endpoint listen_endpoint_;
  // taskid => task
  tasks_map_t tasks_map_;
  // torrent_handle => task
  torrent_map_t torrent_map_;
  // infohash => task;
  infohash_map_t infohash_map_;
  // data_path => task
  data_path_map_t data_path_map_;

  // 锁map的
  boost::mutex tasks_lock_;

  WorkerPool worker_pool_;
  ThriftTracker thrift_tracker_;

  // control server
  UnixSocketServerWithThread control_server_;

  // 任务数据库
  boost::scoped_ptr<TaskDB> task_db_;  //用指针，否则AgentConf对象还未加载

  metadata_list_t metas_;
  int current_metas_total_size_;
  boost::mutex metas_lock_;
  PeerStatFile peer_stat_;
};

} // namespace agent
} // namespace bbts
#endif // OP_OPED_NOAH_TOOLS_BBTS_AGENT_TASK_MANAGER_H_

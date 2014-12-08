/**
 * @file   TaskDB.h
 *
 * @author liuming03
 * @date   2013-5-6
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_AGENT_TASKDB_H_
#define OP_OPED_NOAH_TOOLS_BBTS_AGENT_TASKDB_H_

#include <string>
#include <boost/asio/deadline_timer.hpp>
#include <boost/noncopyable.hpp>
#include <boost/system/error_code.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>

struct sqlite3;

namespace bbts {
namespace agent {

/**
 * @brief 任务数据库类，管理任务
 */
class TaskDB : private boost::noncopyable {
 public:
  /**
   * @brief 回调函数类型
   */
  typedef int (*callback_t)(void *data, int argc, char **argv, char **col_name);

  /**
   * @brief 构造函数
   *
   * @param db_file 数据库文件名
   */
  TaskDB(const std::string &db_file_name, boost::asio::io_service &io_service, int delete_data_interval);

  virtual ~TaskDB();

  /**
   * @brief 重连数据库
   *
   * @return 成功返回0，其他返回非0
   */
  bool Reconnect();

  /**
   * @brief 执行sql语句
   *
   * @param sql SQL语句
   * @param callback 回调函数
   * @param userdata 自定义数据，回调函数中使用
   * @return 成功返回0， 失败返回其他
   */
  bool Excute(const std::string &sql, callback_t callback = NULL, void *userdata = NULL);

 private:
  /**
   * @brief 如果数据库中不存在相关的任务表，则创建
   *
   * @return 成功返回0， 失败返回非0
   */
  bool InitTable();
  void DeleteDataTimerCallback(const boost::system::error_code &ec);

  std::string db_file_name_;
  int delete_data_interval_;

  boost::shared_ptr<sqlite3> task_db_;
  boost::mutex task_db_mutex_;

  bool reconnecting_;
  boost::mutex reconnecting_mutex_;
  boost::condition_variable reconnecting_cond_;

  boost::asio::deadline_timer del_data_timer_;
  // 单位：小时
  static int DEL_DATA_TIMER_INTERVAL;
};

} // namespace agent
} // namespace bbts
#endif // OP_OPED_NOAH_TOOLS_BBTS_AGENT_TASKDB_H_

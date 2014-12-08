/**
 * @file   Task.h
 *
 * @author liuming03
 * @date   2013-1-16
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_AGENT_TASK_H_
#define OP_OPED_NOAH_TOOLS_BBTS_AGENT_TASK_H_

#include <boost/asio/deadline_timer.hpp>
#include <boost/noncopyable.hpp>
#include <boost/smart_ptr.hpp>
#include <libtorrent/torrent_handle.hpp>

#include "bbts/message.pb.h"
#include "bbts/statistics.h"

namespace bbts {
namespace agent {

/**
 * @brief 任务类
 * 每个任务对应一个torrent_handle句柄
 */
class Task : public boost::enable_shared_from_this<Task>, private boost::noncopyable {
 public:
  virtual ~Task();

  inline int64_t get_id() const {
    return id_;
  }

  inline std::string get_infohash() const {
    return infohash_;
  }

  inline std::string get_data_path() const {
    return save_path_ + '/' + new_name_;
  }

  inline libtorrent::torrent_handle get_torrent() const {
    return torrent_;
  }

  inline message::TaskType get_type() const {
    return type_;
  }

  inline void set_error(const std::string &error) {
    error_ = error;
  }

  /**
   * @brief  创建一个未初始化的任务
   *
   * @param  taskid(in) 任务id
   * @return 成功返回任务对象，失败返回空
   */
  static boost::shared_ptr<Task> Create(int64_t taskid);

  /**
   * @brief 创建一个任务
   *
   * @param  message(in) 任务参数
   * @param  ti(in)      种子信息
   * @param  ae(out)     出现错误则该值非空
   * @return 成功返回任务对象，失败返回NULL
   */
  static boost::shared_ptr<Task> Create(const message::AddTask &message,
                                        const boost::intrusive_ptr<libtorrent::torrent_info> &ti,
                                        boost::system::error_code &ec);

  /**
   * @brief 根据指定的文件恢复上次未完成的任务
   *
   * @param  resume_file(in)  任务持久化文件绝对路径
   * @param  file_size(in)    文件长度
   * @return 成功返回任务对象，失败返回NULL
   */
  static boost::shared_ptr<Task> CreateFromResumeFile(const std::string &resume_file, int64_t file_size);

  /**
   * @brief 校验用户是否有操作本任务的权限
   *
   * @param  cred(in)  cmd传来的用户id信息
   * @return 校验成功返回true，失败返回false
   */
  bool CheckCred(const struct ucred &cred) const;

  /**
   * @brief 将本任务暂停
   */
  void Pause() const;

  /**
   * @brief 将本任务恢复执行
   */
  void Resume() const;

  /**
   * @brief 标记本任务被取消；不表示任务立即被删除
   */
  void Cancel();

  /**
   * @brief 设置任务的相关参数，如上传下载限速等
   *
   * @param opt_msg(in) 要设置的选项
   */
  void SetOptions(const message::TaskOptions &task_opt) const;

  /**
   * @brief 获取本任务的相关参数，上传下载限速等
   *
   * @param opt_msg(out) 获取后的参数放在此传出参数中
   */
  void GetOptions(message::TaskOptions *task_opt) const;

  /**
   * @brief 获取本任务的状态信息
   *
   * 如果是正在运行的任务，则直接查询任务状态；如果是已完成的任务，则通过task.db查询任务的状态
   * @param task_msg(out) 保存获取的任务状态信息
   */
  void GetStatus(message::TaskStatus *task_status) const;

  /**
   * @brief 任务进入做种
   */
  void ToSeeding();

  /**
   * @brief 标记任务需持久化到磁盘
   *
   * agent退出时，持久化存储一个任务
   */
  void Persist();

  /**
   * @brief 取消关联的torrent_
   */
  void RemoveTorrent();

 private:
  /**
   * @brief 创建任务的构造函数
   *
   * @param id(in) 任务id，用于向task.db中获取已完成任务的信息等临时任务实例；
   */
  explicit Task(int64_t id);

  /**
   * @brief 更新task.db中本任务的状态信息
   *
   * @param is_finish(in) 任务是否完成
   */
  void UpdateStatus(bool is_finish = false) const;

  /**
   * @brief 根据任务状态获取状态码
   *
   * @param ts(in)        任务状态
   * @param is_finish(in) 任务是否完成
   */
  message::TaskStatus::status_t GetStatusCode(const libtorrent::torrent_status &ts, bool is_finish = false) const;

  /**
   * @brief 删除持久化文件
   */
  void DeleteResumeFile() const;

  /**
   * @brief 将本任务持久化到本地文件中去
   */
  void GenerateResumeFile() const;

  /**
   * @brief 从task.db获取一个新的任务自增id
   */
  bool GenTaskId();

  enum {
    TASK_FLAG_NOT_NEW = 0x00000001,        // 是否是新new的任务，此时还没有关联到一个torrent_hanle
    TASK_FLAG_CANCELED = 0x00000002,           // 任务是否被取消
    TASK_FLAG_PERSIST = 0x00000004,
  };
  volatile int32_t flags_;                       // 状态标记

  int64_t id_;                                   // 任务编号
  std::string infohash_;
  std::string cmd_;                              // 添加进的任务命令行
  struct ucred cred_;                            // 创建该任务的用户、组信息
  std::string save_path_;                        // 存储路径
  std::string new_name_;
  message::TaskType type_;                       // 任务类型
  std::vector<std::pair<std::string, int> > trackers_;
  int seeding_time_;                   // tool指定的是否作为种子任务，如作为种子任务，任务下载完成将不会自动退出。
  boost::asio::deadline_timer seeding_timer_;    // 做种定时器
  libtorrent::torrent_handle torrent_;
  std::string error_;                            // 错误信息
  task_statistics_t stat_;
};

} // namespace agent
} // namespace bbts
#endif /* OP_OPED_NOAH_TOOLS_BBTS_AGENT_TASK_H */

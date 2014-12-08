/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file WorkerPool.h
 *
 * @author liuming03
 * @date 2013-7-10
 * @brief 
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_WORKER_POOL_H_
#define OP_OPED_NOAH_TOOLS_BBTS_WORKER_POOL_H_

#include <string>

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>

namespace bbts {

/**
 * @brief
 */
class WorkerPool : private boost::noncopyable {
 public:
  explicit WorkerPool(const std::string &tag);
  virtual ~WorkerPool();
  void Start(int thread_num);
  void JoinAll();

  inline boost::asio::io_service& get_io_service() {
    return io_service_;
  }

  inline std::string& get_tag() {
    return tag_;
  }

  inline void Stop() {
    empty_work_.reset();
    io_service_.stop();
  }

 private:
  void ThreadMain(int worker_id);

  boost::asio::io_service io_service_;
  boost::scoped_ptr<boost::asio::io_service::work> empty_work_;
  boost::thread_group threads_;
  int thread_num_;
  std::string tag_;
};

} // namespace bbts
#endif // OP_OPED_NOAH_TOOLS_BBTS_WORKER_POOL_H_

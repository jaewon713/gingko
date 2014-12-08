/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   WorkerPool.cpp
 *
 * @author liuming03
 * @date   2013-7-10
 * @brief 
 */

#include "bbts/WorkerPool.h"

#include <string>

#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "bbts/log.h"

namespace bbts {

WorkerPool::WorkerPool(const std::string &tag) : thread_num_(0), tag_(tag) {
}

WorkerPool::~WorkerPool() {
}

void WorkerPool::ThreadMain(int worker_id) {
  OPEN_LOG_R();
  NOTICE_LOG("[%s][%d] run begin.", tag_.c_str(), worker_id);
  boost::system::error_code ec;
  io_service_.run(ec);
  if (ec) {
    WARNING_LOG("[%s]thread pool run fail: %s", tag_.c_str(), ec.message().c_str());
  }
  NOTICE_LOG("worker thread[%d] run end.", worker_id);
  CLOSE_LOG_R();
}

void WorkerPool::Start(int thread_num) {
  empty_work_.reset(new boost::asio::io_service::work(io_service_));
  thread_num_ = thread_num;
  io_service_.reset();
  for (int i = 0; i < thread_num_; ++i) {
    threads_.create_thread(boost::bind(&WorkerPool::ThreadMain, this, i));
  }
}

void WorkerPool::JoinAll() {
  threads_.join_all();
}

} // namespace bbts

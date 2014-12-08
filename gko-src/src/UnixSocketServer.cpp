/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   UnixSocketServer.cpp
 *
 * @author liuming03
 * @date   2014-1-11
 * @brief 
 */

#include "bbts/UnixSocketServer.h"

#include <boost/bind.hpp>
#include <boost/system/error_code.hpp>

#include "bbts/log.h"

namespace bbts {

using std::string;

using boost::asio::io_service;
using boost::system::error_code;
using boost::shared_ptr;

static void EmptyCallback() {}

UnixSocketServer::UnixSocketServer(io_service &io_service) :
    io_service_(io_service),
    acceptor_(io_service_),
    heartbeat_recv_cycle_(0),
    heartbeat_send_cycle_(0),
    accept_callback_(boost::bind(&EmptyCallback)),
    read_callback_(boost::bind(&EmptyCallback)),
    write_callback_(boost::bind(&EmptyCallback)),
    close_callback_(boost::bind(&EmptyCallback)) {}

UnixSocketServer::~UnixSocketServer() {
  Close();
}

void UnixSocketServer::Close() {
  if (acceptor_.is_open()) {
    unlink(endpoint_.path().c_str());
  }
  error_code ec;
  acceptor_.close(ec);
}

void UnixSocketServer::HandleAccepted(shared_ptr<UnixSocketConnection> connection,
                                    const error_code& ec) {
  if (ec) {
    if (ec == boost::asio::error::make_error_code(boost::asio::error::operation_aborted)) {
      DEBUG_LOG("server(%s) accept canceled.", endpoint_.path().c_str());
      return;
    } else {
      WARNING_LOG("server(%s) accept failed: %s", endpoint_.path().c_str(), ec.message().c_str());
    }
    if (!acceptor_.is_open()) {
      return;
    }
  } else {
    accept_callback_(connection);
    connection->Start();
  }
  AsyncAccept();
}

void UnixSocketServer::AsyncAccept() {
  shared_ptr<UnixSocketConnection> connection = UnixSocketConnection::Create(
      io_service_, read_callback_, write_callback_, close_callback_,
      heartbeat_recv_cycle_, heartbeat_send_cycle_);
  acceptor_.async_accept(connection->get_socket(), connection->get_remote_endpoint(),
                         boost::bind(&UnixSocketServer::HandleAccepted, this, connection, _1));
}

bool UnixSocketServer::CanConnect() {
  error_code ec;
  Socket sock(io_service_);
  sock.connect(endpoint_, ec);
  if (ec) {
    unlink(endpoint_.path().c_str());
    return false;
  }
  return true;
}

bool UnixSocketServer::Serve(mode_t mode) {
  if (CanConnect()) {
    WARNING_LOG("bind address(%s) can connect, can't start serve.", endpoint_.path().c_str());
    return false;
  }

  error_code ec;
  acceptor_.open(Acceptor::protocol_type(), ec);
  if (ec) {
    WARNING_LOG("acceptor open failed: %s", ec.message().c_str());
    return false;
  }

  acceptor_.bind(endpoint_, ec);
  if (ec) {
    WARNING_LOG("bind path(%s) failed: %s", endpoint_.path().c_str(), ec.message().c_str());
    return false;
  }
  chmod(endpoint_.path().c_str(), mode);

  Acceptor::non_blocking_io non_block(true);
  acceptor_.io_control(non_block, ec);
  if (ec) {
    WARNING_LOG("set socket(%s) non blocking io failed.", endpoint_.path().c_str());
    return false;
  }
  acceptor_.listen(128, ec);
  if (ec) {
    WARNING_LOG("listen socket(%s) failed: %s", endpoint_.path().c_str(), ec.message().c_str());
    return false;
  }

  AsyncAccept();
  return true;
}

bool UnixSocketServerWithThread::Start(mode_t mode) {
  if (!server_.Serve(mode)) {
    return false;
  }
  boost::thread tmp_thread(boost::bind(&UnixSocketServerWithThread::Run, this));
  thread_.swap(tmp_thread);
  return true;
}

void UnixSocketServerWithThread::Join() {
  thread_.join();
}

void UnixSocketServerWithThread::Run() {
  OPEN_LOG_R();
  DEBUG_LOG("Unix socket server start success.");
  io_service_.reset();
  io_service_.run();
  DEBUG_LOG("Unix socket server stoped success.");
  CLOSE_LOG_R();
}

} // namespace bbts

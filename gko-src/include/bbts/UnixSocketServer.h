/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file UnixSocketServer.h
 *
 * @author liuming03
 * @date 2014-1-11
 * @brief 
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_UNIX_SOCKET_SERVER_H_
#define OP_OPED_NOAH_TOOLS_BBTS_UNIX_SOCKET_SERVER_H_

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include "UnixSocketConnection.h"

namespace boost {
namespace system {
class error_code;
}
}

namespace bbts {

/**
 * @brief
 */
class UnixSocketServer : private boost::noncopyable {
 public:
  typedef boost::asio::local::stream_protocol::socket Socket;
  typedef boost::asio::local::stream_protocol::acceptor Acceptor;
  typedef boost::function<void(const boost::shared_ptr<UnixSocketConnection> &)> AcceptedCallback;

  UnixSocketServer(boost::asio::io_service &io_service);

  virtual ~UnixSocketServer();

  bool Serve(mode_t mode);
  void Close();

  boost::asio::io_service& get_io_service() {
    return io_service_;
  }

  void set_endpoint(const UnixSocketConnection::EndPoint &endpoint) {
    endpoint_ = endpoint;
  }

  void set_accept_callback(AcceptedCallback accept_callback) {
    accept_callback_ = accept_callback;
  }

  void set_read_callback(UnixSocketConnection::RWCallback read_callback) {
    read_callback_ = read_callback;
  }

  void set_write_callback(UnixSocketConnection::RWCallback write_callback) {
    write_callback_ = write_callback;
  }

  void set_close_callback(UnixSocketConnection::CloseCallback close_callback) {
    close_callback_ = close_callback;
  }

  void set_heartbeat_recv_cycle(int cycle) {
    heartbeat_recv_cycle_ = cycle;
  }

  void set_heartbeat_send_cycle(int cycle) {
    heartbeat_send_cycle_ = cycle;
  }

 private:
  void AsyncAccept();

  bool CanConnect();

  void HandleAccepted(boost::shared_ptr<UnixSocketConnection> conn,
                      const boost::system::error_code& ec);

  UnixSocketConnection::EndPoint endpoint_;
  boost::asio::io_service &io_service_;
  Acceptor acceptor_;
  int heartbeat_recv_cycle_;
  int heartbeat_send_cycle_;
  AcceptedCallback accept_callback_;
  UnixSocketConnection::RWCallback read_callback_;
  UnixSocketConnection::RWCallback write_callback_;
  UnixSocketConnection::CloseCallback close_callback_;
};

class UnixSocketServerWithThread : private boost::noncopyable {
public:
  UnixSocketServerWithThread() : server_(io_service_) {}

  virtual ~UnixSocketServerWithThread() {}

  bool Start(mode_t mode);

  void Join();

  void Stop() {
    server_.Close();
    io_service_.stop();
  }

  boost::asio::io_service& get_io_service() {
    return io_service_;
  }

  void set_endpoint(const UnixSocketConnection::EndPoint &endpoint) {
    server_.set_endpoint(endpoint);
  }

  void set_accept_callback(UnixSocketServer::AcceptedCallback accept_callback) {
    server_.set_accept_callback(accept_callback);
  }

  void set_read_callback(UnixSocketConnection::RWCallback read_callback) {
    server_.set_read_callback(read_callback);
  }

  void set_write_callback(UnixSocketConnection::RWCallback write_callback) {
    server_.set_write_callback(write_callback);
  }

  void set_close_callback(UnixSocketConnection::CloseCallback close_callback) {
    server_.set_close_callback(close_callback);
  }

  void set_heartbeat_recv_cycle(int cycle) {
    server_.set_heartbeat_recv_cycle(cycle);
  }

  void set_heartbeat_send_cycle(int cycle) {
    server_.set_heartbeat_send_cycle(cycle);
  }

private:
  void Run();

  boost::asio::io_service io_service_;
  UnixSocketServer server_;
  boost::thread thread_;
};

} // namespace bbts
#endif // OP_OPED_NOAH_TOOLS_BBTS_UNIX_SOCKET_SERVER_H_

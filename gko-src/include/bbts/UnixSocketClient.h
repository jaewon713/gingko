/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   UnixSocketClient.h
 *
 * @author liuming03
 * @date   2014-1-22
 * @brief 
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_UNIX_SOCKET_CLIENT_H_
#define OP_OPED_NOAH_TOOLS_BBTS_UNIX_SOCKET_CLIENT_H_

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include "bbts/UnixSocketConnection.h"

namespace boost {
namespace asio {
class io_service;
}
}

namespace bbts {

/**
 * @brief
 */
class UnixSocketClient : private boost::noncopyable {
 public:
  typedef boost::function<void (const boost::shared_ptr<UnixSocketConnection> &)> ConnectedCallback;

  UnixSocketClient(boost::asio::io_service &ios);
  virtual ~UnixSocketClient();

  bool Start(UnixSocketConnection::EndPoint endpoint);

  void AsynConnect(UnixSocketConnection::EndPoint endpoint);

  inline void set_connect_callback(ConnectedCallback connected_callback) {
    connected_callback_ = connected_callback;
  }

  inline void set_read_callback(UnixSocketConnection::RWCallback read_callback) {
    read_callback_ = read_callback;
  }

  inline void set_write_callback(UnixSocketConnection::RWCallback write_callback) {
    write_callback_ = write_callback;
  }

  inline void set_close_callback(UnixSocketConnection::CloseCallback close_callback) {
    close_callback_ = close_callback;
  }

  inline void set_heartbeat_recv_cycle(int cycle) {
    heartbeat_recv_cycle_ = cycle;
  }

  inline void set_heartbeat_send_cycle(int cycle) {
    heartbeat_send_cycle_ = cycle;
  }

 private:
  void HandleConnected(boost::shared_ptr<UnixSocketConnection> connection,
                       const boost::system::error_code& ec);

  boost::asio::io_service &io_service_;
  int heartbeat_recv_cycle_;
  int heartbeat_send_cycle_;
  ConnectedCallback connected_callback_;
  UnixSocketConnection::RWCallback read_callback_;
  UnixSocketConnection::RWCallback write_callback_;
  UnixSocketConnection::CloseCallback close_callback_;
};

class SyncUnixSocketClient : public boost::noncopyable {
public:
 SyncUnixSocketClient(boost::asio::io_service &ios);
 virtual ~SyncUnixSocketClient();
 bool Connect(const UnixSocketConnection::EndPoint &endpoint);
 bool WriteData(const boost::shared_ptr<const std::vector<char> > &data);
 bool ReadData(boost::shared_ptr<std::vector<char> > *data);
 void Close();

 UnixSocketConnection::Socket& get_socket() {
   return socket_;
 }

private:
 boost::asio::io_service &io_service_;
 UnixSocketConnection::Socket socket_;
};

} // namespace bbts
#endif // OP_OPED_NOAH_TOOLS_BBTS_UNIX_SOCKET_CLIENT_H_

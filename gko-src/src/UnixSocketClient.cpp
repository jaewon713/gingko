/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   UnixSocketClient.cpp
 *
 * @author liuming03
 * @date   2014-1-22
 * @brief 
 */

#include "bbts/UnixSocketClient.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "bbts/log.h"

namespace bbts {

using std::vector;
using boost::system::error_code;
using boost::array;
using boost::shared_ptr;

static void EmptyCallback() {}

UnixSocketClient::UnixSocketClient(boost::asio::io_service &io_service) :
    io_service_(io_service),
    heartbeat_recv_cycle_(0),
    heartbeat_send_cycle_(0),
    connected_callback_(boost::bind(&EmptyCallback)),
    read_callback_(boost::bind(&EmptyCallback)),
    write_callback_(boost::bind(&EmptyCallback)),
    close_callback_(boost::bind(&EmptyCallback)) {}

UnixSocketClient::~UnixSocketClient() {}

bool UnixSocketClient::Start(UnixSocketConnection::EndPoint remote_endpoint) {
  shared_ptr<UnixSocketConnection> connection = UnixSocketConnection::Create(
      io_service_, read_callback_, write_callback_, close_callback_,
      heartbeat_recv_cycle_, heartbeat_send_cycle_);
  error_code ec;
  UnixSocketConnection::Socket &sock = connection->get_socket();
  sock.open(UnixSocketConnection::Socket::protocol_type(), ec);
  if (ec) {
    WARNING_LOG("open socket failed: %s", ec.message().c_str());
    return false;
  }
  UnixSocketConnection::Socket::non_blocking_io non_block(true);
  sock.io_control(non_block, ec);
  if (ec) {
    WARNING_LOG("set unix socket non blocking io fail.");
    return false;
  }
  UnixSocketConnection::EndPoint &rep = connection->get_remote_endpoint();
  rep = remote_endpoint;
  sock.async_connect(rep, boost::bind(&UnixSocketClient::HandleConnected, this, connection, _1));

  return true;
}

void UnixSocketClient::HandleConnected(shared_ptr<UnixSocketConnection> connection,
                                       const error_code& ec) {
  if (ec) {
    UnixSocketConnection::EndPoint &rep = connection->get_remote_endpoint();
    if (ec == boost::asio::error::make_error_code(boost::asio::error::operation_aborted)) {
      DEBUG_LOG("connect server(%s) canceled.", rep.path().c_str());
    } else {
      WARNING_LOG("connect server(%s) failed: %s", rep.path().c_str(), ec.message().c_str());
    }
    WARNING_LOG("connect failed: %s", ec.message().c_str());
    return;
  }
  connected_callback_(connection);
  connection->Start();
}

SyncUnixSocketClient::SyncUnixSocketClient(boost::asio::io_service &io_service) :
    io_service_(io_service), socket_(io_service) {}

SyncUnixSocketClient::~SyncUnixSocketClient() {
  Close();
}

bool SyncUnixSocketClient::Connect(const UnixSocketConnection::EndPoint &endpoint) {
  error_code ec;
  socket_.connect(endpoint, ec);
  if (ec) {
    DEBUG_LOG("connect to server(%s) failed: %s", endpoint.path().c_str(), ec.message().c_str());
  }
  return !ec;
}

bool SyncUnixSocketClient::WriteData(const shared_ptr<const vector<char> > &data) {
  if (!data) {
    return true;
  }
  Header header(UnixSocketConnection::USERDATA, data->size());
  array<boost::asio::const_buffer, 2> buffers = {
      boost::asio::buffer(&header, sizeof(header)),
      boost::asio::buffer(*data)
  };
  error_code ec;
  boost::asio::write(socket_, buffers, ec);
  if (ec) {
    DEBUG_LOG("write data to server failed: %s", ec.message().c_str());
  }
  return !ec;
}

bool SyncUnixSocketClient::ReadData(shared_ptr<vector<char> > *data) {
  assert(data);
  error_code ec;
  Header header;
  boost::asio::read(socket_, boost::asio::buffer(&header, sizeof(header)), ec);
  if (ec) {
    DEBUG_LOG("read data from server failed: %s", ec.message().c_str());
    return false;
  }
  if (!header.IsValid()) {
    DEBUG_LOG("valid header failed!");
    return false;
  }
  data->reset(new vector<char>(header.length()));
  boost::asio::read(socket_, boost::asio::buffer(*data->get()), ec);
  return !ec;
}

void SyncUnixSocketClient::Close() {
  error_code ec;
  socket_.close(ec);
}

} // namespace bbts

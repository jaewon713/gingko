/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   UnixSocketConnection.cpp
 *
 * @author liuming03
 * @date   2014-1-11
 * @brief 
 */

#include "bbts/UnixSocketConnection.h"

#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/system/error_code.hpp>

#include "bbts/log.h"

namespace bbts {

using std::vector;

using boost::posix_time::seconds;
using boost::system::error_code;
using boost::array;
using boost::shared_ptr;

namespace error = boost::asio::error;

UnixSocketConnection::UnixSocketConnection(boost::asio::io_service &io_service,
                                           RWCallback read_callback,
                                           RWCallback write_callback,
                                           CloseCallback close_callback,
                                           int heartbeat_recv_cycle,
                                           int heartbeat_send_cycle) :
    io_service_(io_service),
    socket_(io_service_),
    read_callback_(read_callback),
    write_callback_(write_callback),
    close_callback_(close_callback),
    heartbeat_recv_cycle_(heartbeat_recv_cycle),
    heartbeat_send_cycle_(heartbeat_send_cycle),
    heartbeat_recv_timer_(io_service_),
    heartbead_send_timer_(io_service_) {}

UnixSocketConnection::~UnixSocketConnection() {
  Close();
  DEBUG_LOG("connection release with remote(%s)", remote_endpoint_.path().c_str());
  close_callback_(remote_endpoint_);
}

void UnixSocketConnection::HandleWriteMessage(const error_code &ec, size_t bytes_transferred) {
  if (ec) {
    if (ec == error::make_error_code(error::operation_aborted)) {
      //CDEBUG_LOG("write data to remote(%s) canceled.", remote_endpoint_.path().c_str());
    } else {
      WARNING_LOG("write data to remote(%s) fail: %s",
                   remote_endpoint_.path().c_str(), ec.message().c_str());
    }
    Close();
    return;
  }
  if (write_message_queue_.front().second) {
    write_callback_(shared_from_this(), write_message_queue_.front().second);
  }
  write_message_queue_.pop();
  if (!write_message_queue_.empty()) {
    WriteMessage(write_message_queue_.front());
  }
}

void UnixSocketConnection::WriteMessage(const Message &message) {
  if (message.second) {
    write_header_.Assign(message.first, message.second->size());
    array<boost::asio::const_buffer, 2> buffers = {
        boost::asio::buffer(&write_header_, sizeof(write_header_)),
        boost::asio::buffer(*message.second)
    };
    boost::asio::async_write(
        socket_,
        buffers,
        boost::bind(&UnixSocketConnection::HandleWriteMessage, shared_from_this(), _1, _2));
    return;
  }

  write_header_.Assign(message.first, 0);
  boost::asio::async_write(
      socket_,
      boost::asio::buffer(&write_header_, sizeof(write_header_)),
      boost::bind(&UnixSocketConnection::HandleWriteMessage, shared_from_this(), _1, _2));
}

void UnixSocketConnection::AsyncWrite(MessageTypeEnum type, shared_ptr<const vector<char> > data) {
  bool is_writting = !write_message_queue_.empty();
  write_message_queue_.push(std::make_pair(type, data));
  if (!is_writting) {
    WriteMessage(write_message_queue_.front());
  }
}

void UnixSocketConnection::Write(const shared_ptr<const vector<char> > &data) {
  if (data) {
    io_service_.post(
        boost::bind(&UnixSocketConnection::AsyncWrite, shared_from_this(), USERDATA, data));
  }
}

void UnixSocketConnection::HandleReadData(shared_ptr<vector<char> > data,
                                          const error_code &ec, size_t bytes_readed) {
  if (ec) {
    if (ec == error::make_error_code(error::eof)) {
      DEBUG_LOG("connection close by remote(%s): %s", remote_endpoint_.path().c_str());
    }
    if (ec == error::make_error_code(error::operation_aborted)) {
      //CDEBUG_LOG("read data from remote(%s) canceled.", remote_endpoint_.path().c_str());
    } else {
      WARNING_LOG("read data from remote(%s) failed: %s",
                   remote_endpoint_.path().c_str(), ec.message().c_str());
    }
    Close();
    return;
  }
  if (data->size() != bytes_readed) {
    WARNING_LOG("read data from remote(%s) fail, length too short.",
                 remote_endpoint_.path().c_str());
    Close();
    return;
  }

  read_callback_(shared_from_this(), data);
  boost::asio::async_read(
      socket_, boost::asio::buffer(&read_header_, sizeof(read_header_)),
      boost::bind(&UnixSocketConnection::HandleReadHead, shared_from_this(), _1, _2));
}

void UnixSocketConnection::HandleReadHead(const error_code &ec, size_t bytes_transferred) {
  if (ec) {
    if (ec == error::make_error_code(error::eof)) {
      DEBUG_LOG("connection close by remote(%s)", remote_endpoint_.path().c_str());
    } else if (ec == error::make_error_code(error::operation_aborted)) {
      //CDEBUG_LOG("read header from remote(%s) canceled.", remote_endpoint_.path().c_str());
    } else {
      WARNING_LOG("read header from remote(%s) failed: %s",
                   remote_endpoint_.path().c_str(), ec.message().c_str());
    }
    Close();
    return;
  }
  if (!read_header_.IsValid()) {
    WARNING_LOG("read header from remote(%s) vaild fail",
                 remote_endpoint_.path().c_str(), ec.message().c_str());
    Close();
    return;
  }

  switch (read_header_.type()) {
    case HEARTBEAT:
      UpdateHeartbeatRecvTimer();
      boost::asio::async_read(
          socket_, boost::asio::buffer(&read_header_, sizeof(read_header_)),
          boost::bind(&UnixSocketConnection::HandleReadHead, shared_from_this(), _1, _2));
      break;

    case USERDATA: {
      shared_ptr<vector<char> > data(new vector<char>(read_header_.length()));
      boost::asio::async_read(
          socket_, boost::asio::buffer(*data),
          boost::bind(&UnixSocketConnection::HandleReadData, shared_from_this(), data, _1, _2));
      break;
    }

    default:
      WARNING_LOG("head type(%u) not support.", read_header_.type());
      Close();
      break;
  }
}

void UnixSocketConnection::HandleHeartbeatRecv(const error_code &ec) {
  if (ec) {
    if (ec == error::make_error_code(error::operation_aborted)) {
      //CDEBUG_LOG("remote(%s) heartbeat recv timer canceled.", remote_endpoint_.path().c_str());
    } else {
      WARNING_LOG("remote(%s) heartbeat recv timer failed: %s",
                   remote_endpoint_.path().c_str(), ec.message().c_str());
    }
    return;
  }
  Close();
}

void UnixSocketConnection::HandleHeartbeatSend(const error_code &ec) {
  if (ec) {
    if (ec == error::make_error_code(error::operation_aborted)) {
      //CDEBUG_LOG("remote(%s) heartbeat send timer canceled.", remote_endpoint_.path().c_str());
    } else {
      WARNING_LOG("remote(%s) heartbeat send timer failed: %s",
                   remote_endpoint_.path().c_str(), ec.message().c_str());
    }
    return;
  }
  AsyncWrite(HEARTBEAT, shared_ptr<const vector<char> >());
  UpdateHeartbeatSendTimer();
}

void UnixSocketConnection::UpdateHeartbeatRecvTimer() {
  if (heartbeat_recv_cycle_ > 0) {
    error_code ec;
    heartbeat_recv_timer_.cancel(ec);
    heartbeat_recv_timer_.expires_from_now(seconds(heartbeat_recv_cycle_), ec);
    heartbeat_recv_timer_.async_wait(
        boost::bind(&UnixSocketConnection::HandleHeartbeatRecv, shared_from_this(), _1));
  }
}

void UnixSocketConnection::UpdateHeartbeatSendTimer() {
  if (heartbeat_send_cycle_ > 0) {
    error_code ec;
    heartbead_send_timer_.expires_from_now(seconds(heartbeat_send_cycle_), ec);
    heartbead_send_timer_.async_wait(
        boost::bind(&UnixSocketConnection::HandleHeartbeatSend, shared_from_this(), _1));
  }
}

void UnixSocketConnection::Start() {
  boost::asio::async_read(
      socket_, boost::asio::buffer(&read_header_, sizeof(read_header_)),
      boost::bind(&UnixSocketConnection::HandleReadHead, shared_from_this(), _1, _2));
  UpdateHeartbeatRecvTimer();
  UpdateHeartbeatSendTimer();
}

} // namespace bbts

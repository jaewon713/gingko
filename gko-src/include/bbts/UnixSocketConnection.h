/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file UnixSocketConnection.h
 *
 * @author liuming03
 * @date 2014-1-11
 * @brief 
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_UNIX_SOCKET_CONNECTION_H_
#define OP_OPED_NOAH_TOOLS_BBTS_UNIX_SOCKET_CONNECTION_H_

#include <queue>
#include <utility> // for std::pair
#include <vector>

#include <boost/any.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/smart_ptr.hpp>

namespace bbts {

class Header {
 public:
  Header() : length_(0), type_(0), magic_(0), checksum_(0) {}

  Header(uint32_t type, uint32_t length) {
    Assign(type, length);
  }

  inline bool IsValid() {
    return (length_ ^ type_ ^ magic_) == checksum_;
  }

  inline void Assign(uint32_t type, uint32_t length) {
    length_ = length;
    type_ = type;
    magic_ = 47417;
    checksum_ = length ^ type ^ magic_;
  }

  inline uint32_t type() {
    return type_;
  }

  inline uint32_t length() {
    return length_;
  }

private:
  uint32_t length_;
  uint32_t type_;
  uint32_t magic_;
  uint32_t checksum_;
};

/**
 * @brief
 */
class UnixSocketConnection : public boost::enable_shared_from_this<UnixSocketConnection>,
                             private boost::noncopyable {
 public:
  enum MessageTypeEnum {
    HEARTBEAT,
    USERDATA,
  };

  typedef boost::asio::local::stream_protocol::socket Socket;
  typedef boost::asio::local::stream_protocol::endpoint EndPoint;
  typedef std::pair<MessageTypeEnum, boost::shared_ptr<const std::vector<char> > > Message;
  typedef boost::function<void(const EndPoint &)> CloseCallback;
  typedef boost::function<void(const boost::shared_ptr<UnixSocketConnection> &,
                               const boost::shared_ptr<const std::vector<char> > &)> RWCallback;

  virtual ~UnixSocketConnection();

  static boost::shared_ptr<UnixSocketConnection> Create(boost::asio::io_service &io_service,
                                                        RWCallback read_callback,
                                                        RWCallback write_callback,
                                                        CloseCallback close_callback,
                                                        int heartbeat_recv_cycle,
                                                        int heartbeat_send_cycle) {
    return boost::shared_ptr<UnixSocketConnection>(
        new UnixSocketConnection(io_service, read_callback, write_callback, close_callback,
                                 heartbeat_recv_cycle, heartbeat_send_cycle));
  }

  Socket& get_socket() {
    return socket_;
  }

  EndPoint& get_remote_endpoint() {
    return remote_endpoint_;
  }

  void Start();

  void Write(const boost::shared_ptr<const std::vector<char> > &data);

  void Close() {
    boost::system::error_code ec;
    heartbeat_recv_timer_.cancel(ec);
    heartbead_send_timer_.cancel(ec);
    socket_.close(ec);
  }

  void set_user_argument(const boost::any &user_argument) {
    user_argument_ = user_argument;
  }

  const boost::any& get_user_argument() const {
    return user_argument_;
  }

 private:
  UnixSocketConnection(boost::asio::io_service &io_service,
                       RWCallback read_callback,
                       RWCallback write_callback,
                       CloseCallback close_callback_,
                       int heartbeat_recv_cycle,
                       int heartbeat_send_cycle);

  void HandleReadData(boost::shared_ptr<std::vector<char> > data,
                      const boost::system::error_code &ec,
                      size_t bytes_readed);

  void WriteMessage(const Message &message);
  void HandleReadHead(const boost::system::error_code &ec, size_t bytes_transferred);
  void HandleWriteMessage(const boost::system::error_code &ec, size_t bytes_transferred);
  void AsyncWrite(MessageTypeEnum type, boost::shared_ptr<const std::vector<char> > data);
  void HandleHeartbeatRecv(const boost::system::error_code &ec);
  void HandleHeartbeatSend(const boost::system::error_code &ec);
  void UpdateHeartbeatRecvTimer();
  void UpdateHeartbeatSendTimer();

  boost::asio::io_service &io_service_;
  Socket socket_;
  EndPoint remote_endpoint_;
  RWCallback read_callback_;
  RWCallback write_callback_;
  CloseCallback close_callback_;
  Header read_header_;
  Header write_header_;
  std::queue<Message> write_message_queue_;
  int heartbeat_recv_cycle_;
  int heartbeat_send_cycle_;
  boost::asio::deadline_timer heartbeat_recv_timer_;
  boost::asio::deadline_timer heartbead_send_timer_;

  boost::any user_argument_;
};

} // namespace bbts
#endif // OP_OPED_NOAH_TOOLS_BBTS_UNIX_SOCKET_CONNECTION_H_

/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   BBTSClinet.h
 *
 * @author liuming03
 * @date   2014-1-17
 * @brief 
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_CLIENT_H_
#define OP_OPED_NOAH_TOOLS_BBTS_CLIENT_H_

#include <string>

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

#include "bbts/message.pb.h"
#include "bbts/UnixSocketConnection.h"

namespace bbts {
namespace tool {

/**
 * @brief
 */
class BBTSClient : public boost::noncopyable {
 public:
  BBTSClient();

  BBTSClient(const UnixSocketConnection::EndPoint &endpoint);

  virtual ~BBTSClient();

  int CreateTask(const message::AddTask &params, int64_t *id);

  int SetTaskOptions(const message::TaskOptions &options);

  int GetTaskOptions(message::TaskOptions *opt);

  int SetAgentOptions(const message::AgentOptions &options);

  int GetAgentOptions(message::AgentOptions *options);

  int ListTasks(const std::vector<int64_t> &taskids, message::BatchListRes *res);

  int BatchControl(const std::vector<int64_t> &taskids,
                   message::BatchCtrl::ctrl_t,
                   message::BatchCtrlRes *res);

  int AddMetadata(message::Metadata *metadata);

  inline void set_endpoint(const UnixSocketConnection::EndPoint &endpoint) {
    endpoint_ = endpoint;
  }

  inline void NoSendUcred() {
    send_ucred_ = false;
  }

  inline void SetMaxRetryTimes(int max_retry_times) {
    max_retry_times_ = max_retry_times;
  }

  inline static void set_default_endpoint(const UnixSocketConnection::EndPoint &default_endpoint) {
    default_endpoint_ = default_endpoint;
  }

 private:
  bool TellUnixServer(const google::protobuf::Message &request,
                                  uint32_t *type,
                                  boost::scoped_ptr<google::protobuf::Message> *response);

  bool TellUnixServerWithRetry(const google::protobuf::Message &request,
                               uint32_t *type,
                               boost::scoped_ptr<google::protobuf::Message> *response);

  UnixSocketConnection::EndPoint endpoint_;
  bool send_ucred_;
  int max_retry_times_;

  static UnixSocketConnection::EndPoint default_endpoint_;
};

} // namespace tool
} // namespace bbts
#endif // OP_OPED_NOAH_TOOLS_BBTS_CLIENT_H_

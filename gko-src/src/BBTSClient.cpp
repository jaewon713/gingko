/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   BBTSClient.cpp
 *
 * @author liuming03
 * @date   2014-1-17
 * @brief 
 */

#include "bbts/tool/BBTSClient.h"

#include <vector>

#include <boost/array.hpp>

#include "bbts/constant_def.h"
#include "bbts/log.h"
#include "bbts/path.h"
#include "bbts/socket_util.h"
#include "bbts/UnixSocketClient.h"

namespace bbts {
namespace tool {

using std::string;
using std::vector;
using boost::scoped_ptr;
using boost::shared_ptr;
using boost::array;
using boost::system::error_code;
using boost::asio::local::stream_protocol;
using google::protobuf::Message;

UnixSocketConnection::EndPoint BBTSClient::default_endpoint_;

BBTSClient::BBTSClient()
  : endpoint_(default_endpoint_),
    send_ucred_(true),
    max_retry_times_(1) {}

BBTSClient::BBTSClient(const UnixSocketConnection::EndPoint &endpoint)
  : endpoint_(endpoint),
    send_ucred_(true),
    max_retry_times_(1) {}

BBTSClient::~BBTSClient() {}

int BBTSClient::CreateTask(const message::AddTask &params, int64_t *id) {
  const message::Task &task_info = params.task();
  if (!task_info.has_save_path() || task_info.save_path().empty() ||
      !check_and_mkdir_r(task_info.save_path(), 0755)) {
    FATAL_LOG("save path(%s) not correct or can not create.", task_info.save_path().c_str());
    return 1;
  }

  if (!task_info.has_cmd() || !task_info.has_seeding_time() || !task_info.has_type()) {
    FATAL_LOG("add task params imcomplete", task_info.save_path().c_str());
    return 2;
  }

  if (!task_info.has_torrent_path() && !task_info.has_infohash()) {
    FATAL_LOG("not spec torrent");
    return 3;
  }

  uint32_t type = REQ_ADD_TASK;
  scoped_ptr<google::protobuf::Message> res;
  if (!TellUnixServerWithRetry(params, &type, &res)) {
    FATAL_LOG("Tell agent failed.");
    return 4;
  }

  if (type == RES_BASE) {
    message::BaseRes *base_res = static_cast<message::BaseRes *>(res.get());
    FATAL_LOG("Add new task failed, ret(%d): %s.",
               base_res->ret_code(), base_res->fail_msg().c_str());
    return 5;
  } else {
    *id = static_cast<message::TaskRes *>(res.get())->taskid();
  }
  return 0;
}

int BBTSClient::ListTasks(const vector<int64_t> &taskids, message::BatchListRes *list_res) {
  assert(list_res);
  message::BatchCtrl req;
  if (taskids.empty()) {
    req.set_ctrl_all(true);
  } else {
    req.set_ctrl_all(false);
    for (vector<int64_t>::const_iterator it = taskids.begin();
        it != taskids.end(); ++it) {
      req.add_taskids(*it);
    }
  }
  req.set_ctrl_type(message::BatchCtrl::LIST);
  uint32_t type = REQ_BATCH_CTRL;
  scoped_ptr<google::protobuf::Message> res;
  if (!TellUnixServerWithRetry(req, &type, &res)) {
    FATAL_LOG("tell agent failed.");
    return 1;
  }

  if (type == RES_BASE) {
    message::BaseRes *base_res = static_cast<message::BaseRes *>(res.get());
    FATAL_LOG("list failed, ret(%d): %s", base_res->ret_code(), base_res->fail_msg().c_str());
    return 2;
  } else {
    assert(type == RES_BATCH_LIST);
    list_res->CopyFrom(*(static_cast<message::BatchListRes *>(res.get())));
  }
  return 0;
}

int BBTSClient::BatchControl(const vector<int64_t> &taskids, message::BatchCtrl::ctrl_t ctrl_type,
                               message::BatchCtrlRes *ctrl_res) {
  assert(ctrl_res);
  message::BatchCtrl req;
  if (taskids.empty()) {
    req.set_ctrl_all(true);
  } else {
    req.set_ctrl_all(false);
    for (vector<int64_t>::const_iterator it = taskids.begin(); it != taskids.end(); ++it) {
      req.add_taskids(*it);
    }
  }
  req.set_ctrl_type(ctrl_type);
  uint32_t type = REQ_BATCH_CTRL;
  scoped_ptr<google::protobuf::Message> res;
  if (!TellUnixServerWithRetry(req, &type, &res)) {
    FATAL_LOG("tell agent failed.");
    return 1;
  }

  if (type == RES_BASE) {
    message::BaseRes *base_res = static_cast<message::BaseRes *>(res.get());
    FATAL_LOG("batch ctrl failed, ret(%d): %s",
               base_res->ret_code(), base_res->fail_msg().c_str());
    return 2;
  } else {
    assert(type == RES_BATCH_CTRL);
    ctrl_res->CopyFrom(*(static_cast<message::BatchCtrlRes *>(res.get())));
  }
  return 0;
}

int BBTSClient::SetAgentOptions(const message::AgentOptions &options) {
  uint32_t type = REQ_AGENT_SETOPT;
  scoped_ptr<google::protobuf::Message> res;
  if (!TellUnixServerWithRetry(options, &type, &res)) {
    FATAL_LOG("Tell agent failed.");
    return 1;
  }

  assert(type == RES_BASE);
  message::BaseRes *base_res = static_cast<message::BaseRes *>(res.get());
  if (base_res->ret_code() != 0) {
    FATAL_LOG("agent setopt failed, ret(%d): %s",
               base_res->ret_code(), base_res->fail_msg().c_str());
    return 3;
  }
  return 0;
}

int BBTSClient::GetAgentOptions(message::AgentOptions *options) {
  assert(options);
  uint32_t type = REQ_AGENT_GETOPT;
  scoped_ptr<google::protobuf::Message> res;
  if (!TellUnixServerWithRetry(*options, &type, &res)) {
    FATAL_LOG("Tell agent failed.");
    return 2;
  }

  if (type == RES_BASE) {
    message::BaseRes *base_res = static_cast<message::BaseRes *>(res.get());
    FATAL_LOG("agent setopt failed, ret(%d): %s",
               base_res->ret_code(), base_res->fail_msg().c_str());
    return 3;
  } else {
    assert(type == RES_AGENT_GETOPT);
    options->CopyFrom(*(static_cast<message::TaskOptions *>(res.get())));
  }
  return 0;
}

int BBTSClient::AddMetadata(message::Metadata *metadata) {
  uint32_t type = REQ_ADD_METADATA;
  scoped_ptr<google::protobuf::Message> res;
  if (!TellUnixServerWithRetry(*metadata, &type, &res)) {
    NOTICE_LOG("send metadata to agent failed.");
    return 1;
  }
  return 0;
}

int BBTSClient::SetTaskOptions(const message::TaskOptions &options) {
  if (!options.has_taskid()) {
    WARNING_LOG("not spec taskid");
    return 1;
  }

  uint32_t type = REQ_TASK_SETOPT;
  scoped_ptr<Message> response;
  if (!TellUnixServerWithRetry(options, &type, &response)) {
    WARNING_LOG("Tell unix server failed.");
    return 2;
  }

  assert(type == RES_BASE);
  message::BaseRes *base_res = static_cast<message::BaseRes *>(response.get());
  if (base_res->ret_code() != 0) {
    WARNING_LOG("task(%d) setopt failed, ret(%d): %s", options.taskid(),
                 base_res->ret_code(), base_res->fail_msg().c_str());
    return 3;
  }
  return 0;
}

int BBTSClient::GetTaskOptions(message::TaskOptions *options) {
  assert(options);
  if (!options->has_taskid()) {
    WARNING_LOG("not spec taskid");
    return 1;
  }

  uint32_t type = REQ_TASK_GETOPT;
  scoped_ptr<google::protobuf::Message> response;
  if (!TellUnixServerWithRetry(*options, &type, &response)) {
    WARNING_LOG("Tell unix server failed.");
    return 2;
  }

  if (type == RES_BASE) {
    message::BaseRes *base_res = static_cast<message::BaseRes *>(response.get());
    FATAL_LOG("task(%d) setopt failed, ret(%d): %s", options->taskid(),
               base_res->ret_code(), base_res->fail_msg().c_str());
    return 3;
  } else {
    assert(type == RES_TASK_GETOPT);
    options->CopyFrom(*(static_cast<message::TaskOptions *>(response.get())));
  }
  return 0;
}

bool BBTSClient::TellUnixServer(const Message &request,
                                uint32_t *type,
                                scoped_ptr<Message> *response) {
  boost::asio::io_service ios;
  SyncUnixSocketClient client(ios);
  if (!client.Connect(endpoint_)) {
    WARNING_LOG("connect to server(%s) failed.", endpoint_.path().c_str());
    return false;
  }

  if (send_ucred_) {
    if (!send_cred(client.get_socket().native_handle())) {
      FATAL_LOG("can't send ucred to agent.");
      return false;
    }
  }

  shared_ptr<vector<char> > data(new vector<char>(request.ByteSize() + sizeof(*type)));
  char* ptr = &(*data)[0];
  *reinterpret_cast<uint32_t *>(ptr) = *type;
  if (!request.SerializeToArray(ptr + sizeof(*type), request.ByteSize())) {
    WARNING_LOG("result msg serialize to array failed.");
    return false;
  }
  if (!client.WriteData(data)) {
    WARNING_LOG("sycnc write message failed");
    return false;
  }
  data.reset();
  if (!client.ReadData(&data)) {
    WARNING_LOG("read message failed, : %s.");
    return false;
  }
  client.Close();

  ptr = &(*data)[0];
  *type = *reinterpret_cast<const uint32_t *>(ptr);
  switch (*type) {
    case RES_BASE:
      response->reset(new message::BaseRes());
      break;

    case RES_TASK:
      response->reset(new message::TaskRes);
      break;

    case RES_TASK_STATUS:
      response->reset(new message::TaskStatus);
      break;

    case RES_BATCH_CTRL:
      response->reset(new message::BatchCtrlRes);
      break;

    case RES_BATCH_LIST:
      response->reset(new message::BatchListRes);
      break;

    case RES_TASK_GETOPT:
      response->reset(new message::TaskOptions);
      break;

    case RES_AGENT_GETOPT:
      response->reset(new message::AgentOptions);
      break;

    default:
      WARNING_LOG("not support this response type");
      return false;
  }

  if (!(*response)->ParseFromArray(ptr + sizeof(*type), data->size() - sizeof(*type))) {
    WARNING_LOG("parse message failed.");
    return false;
  }
  return true;
}

bool BBTSClient::TellUnixServerWithRetry(const google::protobuf::Message &request,
                             uint32_t *type,
                             boost::scoped_ptr<google::protobuf::Message> *response) {
  for (int i = 0; i < max_retry_times_;) {
    sleep(i * i);
    if (TellUnixServer(request, type, response)) {
      return true;
    }
    WARNING_LOG("tell unix server failed, will retry %d.", ++i);
  }
  return false;
}

} // namespace tool
} // namespace bbts

/***************************************************************************
 *
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/
#ifndef OP_OPED_NOAH_TOOLS_BBTS_GROUP_CLIENT_H_
#define OP_OPED_NOAH_TOOLS_BBTS_GROUP_CLIENT_H_

#include <string>

#include <boost/smart_ptr.hpp>
#include <thrift/config.h>
#include <thrift/protocol/TCompactProtocol.h>
#include <thrift/transport/TSocket.h> 
#include <thrift/transport/TTransportUtils.h>

#include "GroupConf.pb.h"
#include "gen-cpp/GroupManagerService.h"
#include "utils.h"

using ::apache::thrift::protocol::TProtocol;
using ::apache::thrift::protocol::TCompactProtocol;
using ::apache::thrift::transport::TSocket;      
using ::apache::thrift::transport::TTransport; 
using ::apache::thrift::transport::TFramedTransport;
using ::apache::thrift::TException; 

using ::bbts::tool::down_params_t;

namespace bbts {
namespace group {

class GroupClient {
 public:
  GroupClient();
  ~GroupClient();

  // initialize a client, special a conf file
  int Initialize(const std::string &conf_file);

  // need called manually by caller
  void Shutdown();

  // add a download task into group
  int ProcessAdd(const DownloadParam& down_params);


  // set task options
  int ProcessSetOption(const TaskOptions &options);

  // get task options
  int ProcessGetOption(const std::string &infohash);

  // list tasks in this group, specify list history or not, or use daemon mode
  // infohash: if not empty, specify an infohash task 
  // is_full: if true, show all taks, else show speical infohash TaskStatus
  // is_show_history: if true, show history task, else not
  // is_daemon: if true, show tasks as daemon and not quit, else not
  int ProcessList(const std::string &infohash,
                  const bool is_full,
                  const bool is_show_history,
                  const bool is_daemon);

  // control tasks by infohash or all, type can be cancel/pause/resume
  int ProcessControl(const std::string &infohash,
                     const ControlTaskType::type type,
                     const bool is_all);

 private:
  // default rpc retry times
  static const int kDefaultRpcRetry = 2;
  // in daemon list mode, the default length of one line, this is an important
  // param because if in small size displayer, daemon show will be confused.
  static const int kDefaultFinishedSize = 110;

  // get task status
  int GetTaskStatus(const std::string &infohash,
                    const bool is_full,
                    std::vector<TaskStatus> &task_status_list);

  // print one task status 
  void PrintOneTaskStatus(const TaskStatus &task_status,
                          const bool is_show_history);

  // print one task options
  void PrintOneTaskOptions(const TaskOptions &options);

  // get task status string by a task_status struct
  std::string GetTaskStatusString(const TaskStatus &task_status,
                                  const bool is_show_history);

  // get all tasks sum info, such as total download/upload rate, etc.
  std::string GetAllTaskSumInfo(const std::vector<TaskStatus> &task_status_list);
  
  // print title
  void PrintListTitle();

  // clear all list show in daemon mode, flush stdout to show intelligently
  void ClearList(int line_count);

  // some rpc members...
  boost::shared_ptr<TSocket> socket_;
  boost::shared_ptr<TProtocol> protocol_;
  boost::shared_ptr<TTransport> transport_;
  boost::shared_ptr<GroupManagerServiceClient> rpc_client_;

  // because this class is instantiated by gko3 down/list process, so we can't
  // use kGroupConfig, need use a new
  bbts::GroupConfig group_config_;

  // user cred
  UserCred cred_;

};  // class GroupClient
  
}  // namespace group
}  // namespace bbts

#endif   // OP_OPED_NOAH_TOOLS_BBTS_GROUP_CLIENT_H_

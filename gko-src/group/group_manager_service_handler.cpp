/***************************************************************************
 *
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/
#include "group_manager_service_handler.h"

#include <string>

#include "define.h"
#include "group_manager.h"
#include "utils.h"
#include "bbts/log.h"

namespace bbts {
namespace group {

void GroupManagerServiceHandler::AddGroupTask(
    GeneralResponse &_return,
    const DownloadParam& down_params,
    const UserCred& cred) {
  std::string down_params_string = GeneralParamString(down_params);
  ClientRpcStatus ret_code = GroupManager::GetInstance().AddTask(down_params, cred);
  if (ret_code != kClientRpcSuccess) {
    FATAL_LOG("add download failed, task:%s", down_params_string.c_str());
  }

  _return.ret_code = static_cast<int>(ret_code);
  _return.message = ClientRpcRetCodeToString(ret_code, down_params_string, "add");
  TRACE_LOG("add download task:%s", down_params_string.c_str());
}
void GroupManagerServiceHandler::GetTaskStatus(
    std::vector<TaskStatus> & _return,
    const std::string& infohash, 
    const bool is_full) {
  GroupManager::GetInstance().GetTaskStatus(infohash, is_full, _return);
  DEBUG_LOG("get task status, infohash[%s], is_full:%d", infohash.c_str(), is_full);
}

void GroupManagerServiceHandler::ControlTask(
    GeneralResponse& _return,
    const std::string& infohash, 
    const bool is_all,
    const ControlTaskType::type type,
    const UserCred &cred) {
  ClientRpcStatus ret_code = GroupManager::GetInstance().ControlTask(infohash, is_all, type, cred);
  if (ret_code != kClientRpcSuccess) {
    FATAL_LOG("control task failed, infohash:%s, is_all:%d", infohash.c_str(), is_all);
  }

  _return.ret_code = static_cast<int>(ret_code);
  _return.message = ClientRpcRetCodeToString(ret_code, infohash, "control");
  TRACE_LOG("control task, infohash:%s, is_all:%d, type:%d", infohash.c_str(), is_all, type);
}

void GroupManagerServiceHandler::SetTaskOption(
    GeneralResponse& _return,
    const TaskOptions& options) {

  ClientRpcStatus ret_code = GroupManager::GetInstance().SetTaskOptions(options);
  if (ret_code != kClientRpcSuccess) {
    FATAL_LOG("set task option failed, infohash:%s, download_limit:%d, upload_limit:%d, connect_limit:%d",
               options.infohash.c_str(), options.download_limit, options.upload_limit, options.connections_limit);
  }

  _return.ret_code = static_cast<int>(ret_code);
  _return.message = ClientRpcRetCodeToString(ret_code, options.infohash, "set task option");
  TRACE_LOG("set task option success, infohash:%s, download_limit:%d, upload_limit:%d, connect_limit:%d",
             options.infohash.c_str(), options.download_limit, options.upload_limit, options.connections_limit);
}

void GroupManagerServiceHandler::GetTaskOption(
    std::vector<TaskOptions>& _return,
    const std::string &infohash) {
  GroupManager::GetInstance().GetTaskOptions(infohash, _return);
  TRACE_LOG("get task options, infohash:%s", infohash.c_str());
}

}  // namespace group
}  // namespace bbts


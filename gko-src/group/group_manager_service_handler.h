/***************************************************************************
 *
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

#ifndef OP_OPED_NOAH_TOOLS_BBTS_GROUP_MANAGER_SERVICE_HANDLER_H_
#define OP_OPED_NOAH_TOOLS_BBTS_GROUP_MANAGER_SERVICE_HANDLER_H_

#include <string>
#include <vector>
#include <stdint.h>

#include "gen-cpp/GroupManagerService.h"

namespace bbts {
namespace group {

class GroupManagerServiceHandler : virtual public GroupManagerServiceIf {
 public:
  GroupManagerServiceHandler() {}
  ~GroupManagerServiceHandler() {}

  void AddGroupTask(GeneralResponse& _return,
                    const DownloadParam& down_params,
                    const UserCred& cred);

  void GetTaskStatus(std::vector<TaskStatus> & _return,
                     const std::string& infohash,
                     const bool is_full);

  void ControlTask(GeneralResponse& _return,
                  const std::string& infohash,
                  const bool is_all,
                  const ControlTaskType::type type,
                  const UserCred& cred);

  void SetTaskOption(GeneralResponse& _return,
                     const TaskOptions& options);

  void GetTaskOption(std::vector<TaskOptions>& _return,
                     const std::string &infohash);
};  // class GroupManagerServiceHandler

}  // namespace group
}  // namespace bbts




#endif  // OP_OPED_NOAH_TOOLS_BBTS_GROUP_MANAGER_SERVICE_HANDLER_H_

/***************************************************************************
 *
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/
#ifndef OP_OPED_NOAH_TOOLS_BBTS_GROUP_DEFINE_H_
#define OP_OPED_NOAH_TOOLS_BBTS_GROUP_DEFINE_H_


namespace bbts {
namespace group {

enum ClientRpcStatus {
  kClientRpcSuccess               = 0,
  kClientRpcErrorParams           = -1,
  kClientRpcCredFailed            = -2,
  kClientRpcAddTaskExist          = -3, 
  kClientRpcAddTaskCreateFailed   = -4,
  kClientRpcTaskNotExist          = -5,
  kClientRpcSetOptFailed          = -6,
};

}  // namespace group
}  // namespace bbts

#endif  // OP_OPED_NOAH_TOOLS_BBTS_GROUP_DEFINE_H_

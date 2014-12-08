/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   constant_def.h
 *
 * @author liuming03
 * @date   2013-12-3
 * @brief 
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_CONSTANT_DEF_H_
#define OP_OPED_NOAH_TOOLS_BBTS_CONSTANT_DEF_H_

namespace bbts {

enum {
  REQ_ADD_TASK = 1,
  REQ_TASK_GETOPT,
  REQ_TASK_SETOPT,
  REQ_AGENT_GETOPT,
  REQ_AGENT_SETOPT,
  REQ_BATCH_CTRL,

  REQ_ADD_METADATA = 100,

  RES_BASE = 10000,
  RES_TASK,
  RES_TASK_STATUS,
  RES_BATCH_CTRL,
  RES_BATCH_LIST,
  RES_TASK_GETOPT,
  RES_AGENT_GETOPT,
};

} // namespace bbts

#endif // OP_OPED_NOAH_TOOLS_BBTS_CONSTANT_DEF_H_

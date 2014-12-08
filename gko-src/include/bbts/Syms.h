/***************************************************************************
 *
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file   Syms.h
 *
 * @author liuming03
 * @date   2013-05-30
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_AGENT_SYMS_H_
#define OP_OPED_NOAH_TOOLS_BBTS_AGENT_SYMS_H_

#include "bbts/plugin/hdfstool.h"

namespace bbts {

class Syms {
 public:
  typedef void* (*GetClusterConnectObjectFunc)(const std::string &host,
                                               const int port,
                                               const std::string &user,
                                               const std::string &passwd);

  typedef int (*IntVoidptrFunc)(void*);

  typedef int (*ReadPiecesFromClusterFunc)(const void* fs,
                                           const AbstractBTInfo* info,
                                           int piece_index,
                                           char* data,
                                           size_type size,
                                           std::string &errstr);

  static bool ParseSysm(const std::string &so_path,
                        const std::string &JAVA_HOME,
                        const std::string &CLASS_PATH);

  static void CloseAllSyms();

  static bool SupportHadoop() {
    return support_hadoop_;
  }

  static GetClusterConnectObjectFunc get_cluster_connect_object_;
  static IntVoidptrFunc drop_cluster_connect_object_;
  static ReadPiecesFromClusterFunc read_pieces_from_cluster_;

 private:
  static bool InitEnv(const std::string &JAVA_HOME, const std::string &CLASS_PATH);

  static bool support_hadoop_;
  static void* hadoop_so_handle_;
};

bool ParseSyms();

} // namespace bbts
#endif // OP_OPED_NOAH_TOOLS_BBTS_AGENT_SYMS_H_

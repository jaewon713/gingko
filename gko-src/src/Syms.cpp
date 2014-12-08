/***************************************************************************
 *
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file   Syms.cpp
 *
 * @author liuming03
 * @date   2013-05-30
 */

#include "bbts/Syms.h"

#include <dlfcn.h>

#include "bbts/configure.pb.h"
#include "bbts/LazySingleton.hpp"
#include "bbts/log.h"
#include "bbts/path.h"

using std::string;

using bbts::message::DownloadConfigure;

namespace bbts {

bool Syms::support_hadoop_ = false;
void* Syms::hadoop_so_handle_ = NULL;
Syms::GetClusterConnectObjectFunc Syms::get_cluster_connect_object_ = NULL;
Syms::IntVoidptrFunc Syms::drop_cluster_connect_object_ = NULL;
Syms::ReadPiecesFromClusterFunc Syms::read_pieces_from_cluster_ = NULL;

bool Syms::InitEnv(const string &JAVA_HOME, const string &CLASS_PATH) {
  if (setenv("CLASSPATH", CLASS_PATH.c_str(), 1) != 0) {
    WARNING_LOG("setevn CLASSPATH failed.");
    return false;
  }
  return true;
}

bool Syms::ParseSysm(const string &so_path, const string &JAVA_HOME, const string &CLASS_PATH) {
  if (!InitEnv(JAVA_HOME, CLASS_PATH)) {
    WARNING_LOG("init java and hadoop evn failed.");
    return false;
  }

  hadoop_so_handle_ = dlopen(so_path.c_str(), RTLD_LAZY);
  if (!hadoop_so_handle_) {
    WARNING_LOG("%s: %s", so_path.c_str(), dlerror());
    return false;
  }

  get_cluster_connect_object_ = (GetClusterConnectObjectFunc)dlsym(hadoop_so_handle_, "GetHadoopConnectObject");
  if (!get_cluster_connect_object_) {
    WARNING_LOG("%s", dlerror());
    dlclose(hadoop_so_handle_);
    return false;
  }

  drop_cluster_connect_object_ = (IntVoidptrFunc)dlsym(hadoop_so_handle_, "DropHadoopConnectObject");
  if (!drop_cluster_connect_object_) {
    WARNING_LOG("%s", dlerror());
    dlclose(hadoop_so_handle_);
    return false;
  }

  read_pieces_from_cluster_ = (ReadPiecesFromClusterFunc)dlsym(hadoop_so_handle_, "GetFileConetnsFromHdfs");
  if (!read_pieces_from_cluster_) {
    WARNING_LOG("%s", dlerror());
    dlclose(hadoop_so_handle_);
    return false;
  }

  support_hadoop_ = true;
  return true;
}

void Syms::CloseAllSyms() {
  if (hadoop_so_handle_) {
    dlclose(hadoop_so_handle_);
    hadoop_so_handle_ = NULL;
  }
  support_hadoop_ = false;
}

bool ParseSyms() {
  DownloadConfigure *configure = LazySingleton<message::DownloadConfigure>::instance();
  // get running root path
  string root_path;
  if (!bbts::get_root_path(&root_path)) {
    WARNING_LOG("Get program root path failed.\n");
    return false;
  }
  return Syms::ParseSysm(root_path + "/lib/libhdfstool.so",
                         configure->java_home(),
                         configure->class_path());
}

} // namespace bbts

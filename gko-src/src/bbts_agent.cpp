/***************************************************************************
 *
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file   bbts_agent.cpp
 *
 * @author liuming03
 * @date   2013-1-9
 */

#include <getopt.h>
#include <fcntl.h>

#include <string>

#include "bbts/agent/Task.h"
#include "bbts/agent/TaskManager.h"
#include "bbts/common_def.h"
#include "bbts/configure.pb.h"
#include "bbts/LazySingleton.hpp"
#include "bbts/path.h"
#include "bbts/pb_conf.h"
#include "bbts/log.h"

using std::string;
using bbts::message::AgentConfigure;
using namespace bbts;
using namespace bbts::agent;

// 默认的日志配置文件
#define LOG_CONF_DIR  "/conf"
#define LOG_CONF_FILE "log.conf"

static const char *g_program = "bbts-agent";

static void print_help() {
  fprintf(stdout, "\
Usage: %s\n\
  --help         print this program help\n\
  --version      print this program version\n", g_program);
}

static void print_version() {
  fprintf(stdout, "%s\nversion: %s\n", g_program, GINGKO_VERSION);
}

/*
 * @brief 初始化相关运行时目录
 */
static int init_agent_running_path() {
  AgentConfigure *configure = LazySingleton<AgentConfigure>::instance();
  if (!check_and_mkdir(configure->working_dir(), 0755)) {
    FATAL_LOG("check and mkdir(%s) failed.", configure->working_dir().c_str());
    return -1;
  }

  // 设置当前工作目录
  if (chdir(configure->working_dir().c_str()) != 0) {
    FATAL_LOG("chdir to working path failed: %s", configure->working_dir().c_str());
    exit(1);
  }

  if (!check_and_mkdir(configure->resume_dir(), 0755)) {
    FATAL_LOG("check and mkdir(%s) failed.", configure->resume_dir().c_str());
    return -1;
  }

  // for download task stat log file
  mode_t mode = umask(0);
  int fd = open(configure->task_stat_file().c_str(), O_CREAT | O_WRONLY | O_APPEND, 0666);
  if (fd < 0) {
    FATAL_LOG("open task log file failed: %d.", errno);
    return -1;
  }
  close(fd);
  chmod(configure->task_stat_file().c_str(), 0666);

  fd = open(configure->peer_stat_file().c_str(), O_CREAT | O_WRONLY | O_APPEND, 0666);
  if (fd < 0) {
    FATAL_LOG("open peer log file failed: %d.", errno);
    return -1;
  }
  close(fd);
  chmod(configure->peer_stat_file().c_str(), 0666);

  fd = open(configure->download_log_file().c_str(), O_CREAT | O_WRONLY | O_APPEND, 0666);
  if (fd < 0) {
    FATAL_LOG("open download log file failed: %d.", errno);
    return -1;
  }
  close(fd);
  chmod(configure->download_log_file().c_str(), 0666);
  umask(mode);
  return 0;
}

static void parse_argv(int argc, char *argv[]) {
  int option = -1;
  int longOptionIndex;
  const struct option longOptions[] = {
      { "help", 0, NULL, 'h' },
      { "version", 0, NULL, 'v' },
      { NULL, 0, NULL, 0 }
  };

  g_program = argv[0];
  while ((option = getopt_long(argc, argv, "hv", longOptions, &longOptionIndex)) != -1) {
    switch (option) {
      case 'h':
        print_help();
        exit(0);

      case 'v':
        print_version();
        exit(0);

      case ':':
        case '?':
        default:
        print_help();
        exit(1);
    }
  }
}

static void handle_segv(int sig) {
  NOTICE_LOG("catch sigal %d!", sig);
  TaskManager::instance().Stop();
}

static void handle_sigpipe(int sig) {
  NOTICE_LOG("catch sigal %d!", sig);
}

static void set_signal_action() {
  struct sigaction sa;
  sa.sa_flags = SA_RESETHAND;
  sa.sa_handler = handle_segv;
  sigemptyset(&sa.sa_mask);
  // sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  struct sigaction sigpipe;
  sigpipe.sa_flags = 0;
  sigpipe.sa_handler = handle_sigpipe;
  sigemptyset(&sigpipe.sa_mask);
  sigaction(SIGPIPE, &sigpipe, NULL);
}

static bool lock_file(const string &filename) {
  int fd = open(filename.c_str(), O_WRONLY | O_CREAT, 0600);
  if (fd < 0) {
    return false;
  }
  struct flock flockbuf = { F_WRLCK, 0, SEEK_SET, 0, 0 };
  if (fcntl(fd, F_SETLK, &flockbuf) < 0) {
    return false;
  }
  return true;
}

static void CheckStatFile(boost::asio::deadline_timer *timer,
                          const boost::system::error_code &ec) {
  AgentConfigure *configure = LazySingleton<AgentConfigure>::instance();
  struct stat statbuf;
  if (stat(configure->task_stat_file().c_str(), &statbuf) == 0) {
    if (statbuf.st_size > 10 * 1024 * 1024) {
      if (truncate(configure->task_stat_file().c_str(), 0) != 0) {
        WARNING_LOG("truncate file %s size to 0 failed: %d",
                     configure->task_stat_file().c_str(), errno);
      }
    }
  } else {
    WARNING_LOG("can't stat file %s: %d", configure->task_stat_file().c_str(), errno);
  }

  if (stat(configure->peer_stat_file().c_str(), &statbuf) == 0) {
    if (statbuf.st_size > 10 * 1024 * 1024) {
      if (truncate(configure->peer_stat_file().c_str(), 0) != 0) {
        WARNING_LOG("truncate file %s size to 0 failed: %d",
                     configure->peer_stat_file().c_str(), errno);
      }
    }
  } else {
    WARNING_LOG("can't stat file %s: %d", configure->peer_stat_file().c_str(), errno);
  }
  timer->expires_from_now(boost::posix_time::hours(1));
  timer->async_wait(boost::bind(&CheckStatFile, timer, boost::asio::placeholders::error()));
}

int bbts_agent(int argc, char* argv[]) {
  // 解析命令行参数
  parse_argv(argc, argv);

  // get running root path
  string root_path;
  if (!bbts::get_root_path(&root_path)) {
    fprintf(stderr, "Get program root path failed.\n");
    exit(1);
  }

  AgentConfigure *configure = LazySingleton<AgentConfigure>::instance();
  if (!LoadPBConf(root_path + "/conf/agent.conf", configure)) {
    fprintf(stderr, "load agent.conf failed.\n");
    exit(1);
  }

  // 加载日志模块
  if (!LoadLogByConfigure((root_path + LOG_CONF_DIR).c_str(), LOG_CONF_FILE)) {
    fprintf(stderr, "load log conf(%s/%s) failed.\n", LOG_CONF_DIR, LOG_CONF_FILE);
    exit(1);
  }

  NOTICE_LOG("bbts agent version: %s", GINGKO_VERSION);

  // 初始化数据临时目录等
  if (init_agent_running_path() != 0) {
    exit(1);
  }

  if (!lock_file(configure->lock_file())) {
    FATAL_LOG("lock file %s fail.", configure->lock_file().c_str());
    exit(1);
  }

  set_signal_action();
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  // 创建实例
  int ret = 0;
  if (!TaskManager::instance().Start()) {
    FATAL_LOG("agent start failed.");
    return -2;
  }

  boost::asio::deadline_timer timer(TaskManager::instance().get_io_service(),
                                    boost::posix_time::hours(1));
  timer.async_wait(boost::bind(&CheckStatFile, &timer, boost::asio::placeholders::error()));
  TaskManager::instance().Join();

  google::protobuf::ShutdownProtobufLibrary();
  CLOSE_LOG();
  return ret;
}

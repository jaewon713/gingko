/***************************************************************************
 *
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/file.h>

#include <string>
#include <sstream>

#include <boost/scoped_ptr.hpp>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

#include "bbts/common_def.h"
#include "bbts/path.h"

#include "GroupConf.pb.h"
#include "group_manager.h"
#include "group_params.h"
#include "group_task.h"
#include "utils.h"
#include "bbts/log.h"

using ::bbts::group::GroupManager;

static const char *g_program = "bbts-group";

::bbts::group::GroupParams g_params;
::bbts::GroupConfig kGroupConfig;

static void PrintHelp() {
  fprintf(stdout, "\
Desc:\n\
  bbts-group is a container for download tasks. You can add many download tasks into\n\
a group and set global options for all download tasks such as download limit rate.\n\
\n\
Usage: gko3 bbts-group --conf path/to/conf\n\
  --conf         the group conf path\n\
  --genconf      generate a template conf file\n\
  --help         print this program help\n\
  --version      print this program version\n\
\n\
  You can use gko3 down/list/setopt/getopt/cancel/resume/pause options to add/delete/set/view\n\
tasks in one group.\n\
  A conf file identify an unique group and the dirctory of conf file can start only one group.\n\
\n\
Add task:\n\
  gko3 down [-i infohash | -r torrent_file | -U HTTP-URL] --conf path/to/conf [other down params]\n\
\n\
Control task:\n\
  gko3 cancel/pause/resume [-i infohash | --all] --conf path/to/conf\n\
\n\
List task:\n\
  gko3 list [-i infohash] --conf path/to/conf [--history] [-w --wait]\n\
    -w  --wait      list task and don't quit until all tasks finished\n\
        --history   list history finished tasks of this group\n\
\n\
getopt/setopt:\n\
  gko3 setopt/getopt [-i infohash] [-u --uplimit] [-d --downlimit] [-c --connlimit] --conf path/to/conf\n");
}

static void GenerateTemplateConf() {
  ::bbts::GroupConfig conf;

  // set default value
  conf.set_timeout(-1);
  conf.set_upload_rate_limit(100);
  conf.set_download_rate_limit(100);
  conf.set_connection_limit(8000);
  conf.set_finished_timeout(3600);
  conf.set_max_cluster_thread_num(10);
  conf.set_listen_port(18000);

  conf.set_max_metadata_size(50);
  conf.set_peers_num_want(25);
  conf.set_seed_announce_interval(1800);
  conf.set_min_reconnect_time(1);
  conf.set_max_queued_disk_bytes(20);
  conf.set_max_out_request_queue(1500);
  conf.set_whole_pieces_threshold(20);
  conf.set_request_queue_time(3);
  conf.set_cache_size(128);
  conf.set_cache_expiry(300);
  conf.set_read_cache_line_size(32);
  conf.set_write_cache_line_size(32);
  conf.set_file_pool_size(500);
  conf.set_send_buffer_watermark(10);
  conf.set_send_buffer_low_watermark(1024);
  conf.set_send_socket_buffer_size(2048);
  conf.set_recv_socket_buffer_size(2048);
  conf.set_active_seeds(32000);
  conf.set_active_limit(32000);
  conf.set_active_downloads(32000);
  conf.set_suggest_mode(true);
  conf.set_peer_connection_timeout(3);
  conf.set_disable_os_cache(false);

  std::string dump_str;
  google::protobuf::TextFormat::PrintToString(conf, &dump_str);
  fprintf(stdout, "%s", dump_str.c_str());
}

static void PrintVersion() {
  fprintf(stdout, "%s\nversion: %s\n", g_program, GINGKO_VERSION);
}

static int InitLogConfig() {
  // use bbts-group file
  std::string host_path = kGroupConfig.absolute_path() + kGroupConfig.group_data_path();
  bbts::InitLog(host_path, kGroupConfig.log_file(), g_params.is_debug);
  return 0;
}

static int InitGroupRunningPath() {
  std::string data_path = kGroupConfig.absolute_path() + kGroupConfig.group_data_path();
  if (!::bbts::check_and_mkdir(data_path, 0755)) {
    fprintf(stderr, "check and mkdir(%s) failed.\n", data_path.c_str());
    return -1;
  }

  // change to conf data path 
  if (chdir(data_path.c_str()) != 0) {
    FATAL_LOG("chdir to conf data path failed: %s", data_path.c_str());
    return -1;
  }

  // create or open log file
  int fd = open(kGroupConfig.log_file().c_str(), O_CREAT | O_WRONLY | O_APPEND, 0666);
  if (fd < 0) {
    FATAL_LOG("open log file(%s) failed, errno(%d):%s.",
                kGroupConfig.log_file().c_str(), errno, strerror(errno));
    return -1;
  }
  close(fd);

  // create or open lock file 
  fd = open(kGroupConfig.lock_file().c_str(), O_CREAT | O_WRONLY | O_APPEND, 0666);
  if (fd < 0) {
    FATAL_LOG("open resume file(%s) failed, errno(%d):%s.",
                kGroupConfig.lock_file().c_str(), errno, strerror(errno));
    return -1;
  }
  close(fd);

  // create resume dir
  if (!::bbts::check_and_mkdir(kGroupConfig.resume_dir().c_str(), 0755)) {
    fprintf(stderr, "check and mkdir(%s) failed.\n", kGroupConfig.resume_dir().c_str());
    return -1;
  }
  
  return 0;
}

static void ParseArgv(int argc, char *argv[]) {
  int option = -1;
  int longOptionIndex;
  const struct option longOptions[] = {
      { "help",     no_argument,        NULL, 'h'},
      { "version",  no_argument,        NULL, 'v'},
      { "debug",    no_argument,        NULL, 'D'},
      { "conf",     required_argument,  NULL,  1 },
      { "genconf",  no_argument,        NULL,  2 },
      { NULL,       no_argument,        NULL,  0 }
  };

  g_program = argv[0];
  while ((option = getopt_long(argc, argv, "hvD", longOptions, &longOptionIndex)) != -1) {
    switch (option) {
      case 'h':
        PrintHelp();
        exit(0);

      case 'v':
        PrintVersion();
        exit(0);
      case 'D':
        g_params.is_debug = true;
        break;
      case 1:
        g_params.conf_file = optarg;
        g_params.conf_file = bbts::complete_full_path(g_params.conf_file);
        break;
      case 2:
        GenerateTemplateConf();
        exit(0);
      default:
        PrintHelp();
        exit(1);
    }
  }
}

static void HandleSegv(int sig) {
  NOTICE_LOG("catch sigal %d!", sig);
  GroupManager::GetInstance().Shutdown();
}

static void HandleSigpipe(int sig) {
  NOTICE_LOG("catch sigal %d!", sig);
}

void SetSignalAction() {
  struct sigaction sa;
  sa.sa_flags = SA_RESETHAND;
  sa.sa_handler = HandleSegv;
  sigemptyset(&sa.sa_mask);
  //sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  struct sigaction sigpipe;
  sigpipe.sa_flags = 0;
  sigpipe.sa_handler = HandleSigpipe;
  sigemptyset(&sigpipe.sa_mask);
  sigaction(SIGPIPE, &sigpipe, NULL);
}

static int LockFile(const std::string &filename, int *lock_fd) {
  int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0) {
    FATAL_LOG("Open lock file(%s) failed, errno(%d):%s",
                filename.c_str(), errno, strerror(errno));
    return -1;
  }

  if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
    fprintf(stdout, "Lock file(%s) error, errno(%d):%s\n",
                filename.c_str(), errno, strerror(errno));
    close(fd);
    return -1;
  }

  std::stringstream pid_stream;
  pid_stream << getpid();
  if (write(fd, pid_stream.str().c_str(), pid_stream.str().length()) < 0) {
    FATAL_LOG("Write pid(%s) to lock file error, errno(%d):%s",
                pid_stream.str().c_str(), errno, strerror(errno));
    close(fd);
    return -1;
  }

  *lock_fd = fd;
  return 0;
}

int bbts_group(int argc, char* argv[]) {
  // prase command line
  ParseArgv(argc, argv);

  if (g_params.conf_file.empty()) {
    fprintf(stderr, "must use --conf to assign a conf file\n");
    return -1;
  }

  if (::bbts::group::LoadGroupConfig(g_params.conf_file, &kGroupConfig) != 0) {
    return -1;
  }

  // prepare path first
  if (InitGroupRunningPath() != 0) {
    return -1;
  }

  // load log
  if (InitLogConfig() != 0) {
    fprintf(stderr, "Init Log failed!\n");
    return -1;
  }


  int lock_fd;
  if (LockFile(kGroupConfig.lock_file(), &lock_fd) != 0){
    return -1;
  }
  SetSignalAction();

  GroupManager::GetInstance().Run();
  GroupManager::GetInstance().Join();

  // close comlog may be suspend...
  // com_closelog();
  close(lock_fd);
  DEBUG_LOG("bbts-group exit");

  return 0;
}

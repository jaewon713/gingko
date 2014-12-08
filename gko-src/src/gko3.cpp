/****************************************************************************
 *
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file   gko3.cpp
 *
 * @author liuming03
 * @date   2013-2-9
 */

#include <getopt.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>

#include "bbts/configure.pb.h"
#include "bbts/common_def.h"
#include "bbts/host_info_util.h"
#include "bbts/LazySingleton.hpp"
#include "bbts/number_util.h"
#include "bbts/path.h"
#include "bbts/pb_conf.h"
#include "bbts/string_util.h"
#include "bbts/subnet_mask.h"
#include "bbts/Syms.h"
#include "bbts/tool/BBTSClient.h"
#include "bbts/torrent_file_util.h"
#include "bbts/util.h"
#include "bbts/tool/Downloader.h"
#include "../group/group_client.h"
#include "../group/gen-cpp/GroupManagerService.h"
#include "../group/utils.h"

using std::string;
using std::map;
using std::vector;
using boost::system::error_code;
using libtorrent::cluster_config_entry;
using libtorrent::address;
using libtorrent::ip_filter;
using bbts::message::DownloadConfigure;
using bbts::LazySingleton;
using bbts::tool::down_params_t;
using bbts::tool::Downloader;
using bbts::tool::BBTSClient;
using namespace bbts;
using namespace bbts::group;

static void print_help() {
  DownloadConfigure *configure = LazySingleton<DownloadConfigure>::instance();
  fprintf(stdout, "\
NAME:\n\
  gko3 -- p2p transfer, download data from many other peers by BitTorrent protocol.\n\
\n\
Usage: gko3 operation [options]\n\
  operation described as follows:\n\
\n\
  mkseed -p path [r:s:C:W:l] [--include-files regex] [--exclude-files regex]\n\
    Make a torrent file for spcified path.\n\
      -p,  --path          The root path you want to share.\n\
      -r,  --torrent       Output filename of the torrent file for this path, default to path.torrent.\n\
      -s,  --size          Piece size(KB) for the torrent, default to program auto-computed value(>=2MB), 0 = default.\n\
                           Multiple of 16 KB is recommended.\n\
      -C,  --cluster       The hadoop addr where source data located, like hdfs://user:passwd@host:port/path\n\
      -W,  --webseed       The http addr where source data located, like http://noah.baidu.com/filepath\n\
      -l,  --link          Don't follow symlinks, instead encode them as links in the torrent file.\n\
           --include-files The files that you want to include in .torrent for serve. If use regex, need add quotation\n\
                           marks around.\n\
           --exclude-files The files that you don't want to include in .torrent for serve. As the same, need add\n\
                           quotation marks around if use regex.\n\
\n\
  dump   -r torrent\n\
    Dump spcified torrent file info.\n\
\n\
  add    (-r torrent | -i infohash) (-p path | -n fullpath) -S seedtime [-u uplimit] [--seed]\n\
    Add a p2p task(seeding task or download task).\n\
      -r,  --torrent       The BitTorrent seed file, like *.torrent.\n\
      -i,  --infohash      The 20 bytes infohash of a *.torrent file.\n\
      -p,  --path          For seeder is where seeded file located.\n\
      -n,  --fullpath      Path + root name, if specified, we will ignore -p \n\
      -S,  --seedtime      When task is finished, it will seeding for seedtime seconds. \n\
                           If seedtime equals -1, the task won't quit until user cancel this Task.\n\
                           If not set, this task will quit when it has finished, just like seedtime equals 0.\n\
      -u,  --uplimit       Limit the upload bandwidth(MB/s), default to %d MB/s, 0 = default.\n\
           --seed          This task will be as a seeding task, not download any data.\n\
\n\
  serve  -p path -S seedtime [u:r:s:l] [--include-files] [--exclude-files]\n\
    Atomic operation for mkseed and add. Arguments is like mkseed and add --seed.\n\
\n\
  down   (-r torrent | -i infohash) (-p path | -n fullpath) [S:O:d:u:C:W:P:] [--include-files] [--exclude-files]\n\
    Synchronous download.\n\
      -d,  --downlimit     Limit the download bandwith, unit MB/s. Default or 0 will be set to %d MB/s.\n\
      -O,  --timeout       If download time exceed this value, the download process will quit as timeout\n\
      -P,  --port          Bind port range for p2p transfer, such as 6881,6889.\n\
           --include-files The files that you want to download. Suport regex, should add quotation marks around.\n\
           --exclude-files The files that you don't want to download. Suport regex, should add quotation marks around.\n\
           --continue      Continue getting a partially-downloaded job. Default is disabled\n\
           --tmp-files     The data will be download to tmp files first, and then move to save path that the user set.\n\
           --unix-socket   Set a unix socket path for communicate with download process, such as modify download limit, etc.\n\
           --save-torrent  If download by infohash, set this can save torrent to local file.\n\
           --hang-timeout  This is for check whether download is hang, if exceed this value, download quit as no seeder.\n\
           --debug         Open debug mode. Default not open.\n\
           --ip-white      Ip white ip list for connect to theses peers.\n\
                           e.g. --ip-white=10.26.0.0-10.26.255.255,10.152.15.37,10.38.24.129-10.38.24.192\n\
           --mask          Like subnet mask, only these subnet can connect, host ip 10.26.38.49\n\
                           with --mask=255.255.0.0 is the same as --ip-white=10.26.0.0-10.26.255.255\n\
\n\
  cancel/pause/resume [-t taskid ... ] [--all]\n\
    Cancel/pause/resume task from agent, it can specifiy muti-taskid.\n\
      -t,  --task         The task id that add return, unique mark a task.\n\
           --all          Batch process all task that your account can.\n\
\n\
  list   [-t taskid ... ] [-a attributes] [--noheader]\n\
    List the tasks you specified, if not set -t, will list all tasks.\n\
\n\
  setopt [-t taskid | --unix-socket] [-u uprate] [-d downrate]\n\
    Modify the task or agent option, you should use unix-socket for download task.\n\
\n\
  getopt [-t taskid | --unix-socket]\n\
    Get task or agent option, you should use unix-socket for download task\n\
\n\
  help [attr]\n\
\n\
      -h,  --help         Show this help message.\n\
      -v,  --version      Show version message\n\
\n\
  bbts-group\n\
    Use bbts-group, for more infomation, use gko3 bbts-group -h for help \n\
\n\
EXAMPLES\n\
  You can make seed file(a.torrent) as follows:\n\
     gko3 mkseed -p /path/to/a -r a.torrent\n\
  To seeding /path/to/a with a.torrent in local machine:\n\
     gko3 add -r a.torrent -p /path/to -S 3600 -u 20 --seed\n\
  Or you can combine mkseed and add for seeding /path/to/a as follows:\n\
     gko3 serve -p /path/to/a -r a.torrent -S 3600 -u 20\n\
\n\
  The following is how to download from the torrent file a.torrent to local path /path/to/save\n\
     gko3 down -r a.torrent -p /path/to/save -u 10 -d 10\n\
\n\
   WIKI: http://doc.noah.baidu.com/new/bbts/introduction.md\n\
\n\
AUTHORS: huxiaoxiao@baidu.com, liuming03@baidu.com, bask@baidu.com\n\
     QA: zhangguiying01@baidu.com, sunshengxiang01@baidu.com\n",
configure->upload_limit() / 1024 / 1024,
configure->download_limit() / 1024 / 1024);
}

static void print_attr_help() {
  fprintf(stdout, "\
attr help\n\
    Attributes is used for specifie what you want to display.This arguments can used for operation status,\n\
    list, wait and down. Multiple attributes need separate with blank, colon or semicolon.\n\
\n\
    taskid    task id                             status    task current status\n\
    progress  percentage of the task progress     infohash  seed token\n\
    user      the user who add the task to agent  group     the group of the user\n\
    cmd       command self which add the task     error     display error messsage\n\
    uprate    current upload speeds               upload    total uploaded for this task\n\
    downrate  current upload speeds               download  total uploaded for this task\n\
    peers     the number of the current peers     seeds     the number of the current seed peers\n\
    savepath  the data to be saved to\n\
\n\
    If not specifie the -a arguments, will display 'taskid status progress infohash user cmd'\n\
\n");
}

static void print_version(const char* program) {
  fprintf(stdout, "%s\n  version: %s\n  build date: %s\n", program, GINGKO_VERSION, BUILD_DATE);
}

static void parse_attributes(const map<string, int> &attr_name_map,
                             const string &attr_str,
                             vector<string> *attr_set) {
  attr_set->clear();
  vector<string> v;
  SliptStringToVector(attr_str, ",;: \t\n", &v);
  for (vector<string>::iterator i = v.begin(); i != v.end(); ++i) {
    if (attr_name_map.find(*i) == attr_name_map.end()) {
      continue;
    }
    bool finded = false;
    for (vector<string>::iterator it = attr_set->begin(); it != attr_set->end(); ++it) {
      if (*it == *i) {
        finded = true;
        break;
      }
    }
    if (!finded) {
      attr_set->push_back(*i);
    }
  }
}

static void print_task_status_header(map<string, int> &attr_name_map, const vector<string> &attr_set) {
  vector<string>::const_iterator it;
  for (it = attr_set.begin(); it != attr_set.end(); ++it) {
    switch (attr_name_map[*it]) {
      case 0:  //taskid
        fprintf(stdout, "%12s ", it->c_str());
        break;

      case 1:  //status
        fprintf(stdout, "%8s ", it->c_str());
        break;

      case 2:  //progress
        fprintf(stdout, "%8s ", it->c_str());
        break;

      case 3:  //infohash
        fprintf(stdout, "%-40s ", it->c_str());
        break;

      case 4:  //user
        fprintf(stdout, "%-20s ", it->c_str());
        break;

      case 5:  //group
        fprintf(stdout, "%-20s ", it->c_str());
        break;

      case 6:  //cmd
        fprintf(stdout, "%-10s ", it->c_str());
        break;

      case 7:  //error
        fprintf(stdout, "%-5s ", it->c_str());
        break;

      case 8:  //download
        fprintf(stdout, "%11s ", it->c_str());
        break;

      case 9:  //upload
        fprintf(stdout, "%11s ", it->c_str());
        break;

      case 10:  //downrate
        fprintf(stdout, "%11s ", it->c_str());
        break;

      case 11:  //uprate
        fprintf(stdout, "%11s ", it->c_str());
        break;

      case 12:  //peers
        fprintf(stdout, "%5s ", it->c_str());
        break;

      case 13:  //seeds
        fprintf(stdout, "%5s ", it->c_str());
        break;

      case 14:  //save_path
        fprintf(stdout, "%-10s ", it->c_str());
        break;

      default:
        break;
    }
  }
  fprintf(stdout, "\n");
}

static void print_task_status(map<string, int> &attr_name_map,
                              const message::TaskStatus &task_status,
                              const vector<string> &attr_set) {
  /* 获取passwd字段的缓存大小 */
  const static int PASSWD_BUF_SIZE = 16384;
  const message::Task &task_info = task_status.task();
  string tmp;
  vector<string>::const_iterator it;
  for (it = attr_set.begin(); it != attr_set.end(); ++it) {
    switch (attr_name_map[*it]) {
      case 0:  //taskid
        fprintf(stdout, "%12ld ", task_info.taskid());
        break;

      case 1:  //status
        fprintf(stdout, "%8s ", message::TaskStatus::status_t_Name(task_status.status()).c_str());
        break;

      case 2:  //progress
        fprintf(stdout, "%7.3f%% ", task_status.progress() / 10000.0f);
        break;

      case 3:  //infohash
        fprintf(stdout, "%-40s ", task_info.infohash().c_str());
        break;

      case 4: {  //user
        struct passwd pwd;
        struct passwd *result = NULL;
        char buf[PASSWD_BUF_SIZE] = { 0 };
        int ret = getpwuid_r(task_info.uid(), &pwd, buf, sizeof(buf), &result);
        if (ret == 0 && result != NULL) {
          fprintf(stdout, "%-20s ", pwd.pw_name);
        } else {
          fprintf(stdout, "%-20s ", "unknow");
        }
        break;
      }

      case 5: {  //group
        struct group grp;
        struct group *result = NULL;
        char buf[PASSWD_BUF_SIZE] = { 0 };
        int ret = getgrgid_r(task_info.gid(), &grp, buf, sizeof(buf), &result);
        if (ret == 0 && result != NULL) {
          fprintf(stdout, "%-20s ", grp.gr_name);
        } else {
          fprintf(stdout, "%-20s ", "unknow");
        }
        break;
      }

      case 6:  //cmd
        fprintf(stdout, "%-10s ", task_info.cmd().c_str());
        break;

      case 7:  //error
        if (task_status.error().empty()) {
          fprintf(stdout, "%-5s ", "N/A");
        } else {
          fprintf(stdout, "%-5s ", task_status.error().c_str());
        }
        break;

      case 8:  //download
        BytesToReadable(task_status.total_download(), &tmp);
        fprintf(stdout, "%11s ", tmp.c_str());
        break;

      case 9:  //upload
        BytesToReadable(task_status.total_upload(), &tmp);
        fprintf(stdout, "%11s ", tmp.c_str());
        break;

      case 10:  //downrate
        BytesToReadable(task_status.download_rate(), &tmp);
        fprintf(stdout, "%9s/s ", tmp.c_str());
        break;

      case 11:  //uprate
        BytesToReadable(task_status.upload_rate(), &tmp);
        fprintf(stdout, "%9s/s ", tmp.c_str());
        break;

      case 12:  //peers
        fprintf(stdout, "%5d ", task_status.num_peers());
        break;

      case 13:  //seeds
        fprintf(stdout, "%5d ", task_status.num_seeds());
        break;

      case 14:  //save_path
        fprintf(stdout, "%-10s ", (task_info.save_path() + '/' + task_info.new_name()).c_str());
        break;

      default:
        break;
    }
  }
  fprintf(stdout, "\n");
}

static bool parse_cluster_config(string uri, cluster_config_entry &config) {
  if (uri.substr(0, 7) != "hdfs://") {
    return false;
  }

  string::size_type pos1, pos2, pos3, pos4, npos = string::npos;
  pos1 = uri.find(':', 7);
  pos2 = uri.find('@', pos1 + 1);
  pos3 = uri.find(':', pos2 + 1);
  pos4 = uri.find('/', pos3 + 1);
  if (pos1 == npos || pos2 == npos || pos3 == npos || pos4 == npos) {
    return false;
  }

  config.user = uri.substr(7, pos1 - 7);
  config.passwd = uri.substr(pos1 + 1, pos2 - pos1 - 1);
  config.host = uri.substr(pos2 + 1, pos3 - pos2 - 1);
  config.port = atoi(uri.substr(pos3 + 1, pos4 - pos3 - 1).c_str());
  config.prefix_path = uri.substr(pos4);

  if (config.user.empty() || config.passwd.empty() || config.host.empty()
      || config.port == 0 || config.prefix_path.empty()) {
    return false;
  }

  return true;
}

static string get_cmd_string(int argc, char* argv[]) {
  string cmd;
  for (int i = 0; i < argc; ++i) {
    cmd.append(argv[i]).append(" ");
  }
  return cmd;
}

static int process_mkseed(int argc, char* argv[]) {
  int option = -1;
  int index;
  const char* short_opt = "r:C:W:p:s:lH";
  const struct option long_opt[] = {
      { "torrent",       required_argument, NULL, 'r' },
      { "cluster",       required_argument, NULL, 'C' },
      { "webseed",       required_argument, NULL, 'W' },
      { "path",          required_argument, NULL, 'p' },
      { "size",          required_argument, NULL, 's' },
      { "link",          no_argument,       NULL, 'l' },
      { "hash",          no_argument,       NULL, 'H' },
      { "besthash",      no_argument,       NULL,  1  },
      { "include-files", required_argument, NULL,  2  },
      { "exclude-files", required_argument, NULL,  3  },
      { NULL,            no_argument,       NULL,  0  }
  };

  string torrent_file_path;
  make_torrent_args_t make_torrent_args;
  while ((option = getopt_long(argc - 1, &argv[1], short_opt, long_opt, &index)) != -1) {
    switch (option) {
      case 'r':
        torrent_file_path = trim_path(complete_full_path(optarg));
        break;

      case 'C':
        if (!parse_cluster_config(optarg, make_torrent_args.cluster_config)) {
          fprintf(stderr, "Invalid cluster uri.\n");
          exit(1);
        }
        break;

      case 'W':
        make_torrent_args.web_seeds.push_back(optarg);
        break;

      case 'p':
        make_torrent_args.full_path = trim_path(complete_full_path(optarg));
        break;

      case 's':
        make_torrent_args.piece_size = atoi(optarg) * 1024;
        if (make_torrent_args.piece_size < 0 || make_torrent_args.piece_size > 20 * 1024 * 1024) {
          fprintf(stderr, "Piece size(%dKB) not valid range(1K ~ 20M).\n",
                  make_torrent_args.piece_size / 1024);
          exit(1);
        }
        break;

      case 'l':
        make_torrent_args.flags |= make_torrent_args_t::SYMLINKS;
        break;

      case 'H':
        make_torrent_args.flags |= make_torrent_args_t::FILE_HASH;
        break;

      case 1:
        make_torrent_args.flags |= make_torrent_args_t::BEST_HASH;
        break;

      case 2:
        try {
          make_torrent_args.include_regex.push_back(boost::regex(optarg));
        } catch (boost::bad_expression &e) {
          fprintf(stderr, "include files regex fail: %s\n", e.what());
          exit(1);
        }
        break;

      case 3:
        try {
          make_torrent_args.exclude_regex.push_back(boost::regex(optarg));
        } catch (boost::bad_expression &e) {
          fprintf(stderr, "exclude files regex fail: %s\n", e.what());
          exit(1);
        }
        break;

      case ':':
      case '?':
      default:
        exit(1);
    }
  }

  if (make_torrent_args.full_path.empty()) {
    fprintf(stderr, "Path is empty or incorrect.\n");
    exit(1);
  }
  if (torrent_file_path.empty()) {
    if (make_torrent_args.full_path.find('*') == string::npos) {
      torrent_file_path = complete_full_path(file_name(make_torrent_args.full_path) + ".torrent");
    } else {
      torrent_file_path = complete_full_path(
          file_name(parent_path(make_torrent_args.full_path)) + ".torrent");
    }
  }

  string infohash;
  vector<char> torrent;
  if (!make_torrent(make_torrent_args, &infohash, &torrent)) {
    fprintf(stderr, "Make seed failed.\n");
    return 2;
  }
  if (!WriteToFile(torrent_file_path, torrent)) {
    fprintf(stderr, "write torrent file failed.\n");
    return 3;
  }
  fprintf(stdout, "%s\n", torrent_file_path.c_str());
  fprintf(stdout, "%s\n", infohash.c_str());
  fprintf(stdout, "infohash:%s\n", infohash.c_str());
  return 0;
}

static int process_dump(int argc, char* argv[]) {
  int option = -1;
  int index;
  const char* short_opt = "r:";
  const struct option long_opt[] = {
      { "torrent", required_argument, NULL, 'r' },
      { NULL,      no_argument,       NULL,  0  }
  };

  string torrent_path;
  while ((option = getopt_long(argc - 1, &argv[1], short_opt, long_opt, &index)) != -1) {
    switch (option) {
      case 'r':
        torrent_path = trim_path(complete_full_path(optarg));
        break;

      case ':':
      case '?':
      default:
        exit(1);
    }
  }

  if (torrent_path.empty()) {
    fprintf(stderr, "Torrent path is empty or incorrect.\n");
    exit(1);
  }
  if (!dump_torrent(torrent_path)) {
    fprintf(stderr, "Dump torrent failed.\n");
    exit(1);
  }
  return 0;
}

static int process_add(int argc, char* argv[]) {
  DownloadConfigure *configure = LazySingleton<DownloadConfigure>::instance();
  message::AddTask add_task_params;
  message::Task *task_info = add_task_params.mutable_task();
  task_info->set_type(message::NOCHECK_TASK);
  task_info->set_cmd(get_cmd_string(argc, argv));
  message::TaskOptions *options = add_task_params.mutable_options();
  options->set_upload_limit(configure->upload_limit());
  options->set_max_conections(configure->connection_limit());

  int option = -1;
  int index;
  const char* short_opt = "r:i:p:n:S:u:c:";
  const struct option long_opt[] = {
      { "torrent",   required_argument, NULL, 'r' },
      { "infohash",  required_argument, NULL, 'i' },
      { "path",      required_argument, NULL, 'p' },
      { "fullpath",  required_argument, NULL, 'n' },
      { "seedtime",  required_argument, NULL, 'S' },
      { "uplimit",   required_argument, NULL, 'u' },
      { "connlimit", required_argument, NULL, 'c' },
      { "seed",      no_argument,       NULL,  1  },
      { "nocheck",   no_argument,       NULL,  2  },
      { "tracker",   required_argument, NULL,  3  },
      { NULL,        no_argument,       NULL,  0  }
  };

  while ((option = getopt_long(argc - 1, &argv[1], short_opt, long_opt, &index)) != -1) {
    switch (option) {
      case 'r':
        task_info->set_torrent_path(trim_path(complete_full_path(optarg)));
        break;

      case 'i': {
        string info_hash = optarg;
        if (info_hash.length() != 40) {
          fprintf(stderr, "Invalid infohash, must be length 40 for hex\n");
          exit(1);
        }
        task_info->set_infohash(info_hash);
        break;
      }

      case 'p':
        if (task_info->new_name().empty()) {
          task_info->set_save_path(trim_path(complete_full_path(optarg)));
        }
        break;

      case 'n': {
        string tmp_str = trim_path(complete_full_path(optarg));
        string path, newname;
        if (!path_slipt(tmp_str, &path, &newname)) {
          fprintf(stderr, "newname(%s) not correct!\n", tmp_str.c_str());
        }
        task_info->set_save_path(path);
        task_info->set_new_name(newname);
        break;
      }

      case 'S': {
        int seeding_time = atoi(optarg);
        if (seeding_time < -1) {
          fprintf(stderr, "Seeding time(%d) less than -1\n", seeding_time);
          exit(1);
        }
        task_info->set_seeding_time(seeding_time);
        break;
      }

      case 'u': {
        int upload_limit = ReadableToBytes(string(optarg));
        if (upload_limit < 0 || upload_limit > 1000 * 1024 * 1024) {
          fprintf(stderr, "Upload limit invalid.\n");
          exit(1);
        }
        options->set_upload_limit(upload_limit);
        break;
      }

      case 'c': {
        int max_connections = atoi(optarg);
        if (max_connections < 0 || max_connections > 65535) {
          fprintf(stderr, "Max connections invalid.\n");
          exit(1);
        }
        options->set_max_conections(max_connections);
        break;
      }

      case 1:
        task_info->set_type(message::SEEDING_TASK);
        break;

      case 2:
        task_info->set_type(message::NOCHECK_TASK);
        break;

      case 3: {
        vector<string> v;
        SliptStringToVector(optarg, ":", &v);
        if (v.size() != 2) {
          fprintf(stderr, "tracker not correct: %s\n", optarg);
          exit(1);
        }
        int port = atoi(v[1].c_str());
        if (port < 1024 || port > 65535) {
          fprintf(stderr, "tracker not correct: %s\n", optarg);
          exit(1);
        }
        message::Host tracker;
        tracker.set_ip(v[0]);
        tracker.set_port(port);
        task_info->add_trackers()->CopyFrom(tracker);
        break;
      }

      case ':':
      case '?':
      default:
        exit(1);
    }
  }

  int64_t taskid = -1;
  BBTSClient client;
  if (task_info->type() == message::SEEDING_TASK) {
    client.SetMaxRetryTimes(5);
  }
  if (client.CreateTask(add_task_params, &taskid) != 0) {
    fprintf(stderr, "create task failed.\n");
    return 2;
  }
  fprintf(stdout, "%ld\n", taskid);
  fprintf(stdout, "taskid:%ld\n", taskid);
  return 0;
}

static int process_serve(int argc, char* argv[]) {
  DownloadConfigure *configure = LazySingleton<DownloadConfigure>::instance();
  message::AddTask add_task_params;
  message::Task *task_info = add_task_params.mutable_task();
  task_info->set_type(message::SEEDING_TASK);
  task_info->set_cmd(get_cmd_string(argc, argv));
  message::TaskOptions *options = add_task_params.mutable_options();
  options->set_upload_limit(configure->upload_limit());
  options->set_max_conections(configure->connection_limit());

  int option = -1;
  int index;
  const char* short_opt = "r:p:S:u:c:s:lH";
  const struct option long_opt[] = {
      { "torrent",       required_argument, NULL, 'r' },
      { "path",          required_argument, NULL, 'p' },
      { "seedtime",      required_argument, NULL, 'S' },
      { "uplimit",       required_argument, NULL, 'u' },
      { "connlimit",     required_argument, NULL, 'c' },
      { "size",          required_argument, NULL, 's' },
      { "link",          no_argument,       NULL, 'l' },
      { "hash",          no_argument,       NULL, 'H' },
      { "seed",          no_argument,       NULL,  1  },
      { "nocheck",       no_argument,       NULL,  2  },
      { "besthash",      no_argument,       NULL,  3  },
      { "include-files", required_argument, NULL,  4  },
      { "exclude-files", required_argument, NULL,  5  },
      { "tracker",       required_argument, NULL,  6  },
      { NULL,            no_argument,       NULL,  0  }
  };

  string torrent_file_path;
  make_torrent_args_t make_torrent_args;
  while ((option = getopt_long(argc - 1, &argv[1], short_opt, long_opt, &index)) != -1) {
    switch (option) {
      case 'r':
        torrent_file_path = trim_path(complete_full_path(optarg));
        break;

      case 'p':
        make_torrent_args.full_path = trim_path(complete_full_path(optarg));
        break;

      case 'S': {
        int seeding_time = atoi(optarg);
        if (seeding_time < -1) {
          fprintf(stderr, "Seeding time(%d) less than -1\n", seeding_time);
          exit(1);
        }
        task_info->set_seeding_time(seeding_time);
        break;
      }

      case 'u': {
        int upload_limit = ReadableToBytes(string(optarg));
        if (upload_limit < 0 || upload_limit > 1000 * 1024 * 1024) {
          fprintf(stderr, "Upload limit invalid.\n");
          exit(1);
        }
        options->set_upload_limit(upload_limit);
        break;
      }

      case 'c': {
        int max_connections = atoi(optarg);
        if (max_connections < 0 || max_connections > 65535) {
          fprintf(stderr, "Max connections invalid.\n");
          exit(1);
        }
        options->set_max_conections(max_connections);
        break;
      }

      case 's':
        make_torrent_args.piece_size = atoi(optarg) * 1024;
        if (make_torrent_args.piece_size < 0 || make_torrent_args.piece_size > 20 * 1024 * 1024) {
          fprintf(stderr, "Piece size(%dKB) not valid range(1K ~ 20M).\n",
                  make_torrent_args.piece_size / 1024);
          exit(1);
        }
        break;

      case 'l':
        make_torrent_args.flags |= make_torrent_args_t::SYMLINKS;
        break;

      case 'H':
        make_torrent_args.flags |= make_torrent_args_t::FILE_HASH;
        break;

      case 1:
        task_info->set_type(message::SEEDING_TASK);
        break;

      case 2:
        task_info->set_type(message::NOCHECK_TASK);
        break;

      case 3:
        make_torrent_args.flags |= make_torrent_args_t::BEST_HASH;
        break;

      case 4:
        try {
          make_torrent_args.include_regex.push_back(boost::regex(optarg));
        } catch (boost::bad_expression &e) {
          fprintf(stderr, "include files regex fail: %s\n", e.what());
          exit(1);
        }
        break;

      case 5:
        try {
          make_torrent_args.exclude_regex.push_back(boost::regex(optarg));
        } catch (boost::bad_expression &e) {
          fprintf(stderr, "exclude files regex fail: %s\n", e.what());
          exit(1);
        }
        break;

      case 6: {
        vector<string> v;
        SliptStringToVector(optarg, ":", &v);
        if (v.size() != 2) {
          fprintf(stderr, "tracker not correct: %s\n", optarg);
          exit(1);
        }
        int port = atoi(v[1].c_str());
        if (port < 1024 || port > 65535) {
          fprintf(stderr, "tracker not correct: %s\n", optarg);
          exit(1);
        }
        message::Host tracker;
        tracker.set_ip(v[0]);
        tracker.set_port(port);
        task_info->add_trackers()->CopyFrom(tracker);
        break;
      }

      case ':':
      case '?':
      default:
        exit(1);
    }
  }

  if (!task_info->has_seeding_time()) {
    fprintf(stderr, "Seeding time not set.\n");
    return 1;
  }

  if (make_torrent_args.full_path.empty()) {
    fprintf(stderr, "Path is empty or incorrect.\n");
    return 1;
  }

  string infohash;
  vector<char> torrent;
  if (!make_torrent(make_torrent_args, &infohash, &torrent)) {
    fprintf(stderr, "Make seed failed.\n");
    return 2;
  }
  fprintf(stdout, "%s\n", torrent_file_path.c_str());
  fprintf(stdout, "%s\n", infohash.c_str());
  if (torrent_file_path.empty()) {
    task_info->set_data(&torrent[0], torrent.size());
    task_info->set_infohash(infohash);
  } else {
    if (!WriteToFile(torrent_file_path, torrent)) {
      fprintf(stderr, "write torrent file failed.\n");
      return 3;
    }
    task_info->set_torrent_path(torrent_file_path);
  }
  string parent_dir, filename;
  path_slipt(make_torrent_args.full_path, &parent_dir, &filename);
  string::size_type pos = filename.find('*');
  if (pos != string::npos) {
    parent_dir = parent_path(parent_dir);
  }
  task_info->set_save_path(parent_dir);

  int64_t taskid = -1;
  BBTSClient client;
  client.SetMaxRetryTimes(5);
  if (client.CreateTask(add_task_params, &taskid) != 0) {
    fprintf(stderr, "create task failed.\n");
    return 3;
  }
  fprintf(stdout, "%ld\n", taskid);
  fprintf(stdout, "infohash:%s\n", infohash.c_str());
  fprintf(stdout, "taskid:%ld\n", taskid);
  return 0;
}

static bool AddIpFilterRule(const string &ip_range, int flags, ip_filter *filter) {
  vector<string> v;
  bbts::SliptStringToVector(ip_range, "-", &v);
  size_t size = v.size();
  if (size == 0 || size > 2) {
    return false;
  }

  error_code ec;
  address start_address;
  start_address = address::from_string(v[0], ec);
  if (ec) {
    fprintf(stderr, "address(%s) not correct: %s\n", v[0].c_str(), ec.message().c_str());
    return false;
  }

  address last_address;
  if (size == 2) {
    last_address = address::from_string(v[1], ec);
    if (ec) {
      fprintf(stderr, "address(%s) not correct: %s\n", v[1].c_str(), ec.message().c_str());
      return false;
    }
  } else {
    last_address = start_address;
  }
  filter->add_rule(start_address, last_address, flags);
  return true;
}

static bool ParseIpFilter(const string &filter_string, int flags, ip_filter *filter) {
  vector<string> ip_range_vector;
  bbts::SliptStringToVector(filter_string, ",", &ip_range_vector);
  for (vector<string>::iterator it = ip_range_vector.begin(); it != ip_range_vector.end(); ++it) {
    if (!AddIpFilterRule(*it, flags, filter)) {
      return false;
    }
  }
  return true;
}

static void ClearIpFilter(int flags, ip_filter *filter) {
  error_code ec;
  filter->add_rule(address::from_string("0.0.0.0", ec),
                   address::from_string("255.255.255.255", ec),
                   !flags);
}

static int process_download(int argc, char* argv[]) {
  down_params_t params;
  DownloadConfigure *configure = LazySingleton<DownloadConfigure>::instance();
  params.hang_timeout = configure->download_timeout();
  params.cmd = get_cmd_string(argc, argv);

  int option = -1;
  int index;
  const char* short_opt = "r:i:U:C:W:p:n:S:P:d:u:c:O:";
  const struct option long_opt[] = {
      { "torrent",       required_argument, NULL, 'r' },
      { "infohash",      required_argument, NULL, 'i' },
      { "url",           required_argument, NULL, 'U' },
      { "cluster",       required_argument, NULL, 'C' },
      { "webseed",       required_argument, NULL, 'W' },
      { "path",          required_argument, NULL, 'p' },
      { "fullpath",      required_argument, NULL, 'n' },
      { "seedtime",      required_argument, NULL, 'S' },
      { "port",          required_argument, NULL, 'P' },
      { "downlimit",     required_argument, NULL, 'd' },
      { "uplimit",       required_argument, NULL, 'u' },
      { "connlimit",     required_argument, NULL, 'c' },
      { "timeout",       required_argument, NULL, 'O' },
      { "offline",       no_argument,       NULL,  1  },
      { "debug",         no_argument,       NULL,  2  },
      { "nodebug",       no_argument,       NULL,  3  },
      { "progress",      no_argument,       NULL,  4  },
      { "hang-timeout",  required_argument, NULL,  5  },
      { "include-files", required_argument, NULL,  6  },
      { "exclude-files", required_argument, NULL,  7  },
      { "continue",      no_argument,       NULL,  8  },
      { "tmp-files",     no_argument,       NULL,  9  },
      { "unix-socket",   required_argument, NULL,  10 },
      { "save-torrent",  required_argument, NULL,  11 },
      { "sndbuf",        required_argument, NULL,  12 },
      { "rcvbuf",        required_argument, NULL,  13 },
      { "hdfs-thread",   required_argument, NULL,  14 },
      { "ip-white",      required_argument, NULL,  15 },
      { "ip-black",      required_argument, NULL,  16 },
      { "mask",          required_argument, NULL,  17 },
      { "numwant",       required_argument, NULL,  18 },
      { "ainterval",     required_argument, NULL,  19 },
      { "tracker",       required_argument, NULL,  20 },
      { "tracker-failed-quit", no_argument, NULL,  21 },
      { "classpath",     required_argument, NULL,  22 },
      { "pre-allocate",  no_argument,       NULL,  23 },
      { "use-dio-read",  no_argument,       NULL,  24 },
      { "use-dio-write", no_argument,       NULL,  25 },
      { "d-allocate",    required_argument, NULL,  26 },
      { "ignore-hdfs-error", no_argument,   NULL,  28 },
      { "mem-limit",     required_argument, NULL,  29 },
      { "conf",          required_argument, NULL,  30 },
      { NULL,            no_argument,       NULL,  0  }
  };

  bool ip_filter_white = false;
  bool is_use_group = false;
  string group_conf_file;
  DownloadParam download_param;
  while ((option = getopt_long(argc - 1, &argv[1], short_opt, long_opt, &index)) != -1) {
    switch (option) {
      case 'r':
        params.torrent_path = trim_path(complete_full_path(optarg));
        break;

      case 'i':
        params.infohash = optarg;
        if (params.infohash.length() != 40) {
          fprintf(stderr, "Invalid infohash, must be length 40 for hex\n");
          exit(1);
        }
        break;

      case 'U':
        params.torrent_url = optarg;
        break;

      case 'C': {
        if (!parse_cluster_config(optarg, params.cluster_config)) {
          fprintf(stderr, "Invalid cluster uri.\n");
          exit(1);
        }
        break;
      }

      case 'W':
        params.web_seeds.push_back(string(optarg));
        break;

      case 'p':
        if (params.new_name.empty()) {
          params.save_path = trim_path(complete_full_path(optarg));
        }
        break;

      case 'n': {
        string tmp_str = trim_path(complete_full_path(optarg));
        if (!path_slipt(tmp_str, &params.save_path, &params.new_name)) {
          fprintf(stderr, "newname(%s) not correct!\n", tmp_str.c_str());
          exit(1);
        }
        break;
      }

      case 'P': {
        int start, end;
        string range = optarg;
        string::size_type pos = range.find_first_of(",; \t");
        if (pos != string::npos) {
          start = atoi(range.substr(0, pos).c_str());
          end = atoi(range.substr(pos + 1).c_str());
        } else {
          start = atoi(range.c_str());
          end = start + 100;
          if (end > 65535) {
            end = 65535;
          }
        }
        if (start < 1025 || start > 65535 || end < 1025 || end > 65535) {
          fprintf(stderr, "Can't bind port less than 1024 or big than 65535\n");
          exit(1);
        }
        params.listen_port_range = std::make_pair(start, end);
        break;
      }

      case 'S':
        params.seeding_time = atoi(optarg);
        if (params.seeding_time < -1) {
          fprintf(stderr, "Seeding time(%d) less than -1\n", params.seeding_time);
          exit(1);
        }
        break;

      case 'd': {
        int download_limit = ReadableToBytes(string(optarg));
        if (download_limit < 0 || download_limit > 2000 * 1024 * 1024) {
          fprintf(stderr, "Download limit(%s) invalid.\n", optarg);
          exit(1);
        }
        configure->set_download_limit(download_limit);
        download_param.download_limit = download_limit;
        break;
      }

      case 'u': {
        int upload_limit = ReadableToBytes(string(optarg));
        if (upload_limit < 0 || upload_limit > 2000 * 1024 * 1024) {
          fprintf(stderr, "Upload limit(%s) invalid.\n", optarg);
          exit(1);
        }
        configure->set_upload_limit(upload_limit);
        download_param.upload_limit = upload_limit;
        break;
      }

      case 'c': {
        int connections_limit = atoi(optarg);
        if (connections_limit < 0 || connections_limit > 65535) {
          fprintf(stderr, "connections limit(%d) invalid.\n", connections_limit);
          exit(1);
        }
        configure->set_connection_limit(connections_limit);
        download_param.connections_limit = connections_limit;
        break;
      }

      case 'O':
        params.timeout = atoi(optarg);
        if (params.timeout < -1) {
          fprintf(stderr, "Timeout(%d) less than -1\n", params.timeout);
          exit(1);
        }
        break;

      case 1:
        params.listen_port_range = std::make_pair(12000 + get_random_num() % 1500, 13999);
        break;

      case 2:
        params.debug = true;
        break;

      case 3:
        params.debug = false;
        break;

      case 4:
        params.print_progress = true;
        break;

      case 5:
        params.hang_timeout = atoi(optarg);
        if (params.hang_timeout < 0) {
          fprintf(stderr, "hang check timeout(%d) less than -1\n", params.hang_timeout);
          exit(1);
        }
        break;

      case 6:
        try {
          download_param.include_regex.push_back(optarg);
          params.include_regex.push_back(boost::regex(optarg));
        } catch (boost::bad_expression &e) {
          fprintf(stderr, "include files regex fail: %s\n", e.what());
          exit(1);
        }
        params.patition_download = true;
        break;

      case 7:
        try {
          download_param.exclude_regex.push_back(optarg);
          params.exclude_regex.push_back(boost::regex(optarg));
        } catch (boost::bad_expression &e) {
          fprintf(stderr, "exclude files regex fail: %s\n", e.what());
          exit(1);
        }
        params.patition_download = true;
        break;

      case 8:
        params.need_save_resume = true;
        break;

      case 9:
        params.need_down_to_tmp_first = true;
        break;

      case 10:
        params.control_path = optarg;
        break;

      case 11:
        params.save_torrent_path = trim_path(complete_full_path(optarg));
        break;

      case 12: {
        int bufsize = atoi(optarg);
        if (bufsize < 0 || bufsize > 4096) {
          fprintf(stderr, "send buffer size invalid\n");
          exit(1);
        }
        configure->set_send_socket_buffer_size(bufsize * 1024);
        break;
      }

      case 13: {
        int bufsize = atoi(optarg);
        if (bufsize < 0 || bufsize > 4096) {
          fprintf(stderr, "recv buffer size invalid\n");
          exit(1);
        }
        configure->set_recv_socket_buffer_size(bufsize * 1024);
        break;
      }

      case 14: {
        int cluster_thread_num = atoi(optarg);
        if (cluster_thread_num < 0 || cluster_thread_num > 32) {
          fprintf(stderr, "hdfs thread num invalid\n");
          exit(1);
        }
        configure->set_cluster_thread_num(cluster_thread_num);
        download_param.cluster_thread_num = cluster_thread_num;
        break;
      }

      case 15: {
        if (!ip_filter_white) {
          ClearIpFilter(0, &params.filter);
          ip_filter_white = true;
        }
        if (!ParseIpFilter(optarg, 0, &params.filter)) {
          exit(1);
        }
        break;
      }

      case 16: {
        if (ip_filter_white) {
          ClearIpFilter(ip_filter::blocked, &params.filter);
          ip_filter_white = false;
        }
        if (!ParseIpFilter(optarg, ip_filter::blocked, &params.filter)) {
          exit(1);
        }
        break;
      }

      case 17: {
        string ip = GetLocalHostIPString();
        string range;
        if (!ParseMaskToIPRange(ip, optarg, &range)) {
          exit(1);
        }
        if (!ip_filter_white) {
          ClearIpFilter(0, &params.filter);
          ip_filter_white = true;
        }
        if (!AddIpFilterRule(range, 0, &params.filter)) {
          exit(1);
        }
        break;
      }

      case 18: {
        int num_want = atoi(optarg);
        if (num_want < 0 || num_want > 10000) {
          fprintf(stderr, "num want invalid\n");
          exit(1);
        }
        configure->set_peers_num_want(num_want);
        break;
      }

      case 19: {
        int max_annouce_interval = atoi(optarg);
        if (max_annouce_interval < 10 || max_annouce_interval > 3600) {
          fprintf(stderr, "the value of max announce interval is not valid\n");
          exit(1);
        }
        configure->set_max_announce_interval(max_annouce_interval);
        break;
      }

      case 20: {
        vector<string> v;
        SliptStringToVector(optarg, ":", &v);
        if (v.size() != 2) {
          fprintf(stderr, "tracker not correct: %s\n", optarg);
          exit(1);
        }
        int port = atoi(v[1].c_str());
        if (port < 1024 || port > 65535) {
          fprintf(stderr, "tracker not correct: %s\n", optarg);
          exit(1);
        }
        params.trackers.push_back(std::make_pair(v[0], port));
        break;
      }

      case 21: {
        params.quit_by_tracker_failed = true;
        break;
      }

      case 22: {
        configure->set_class_path(optarg);
        break;
      }

      case 23: {
        params.storage_pre_allocate = true;
        break;
      }

      case 24: {
        params.use_dio_read = true;
        break;
      }

      case 25: {
        params.use_dio_write = true;
        break;
      }

      case 26: {
        int dynamic_allocate = atoi(optarg);
        if (dynamic_allocate < 0 || dynamic_allocate > 300) {
          fprintf(stderr, "dynamic allocate must between 0 and 300");
        }
        // 0 use default value
        params.dynamic_allocate = atoi(optarg);
        break;
      }

      case 28: {
        params.ignore_hdfs_error = true;
        break;
      }

      case 29: {
       params.mem_limit = atoi(optarg);
       if (params.mem_limit < 100) {
         fprintf(stderr, "memery limit %dM less than 100M\n", params.mem_limit);
         exit(1);
       }
       break;
      }

      case 30: {
        is_use_group = true;
        group_conf_file = optarg;
        break;
      }

      case ':':
      case '?':
        exit(1);
    }
  }

  if (params.patition_download && params.dynamic_allocate >= 0) {
    fprintf(stderr, "Use dynamic allocate mode, but not support partition download currently!\n");
    exit(1);
  }

  if (is_use_group) {
    GroupClient client;
    if (client.Initialize(group_conf_file) != 0) {
      fprintf(stderr, "Initialize group client failed\n");
      exit(-1);
    }
    TurnDownParamToGroup(params, &download_param);
    if (client.ProcessAdd(download_param) != 0) {
      fprintf(stderr, "add download task failed!\n");
      exit(-1);
    }
    client.Shutdown();
    fprintf(stdout, "add download task success!\n");
    exit(0);
  }

  Downloader downloader(params);
  exit(downloader.Download());
}

static int process_list(int argc, char* argv[]) {
  map<string, int> attr_name_map;
  attr_name_map["taskid"] = 0;
  attr_name_map["status"] = 1;
  attr_name_map["progress"] = 2;
  attr_name_map["infohash"] = 3;
  attr_name_map["user"] = 4;
  attr_name_map["group"] = 5;
  attr_name_map["cmd"] = 6;
  attr_name_map["error"] = 7;
  attr_name_map["download"] = 8;
  attr_name_map["upload"] = 9;
  attr_name_map["downrate"] = 10;
  attr_name_map["uprate"] = 11;
  attr_name_map["peers"] = 12;
  attr_name_map["seeds"] = 13;
  attr_name_map["savepath"] = 14;

  const char *default_attr[] = {
      "taskid",
      "status",
      "progress",
      "infohash",
      "user",
      "cmd",
  };
  vector<string> attr_set;
  attr_set.insert(attr_set.end(),
                  default_attr,
                  default_attr + sizeof(default_attr) / sizeof(char *));

  int option = -1;
  int index;
  const char* short_opt = "t:a:i:w";
  const struct option long_opt[] = {
      { "task",        required_argument, NULL, 't' },
      { "attr",        required_argument, NULL, 'a' },
      { "infohash",    required_argument, NULL, 'i' },
      { "wait",        no_argument,       NULL, 'w' },
      { "noheader",    no_argument,       NULL,  1  },
      { "unix-socket", required_argument, NULL,  2  },
      { "conf",        required_argument, NULL,  3  },
      { "history",     no_argument,       NULL,  4  },
      { NULL,          no_argument,       NULL,  0  }
  };

  bool no_header = false;
  vector<int64_t> taskids;
  string unix_socket_path;
  bool is_use_group = false;
  bool is_show_history = false;
  bool is_group_daemon = false;  // default don't use daemon
  std::string group_conf_file;
  std::string infohash;

  while ((option = getopt_long(argc - 1, &argv[1], short_opt, long_opt, &index)) != -1) {
    switch (option) {
      case 't': {
        int64_t taskid = atoll(optarg);
        if (taskid < 0) {
          fprintf(stderr, "Task id(%ld) invalid\n", taskid);
          exit(-1);
        }
        taskids.push_back(taskid);
        break;
      }

      case 'a':
        parse_attributes(attr_name_map, optarg, &attr_set);
        break;

      case 'i':
        is_use_group = true;
        infohash = optarg;
        break;

      case 'w':
        is_use_group = true;
        is_group_daemon = true;
        break;

      case 1:
        no_header = true;
        break;

      case 2:
        unix_socket_path = optarg;
        break;

      case 3:
        is_use_group = true;
        group_conf_file = optarg;
        break;

      case 4:
        is_use_group = true;
        is_show_history = true;
        break;

      case ':':
      case '?':
        exit(1);
    }
  }

  // check if use bbts-group
  if (is_use_group) {
    if (group_conf_file.empty()) {
      fprintf(stderr, "Use group conf, but conf file is empty!\n");
      return 4;
    }

    GroupClient client;
    if (client.Initialize(group_conf_file) != 0) {
      fprintf(stderr, "Initialize group client failed\n");
      return 4;
    }

    bool is_full = false;
    if (infohash.empty()) {
      is_full = true;
    }

    std::vector<TaskStatus> task_status_list;
    if (client.ProcessList(infohash, is_full, is_show_history, is_group_daemon) != 0) {
      fprintf(stderr, "List infohash[%s], is_full:%d failed!\n", infohash.c_str(), is_full);
      return 4;
    }

    client.Shutdown();

    return 0;
  }

  BBTSClient client;
  if (!unix_socket_path.empty()) {
    client.set_endpoint(unix_socket_path);
    client.NoSendUcred();
  }
  message::BatchListRes res;
  if (client.ListTasks(taskids, &res) != 0) {
    fprintf(stderr, "List tasks failed.\n");
    return 3;
  }

  if (!no_header) {
    print_task_status_header(attr_name_map, attr_set);
  }
  bool success = true;
  for (int i = 0; i < res.status_size(); ++i) {
    const message::TaskStatus &task_status = res.status(i);
    if (task_status.status() != message::TaskStatus::UNKNOW) {
      print_task_status(attr_name_map, res.status(i), attr_set);
    } else {
      fprintf(stdout, "%12ld: %s\n", task_status.task().taskid(), task_status.error().c_str());
      success = false;
    }
  }
  if (success) {
    return 0;
  } else {
    return 2;
  }
}

static int process_batch_ctrl(int argc, char* argv[], message::BatchCtrl::ctrl_t ctrl_type) {
  int option = -1;
  int index;
  const char* short_opt = "t:i:";
  const struct option long_opt[] = {
      { "task",     required_argument, NULL, 't' },
      { "infohash", required_argument, NULL, 'i' },
      { "conf",     required_argument, NULL,  1 },
      { "all",      no_argument,       NULL,  2  },
      { NULL,       no_argument,       NULL,  0  }
  };

  bool ctrl_all = false;
  bool is_use_group = false;
  std::string group_conf_file;
  std::string infohash;
  vector<int64_t> taskids;
  while ((option = getopt_long(argc - 1, &argv[1], short_opt, long_opt, &index)) != -1) {
    switch (option) {
      case 't': {
        int64_t taskid = atoll(optarg);
        if (taskid < 0) {
          fprintf(stderr, "Task id(%ld) invalid\n", taskid);
          exit(1);
        }
        taskids.push_back(taskid);
        break;
      }

      case 'i':
        is_use_group = true;
        infohash = optarg;
        break;

      case 1:
        is_use_group = true;
        group_conf_file = optarg;
        break;

      case 2:
        ctrl_all = true;
        break;

      case ':':
      case '?':
        exit(1);
    }
  }

  if (!ctrl_all && taskids.empty() && infohash.empty()) {
    fprintf(stderr, "Must specify some taskid or infohash or --all.\n");
    exit(1);
  }

  if (is_use_group) {
    if (group_conf_file.empty()) {
      fprintf(stderr, "Use group conf, but conf file is empty!\n");
      return 3;
    }

    GroupClient client;
    if (client.Initialize(group_conf_file) != 0) {
      fprintf(stderr, "Initialize group client failed\n");
      return 4;
    }

    // get ctrl type
    ControlTaskType::type type = ControlTaskType::UNKNOWN;
    switch (ctrl_type) {
      case message::BatchCtrl::CANCEL:
        type = ControlTaskType::CANCEL;
        break;
      case message::BatchCtrl::PAUSE:
        type = ControlTaskType::PAUSE;
        break;
      case message::BatchCtrl::RESUME:
        type = ControlTaskType::RESUME;
        break;
      default:
        break;
    }

    if (client.ProcessControl(infohash, type, ctrl_all) != 0) {
      fprintf(stderr, "control infohash[%s], is_all:%d failed!\n", infohash.c_str(), ctrl_all);
      return 5;
    }

    if (!ctrl_all) {
      fprintf(stdout, "control infohash[%s] success.\n", infohash.c_str());
    } else {
      fprintf(stdout, "control tasks success.\n");
    }

    client.Shutdown();
    return 0;
  }

  BBTSClient client;
  message::BatchCtrlRes res;
  if (client.BatchControl(taskids, ctrl_type, &res) != 0) {
    fprintf(stderr, "batch control failed.\n");
    return 1;
  }
  bool success = true;
  for (int i = 0; i < res.tasks_size(); ++i) {
    const message::TaskRes &task_res = res.tasks(i);
    const message::BaseRes &base_res = task_res.res();
    if (base_res.ret_code() == 0) {
      fprintf(stdout, "ctrl task %ld success\n", task_res.taskid());
    } else {
      fprintf(stderr, "ctrl task %ld fail: %s\n", task_res.taskid(), base_res.fail_msg().c_str());
      success = false;
    }
  }
  if (success) {
    return 0;
  } else {
    return 2;
  }
}

static int process_getopt(int argc, char* argv[]) {
  int option = -1;
  int index;
  const char* short_opt = "t:i:";
  const struct option long_opt[] = {
      { "task",        required_argument, NULL, 't' },
      { "infohash",    required_argument, NULL, 'i' },
      { "unix-socket", required_argument, NULL,  1  },
      { "conf",        required_argument, NULL,  2  },
      { NULL,          no_argument,       NULL,  0  }
  };

  int64_t taskid = -1;
  string unix_socket_path;
  bool is_use_group = false;
  std::string group_conf_file;
  std::string infohash;
  while ((option = getopt_long(argc - 1, &argv[1], short_opt, long_opt, &index)) != -1) {
    switch (option) {
      case 't': {
        taskid = atoll(optarg);
        if (taskid < 0) {
          fprintf(stderr, "Task id(%ld) invalid\n", taskid);
          exit(1);
        }
        break;
      }

      case 'i':
        is_use_group = true;
        infohash = optarg;
        break;

      case 1:
        unix_socket_path = optarg;
        break;

      case 2:
        is_use_group = true;
        group_conf_file = optarg;
        break;

      case ':':
      case '?':
        exit(1);
    }
  }

  if (is_use_group) {
    GroupClient client;
    if (client.Initialize(group_conf_file) != 0) {
      fprintf(stderr, "Initialize group client failed\n");
      return 1;
    }

    if (client.ProcessGetOption(infohash) != 0) {
      fprintf(stderr, "get task option failed!\n");
      return 2;
    }
    client.Shutdown();
    fprintf(stdout, "get task option success!\n");
    return 0;
  }

  BBTSClient client;
  if (!unix_socket_path.empty()) {
    client.set_endpoint(unix_socket_path);
    client.NoSendUcred();
  }
  if (taskid > 0 || !unix_socket_path.empty()) {
    message::TaskOptions options;
    options.set_taskid(taskid);
    options.set_upload_limit(0);
    options.set_download_limit(0);
    options.set_max_conections(0);
    if (client.GetTaskOptions(&options) != 0) {
      fprintf(stderr, "Get task(%ld) opt failed.\n", taskid);
      return 1;
    }
    string upload_limit, download_limit;
    BytesToReadable(options.upload_limit(), &upload_limit);
    BytesToReadable(options.download_limit(), &download_limit);
    fprintf(stdout, "download limit: %s/s\nupload limit: %s/s\nmax connections: %d\n",
            download_limit.c_str(),upload_limit.c_str(), options.max_conections());
  } else {
    message::AgentOptions options;
    options.set_upload_limit(0);
    options.set_max_conections(0);
    if (client.GetAgentOptions(&options) != 0) {
      fprintf(stderr, "Get agent opt failed.\n");
      return 1;
    }

    string upload_limit;
    BytesToReadable(options.upload_limit(), &upload_limit);
    fprintf(stdout, "bind port: %d\nupload limit: %s/s\nmax connections: %d\nsupport hadoop: %s\n",
            options.bind_port(),
            upload_limit.c_str(),
            options.max_conections(),
            ParseSyms() ? "true" : "false");
  }
  return 0;
}

static int process_setopt(int argc, char* argv[]) {
  int option = -1;
  int index;
  const char* short_opt = "t:d:u:c:i:";
  const struct option long_opt[] = {
      { "task",        required_argument, NULL, 't' },
      { "infohash",    required_argument, NULL, 'i' },
      { "downlimit",   required_argument, NULL, 'd' },
      { "uplimit",     required_argument, NULL, 'u' },
      { "connlimit",   required_argument, NULL, 'c' },
      { "unix-socket", required_argument, NULL,  1  },
      { "conf",        required_argument, NULL,  2  },
      { NULL,          no_argument,       NULL,  0  }
  };

  int64_t taskid = -1;
  int download_limit = 0;
  int upload_limit = 0;
  int max_connections = 0;
  string unix_socket_path;

  TaskOptions options;
  bool is_use_group = false;
  std::string group_conf_file;

  while ((option = getopt_long(argc - 1, &argv[1], short_opt, long_opt, &index)) != -1) {
    switch (option) {
      case 't': {
        taskid = atoll(optarg);
        if (taskid < 0) {
          fprintf(stderr, "Task id(%ld) invalid\n", taskid);
          exit(1);
        }
        break;
      }
      
      case 'i':
        is_use_group = true;
        options.__set_infohash(optarg);
        break;

      case 'd':
        download_limit = ReadableToBytes(string(optarg));
        if (download_limit < 0 || download_limit > 1000 * 1024 * 1024) {
          fprintf(stderr, "Download limit invalid.\n");
          exit(1);
        }
        options.__set_download_limit(download_limit);
        break;

      case 'u':
        upload_limit = ReadableToBytes(string(optarg));
        if (upload_limit < 0 || upload_limit > 1000 * 1024 * 1024) {
          fprintf(stderr, "Upload limit invalid.\n");
          exit(1);
        }
        options.__set_upload_limit(upload_limit);
        break;

      case 'c':
        max_connections = atoi(optarg);
        if (max_connections < 0 || max_connections > 65535) {
          fprintf(stderr, "Max connections invalid.\n");
          exit(1);
        }
        options.__set_connections_limit(max_connections);
        break;

      case 1:
        unix_socket_path = optarg;
        break;

      case 2:
        is_use_group = true;
        group_conf_file = optarg;
        break;

      case ':':
      case '?':
      default:
        exit(1);
    }
  }

  if (is_use_group) {
    GroupClient client;
    if (client.Initialize(group_conf_file) != 0) {
      fprintf(stderr, "Initialize group client failed\n");
      return 1;
    }

    if (client.ProcessSetOption(options) != 0) {
      fprintf(stderr, "set task option failed!\n");
      return 2;
    }
    client.Shutdown();
    fprintf(stdout, "set task option success!\n");
    return 0;
  }

  int ret = 0;
  BBTSClient client;
  if (!unix_socket_path.empty()) {
    client.set_endpoint(unix_socket_path);
    client.NoSendUcred();
  }
  if (taskid > 0 || !unix_socket_path.empty()) {
    message::TaskOptions options;
    options.set_taskid(taskid);
    if (download_limit) {
      options.set_download_limit(download_limit);
    }
    if (upload_limit) {
      options.set_upload_limit(upload_limit);
    }
    if (max_connections) {
      options.set_max_conections(max_connections);
    }
    ret = client.SetTaskOptions(options);
  } else {
    message::AgentOptions options;
    if (upload_limit) {
      options.set_upload_limit(upload_limit);
    }
    if (max_connections) {
      options.set_max_conections(max_connections);
    }
    ret = client.SetAgentOptions(options);
  }
  if (ret != 0) {
    fprintf(stderr, "Set opt failed.\n");
    return 1;
  }
  return 0;
}

static int process_rmfiles(int argc, char* argv[]) {
  int option = -1;
  int index;
  const char* short_opt = "r:p:n:l";
  const struct option long_opt[] = {
      { "torrent",  required_argument, NULL, 't' },
      { "path",     required_argument, NULL, 'p' },
      { "fullpath", required_argument, NULL, 'n' },
      { "link",     no_argument,       NULL, 'l' },
      { "in",       no_argument,       NULL,  1  },
      { "not-in",   no_argument,       NULL,  2  },
      { NULL,       no_argument,       NULL,  0  }
  };

  bool flags_is_set = false;
  delete_files_args_t delete_args;
  while ((option = getopt_long(argc - 1, &argv[1], short_opt, long_opt, &index)) != -1) {
    switch (option) {
      case 'r':
        delete_args.torrent_path = trim_path(complete_full_path(optarg));
        break;

      case 'p':
        if (delete_args.new_name.empty()) {
          delete_args.save_path = trim_path(complete_full_path(optarg));
        }
        break;

      case 'n': {
        string tmp_str = trim_path(complete_full_path(optarg));
        if (!path_slipt(tmp_str, &delete_args.save_path, &delete_args.new_name)) {
          fprintf(stderr, "newname(%s) not correct!\n", tmp_str.c_str());
          exit(1);
        }
        break;
      }

      case 'l':
        delete_args.flags |= delete_files_args_t::SYMLINKS;
        break;

      case 1:
        flags_is_set = true;
        delete_args.flags |= delete_files_args_t::DELETE_IN_TORRENT;
        break;

      case 2:
        flags_is_set = true;
        delete_args.flags &= ~delete_files_args_t::DELETE_IN_TORRENT;
        break;

      case ':':
      case '?':
        exit(1);
    }
  }
  if (!flags_is_set) {
    fprintf(stderr, "you must set delete in or not in torrent flag\n");
    exit(2);
  }
  if (delete_args.torrent_path.empty() || delete_args.save_path.empty()) {
    fprintf(stderr, "Not set torrent file or save path or rm type.\n");
    return 1;
  }
  if (!DeleteFilesByTorrent(delete_args)) {
    return 1;
  }
  return 0;
}

static int process(int argc, char* argv[]) {
  if (argc < 2) {
    print_help();
    exit(1);
  }

  string opstr = argv[1];
  if (opstr == "add") {
    return process_add(argc, argv);
  } else if (opstr == "serve") {
    return process_serve(argc, argv);
  } else if (opstr == "down" || opstr == "sdown") {
    return process_download(argc, argv);
  } else if (opstr == "list" || opstr == "status") {
    return process_list(argc, argv);
  } else if (opstr == "mkseed") {
    return process_mkseed(argc, argv);
  } else if (opstr == "cancel") {
    return process_batch_ctrl(argc, argv, message::BatchCtrl::CANCEL);
  } else if (opstr == "pause") {
    return process_batch_ctrl(argc, argv, message::BatchCtrl::PAUSE);
  } else if (opstr == "resume") {
    return process_batch_ctrl(argc, argv, message::BatchCtrl::RESUME);
  } else if (opstr == "setopt") {
    return process_setopt(argc, argv);
  } else if (opstr == "getopt") {
    return process_getopt(argc, argv);
  } else if (opstr == "rmfiles") {
    return process_rmfiles(argc, argv);
  } else if (opstr == "dump") {
    return process_dump(argc, argv);
  } else if (opstr == "help") {
    if (argc < 3) {
      print_help();
      exit(0);
    } else if (strncmp("attr", argv[2], 5) == 0) {
      print_attr_help();
      exit(0);
    } else {
      fprintf(stderr, "unknow help %s\n", argv[2]);
      print_help();
      exit(1);
    }
  } else if (opstr == "bbts-agent") {
    extern int bbts_agent(int argc, char* argv[]);
    return bbts_agent(argc, argv);
  } else if (opstr == "bbts-group") {
    extern int bbts_group(int argc, char* argv[]);
    return bbts_group(argc, argv);
  } else if (opstr == "-v" || opstr == "--version") {
    print_version(argv[0]);
    exit(0);
  } else if (opstr == "-h" || opstr == "--help") {
    print_help();
    exit(0);
  } else {
    fprintf(stderr, "no this operation: %s\n", opstr.c_str());
    print_help();
    exit(1);
  }
}

static bool InitConfigure() {
  // get running root path
  string root_path;
  if (!bbts::get_root_path(&root_path)) {
    fprintf(stderr, "Get program root path failed.\n");
    return false;
  }

  DownloadConfigure *configure = LazySingleton<DownloadConfigure>::instance();
  if (!LoadPBConf(root_path + "/conf/download.conf", configure)) {
    fprintf(stderr, "load download.conf failed.\n");
    return false;
  }
  configure->set_download_limit(configure->download_limit() * 1024 * 1024);
  configure->set_upload_limit(configure->upload_limit() * 1024 * 1024);
  configure->set_cache_size(configure->cache_size() * 64);
  configure->set_max_queued_disk_bytes(configure->max_queued_disk_bytes() * 1024 * 1024);
  configure->set_send_buffer_watermark(configure->send_buffer_watermark() * 1024 * 1024);
  configure->set_send_buffer_low_watermark(configure->send_buffer_low_watermark() * 1024);
  configure->set_send_socket_buffer_size(configure->send_socket_buffer_size() * 1024);
  configure->set_recv_socket_buffer_size(configure->recv_socket_buffer_size() * 1024);
  configure->set_max_metadata_size(configure->max_metadata_size() * 1024 * 1024);
  BBTSClient::set_default_endpoint(configure->socket_file());
  return true;
}

int main(int argc, char* argv[]) {
  if (!InitConfigure()) {
    exit(1);
  }

  GOOGLE_PROTOBUF_VERIFY_VERSION;
  int ret = process(argc, argv);
  google::protobuf::ShutdownProtobufLibrary();
  return ret;
}

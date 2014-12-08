/***************************************************************************
 *
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/
#include "group_client.h"

#include "bbts/number_util.h"

namespace bbts {
namespace group {

GroupClient::GroupClient() {
  // do nothing
}

GroupClient::~GroupClient() {
  // do nothing
}

int GroupClient::Initialize(const std::string &conf_file) {
  if (conf_file.empty()) {
    fprintf(stderr, "conf file must not be empty!\n");
    return -1;
  }

  if (LoadGroupConfig(conf_file, &group_config_) != 0) {
    fprintf(stderr, "parse config file(%s) failed.\n", conf_file.c_str());
    return -1;
  }

  // because the thrift unix_socket path should not be longer than 256, so change dir
  std::string data_path = group_config_.absolute_path() + group_config_.group_data_path();
  if (chdir(data_path.c_str()) != 0) {
    fprintf(stderr, "chdir to conf data path failed: %s\n", data_path.c_str());
    return -1;
  }

  cred_.uid = static_cast<int32_t>(getuid());
  cred_.pid = static_cast<int32_t>(getpid());
  cred_.gid = static_cast<int32_t>(getgid());

  socket_.reset(new TSocket(group_config_.unix_socket().c_str())); 
  socket_->setConnTimeout(3000);
  socket_->setSendTimeout(3000);
  socket_->setRecvTimeout(5000);
  transport_.reset(new TFramedTransport(socket_));
  protocol_.reset(new TCompactProtocol(transport_));
  rpc_client_.reset(new GroupManagerServiceClient(protocol_));

  return 0;
}

void GroupClient::Shutdown() {
  try {
    transport_->close();
  } catch (const TException &ex) {
    fprintf(stderr, "close transport failed\n");
  }
}

int GroupClient::ProcessAdd(const DownloadParam &down_params) {
  bool is_ok = false;
  int retry = kDefaultRpcRetry;
  GeneralResponse response;

  while (retry-- > 0) {
    try {
      transport_->open();
      rpc_client_->AddGroupTask(response, down_params, cred_);
      is_ok = true;
      break;
    } catch (const TException &ex) {
      fprintf(stderr, "Add to Manager error:%s, retry:%d\n", ex.what(), retry);
      sleep(1);
    }
  }

  if (!is_ok || response.ret_code != 0) {
    fprintf(stderr, "response from group manager failed:%s\n", response.message.c_str());
    return -1;
  }

  fprintf(stdout, "%s\n", response.message.c_str());
  return 0;
}

int GroupClient::ProcessSetOption(const TaskOptions &options) {
  bool is_ok = false;
  int retry = kDefaultRpcRetry;
  GeneralResponse response;

  while (retry-- > 0) {
    try {
      transport_->open();
      rpc_client_->SetTaskOption(response, options);
      is_ok = true;
      break;
    } catch (const TException &ex) {
      fprintf(stderr, "set option to Manager error:%s, retry:%d\n", ex.what(), retry);
      sleep(1);
    }
  }

  if (!is_ok || response.ret_code != 0) {
    fprintf(stderr, "response from group manager failed:%s\n", response.message.c_str());
    return -1;
  }

  return 0;
}

int GroupClient::ProcessGetOption(const std::string &infohash) {
  bool is_ok = false;
  int retry = kDefaultRpcRetry;
  std::vector<TaskOptions> task_options_list;

  while (retry-- > 0) {
    try {
      transport_->open();
      rpc_client_->GetTaskOption(task_options_list, infohash);
      is_ok = true;
      break;
    } catch (const TException &ex) {
      fprintf(stderr, "get option to Manager error:%s, retry:%d\n", ex.what(), retry);
      sleep(1);
    }
  }

  if (!is_ok ) {
    fprintf(stderr, "get task[%s] status failed\n", infohash.c_str());
    return -1;
  }

  // print list
  std::vector<TaskOptions>::iterator it = task_options_list.begin();
  for (; it != task_options_list.end(); ++it) {
    PrintOneTaskOptions(*it);
  }
  return 0;
}

void GroupClient::PrintOneTaskOptions(const TaskOptions &options) {
  if (options.infohash.empty()) {
    // group options
    fprintf(stdout, "group options:\n");
  } else {
    // normal options
    fprintf(stdout, "infohash:%s\n", options.infohash.c_str());
  }

  std::string download_limit, upload_limit;
  BytesToReadable(options.download_limit, &download_limit);
  BytesToReadable(options.upload_limit, &upload_limit);
  fprintf(stdout, "download_limit:%s/s\n", download_limit.c_str());
  fprintf(stdout, "upload_limit:%s/s\n", upload_limit.c_str());
  fprintf(stdout, "max connections:%d\n", options.connections_limit);
}

int GroupClient::GetTaskStatus(
    const std::string &infohash,
    const bool is_full,
    std::vector<TaskStatus> &task_status_list) {
  bool is_ok = false;
  int retry = kDefaultRpcRetry;

  while (retry-- > 0) {
    try {
      transport_->open();
      rpc_client_->GetTaskStatus(task_status_list, infohash, is_full);
      is_ok = true;
      break;
    } catch (const TException &ex) {
      fprintf(stderr, "get task[%s] status error:%s, retry:%d\n", infohash.c_str(), ex.what(), retry);
      sleep(1);
    }
  }

  if (!is_ok) {
    fprintf(stderr, "get task[%s] status failed\n", infohash.c_str());
    return -1;
  }

  return 0;
}

int GroupClient::ProcessControl(
    const std::string &infohash,
    const ControlTaskType::type type,
    const bool is_all) {
  bool is_ok = false;
  int retry = kDefaultRpcRetry;
  GeneralResponse response;

  while (retry-- > 0) {
    try {
      transport_->open();
      rpc_client_->ControlTask(response, infohash, is_all, type, cred_);
      is_ok = true;
      break;
    } catch (const TException &ex) {
      fprintf(stderr, "Control error:%s, retry:%d\n", ex.what(), retry);
      sleep(1);
    }
  }

  if (!is_ok || response.ret_code != 0) {
    fprintf(stderr, "response from group manager failed:%s\n", response.message.c_str());
    return -1;
  }

  fprintf(stdout, "response from group manager success\n");
  return 0;
}

void GroupClient::PrintOneTaskStatus(
    const TaskStatus &task_status,
    const bool is_show_history) {
  if (!is_show_history && task_status.is_task_finished) {
    return;
  }

  std::string downrate, uprate, download, upload;
  BytesToReadable(task_status.download_rate, &downrate);
  BytesToReadable(task_status.upload_rate, &uprate);
  BytesToReadable(task_status.payload_downloaded, &download);
  BytesToReadable(task_status.payload_uploaded, &upload);
  downrate += "/s";
  uprate += "/s";
  char buf[128] = {'\0'};
  snprintf(buf, 128, "%5.2f%%", task_status.progress * 100);

  // the length shouldn't be longer than this...
  fprintf(stdout, "%-41s%-9s%-8s%-12s%-12s%-9s%-9s%-10s%-10s\n",
          task_status.infohash.c_str(), task_status.state_string.c_str(), buf,
          downrate.c_str(), uprate.c_str(), download.c_str(), upload.c_str(),
          GetTimeStringByTimestamp(task_status.start_time).c_str(),
          GetTimeStringByTimestamp(task_status.end_time).c_str());
}

std::string GroupClient::GetTaskStatusString(
    const TaskStatus &task_status,
    const bool is_show_history) {
  if (!is_show_history && task_status.is_task_finished) {
    // don't show history, and task is finished, so don't print
    return std::string("");
  }

  std::string downrate, uprate, download, upload;
  BytesToReadable(task_status.download_rate, &downrate);
  BytesToReadable(task_status.upload_rate, &uprate);
  BytesToReadable(task_status.payload_downloaded, &download);
  BytesToReadable(task_status.payload_uploaded, &upload);
  downrate += "/s";
  uprate += "/s";
  char progress_buf[128] = {'\0'};
  snprintf(progress_buf, 128, "%5.2f%%", task_status.progress * 100);

  char buf[2048] = {'\0'};   // 2M may be enough
  snprintf(buf, 2048, "%s%-41s%-9s%-8s%-12s%-12s%-9s%-9s%-10s%-10s\n",
           "\33[10000D\33[K", task_status.infohash.c_str(), task_status.state_string.c_str(),
           progress_buf, downrate.c_str(), uprate.c_str(), download.c_str(), upload.c_str(),
           GetTimeStringByTimestamp(task_status.start_time).c_str(),
           GetTimeStringByTimestamp(task_status.end_time).c_str());

  // general progress line...
  int progress_size = static_cast<int>(task_status.progress * kDefaultFinishedSize);
  if (progress_size > kDefaultFinishedSize) {
    progress_size = kDefaultFinishedSize;
  }
  std::string final_str;
  final_str.assign(buf);
  final_str.append("\33[10000D\33[K");

  for (int i = 0; i < progress_size; ++i) {
    final_str.append("=");
  }
  for(int i = progress_size; i < kDefaultFinishedSize; ++i) {
    final_str.append(".");
  }
  final_str.append("\n");

  return final_str;
}

void GroupClient::PrintListTitle() {
  fprintf(stdout, "%-41s%-9s%-9s%-12s%-12s%-9s%-9s%-10s%-10s\n",
          "infohash", "status", "progress", "downrate", "uprate",
          "download", "upload", "starttime", "endtime");
}

void GroupClient::ClearList(int line_count) {
  for (int i = 0; i < line_count; ++i) {
    fprintf(stdout, "\33[1B\33[10000D\33[K");
  }
  fprintf(stdout, "\33[%dA\33[10000D\33[K", line_count);
}

std::string GroupClient::GetAllTaskSumInfo(
    const std::vector<TaskStatus> &task_status_list) {
  int64_t total_download_rate  = 0;
  int64_t total_upload_rate    = 0;
  int64_t total_download       = 0;
  int64_t total_upload         = 0;
  std::vector<TaskStatus>::const_iterator it = task_status_list.begin();
  for (; it != task_status_list.end(); ++it) {
    if (it->is_task_finished) {
      continue;
    }

    total_download_rate += it->download_rate;
    total_upload_rate   += it->upload_rate;
    total_download      += it->payload_downloaded;
    total_upload        += it->payload_uploaded;
  }

  std::string downrate, uprate, download, upload;
  BytesToReadable(total_download_rate, &downrate);
  BytesToReadable(total_upload_rate, &uprate);
  BytesToReadable(total_download, &download);
  BytesToReadable(total_upload, &upload);
  downrate += "/s";
  uprate += "/s";

  char buf[2048] = {'\0'};   // 2M may be enough
  snprintf(buf, 2048, "\33[10000D\33[K\n\33[10000D\33[KTotal Download Rate:%s\tTotal Upload Rate:%s\tTotal Download:%s\tTotal Upload:%s\n\33[K",
           downrate.c_str(), uprate.c_str(), download.c_str(), upload.c_str()); 
  return std::string(buf);
}

int GroupClient::ProcessList(
    const std::string &infohash,
    const bool is_full,
    const bool is_show_history,
    const bool is_daemon) {
  std::vector<TaskStatus> task_status_list;

  PrintListTitle();
  if (!is_daemon) {
    // use no daemon, so just print one line and return
    if (GetTaskStatus(infohash, is_full, task_status_list) != 0) {
      fprintf(stderr, "get task status error, infohash[%s], is_full[%d]\n", infohash.c_str(), is_full);
      return -1;
    }
    std::vector<TaskStatus>::iterator it = task_status_list.begin();
    for (; it != task_status_list.end(); ++it) {
      PrintOneTaskStatus(*it, is_show_history);
    }
    return 0;
  }

  int line_count = 0;
  while(true) {
    if (GetTaskStatus(infohash, is_full, task_status_list) != 0) {
      fprintf(stderr, "get task status error, infohash[%s], is_full[%d]\n", infohash.c_str(), is_full);
      break;
    }

    // clear history print
    if (line_count != 0) {
      ClearList(line_count);
    }

    bool has_not_finished = false;
    std::string print_string;
    line_count = 0;
    std::vector<TaskStatus>::iterator it = task_status_list.begin();
    for (; it != task_status_list.end(); ++it) {
      std::string task_status_string;
      if (!it->is_task_finished || it->state == -1) {
        has_not_finished = true;
      }
      task_status_string = GetTaskStatusString(*it, is_show_history);

      if (task_status_string.empty()) {
        continue;
      }
      print_string.append(task_status_string);
      // each task status string need 2 line
      line_count += 2;
    }  // for

    print_string.append(GetAllTaskSumInfo(task_status_list));
    line_count += 2;
  
    if (!has_not_finished) {
      fprintf(stdout, "%s\33[%dA", print_string.c_str(), line_count);
      break;
    }
    fprintf(stdout, "%s\33[%dA", print_string.c_str(), line_count);
    sleep(1);
  }  // while

  fprintf(stdout, "\33[%dB\n\33[10000D\33[KAll task finished\n\33[K\33[0m", line_count+1);
  return 0;
}

}  // namespace group
}  // name bbts

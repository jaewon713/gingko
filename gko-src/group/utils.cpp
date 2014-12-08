/***************************************************************************
 *
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/
#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>

#include "bbts/path.h"
#include "bbts/number_util.h"
#include "bbts/log.h"

#include "conf_range.h"

using apache::thrift::transport::TMemoryBuffer;
using apache::thrift::protocol::TBinaryProtocol;

using namespace bbts;

namespace bbts {
namespace group {

int SetLimitIntValue(int min, int max, int value) {
  if (value > max) {
    value = max;
  } else if (value < min) {
    value = min;
  }

  return value;
}

int LoadGroupConfig(
    const std::string &conf_path,
    ::bbts::GroupConfig *group_config) {
  if (conf_path.empty() || group_config == NULL) {
    return -1;
  }

  // load configure file
  int conf_fd = open(conf_path.c_str(), O_RDONLY);
  if (conf_fd < 0) {
    fprintf(stderr, "open conf file(%s) failed, errno(%d): %s\n",
               conf_path.c_str(), errno, strerror(errno));
    return -1;
  }
  ::google::protobuf::io::FileInputStream conf_stream(conf_fd);
  conf_stream.SetCloseOnDelete(true);
  if (!::google::protobuf::TextFormat::Parse(&conf_stream, group_config)) {
    fprintf(stderr, "prase conf file(%s) failed!\n", conf_path.c_str());
    return -1;
  }

  // set absolute_path
  std::string absolute_path = ::bbts::complete_full_path(::bbts::parent_path(conf_path));
  group_config->set_absolute_path(absolute_path);

  // check if param out of range, so terrible...
  group_config->set_download_rate_limit(SetLimitIntValue(kMinDownloadLimit, kMaxDownloadLimit, group_config->download_rate_limit()));
  group_config->set_upload_rate_limit(SetLimitIntValue(kMinUploadLimit, kMaxUploadLimit, group_config->upload_rate_limit()));
  group_config->set_connection_limit(SetLimitIntValue(kMinConnectionLimit, kMaxConnectionLimit, group_config->connection_limit()));

  group_config->set_max_metadata_size(SetLimitIntValue(kMinMetadataSize, kMaxMetadataSize, group_config->max_metadata_size()));
  group_config->set_peers_num_want(SetLimitIntValue(kMinPeersNumWant, kMaxPeersNumWant, group_config->peers_num_want()));
  group_config->set_seed_announce_interval(SetLimitIntValue(kMinSeedAnnounceInterval, kMaxSeedAnnounceInterval, group_config->seed_announce_interval()));
  group_config->set_min_reconnect_time(SetLimitIntValue(kMinReconnectTime, kMaxReconnectTime, group_config->min_reconnect_time()));
  group_config->set_max_queued_disk_bytes(SetLimitIntValue(kMinQueuedDiskBytes, kMaxQueuedDiskBytes, group_config->max_queued_disk_bytes()));
  group_config->set_max_out_request_queue(SetLimitIntValue(kMinOutRequestQueue, kMaxOutRequestQueue, group_config->max_out_request_queue()));
  group_config->set_max_allowed_in_request_queue(SetLimitIntValue(kMinAllowedInRequestQueue, kMaxAllowedInRequestQueue, group_config->max_allowed_in_request_queue()));
  group_config->set_whole_pieces_threshold(SetLimitIntValue(kMinWholePiecesThreshold, kMaxWholePiecesThreshold, group_config->whole_pieces_threshold()));
  group_config->set_request_queue_time(SetLimitIntValue(kMinRequestQueueTime, kMaxRequestQueueTime, group_config->request_queue_time()));
  group_config->set_cache_size(SetLimitIntValue(kMinCacheSize, kMaxCacheSize, group_config->cache_size()));
  group_config->set_cache_expiry(SetLimitIntValue(kMinCacheExpiry, kMaxCacheExpiry, group_config->cache_expiry()));
  group_config->set_read_cache_line_size(SetLimitIntValue(kMinReadCacheLineSize, kMaxReadCacheLineSize, group_config->read_cache_line_size()));
  group_config->set_write_cache_line_size(SetLimitIntValue(kMinWriteCacheLineSize, kMaxWriteCacheLineSize, group_config->write_cache_line_size()));
  group_config->set_file_pool_size(SetLimitIntValue(kMinFilePoolSize, kMaxFilePoolSize, group_config->file_pool_size()));
  group_config->set_send_buffer_watermark(SetLimitIntValue(kMinSendBufferWatermark, kMaxSendBufferWatermark, group_config->send_buffer_watermark()));
  group_config->set_send_buffer_low_watermark(SetLimitIntValue(kMinSendBufferLowWatermark, kMaxSendBufferLowWatermark, group_config->send_buffer_low_watermark()));
  group_config->set_send_socket_buffer_size(SetLimitIntValue(kMinSendSocketBufferSize, kMaxSendSocketBufferSize, group_config->send_socket_buffer_size()));
  group_config->set_recv_socket_buffer_size(SetLimitIntValue(kMinRecvSocketBufferSize, kMaxRecvSocketBufferSize, group_config->recv_socket_buffer_size()));
  group_config->set_active_seeds(SetLimitIntValue(kMinActiveSeeds, kMaxActiveSeeds, group_config->active_seeds()));
  group_config->set_active_limit(SetLimitIntValue(kMinActiveLimit, kMaxActiveLimit, group_config->active_limit()));
  group_config->set_active_downloads(SetLimitIntValue(kMinActiveDownloads, kMaxActiveDownloads, group_config->active_downloads()));
  group_config->set_listen_port(SetLimitIntValue(kMinListenPort, kMaxListenPort, group_config->listen_port()));
  group_config->set_peer_connection_timeout(SetLimitIntValue(kMinPeerConnectionTimeout, kMaxPeerConnectionTimeout, group_config->peer_connection_timeout()));

  // change unit format
  group_config->set_download_rate_limit(group_config->download_rate_limit() * 1024 * 1024);
  group_config->set_upload_rate_limit(group_config->upload_rate_limit() * 1024 * 1024);
  group_config->set_max_metadata_size(group_config->max_metadata_size() * 1024 * 1024);
  group_config->set_max_queued_disk_bytes(group_config->max_queued_disk_bytes() * 1024 * 1024);
  group_config->set_cache_size(group_config->cache_size() * 1024 * 1024);
  group_config->set_send_buffer_watermark(group_config->send_buffer_watermark() * 1024 * 1024);

  group_config->set_send_buffer_low_watermark(group_config->send_buffer_low_watermark() * 1024);
  group_config->set_send_socket_buffer_size(group_config->send_socket_buffer_size() * 1024);
  group_config->set_recv_socket_buffer_size(group_config->recv_socket_buffer_size() * 1024);

  return 0;
}

int TurnDownParamToGroup(const struct down_params_t &params,
                         DownloadParam *group_down_param) {
  if (group_down_param == NULL) {
    return -1;
  }

  group_down_param->seeding_time = params.seeding_time;
  group_down_param->timeout = params.timeout;
  group_down_param->hang_timeout = params.hang_timeout;
  group_down_param->mem_limit = params.mem_limit;
  group_down_param->debug = params.debug;
  group_down_param->print_progress = params.print_progress;
  group_down_param->patition_download = params.patition_download;
  group_down_param->need_save_resume = params.need_save_resume;
  group_down_param->need_down_to_tmp_first = params.need_down_to_tmp_first;
  group_down_param->quit_by_tracker_failed = params.quit_by_tracker_failed;
  group_down_param->storage_pre_allocate = params.storage_pre_allocate;
  group_down_param->dynamic_allocate = params.dynamic_allocate;
  group_down_param->use_dio_read = params.use_dio_read;
  group_down_param->use_dio_write = params.use_dio_write;
  group_down_param->ignore_hdfs_error = params.ignore_hdfs_error;
  group_down_param->torrent_path = params.torrent_path;
  group_down_param->infohash = params.infohash;
  group_down_param->torrent_url = params.torrent_url;
  group_down_param->new_name = params.new_name;
  group_down_param->save_path = params.save_path;

  ClusterParam cluster_config;
  // ignore return value
  TurnClusterConfigEntryToClusterParam(params.cluster_config, &cluster_config);
  group_down_param->cluster_config = cluster_config;

  group_down_param->web_seeds = params.web_seeds;

  ListenPortRange listen_port_range;
  listen_port_range.start_port = params.listen_port_range.first;
  listen_port_range.end_port = params.listen_port_range.second;
  group_down_param->listen_port_range = listen_port_range;

  group_down_param->control_path = params.control_path;
  group_down_param->save_torrent_path = params.save_torrent_path;
  group_down_param->cmd = params.cmd;
  //group_down_param->filter = params.filter;
  TrackersPair pair;
  std::vector<std::pair<std::string, int> >::const_iterator it = params.trackers.begin();
  for (; it != params.trackers.end(); ++it) {
    pair.name = it->first;
    pair.id = it->second;
    group_down_param->trackers.push_back(pair);
  }

  return 0;
}

int TurnStdListToRegexList (
    const std::vector<std::string> &regex_string,
    std::vector<boost::regex> *regex_output) {
  if (regex_output == NULL) {
    return -1;
  }
  
  std::vector<std::string>::const_iterator it = regex_string.begin();
  for (; it != regex_string.end(); ++it) {
    regex_output->push_back(boost::regex(*it));
  }
  return 0;
}

int TurnTrackersListToStdList(
    const std::vector<TrackersPair> &trackers,
    std::vector<std::pair<std::string, int> > *std_trackers) {
  if (std_trackers == NULL) {
    return -1;
  }

  std::vector<TrackersPair>::const_iterator it = trackers.begin();
  for (; it != trackers.end(); ++it) {
    std_trackers->push_back(std::make_pair(it->name, it->id));
  }

  return 0;
}

int TurnClusterConfigEntryToClusterParam(
    const struct libtorrent::cluster_config_entry cluster_config,
    ClusterParam *cluster_param) {
  if (cluster_param == NULL) {
    return -1;
  }

  cluster_param->host = cluster_config.host;
  cluster_param->port = cluster_config.port;
  cluster_param->user = cluster_config.user;
  cluster_param->passwd = cluster_config.passwd;
  cluster_param->prefix_path = cluster_config.prefix_path;

  return 0;
}

int TurnClusterParamToClusterConfigEntry(
    const ClusterParam &cluster_param,
    struct libtorrent::cluster_config_entry *cluster_config) {
  if (cluster_config == NULL) {
    return -1;
  }

  cluster_config->host = cluster_param.host;
  cluster_config->port = cluster_param.port;
  cluster_config->user = cluster_param.user;
  cluster_config->passwd = cluster_param.passwd;
  cluster_config->prefix_path = cluster_param.prefix_path;

  return 0;
}

std::string GeneralParamString(const DownloadParam &params) {
  std::string identify;
  if (!params.torrent_path.empty()) {
    identify = params.torrent_path;
  } else if (!params.infohash.empty()) {
    identify = params.infohash;
  } else if (!params.torrent_url.empty()) {
    identify = params.torrent_url;
  } else {
    identify = "invalid params";
  }

  return identify;
}

void LogProgress(const libtorrent::torrent_status &ts) {
  std::string downrate, uprate, download, upload, infohash;
  BytesToReadable(ts.download_rate, &downrate);
  BytesToReadable(ts.upload_rate, &uprate);
  BytesToReadable(ts.total_payload_download, &download);
  BytesToReadable(ts.total_payload_upload, &upload);
  infohash = libtorrent::to_hex(ts.info_hash.to_string());
  TRACE_LOG("[%s] status: %d, progress: %5.2f%%, downrate: %8s/s, uprate: %8s/s, download: %12s, upload: %12s",
             infohash.c_str(), ts.state, (ts).progress * 100, downrate.c_str(), uprate.c_str(), download.c_str(), upload.c_str());
}

std::string GetTimeStringByTimestamp(int64_t timestamp) {
  std::string time_str;
  if (timestamp <= 0) {
    //time_str.assign("0000-00-00 00:00:00");
    time_str.assign("00:00:00");
    return time_str;
  }

  time_t t;
  t = static_cast<time_t>(timestamp);
  struct tm *p = localtime(&t);
  char s[128];  // 128 bytes should be enough
  //strftime(s, sizeof(s), "%Y-%m-%d %H:%M:%S", p);
  strftime(s, sizeof(s), "%H:%M:%S", p);
  time_str.assign(s);

  return time_str;
}

void SerializedDownParam(
    const DownloadParam &param,
    std::string *serialized_str) {
  assert(serialized_str != NULL);
  serialized_str->clear();

  boost::shared_ptr<TMemoryBuffer> str_buff(new TMemoryBuffer());
  boost::shared_ptr<TBinaryProtocol> binary_protocol(new TBinaryProtocol(str_buff));

  param.write(binary_protocol.get());
  serialized_str->assign(str_buff->getBufferAsString());
}

void UnSerializedDownParam(
    const std::string serialized_str,
    DownloadParam *param) {
  assert(param != NULL);

  boost::shared_ptr<TMemoryBuffer> str_buff(new TMemoryBuffer());
  boost::shared_ptr<TBinaryProtocol> binary_protocol(new TBinaryProtocol(str_buff));

  str_buff->resetBuffer((uint8_t *)serialized_str.data(), serialized_str.length());
  param->read(binary_protocol.get());
}

void SerializedTaskStatus(
    const TaskStatus &status,
    std::string *serialized_str) {
  assert(serialized_str != NULL);
  serialized_str->clear();

  boost::shared_ptr<TMemoryBuffer> str_buff(new TMemoryBuffer());
  boost::shared_ptr<TBinaryProtocol> binary_protocol(new TBinaryProtocol(str_buff));

  status.write(binary_protocol.get());
  serialized_str->assign(str_buff->getBufferAsString());
}

void UnSerializedTaskStatus(
    const std::string serialized_str,
    TaskStatus *status) {
  assert(status != NULL);

  boost::shared_ptr<TMemoryBuffer> str_buff(new TMemoryBuffer());
  boost::shared_ptr<TBinaryProtocol> binary_protocol(new TBinaryProtocol(str_buff));

  str_buff->resetBuffer((uint8_t *)serialized_str.data(), serialized_str.length());
  status->read(binary_protocol.get());
}

std::string ClientRpcRetCodeToString(
    ClientRpcStatus ret_code,
    const std::string &identify_string,
    const std::string type) {
  std::string message(type);
  message += " task[" + identify_string + "]: ";

  switch (ret_code) {
    case kClientRpcSuccess:
      message.append("success");
      break;

    case kClientRpcErrorParams:
      message.append("error params");
      break;

    case kClientRpcCredFailed:
      message.append("cred check failed");
      break;

    case kClientRpcAddTaskExist:
      message.append("task alread in group");
      break;

    case kClientRpcAddTaskCreateFailed:
      message.append("create task failed");
      break;

    case kClientRpcTaskNotExist:
      message.append("task not found");
      break;

    case kClientRpcSetOptFailed:
      message.append("task set options failed");
      break;

    default:
      message.append("some unknown error happens");
      break;
  }

  return message;
}

}  // namespace group
}  // namespace bbts

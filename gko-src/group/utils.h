/***************************************************************************
 *
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/
#ifndef OP_OPED_NOAH_TOOLS_BBTS_GROUP_UTILS_H_
#define OP_OPED_NOAH_TOOLS_BBTS_GROUP_UTILS_H_

#include <string>

#include <boost/regex.hpp>
#include <libtorrent/torrent_info.hpp>

#include "bbts/RegexMatch.hpp"

#include "bbts/tool/Downloader.h"
#include "bbts/log.h"
#include "define.h"
#include "GroupConf.pb.h"
#include "gen-cpp/GroupManagerService.h"

using ::bbts::tool::down_params_t;

namespace bbts {
namespace group {

// load config file into GroupConfig object
int LoadGroupConfig(const std::string &conf_file, ::bbts::GroupConfig *config);

// turn down_params_t to DownloadParam
int TurnDownParamToGroup(const struct down_params_t &params,
                         DownloadParam *group_down_param);

// turn DownloadParam to down_param_t
int TurnGroupParmToDownParam(const DownloadParam &group_down_param,
                             struct down_params_t &params);

// turn cluster_config_entry to ClusterParam
int TurnClusterConfigEntryToClusterParam(
    const struct libtorrent::cluster_config_entry cluster_config,
    ClusterParam *cluster_param);

// turn ClusterParam to cluster_config_entry
int TurnClusterParamToClusterConfigEntry(
    const ClusterParam &cluster_param,
    struct libtorrent::cluster_config_entry *cluster_config);

// turn tracker list to stdlist
int TurnTrackersListToStdList(
    const std::vector<TrackersPair> &trackers,
    std::vector<std::pair<std::string, int> > *std_trackers);

// turn stdlist to regex list
int TurnStdListToRegexList(const std::vector<std::string> &regex_string,
                           std::vector<boost::regex> *regex_output);

// log progress of torrent_status
void LogProgress(const libtorrent::torrent_status &ts);

// show download param identify string, such as infohash or torrent path
std::string GeneralParamString(const DownloadParam &params);

// turn timestamp to timestring
std::string GetTimeStringByTimestamp(int64_t timestamp);

// serialize a DownloadParam to a string
void SerializedDownParam(const DownloadParam &param,
                         std::string *serialized_str);

// Unserialize a DownloadParam from string
void UnSerializedDownParam(const std::string serialized_str,
                           DownloadParam *param);

// serialize a TaskStatus to a string
void SerializedTaskStatus(const TaskStatus &status,
                          std::string *serialized_str);

// Unserialize a TaskStatus from string
void UnSerializedTaskStatus(const std::string serialized_str,
                            TaskStatus *status);

// limit value between min and max
int SetLimitIntValue(int min, int max, int value);

// turn client rpc code to human readable string
std::string ClientRpcRetCodeToString(ClientRpcStatus ret_code,
                                     const std::string &identify_string,
                                     const std::string type);

template <typename IntegerType>
static void ParseNeedDownloadFiles(
    const std::vector<std::string> &include,
    const std::vector<std::string> &exclude,
    const libtorrent::torrent_info &ti, 
    std::vector<IntegerType> *file_priorities) {
  std::vector<boost::regex> include_regex, exclude_regex;
  std::vector<std::string>::const_iterator it;
  for (it = include.begin(); it != include.end(); ++it) {
    include_regex.push_back(boost::regex(*it));
  }
  for (it = exclude.begin(); it != exclude.end(); ++it) {
    exclude_regex.push_back(boost::regex(*it));
  }

  int num_files = ti.num_files();
  file_priorities->resize(num_files, 0);
  for (int i = 0; i < num_files; ++i) {
    std::string filepath = ti.files().file_path(i);
    RegexMatch regex_m(filepath);
    if (include_regex.empty() ||
        std::find_if(include_regex.begin(), include_regex.end(), regex_m) != include_regex.end()) {
      file_priorities->at(i) = 1;
    }
    if (std::find_if(exclude_regex.begin(), exclude_regex.end(), regex_m) != exclude_regex.end()) {
      file_priorities->at(i) = 0;
    }
    if (file_priorities->at(i)) {
      NOTICE_LOG("will download file: %s", filepath.c_str());
    }
  }
}


}  // namespace group
}  // namespace bbts
#endif  // OP_OPED_NOAH_TOOLS_BBTS_GROUP_UTILS_H_

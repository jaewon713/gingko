/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   TaskStatistics.h
 *
 * @author liuming03
 * @date   2014-3-6
 * @brief 
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_STATISTICS_H_
#define OP_OPED_NOAH_TOOLS_BBTS_STATISTICS_H_

#include <stdio.h>

#include <string>

#include "bbts/BBTSTorrentPlugin.h"

namespace bbts {

class PeerStatFile {
 public:
  PeerStatFile() : file(NULL) {};

  ~PeerStatFile() {
    if (file) {
      fclose(file);
    }
  }

  inline bool Open(const std::string &filename) {
    file = fopen(filename.c_str(), "a");
    if (!file) {
      return false;
    }
    return true;
  }

  inline void Print(const peer_stat_alert &alert) {
    if (file && (alert.uploaded_ != 0 || alert.downloaded_ != 0)) {
      fprintf(file, "%s\n", alert.message().c_str());
      fflush(file);
    }
  }

 private:
  FILE *file;
};

struct task_statistics_t {
  task_statistics_t()
    : port(0),
      total_size(0),
      payload_downloaded(0),
      payload_uploaded(0),
      progress_ppm(0),
      start_time(0),
      end_time(0),
      time_for_download_metadata(0),
      time_for_check_files(0),
      time_for_download(0),
      time_for_seeding(0),
      upload_limit(0),
      download_limit(0),
      retval(0),
      num_files(0),
      num_paths(0),
      num_symlinks(0),
      piece_length(0),
      num_pieces(0),
      is_hdfs_download(false),
      downloaded_from_hdfs(0) {}

  std::string hostname;
  std::string ip;
  int port;
  std::string infohash;
  int64_t total_size;
  int64_t payload_downloaded;
  int64_t payload_uploaded;
  int32_t progress_ppm;
  int32_t start_time;
  int32_t end_time;
  int32_t time_for_download_metadata;
  int32_t time_for_check_files;
  int32_t time_for_download;
  int32_t time_for_seeding;
  int32_t upload_limit;
  int32_t download_limit;
  int32_t retval;
  int32_t num_files;
  int32_t num_paths;
  int32_t num_symlinks;
  int32_t piece_length;
  int32_t num_pieces;
  bool is_hdfs_download;
  std::string hdfs_address;
  int64_t downloaded_from_hdfs;
};

inline bool PrintTaskStatistics(const task_statistics_t &stat, const std::string &filename) {
  FILE *fp = fopen(filename.c_str(), "a");
  if (!fp) {
    return false;
  }
  fprintf(fp, "%s,%s,%d,%s,%ld,%d,%d,%d,%d,%d,%d,%d,%ld,%ld,%d,%d,%d,%d,%d,%d,%d,%d,%d,%ld,%s\n",
          stat.hostname.c_str(),
          stat.ip.c_str(),
          stat.port,
          stat.infohash.c_str(),
          stat.total_size,
          stat.piece_length,
          stat.num_pieces,
          stat.num_files,
          stat.num_paths,
          stat.num_symlinks,
          stat.download_limit,
          stat.upload_limit,
          stat.payload_downloaded,
          stat.payload_uploaded,
          stat.progress_ppm,
          stat.start_time,
          stat.end_time,
          stat.retval,
          stat.time_for_download_metadata,
          stat.time_for_check_files,
          stat.time_for_download,
          stat.time_for_seeding,
          stat.is_hdfs_download,
          stat.downloaded_from_hdfs,
          stat.hdfs_address.c_str());
  fclose(fp);
  return true;
}

}

#endif // OP_OPED_NOAH_TOOLS_BBTS_STATISTICS_H_

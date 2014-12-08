/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file torrent_file_util.h
 *
 * @author liuming03
 * @date 2013-12-12
 * @brief 
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_TORRENT_FILE_UTIL_H_
#define OP_OPED_NOAH_TOOLS_BBTS_TORRENT_FILE_UTIL_H_

#include <string>
#include <boost/regex.hpp>
#include <libtorrent/torrent_info.hpp> // for cluster_config_entry

namespace bbts {

struct make_torrent_args_t {
  enum {
    BEST_HASH = 0x0001,
    SYMLINKS  = 0x0002,
    FILE_HASH = 0x0004,
  };

  make_torrent_args_t() :
      piece_size(0), flags(0) {
  }

  int piece_size;
  int flags;
  std::string full_path;
  libtorrent::cluster_config_entry cluster_config;
  std::vector<std::string> web_seeds;
  std::vector<boost::regex> include_regex;
  std::vector<boost::regex> exclude_regex;
};

/**
 * @brief 制作种子文件
 *
 * @param  args(in)      所需参数列表
 * @param  infohash(out) 生成的种子文件的infohash
 * @return 成功返回true，失败返回非false
 */
bool make_torrent(const make_torrent_args_t &args,
                  std::string* infohash,
                  std::vector<char> *torrent);
/**
 * @brief 打印输出种子文件中信息
 *
 * @param torrent_path 种子文件绝对路径
 * @return 成功返回true，失败返回非false
 */
bool dump_torrent(const std::string &torrent_path);

struct delete_files_args_t {
  enum {
    DELETE_IN_TORRENT = 0x0001,
    SYMLINKS          = 0x0002,
  };

  delete_files_args_t() : flags(0) {}

  int flags;
  std::string save_path;
  std::string torrent_path;
  std::string new_name;
};

bool DeleteFilesByTorrent(const delete_files_args_t &args);

} // namespace bbts

#endif // OP_OPED_NOAH_TOOLS_BBTS_TORRENT_FILE_UTIL_H_

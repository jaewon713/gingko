/***************************************************************************
 *
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file   dump_torrent.cpp
 *
 * @author liuming03
 * @date   2013-2-1
 */

#include <libtorrent/bencode.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/lazy_entry.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/torrent_info.hpp>

#include "bbts/torrent_file_util.h"

namespace bbts {

bool dump_torrent(const std::string &torrent_path) {
  int item_limit = 10000000;
  int depth_limit = 1000;

  int64_t size = libtorrent::file_size(torrent_path.c_str());
  if (size > 50 * 1024 * 1024 || size <= 0) {
    fprintf(stderr, "file too big (%ld) > 50MB or empty, aborting\n", size);
    return false;
  }

  std::vector<char> buf(size);
  libtorrent::error_code ec;
  int ret = libtorrent::load_file(torrent_path.c_str(), buf, ec, 40 * 1000000);
  if (ret != 0) {
    fprintf(stderr, "failed to load file: %s\n", ec.message().c_str());
    return false;
  }

  int pos;
  libtorrent::lazy_entry e;
  ret = libtorrent::lazy_bdecode(&buf[0], &buf[0] + buf.size(), e, ec, &pos, depth_limit, item_limit);
  if (ret != 0) {
    fprintf(stderr, "failed to decode: '%s' at character: %d\n", ec.message().c_str(), pos);
    return false;
  }

  libtorrent::torrent_info t(e, ec);
  if (ec) {
    fprintf(stderr, "%s\n", ec.message().c_str());
    return false;
  }
  e.clear();
  std::vector<char>().swap(buf);

  printf("\n----- torrent file info -----\n");
  char ih[41];
  libtorrent::to_hex((char const*)&t.info_hash()[0], 20, ih);
  printf("total size: %ld\n"
         "number of pieces: %d\n"
         "piece length: %d\n"
         "info hash: %s\n"
         "comment: %s\n"
         "created by: %s\n"
         "name: %s\n"
         "number of files: %d\n"
         "cluster path: hdfs://%s:%s@%s:%d%s\n"
         "paths:\n",
         t.total_size(),
         t.num_pieces(),
         t.piece_length(),
         ih,
         t.comment().c_str(),
         t.creator().c_str(),
         t.name().c_str(),
         t.num_files(),
         t.cluster_config().user.c_str(), t.cluster_config().passwd.c_str(),
         t.cluster_config().host.c_str(), t.cluster_config().port,
         t.cluster_config().prefix_path.c_str());

  t.files().print_paths();
  if (t.files().has_symlinks()) {
    printf("symlinks:\n");
    t.files().print_symlinks();
  }

  printf("files:\n");
  for (int index = 0; index < t.num_files(); ++index) {
    libtorrent::file_entry fe = t.file_at(index);
    int first = t.map_file(index, 0, 0).piece;
    int last = t.map_file(index, (std::max)(libtorrent::size_type(fe.size) - 1, libtorrent::size_type(0)), 0).piece;
    printf("  %11"PRId64" %c %o [ %4d, %4d ] %7u %s %s %s%s\n",
           fe.size, (fe.pad_file ? 'p' : '-'), fe.mode, first, last, boost::uint32_t(t.files().mtime(index)),
           t.files().hash(index) != libtorrent::sha1_hash(0) ? libtorrent::to_hex(t.files().hash(index).to_string()).c_str() : "",
           t.files().file_path(index).c_str(), fe.symlink_attribute ? "-> " : "",
           fe.symlink_attribute ? t.files().symlink(index).c_str() : "");
  }

  return true;
}

} /* namespace bbts */


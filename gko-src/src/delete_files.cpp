/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   rm_files.cpp
 *
 * @author liuming03
 * @date   2014-1-20
 * @brief 
 */

#include "bbts/torrent_file_util.h"

#include <ftw.h>

#include <vector>

#include <boost/smart_ptr.hpp>
#include <libtorrent/file.hpp>
#include <libtorrent/torrent_info.hpp>

namespace bbts {

using std::string;
using std::vector;
using boost::system::error_code;
using libtorrent::torrent_info;
using libtorrent::symlink_file;

static boost::scoped_ptr<torrent_info> ti;
static int base = 0;

static int DeleteFileCallback(const char* path, const struct stat* statbuf,
                              int type, struct FTW* ftw_info) {
  string base_path(path + base);
  switch (type) {
    case FTW_F: {
      if (!(statbuf->st_mode & S_IFREG)) {  //非规则文件，如pipe等，则跳过
        fprintf(stderr, "skip pipe, sock file: %s\n", path);
        break;
      }
      bool found = false;
      for (int i = 0; i < ti->num_files(); ++i) {
        libtorrent::file_entry fe = ti->file_at(i);
        if (fe.path == base_path) {
          found = true;
          break;
        }
      }
      if (!found) {
        if (unlink(path) != 0) {
          fprintf(stderr, "delete file(%s) error(%d): %s\n ", path, errno, strerror(errno));
        } else {
          fprintf(stdout, "delete file: %s\n", path);
        }
      } else {
        fprintf(stdout, "file(%s) is in torrent\n", base_path.c_str());
      }
      break;
    }

    case FTW_SL: // no break;
    case FTW_SLN: {
      const vector<symlink_file> &symlinks = ti->files().get_symlink_file();
      bool found = false;
      for (vector<symlink_file>::const_iterator it = symlinks.begin(); it != symlinks.end(); ++it) {
        if (it->path == base_path) {
          found = true;
          break;
        }
      }
      if (!found) {
        if (unlink(path) != 0) {
          fprintf(stderr, "delete symlink file(%s) error(%d): %s\n ", path, errno, strerror(errno));
        } else {
          fprintf(stdout, "delete symlink file: %s\n", path);
        }
      } else {
        fprintf(stdout, "symlink file(%s) is in torrent\n", base_path.c_str());
      }
      break;
    }

    case FTW_DP: {
      base_path.append("/");
      printf("path: %s\n", base_path.c_str());
      const vector<string>& paths = ti->get_paths();
      int path_len = paths.size();
      bool found = false;
      for (int i = 0; i < path_len; ++i) {
        if (paths[i] == base_path) {
          found = true;
        }
      }
      if (!found) {
        if (rmdir(path) != 0) {
          fprintf(stderr, "delete path(%s) error(%d): %s\n ", path, errno, strerror(errno));
        } else {
          fprintf(stdout, "delete path: %s\n", path);
        }
      } else {
        fprintf(stdout, "path(%s) is in torrent\n", base_path.c_str());
      }
      break;
    }

    case FTW_DNR:
      fprintf(stderr, "find a not be read path: %s\n", path);
      return FTW_STOP;

    case FTW_NS: // no break
    default:
      fprintf(stderr, "%s can't process, skip\n", path);
      return FTW_STOP;
  }

  return FTW_CONTINUE;
}

static bool DeleteFilesOutTorrent(const string &save_path, const string &new_name, bool symlinks) {
  const static int NFTW_DEPTH = 10;
  int nftw_flag = FTW_ACTIONRETVAL | FTW_DEPTH;
  if (symlinks) {
    nftw_flag |= FTW_PHYS;
  }
  base = save_path.length() + 1;
  string root_path = save_path + '/' + (new_name.empty() ? ti->name() : new_name);
  if (nftw(root_path.c_str(), &DeleteFileCallback, NFTW_DEPTH, nftw_flag) != 0) {
    fprintf(stdout, "nftw path(%s) error(%d): %s.\n", root_path.c_str(), errno, strerror(errno));
    return false;
  }
  return true;
}

static bool DeleteFilesInTorrent() {
  return true;
}

bool DeleteFilesByTorrent(const delete_files_args_t &args) {
  int64_t size = libtorrent::file_size(args.torrent_path.c_str());
  if (size > 50 * 1024 * 1024 || size <= 0) {
    fprintf(stderr, "file too big (%ld) > 50MB or empty, aborting\n", size);
    return false;
  }

  error_code ec;
  ti.reset(new torrent_info(args.torrent_path, ec, 0, args.new_name));
  if (ec) {
    fprintf(stderr, "%s\n", ec.message().c_str());
    return false;
  }

  if (args.flags & delete_files_args_t::DELETE_IN_TORRENT) {
    return DeleteFilesInTorrent();
  }
  return DeleteFilesOutTorrent(args.save_path, args.new_name,
                               args.flags & delete_files_args_t::SYMLINKS);
}

} // namespace bbts

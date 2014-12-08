/***************************************************************************
 *
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file   make_torrent.cpp
 *
 * @author liuming03
 * @date   2013-2-1
 */

#include "bbts/torrent_file_util.h"

#include <boost/bind.hpp>
#include <boost/noncopyable.hpp>
#include <boost/regex.hpp>
#include <boost/thread.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/bitfield.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/hasher.hpp>

#include "bbts/path.h"
#include "bbts/RegexMatch.hpp"

using std::string;
using std::vector;
using boost::regex;
using libtorrent::create_torrent;

namespace bbts {

class FileFilter {
public:
  FileFilter(const string &root_path, const string &path_reg,
             const vector<regex> &include, const vector<regex> &exclude) :
               root_path_(root_path), prefix_len_(0), include_(include), exclude_(exclude) {
    try {
      re_.assign(path_reg);
    } catch (boost::bad_expression &e) {
      error_ = e.what();
      fprintf(stderr, "path reg(%s) init failed, %s", path_reg.c_str(), e.what());
    }
    prefix_len_ = parent_path(root_path_).length() + 1;
  }

  inline bool IsError() {
    return !error_.empty();
  }

  bool operator ()(const string &filename) {
    if (filename == root_path_) {
      return true;
    }
    if (!boost::regex_match(filename.c_str(), re_)) {
      return false;
    }
    RegexMatch regex_m(filename.substr(prefix_len_));
    if (!include_.empty() && std::find_if(include_.begin(), include_.end(), regex_m) == include_.end()) {
      return false;
    }
    return std::find_if(exclude_.begin(), exclude_.end(), regex_m) == exclude_.end();
  }

private:
  string root_path_;
  regex re_;
  string error_;
  int prefix_len_;
  const vector<regex> &include_;
  const vector<regex> &exclude_;
};

static void print_progress(int i, int num) {
  //fprintf(stdout, "\r%d/%d", i + 1, num);
}

class QuickPiecesHashes : private boost::noncopyable {
public:
  QuickPiecesHashes(create_torrent& t, string const& path, int thread_num);
  virtual ~QuickPiecesHashes();
  bool SetPieceHashes();

private:
  void ThreadMain();
  bool ReadPiece(int i, char *buf, int buf_len);

  int thread_num_;
  string root_path_;
  create_torrent &ct_;
  libtorrent::bitfield pieces_completed_;
  boost::mutex pieces_mutex_;
  boost::thread_group threads_;
  volatile bool have_error_;
};

QuickPiecesHashes::QuickPiecesHashes(create_torrent& t, string const& root_path, int thread_num) :
    thread_num_(thread_num), root_path_(root_path), ct_(t),
    pieces_completed_(ct_.num_pieces(), false), have_error_(false) {}

QuickPiecesHashes::~QuickPiecesHashes() {}

bool QuickPiecesHashes::ReadPiece(int i, char *buf, int buf_len) {
  FILE *fp = NULL;
  char *ptr = buf;
  const libtorrent::file_storage &fs = ct_.files();
  vector<libtorrent::file_slice> files = fs.map_block(i, 0, fs.piece_size(i));
  for (vector<libtorrent::file_slice>::iterator it = files.begin(); it != files.end() && !have_error_; ++it) {
    string filename = root_path_ + "/" + fs.at(it->file_index).path;
    fp = fopen(filename.c_str(), "rb");
    if (!fp) {
      fprintf(stderr, "open file failed: %s\n", filename.c_str());
      return false;
    }
    if (fseek(fp, it->offset, SEEK_SET) != 0) {
      fprintf(stderr, "fseek file(%s) to offset(%ld) failed.\n", filename.c_str(), it->offset);
      fclose(fp);
      return false;
    }
    if (fread(ptr, 1, it->size, fp) != (size_t)it->size) {
      fprintf(stderr, "fread file(%s) failed, offset: %ld, len: %ld.\n", filename.c_str(), it->offset, it->size);
      fclose(fp);
      return false;
    }
    fclose(fp);
    ptr += it->size;
  }
  return true;
}

void QuickPiecesHashes::ThreadMain() {
  int num = ct_.num_pieces();
  int piece_length = ct_.piece_length();
  boost::scoped_array<char> buffer(new char[piece_length]);
  for (int i = 0; i < num && !have_error_; ++i) {
    {
      boost::mutex::scoped_lock lock;
      if (pieces_completed_.get_bit(i)) {
        continue;
      }
      pieces_completed_.set_bit(i);
    }

    if (!ReadPiece(i, buffer.get(), piece_length)) {
      have_error_ = true;
      return;
    }
    libtorrent::hasher h(buffer.get(), ct_.piece_size(i));
    ct_.set_hash(i, h.final());
  }
}

bool QuickPiecesHashes::SetPieceHashes() {
  if (ct_.files().num_pieces() == 0) {
    return true;
  }

  for (int i = 0; i < thread_num_; ++i) {
    threads_.create_thread(boost::bind(&QuickPiecesHashes::ThreadMain, this));
  }
  threads_.join_all();
  if (have_error_) {
    return false;
  }
  return true;
}

bool make_torrent(const make_torrent_args_t &args, string* infohash, vector<char> *torrent) {
  assert(infohash);
  assert(torrent);
  string creator_str = "gingko 3.0";
  string comment_str;
  int pad_file_limit = -1;

  string parent_path, short_name, reg_path, root_path;
  path_slipt(args.full_path, &parent_path, &short_name);
  string::size_type pos = short_name.find('*');
  if (pos == string::npos) {
    reg_path = args.full_path + '/' + ".*";
    root_path = args.full_path;
  } else {
    do {
      short_name.replace(pos, 1, ".*");
      pos += 2;
    } while (string::npos != (pos = short_name.find('*', pos)));
    reg_path = parent_path + '/' + short_name;
    root_path = parent_path;
  }
  FileFilter filter(root_path, reg_path, args.include_regex, args.exclude_regex);
  if (filter.IsError()) {
    return false;
  }

  int flags = 0;
  if (args.flags & make_torrent_args_t::SYMLINKS) {
    flags |= create_torrent::symlinks;
  }
  if (args.flags & make_torrent_args_t::FILE_HASH) {
    flags |= create_torrent::calculate_file_hashes;
  }

  libtorrent::file_storage fs;
  add_files(fs, root_path, filter, flags);
  if (fs.num_files() == 0) {
    fprintf(stderr, "failed: no files.\n");
    return false;
  }

  create_torrent t(fs, args.piece_size, pad_file_limit, flags);
  if (!args.cluster_config.prefix_path.empty()) {
    t.set_cluster_config(args.cluster_config.host, args.cluster_config.port, args.cluster_config.user,
                         args.cluster_config.passwd, args.cluster_config.prefix_path);
  }

  t.set_creator(creator_str.c_str());
  if (!comment_str.empty()) {
    t.set_comment(comment_str.c_str());
  }
  if (fs.num_files() != 0) {
    if (!(args.flags & make_torrent_args_t::BEST_HASH)) {
      libtorrent::error_code ec;
      libtorrent::set_piece_hashes(t, libtorrent::parent_path(root_path),
                                   boost::bind(&print_progress, _1, t.num_pieces()), ec);
      if (ec) {
        fprintf(stderr, "failed: %s\n", ec.message().c_str());
        return false;
      }
    } else {
      QuickPiecesHashes quick_hashes(t, libtorrent::parent_path(root_path), 2);
      if (!quick_hashes.SetPieceHashes()) {
        fprintf(stderr, "failed for hash pieces!\n");
        return false;
      }
    }
  }

  for (vector<string>::const_iterator it = args.web_seeds.begin(); it != args.web_seeds.end(); ++it) {
    t.add_url_seed(*it);
  }

  torrent->clear();
  bencode(back_inserter(*torrent), t.generate());
  *infohash = libtorrent::to_hex(t.get_info_hash().to_string());
  return true;
}

} // namespace bbts

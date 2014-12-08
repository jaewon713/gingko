/***************************************************************************
 *
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file   BTInfo.h
 *
 * @author liuming03
 * @date   2013-05-29
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_BTINFO_H_
#define OP_OPED_NOAH_TOOLS_BBTS_BTINFO_H_

#include "bbts/plugin/hdfstool.h"

namespace bbts {

class BTInfo : public AbstractBTInfo {
 public:
  inline BTInfo(const libtorrent::torrent_info& info, const std::string &path) :
      info_(info), path_(path) {
  }

  inline virtual ~BTInfo() {
  }

  inline std::vector<file_slice> map_block(int piece, libtorrent::size_type offset, int size) const {
    std::vector<libtorrent::file_slice> vfs =
        info_.map_block(piece, offset, size);
    std::vector<file_slice> nvfs;
    for (std::vector<libtorrent::file_slice>::iterator i = vfs.begin(), end(vfs.end()); i != end; ++i) {
      file_slice fs;
      fs.file_index = i->file_index;
      fs.offset = i->offset;
      fs.size = i->size;
      nvfs.push_back(fs);
    }
    return nvfs;
  }

  inline file_entry file_at(int index) const {
    libtorrent::file_entry fe = info_.file_at(index);
    file_entry nfe;
    nfe.path = fe.path;
    nfe.offset = fe.offset;
    nfe.size = fe.size;
    nfe.file_base = fe.file_base;
    nfe.mtime = fe.mtime;
    nfe.hidden_attribute = fe.hidden_attribute;
    nfe.executable_attribute = fe.executable_attribute;
    nfe.symlink_attribute = fe.symlink_attribute;
    nfe.pad_file = fe.pad_file;
    return nfe;
  }

  inline size_type total_size() const {
    return info_.total_size();
  }

  inline int piece_length() const {
    return info_.piece_length();
  }

  inline int num_pieces() const {
    return info_.num_pieces();
  }

  inline int piece_size(unsigned int inx) const {
    return info_.piece_size(inx);
  }

  inline int num_files() const {
    return info_.num_files();
  }

  inline std::string getPrefixPath() const {
    return path_;
  }

  inline libtorrent::sha1_hash hash_for_piece(int index) const {
    return info_.hash_for_piece(index);
  }

 private:
  libtorrent::torrent_info info_;
  std::string path_;
};

} // namespace bbts

#endif // OP_OPED_NOAH_TOOLS_BBTS_BTINFO_H_

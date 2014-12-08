/*
 * hdfstool.cpp
 *
 *  创建: 2013-05-27
 *  作者: caijunjie@baidu.com
 *  描述：访问HDFS文件的接口
 */

#include "bbts/plugin/hdfstool.h"

#include <string.h>

#include <string>

#include <hdfs.h>

using std::string;

extern "C" {

/*
 * 根据torrent_info的pieces的start位置,读取length个piece.
 *
 * */
int GetFileConetnsFromHdfs(const void* fs_point, const AbstractBTInfo* info, int piece_index, char* data,
                           size_type size, string &errstr) {
  char *ptr = data;
  string prefix_path = info->getPrefixPath();
  hdfsFS fs = (hdfsFS)fs_point;
  //获取piece内的文件列表
  std::vector<file_slice> files = info->map_block(piece_index, 0, info->piece_size(piece_index));
  for (std::vector<file_slice>::iterator i = files.begin(); i != files.end(); ++i) {
    file_slice const& f = *i;
    file_entry fileentry = info->file_at(f.file_index);
    if (fileentry.pad_file) {
      memset(ptr, 0, f.size);
      ptr += f.size;
      continue;
    }
    string path = fileentry.path;
    //拼接完整的HDFS文件路径
    string filename = prefix_path + "/" + path;
    //调用libhdfs接口打开文件
    hdfsFile readFile = hdfsOpenFile(fs, filename.c_str(), O_RDONLY, 0, 0, 0);
    if (!readFile) {
      errstr = "Open hdfs file failed: " + filename;
      return -1;
    }
    //调用libhdfs接口读取文件
    size_type readpos = f.offset;
    size_type no_read_size = f.size;
    while (no_read_size > 0) {
      size_type num_read_bytes = hdfsPread(fs, readFile, readpos, (void *)ptr, no_read_size);
      if (num_read_bytes < 0) {
        hdfsCloseFile(fs, readFile);
        errstr = "read hdfs file failed: " + filename;
        return -2;
      }
      if (num_read_bytes == 0) {
        hdfsCloseFile(fs, readFile);
        errstr = "file size too small: " + filename;
        return -3;
      }
      readpos += num_read_bytes;
      ptr += num_read_bytes;
      no_read_size -= num_read_bytes;
    }
    hdfsCloseFile(fs, readFile);
  }
  return 0;
}

/*
 * 获取Hadoop Connect的连接对象
 *
 * */
void* GetHadoopConnectObject(const string& host, const int port, const string& user, const string& passwd) {
#if BUILD_BAIDU
  hdfsFS fs = hdfsConnectAsUser(host.c_str(), port, user.c_str(), passwd.c_str());
#else
  hdfsFS fs = hdfsConnectAsUser(host.c_str(), port, user.c_str());
#endif
  return (void *)(fs);
}
;

/*
 * 释放Hadoop Connect的连接对象
 *
 */
int DropHadoopConnectObject(void* fs_point) {
  hdfsFS fs = (hdfsFS)fs_point;
  return hdfsDisconnect(fs);
}

}

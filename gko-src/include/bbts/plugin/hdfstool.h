/***************************************************************************
 *
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

/*
 * @file   hdfstool.h
 *
 * @author liuming03
 * @date   2013-05-29
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_AGENT_HDFSTOOL_H_
#define OP_OPED_NOAH_TOOLS_BBTS_AGENT_HDFSTOOL_H_

#include <stdint.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include <string>
#include <vector>

typedef int64_t size_type;

struct file_entry {
  std::string path;
  size_type offset;  // the offset of this file inside the torrent
  size_type size;  // the size of this file
  // the offset in the file where the storage starts.
  // This is always 0 unless parts of the torrent is
  // compressed into a single file, such as a so-called part file.
  size_type file_base;
  time_t mtime;
  bool pad_file :1;
  bool hidden_attribute :1;
  bool executable_attribute :1;
  bool symlink_attribute :1;
  mode_t mode;  // added by liuming03
  std::string symlink_path;
};

struct file_slice {
  int file_index;
  size_type offset;
  size_type size;
};

class AbstractBTInfo {
 public:
  virtual std::vector<file_slice> map_block(int piece, size_type offset, int size) const = 0;

  virtual size_type total_size() const = 0;

  /*
   * 默认设置的piece_length长度
   * 
   * */
  virtual int piece_length() const = 0;

  /* 
   * torrent_info中的piece个数大小
   * 
   * */
  virtual int num_pieces() const = 0;

  /*
   * 获取当前piece的size大小
   * 
   * */
  virtual int piece_size(unsigned int index) const = 0;

  /*
   * 根据piece中的file_index，获取具体文件
   *
   * */
  virtual file_entry file_at(int idx) const = 0;

  /*
   * 获取一个piece中的文件数量
   * 
   * */
  virtual int num_files() const = 0;

  /*
   * 获取HDFS的前缀路径
   *
   * */
  virtual std::string getPrefixPath() const = 0;

  /*
   * 构造函数
   *
   * */
  AbstractBTInfo() {
  }
  ;

  /*
   * 析构函数
   *
   * */
  virtual ~AbstractBTInfo() {
  }
  ;

};

/*
 * 根据torrent_info的pieces的start位置, 读取length个piece.
 * @param fs_point: HDFSConect对象
 * @param info: AbstractBTInfo接口
 * @param pieces: piece块,first:开始位置, second:数目
 * @param data: 返回的数据缓存
 * @param size: 传入可读入的最大字节，返回实际读取的字节
 * @param errstr: 保存错误信息
 * 
 * @retval  0: 成功
 *         -1: 打开hdfs文件错误
 *         -2: 读hdfs文件错误
 *         -3: 读取的字节超过size大小错误
 * */
extern "C" {
int GetFileConetnsFromHdfs(const void* fs_point,
                           const AbstractBTInfo* info,
                           int piece_index,
                           char* data,
                           size_type size,
                           std::string &errstr);
}

/*
 * 获取hadoop Connect对象
 * @param host: Hadoop集群名
 * @param port: HAdoop集群端口号
 * @param user: Hadoop集群用户名
 * @param passwd: Hadoop集群用户密码
 *
 * @retval null: 获取失败
 *        hdfsFS: 获取的对象
 * */
extern "C" {
void* GetHadoopConnectObject(const std::string& host,
                             const int port,
                             const std::string& user,
                             const std::string& passwd);
}

/*
 * 释放hadoop Connect对象
 * @param fs: hdfsFS 对象
 * */
extern "C" {
int DropHadoopConnectObject(void* fs);
}

extern "C" {
int CreateJVM(const std::string &classpath);
}

#endif // OP_OPED_NOAH_TOOLS_BBTS_AGENT_HDFSTOOL_H_

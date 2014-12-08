#ifndef  OP_OPED_NOAH_BBTS_COMMON_CONF_H_
#define  OP_OPED_NOAH_BBTS_COMMON_CONF_H_

#include <fcntl.h>

#include <string>

#include <glog/logging.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

namespace bbts {
namespace tracker {

template<typename Conf> bool LoadConf(const std::string &conf_file, Conf *conf) {
  LOG(INFO) << "Start to load tracker conf " << conf_file;

  int conf_fd = open(conf_file.c_str(), O_RDONLY);
  if (conf_fd < 0) {
    LOG(WARNING) << "open " << conf_file << " failed, errno=" << errno;
    return false;
  }

  google::protobuf::io::FileInputStream finstream(conf_fd);
  if (!google::protobuf::TextFormat::Parse(&finstream, conf)) {
    LOG(WARNING) << "load " << conf_file << " failed";
    return false;
  }

  finstream.Close();
  LOG(INFO) << "Tracker conf\n" << conf->DebugString();
  return true;
}

} // namespace tracker
} // namespace bbts

#endif  // OP_OPED_NOAH_BBTS_COMMON_CONF_H_

#ifndef  OP_OPED_NOAH_BBTS_PB_CONF_H_
#define  OP_OPED_NOAH_BBTS_PB_CONF_H_

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include <string>

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

namespace bbts {

template<typename Conf> bool LoadPBConf(const std::string &conf_file, Conf *conf) {
  //fprintf(stderr, "start to load conf: %s\n", conf_file.c_str());
  int conf_fd = open(conf_file.c_str(), O_RDONLY);
  if (conf_fd < 0) {
    fprintf(stderr, "open conf %s failed: %d\n", conf_file.c_str(), errno);
    return false;
  }

  google::protobuf::io::FileInputStream ifs(conf_fd);
  ifs.SetCloseOnDelete(true);
  if (!google::protobuf::TextFormat::Parse(&ifs, conf)) {
    fprintf(stderr, "parse conf %s failed\n", conf_file.c_str());
    return false;
  }
  //fprintf(stderr, "conf %s :\n%s\n", conf_file.c_str(), conf->DebugString().c_str());
  return true;
}

} // namespace bbts

#endif  // OP_OPED_NOAH_BBTS_PB_CONF_H_

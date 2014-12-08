/***************************************************************************
 *
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/
#ifndef OP_OPED_NOAH_TOOLS_BBTS_GROUP_PARAM_DEFINE_H_
#define OP_OPED_NOAH_TOOLS_BBTS_GROUP_PARAM_DEFINE_H_

#include <string>

namespace bbts {

namespace group {

struct GroupParams {
  GroupParams()
      : conf_file(""),
        is_debug(false) {
  }

  std::string conf_file;
  bool is_debug;
};

}  // namespace group
}  // namespace bbts

#endif  // OP_OPED_NOAH_TOOLS_BBTS_GROUP_PARAM_DEFINE_H_


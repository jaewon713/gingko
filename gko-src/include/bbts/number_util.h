/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   number_util.h
 *
 * @author liuming03
 * @date   2013-11-18
 * @brief 
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_NUMBER_UTIL_H_
#define OP_OPED_NOAH_TOOLS_BBTS_NUMBER_UTIL_H_

#include <stdint.h>

#include <string>

namespace bbts {

void BytesToReadable(int64_t bytes, std::string *bytes_string);

int64_t ReadableToBytes(const std::string &bytes_string, int64_t unit);

inline int64_t ReadableToBytes(const std::string &bytes_string) {
  return ReadableToBytes(bytes_string, 1024LL * 1024);
}


} // namespace bbts
#endif // OP_OPED_NOAH_TOOLS_BBTS_NUMBER_UTIL_H_

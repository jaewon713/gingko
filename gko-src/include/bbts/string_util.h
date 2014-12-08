/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   string_util.h
 *
 * @author liuming03
 * @date   2013-12-30
 * @brief 
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_STRING_UTIL_H_
#define OP_OPED_NOAH_TOOLS_BBTS_STRING_UTIL_H_

#include <string>
#include <vector>
#include <set>

namespace bbts {

void SliptStringToVector(const std::string &str,
                         const std::string &delimiter,
                         std::vector<std::string> *v);

void SliptStringToSet(const std::string &str,
                      const std::string &delimiter,
                      std::set<std::string> *s);

} // namespace bbts
#endif // OP_OPED_NOAH_TOOLS_BBTS_STRING_UTIL_H_

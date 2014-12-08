/***************************************************************************
 *
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file   util.h
 *
 * @author liuming03
 * @date   2013-1-7
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_UTIL_H_
#define OP_OPED_NOAH_TOOLS_BBTS_UTIL_H_

#include <string>
#include <vector>

namespace bbts {

bool WriteToFile(const std::string &filename, const std::vector<char> &buffer);

} // namespace bbts

#endif // OP_OPED_NOAH_TOOLS_BBTS_UTIL_H_

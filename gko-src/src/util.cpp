/***************************************************************************
 *
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file   util.cpp
 *
 * @author liuming03
 * @date   2013-1-7
 */

#include "bbts/util.h"

#include <errno.h>

#include "bbts/log.h"

using std::string;
using std::vector;

namespace bbts {

bool WriteToFile(const string &filename, const vector<char> &buffer) {
  FILE *fp = fopen(filename.c_str(), "wb+");
  if (!fp) {
    WARNING_LOG("failed: open file \"%s\" fail: (%d)\n", filename.c_str(), errno);
    return false;
  }
  fwrite(&buffer[0], 1, buffer.size(), fp);
  fclose(fp);
  return true;
}

} // namespace bbts

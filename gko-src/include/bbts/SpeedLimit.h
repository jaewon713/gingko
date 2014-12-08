/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   SpeedLimit.h
 *
 * @author liuming03
 * @date   2013-9-3
 * @brief 
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_SPEED_LIMIT_H_
#define OP_OPED_NOAH_TOOLS_BBTS_SPEED_LIMIT_H_

#include <time.h>
#include <boost/thread/mutex.hpp>

namespace bbts {

/**
 * @brief
 */
class SpeedLimit {
 public:
  SpeedLimit();
  virtual ~SpeedLimit();

  void BandwidthLimit(int amount, int limit_rate);

 private:
  struct timeval bw_start_;
  int lamt_;
  boost::mutex mutex_;
};

} // namespace bbts
#endif // OP_OPED_NOAH_TOOLS_BBTS_SPEED_LIMIT_H_

/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   log.h
 *
 * @author liuming03
 * @date   2014年9月29日
 * @brief 
 */

#ifndef OP_OPED_NOAH_BBTS_AGENT_LOG_H_
#define OP_OPED_NOAH_BBTS_AGENT_LOG_H_

#define LOGLINE(x) LOGLINE_(x)
#define LOGLINE_(x) #x
#define LOGINFO() "["__FILE__":"LOGLINE(__LINE__)"]"
//#define LOGINFO()

#if BUILD_BAIDU

#include <comlog/comlog.h>

#define OPEN_LOG_R() com_openlog_r()
#define CLOSE_LOG_R() com_closelog_r()
#define CLOSE_LOG() com_closelog()

#define FATAL_LOG(fmt, arg...) \
do { \
  com_writelog(COMLOG_FATAL, LOGINFO() fmt, ##arg); \
} while (0)

#define WARNING_LOG(fmt, arg...) \
do { \
  com_writelog(COMLOG_WARNING, LOGINFO() fmt, ##arg); \
} while (0)

#define NOTICE_LOG(fmt, arg...) \
do { \
  com_writelog(COMLOG_NOTICE, LOGINFO() fmt, ##arg); \
} while (0)

#ifdef CLOSE_TRACE_LOG
#define TRACE_LOG(fmt, arg...) ((void *)(0))
#else
#define TRACE_LOG(fmt, arg...) \
do { \
  com_writelog(COMLOG_TRACE, LOGINFO() fmt, ##arg); \
} while (0)
#endif

#ifdef CLOSE_DEBUG_LOG
#define DEBUG_LOG(fmt, arg...) ((void *)(0))
#else
#define DEBUG_LOG(fmt, arg...) \
do { \
  com_writelog(COMLOG_DEBUG, LOGINFO() fmt, ##arg); \
} while (0)
#endif

#else

#include <log4cpp/Category.hh>

#define OPEN_LOG_R()
#define CLOSE_LOG_R()
#define CLOSE_LOG() log4cpp::Category::getRoot().shutdown()

#define FATAL_LOG(fmt, arg...) \
do { \
  log4cpp::Category::getRoot().fatal(LOGINFO() fmt, ##arg); \
} while (0)

#define WARNING_LOG(fmt, arg...) \
do { \
  log4cpp::Category::getRoot().warn(LOGINFO() fmt, ##arg); \
} while (0)

#define NOTICE_LOG(fmt, arg...) \
do { \
  log4cpp::Category::getRoot().notice(LOGINFO() fmt, ##arg); \
} while (0)

#define TRACE_LOG(fmt, arg...) \
do { \
  log4cpp::Category::getRoot().info(LOGINFO() fmt, ##arg); \
} while (0)

#define DEBUG_LOG(fmt, arg...) \
do { \
  log4cpp::Category::getRoot().debug(LOGINFO() fmt, ##arg); \
} while (0)

#endif // if BUILD_BAIDU

#include <string>

namespace bbts {
void InitLog(const std::string &dir, const std::string &log, bool debug);
bool LoadLogByConfigure(const std::string &dir, const std::string &conf);
}

#endif // OP_OPED_NOAH_BBTS_AGENT_LOG_H_

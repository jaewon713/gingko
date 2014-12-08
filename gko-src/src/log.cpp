/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   log.cpp
 *
 * @author liuming03
 * @date   2014年10月8日
 * @brief 
 */

#include "bbts/log.h"

using std::string;

#if BUILD_BAIDU

#include <comlog/layout.h>

namespace bbts {
void InitLog(const string &dir, const string &log, bool debug) {
  static comspace::Layout layout;
  layout.setPattern("[%L][%D][%T]%R");
  com_device_t dev[2];
  strncpy(dev[0].type, "TTY", sizeof(dev[0].type));
  strncpy(dev[1].type, "AFILE", sizeof(dev[1].type));
  dev[0].layout = &layout;
  dev[1].layout = &layout;
  strncpy(dev[1].host, dir.c_str(), sizeof(dev[1].host));
  strncpy(dev[1].file, log.c_str(), sizeof(dev[1].file));
  dev[1].log_size = 10;
  dev[1].splite_type = COM_TRUNCT;
  COMLOG_SETSYSLOG(dev[0]);
  COMLOG_SETSYSLOG(dev[1]);
  if (!debug) {
    COMLOG_DELMASK(dev[0], COMLOG_DEBUG);
    COMLOG_DELMASK(dev[1], COMLOG_DEBUG);
  }
  com_openlog(log.c_str(), dev, 2, NULL);
}

bool LoadLogByConfigure(const string &dir, const string &conf) {
  return com_loadlog(dir.c_str(), conf.c_str()) == 0;
}

}

#else

#include <log4cpp/RollingFileAppender.hh>
#include <log4cpp/OstreamAppender.hh>
#include <log4cpp/PatternLayout.hh>
#include <log4cpp/PropertyConfigurator.hh>

namespace bbts {
void InitLog(const string &dir, const string &log, bool debug) {
  log4cpp::PatternLayout* file_layout = new log4cpp::PatternLayout();
  file_layout->setConversionPattern("[%p][%d{%m-%d %H:%M:%S}][%t]%m%n");
  string logfile = dir + '/' + log;
  log4cpp::Appender* file_appender = new log4cpp::RollingFileAppender(log, logfile, 10 * 1024 * 1024, 1);
  file_appender->setLayout(file_layout);

  log4cpp::PatternLayout* stderr_layout = new log4cpp::PatternLayout();
  stderr_layout->setConversionPattern("[%p][%d{%m-%d %H:%M:%S}][%t]%m%n");
  log4cpp::OstreamAppender* stderr_appender = new log4cpp::OstreamAppender("stderr_appender", &std::cerr);
  stderr_appender->setLayout(stderr_layout);
  log4cpp::Category& category = log4cpp::Category::getRoot();
  category.setAdditivity(false);
  category.setAppender(file_appender);
  category.addAppender(stderr_appender);
  if (debug) {
    category.setPriority(log4cpp::Priority::DEBUG);
  } else {
    category.setPriority(log4cpp::Priority::INFO);
  }
}

bool LoadLogByConfigure(const string &dir, const string &conf) {
  string conf_file = dir + '/' + conf;
  try {
    log4cpp::PropertyConfigurator::configure(conf_file);
  } catch(log4cpp::ConfigureFailure& f) {
    return false;
  }
  return true;
}

}

#endif // if BUILD_BAIDU


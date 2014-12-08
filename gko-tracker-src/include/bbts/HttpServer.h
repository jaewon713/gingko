/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file http_manager.h
 * @author huangwei(huangwei02@baidu.com)
 * @date 2013/06/13 15:56:17
 * @brief
 *      provide http interface and export module status
 **/

#ifndef  OP_OPED_NOAH_BBTS_HTTP_SERVER_H_
#define  OP_OPED_NOAH_BBTS_HTTP_SERVER_H_

#include <string>

#include <boost/thread/thread.hpp>

#include "bbts/LazySingleton.hpp"

namespace argus {
namespace common {
class MiniHttpd;
class EventLoop;
}
}

namespace bbts {

// wrap mini-httpd and leverage module_status_manager
class HttpServer  {
 public:
  typedef std::string (*callback_fn)(const std::string&);

  // default make object unuseful in constructor
  HttpServer() : httpd_(NULL), loop_(NULL) {}

  bool start(uint16_t port);

  bool SetCallback(const std::string &path, callback_fn cb);

 private:
  // init httpd-server, module_status_manager
  void Run(uint16_t port);

  static std::string QueryMonitorStatusCallBack(const std::string& query);

  // server has been initialized already?
  argus::common::MiniHttpd  *httpd_;
  argus::common::EventLoop  *loop_;
  boost::thread thread_;
};

}  // namespace bbts

#endif  // OP_OPED_NOAH_BBTS_HTTP_SERVER_H_

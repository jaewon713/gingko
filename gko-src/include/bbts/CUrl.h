/***************************************************************************
 *
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file   CUrl.h
 *
 * @author liuming03
 * @date   2013-1-24
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_CURL_H_
#define OP_OPED_NOAH_TOOLS_BBTS_CURL_H_

#include <string>

#include <boost/noncopyable.hpp>

typedef void CURL;
struct curl_slist;

namespace bbts {

/**
 * @brief 封装了CURL的类
 * 执行给定url的http请求
 */
class CUrl : public boost::noncopyable {
 public:
  CUrl();
  ~CUrl();
  /**
   * 执行一次HTTP GET请求，并获取响应数据
   * @param url(in)       请求的HTTP url
   * @param response(out) 请求成功后，得到的响应数据
   * @param return        成功返回0，其他值失败
   */
  int Get(const std::string &url, std::string *response);

  /**
   * 执行一次HTTP POST请求，并获取响应数据
   * @param url(in)       请求的HTTP url
   * @param port(in)      POST请求时的端口号。
   * @param post_data(in) POST的数据，e.g.: "name=xxx&sex=F"
   * @param response(out) 请求成功后，得到的响应数据
   * @param return        成功返回0，其他值失败
   */
  int Post(const std::string &url, int port, const std::string &post_data,
                std::string *response);

  /**
   * 返回出错原因
   * @param ec(in) get或post返回值
   * @return       错误常量字符串
   */
  static const char *StringError(int ec);

 private:
  bool Init();
  void Destroy();

  CURL* curl_;
  struct curl_slist* chunk_;
};

} // namespace bbts
#endif  // OP_OPED_NOAH_TOOLS_BBTS_CURL_H_

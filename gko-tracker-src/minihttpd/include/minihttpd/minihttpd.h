#ifndef ARGUS_COMMON_MINIHTTPD_H_
#define ARGUS_COMMON_MINIHTTPD_H_

#include "common.h"
#include "spinlock.h"
#include "scoped_ptr.h"
#include <map>
#include <vector>

struct evhttp;
struct evhttp_request;
struct evhttp_bound_socket;

namespace argus {
namespace common {

class EventLoop;
class ProcessInspector;

class MiniHttpd {
public:
  // if @port binded failed, program will be aborted. be care!!!
  MiniHttpd(EventLoop* loop, uint16_t port);

  MiniHttpd(EventLoop* loop);

  // return true if @port binded successfully, otherwise return false
  bool bind(uint16_t port);

  // if @port binded faild, program will be aborted. be care!!!
  void bindOrDie(uint16_t port);

  // Set response content of request @path.
  // Safe to call from other threads.
  // return false if failed, and @retmsg contains failed reason
  bool setResponse(const string &path, const string &response, string *retmsg=NULL);

  // Set callBack for request @path
  // Safe to call from other threads.
  // return false if failed, and @retmsg contains failed reason
  typedef string (*http_callback_fn)(const string&);
  bool setCallBack(const string &path, http_callback_fn cb, string *retmsg=NULL);

  // whether logging http response content
  void enableLogging(bool on=true);

  // enable ip filtering
  void enableIpFilter(bool on=true);

  // set the timeout seconds for an HTTP request
  void setTimeout(int32_t seconds);

  int32_t timeout() const {return reqTimeout_;}

  ~MiniHttpd();
  void stop();
  struct evhttp* evHttp();

  static const string baseInfoPath;
  static const int kMaxHeaderLen = 8192;

private:
  void requestCallback(struct evhttp_request*);
  static void requestCallback(struct evhttp_request*, void*);

  void parseUri(const char *uri, string *path, string *query, string *fragment);
  bool checkPath(const string &path);
  void initPathCharacterMap();

  void getSupportedUri(string *response);
  bool getFromLocal   (const string &path, string *response);
  bool getFromCallBack(const string &path, const string &query, string *response);

  bool inAllowIps(string ipstr);

private:
  bool enableLogging_;
  bool enableIpFilter_;
  std::vector<uint32_t> ipRange_;

  scoped_ptr<ProcessInspector> processInspector_;
  EventLoop *loop_;
  struct evhttp* const evhttp_;
  struct evhttp_bound_socket *boundSocket_;
  int32_t reqTimeout_;
  std::map<char, int8_t> pathCharacterMap_;

  std::map<string, http_callback_fn> uriHandlerMap_;
  std::map<string, string> responseMap_;
  SpinLock lock_;

  ARGUS_DISALLOW_EVIL_CONSTRUCTORS(MiniHttpd);
};

} // namespace common
} // namespace argus

#endif  // ARGUS_COMMON_MINIHTTPD_H_


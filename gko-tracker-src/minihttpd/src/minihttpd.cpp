#include "minihttpd/minihttpd.h"
#include "minihttpd/eventloop.h"
#include "minihttpd/process_inspector.h"
#include "minihttpd/strutil.h"

#include <stdio.h>
#include <event2/buffer.h>
#include <event2/http.h>
#include <event2/http_struct.h>

namespace argus {
namespace common {

static uint32_t ipToUint32(const string &ipstr) {
  uint32_t ret, a, b, c, d;
  sscanf(ipstr.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d);
  ret = (a<<24) + (b<<16) + (c<<8) + d;
  return ret;
}

const string MiniHttpd::baseInfoPath = "baseinfo";

MiniHttpd::MiniHttpd(EventLoop* loop, uint16_t port)
  : enableLogging_(true),
    enableIpFilter_(false),
    processInspector_(CHECK_NOTNULL(new(std::nothrow) ProcessInspector)),
    loop_(loop),
    evhttp_(CHECK_NOTNULL(::evhttp_new(loop->eventBase()))),
    boundSocket_(NULL),
    reqTimeout_(30)
{
  // CHECK_EQ(::evhttp_set_cb(http_, "/hello", requestCallback, this), 0);
  // libevent lookup uri by linear searching,
  // so we provide general callback only.
  boundSocket_ = ::evhttp_bind_socket_with_handle(evhttp_, "0.0.0.0", port);
  CHECK_NOTNULL(boundSocket_);
  ::evhttp_set_max_headers_size(evhttp_, MiniHttpd::kMaxHeaderLen);
  ::evhttp_set_allowed_methods(evhttp_, EVHTTP_REQ_GET);
  ::evhttp_set_timeout(evhttp_, reqTimeout_);
  ::evhttp_set_gencb(evhttp_, requestCallback, this);

  initPathCharacterMap();
  processInspector_->registerCallBacks(this);
}

MiniHttpd::MiniHttpd(EventLoop* loop)
  : enableLogging_(true),
    enableIpFilter_(false),
    processInspector_(CHECK_NOTNULL(new(std::nothrow) ProcessInspector)),
    loop_(loop),
    evhttp_(CHECK_NOTNULL(::evhttp_new(loop->eventBase()))),
    boundSocket_(NULL),
    reqTimeout_(30)
{
  ::evhttp_set_max_headers_size(evhttp_, MiniHttpd::kMaxHeaderLen);
  ::evhttp_set_allowed_methods(evhttp_, EVHTTP_REQ_GET);
  ::evhttp_set_timeout(evhttp_, reqTimeout_);
  ::evhttp_set_gencb(evhttp_, requestCallback, this);

  initPathCharacterMap();
  processInspector_->registerCallBacks(this);
}

bool MiniHttpd::bind(uint16_t port) {
  if (NULL == boundSocket_) {
    boundSocket_ = ::evhttp_bind_socket_with_handle(evhttp_, "0.0.0.0", port);
  }
  return (boundSocket_) ? true : false;
}

void MiniHttpd::bindOrDie(uint16_t port) {
  if (NULL == boundSocket_) {
    boundSocket_ = ::evhttp_bind_socket_with_handle(evhttp_, "0.0.0.0", port);
  }
  CHECK_NOTNULL(boundSocket_);
}

void MiniHttpd::enableLogging(bool flag) {
  enableLogging_ = flag;
}

// ipRange_ record ip range <start, end>
// |-----------------------------------------------|
// | <start | end> | <start | end> | <start | end> |
// |-----------------------------------------------|
//
void MiniHttpd::enableIpFilter(bool flag) {
  enableIpFilter_ = flag;
  if (enableIpFilter_ && ipRange_.empty()) {
    // http://en.wikipedia.org/wiki/Private_network
    ipRange_.push_back(ipToUint32("10.0.0.0"));
    ipRange_.push_back(ipToUint32("10.255.255.255"));

    ipRange_.push_back(ipToUint32("172.16.0.0"));
    ipRange_.push_back(ipToUint32("172.31.255.255"));

    ipRange_.push_back(ipToUint32("192.168.0.0"));
    ipRange_.push_back(ipToUint32("192.168.255.255"));

    ipRange_.push_back(ipToUint32("127.0.0.0"));
    ipRange_.push_back(ipToUint32("127.255.255.255"));
  }
}

void MiniHttpd::setTimeout(int32_t seconds) {
  assert(seconds > 0);
  reqTimeout_ = seconds;
  ::evhttp_set_timeout(evhttp_, reqTimeout_);
}

MiniHttpd::~MiniHttpd() {
  ::evhttp_free(evhttp_);
}

void MiniHttpd::stop() {
  ::evhttp_del_accept_socket(evhttp_, boundSocket_);
}

struct evhttp* MiniHttpd::evHttp() {
   return evhttp_;
}

bool MiniHttpd::inAllowIps(string ipstr) {
  uint32_t ip = ipToUint32(ipstr);
  for (unsigned i = 0; i < ipRange_.size(); i += 2) {
    if (ipRange_[i] <= ip && ip <= ipRange_[i+1]) {
      return true;
    }
  }
  return false;
}

void MiniHttpd::requestCallback(struct evhttp_request* req) {
  // see <libevent/include/event2/http.h>
  // scheme://[[userinfo]@]foo.com[:port]]/[path][?query][#fragment]
  // and relative-refs like
  // [path][?query][#fragment]

  LOG(INFO) << "request from " << req->remote_host << ":"
            <<  req->remote_port << ", uri " << req->uri;

  if (enableIpFilter_ && !inAllowIps(req->remote_host)) {
    const char *res = " forbidden!!!\n";
    struct evbuffer *evb = evbuffer_new();
    evbuffer_add(evb, req->remote_host, strlen(req->remote_host));
    evbuffer_add(evb, res, strlen(res));
    ::evhttp_send_reply(req, HTTP_SERVUNAVAIL, "forbidden", evb);
    evbuffer_free(evb);
    LOG(WARNING) << req->remote_host << " forbidden!!!";
    return;
  }

  if (req->type != EVHTTP_REQ_GET) {
    const char *res = "only GET method supported";
    struct evbuffer *evb = evbuffer_new();
    evbuffer_add(evb, res, strlen(res));
    ::evhttp_send_reply(req, HTTP_OK, "OK", evb);
    evbuffer_free(evb);
    LOG(WARNING) << "response to " << req->remote_host << ":"
                 << req->remote_port << ", content:\n" << res;
    return;
  }

  // see <libevent/http.c>
  // struct evhttp_uri {
  //   unsigned flags;
  //   char *scheme;   /* scheme; e.g http, ftp etc */
  //   char *userinfo; /* userinfo (typically username:pass), or NULL */ 
  //   char *host;     /* hostname, IP address, or NULL */
  //   int port;       /* port, or zero */
  //   char *path;     /* path, or "". */
  //   char *query;    /* query, or NULL */
  //   char *fragment; /* fragment or NULL */
  // };
  string response;
  string path;
  string query;
  string fragment;
  parseUri(req->uri, &path, &query, &fragment);

  if (path.empty()) {
    getSupportedUri(&response);
  }
  else if (getFromLocal(path, &response)) {
  }
  else if (getFromCallBack(path, query, &response)) {
  }

  if (response.empty()) {
      response = "not found";
  }

  // http response header
  evhttp_add_header(req->output_headers, "Server", "Noah-argus-MiniHttpd");
  evhttp_add_header(req->output_headers, "Content-Type", "text/plain; charset=UTF-8");
  evhttp_add_header(req->output_headers, "Connection", "close");

  LOG(INFO) << "response to " << req->remote_host << ":" <<  req->remote_port
            << ". path='" << path << "', query='" << query << "'";
  LOG_IF(INFO, enableLogging_) << "response content:\n" << response;
  struct evbuffer *evb = evbuffer_new();
  evbuffer_add(evb, response.data(), response.size());
  ::evhttp_send_reply(req, HTTP_OK, "OK", evb);
  evbuffer_free(evb);
}

void MiniHttpd::getSupportedUri(string *content) {
  std::map<string, http_callback_fn>::iterator cb_ite;
  std::map<string, string>::iterator ite;

  content->append("[supported uri:]\n");
  SpinLockGuard lock(lock_);
  for (ite = responseMap_.begin(); ite != responseMap_.end(); ++ite) {
    content->append(ite->first + "\n");
  }
  content->append("\n");

  for (cb_ite = uriHandlerMap_.begin(); cb_ite != uriHandlerMap_.end(); ++cb_ite) {
    content->append(cb_ite->first + "\n");
  }
}

bool MiniHttpd::getFromLocal(const string &path, string *content) {
  std::map<string, string>::iterator ite;
  SpinLockGuard lock(lock_);
  ite = responseMap_.find(path);
  if (ite != responseMap_.end()) {
    content->assign(ite->second);
    return true;
  }
  return false;
}

bool MiniHttpd::getFromCallBack(const string &path, const string &query, string *content) {
  std::map<string, http_callback_fn>::iterator cb_ite;
  http_callback_fn func;
  bool hasHandler = false;

  {
    SpinLockGuard lock(lock_);
    cb_ite = uriHandlerMap_.find(path);
    if (cb_ite != uriHandlerMap_.end()) {
      hasHandler = true;
      func = cb_ite->second;
    }
  }

  if (hasHandler) {
    (*content) = func(query);
  }
  return hasHandler;
}

// parse uri like "/monitor?a=12&b=34#bot" to serparate parts
// here I just supported uri like "/monitor?a=12&b=34"
// http://tech.49jie.com/?p=925
void MiniHttpd::parseUri(const char *uri, string *path, string *query, string *fragment) {
  string uri_str(uri);
  //StringToLower(&uri_str);

  int32_t uri_len = strlen(uri);
  if (uri_len <=0 || '/' != uri[0]) {
    return;
  }
  assert(uri_len > 0);
  assert('/' == uri[0]);

  int path_end = 0;
  for (int i = 1; i < uri_len; ++i) {
    if (uri_str[i] == '?') {
      path_end = i;
      break;
    }
  }

  if (path_end == 0) {
    *path = uri_str.substr(1);
  }
  else {
    *path = uri_str.substr(1, path_end-1);
    *query = uri_str.substr(path_end+1);
  }
}

bool MiniHttpd::setResponse(const string &path, const string &response, string *retmsg) {
  string hint("ok");
  bool ok = false;
  if (!checkPath(path)) {
    hint =  "path don't match ([A-Za-z0-9.-_]+)";
  }
  else if (response.empty()) {
    hint = "response is empty";
  }
  else {
    ok = true;
    SpinLockGuard lock(lock_);
    responseMap_[path]  = response;
  }

  if (retmsg) {
    *retmsg = hint;
  }
  return ok;
}

bool MiniHttpd::setCallBack(const string &path, http_callback_fn cb, string *retmsg) {
  string hint("ok");
  bool ok = false;
  if (!checkPath(path)) {
    hint =  "path don't match ([A-Za-z0-9.-_]+)";
  }
  else {
    ok = true;
    SpinLockGuard lock(lock_);
    uriHandlerMap_[path]  = cb;
  }
  if (retmsg) {
      *retmsg = hint;
  }
  return ok;
}

// keep simple, path must match ([A-Za-z0-9.-_]+)
void MiniHttpd::initPathCharacterMap() {
  pathCharacterMap_['.'] = 1;
  pathCharacterMap_['-'] = 1;
  pathCharacterMap_['_'] = 1;
  for (char x='a'; x <= 'z'; ++x) {
    pathCharacterMap_[x] = 1;
  }
  for (char x='A'; x <= 'Z'; ++x) {
    pathCharacterMap_[x] = 1;
  }
  for (char x='0'; x <= '9'; ++x) {
    pathCharacterMap_[x] = 1;
  }
}

bool MiniHttpd::checkPath(const string &path) {
  if (path.empty()) {
    return false;
  }
  string::const_iterator end = path.end();
  for (string::const_iterator i = path.begin(); i != end; ++i) {
    if (pathCharacterMap_.end() == pathCharacterMap_.find(*i)) {
      return false;
    }
  }
  return true;
}

void MiniHttpd::requestCallback(struct evhttp_request* req, void* obj) {
  static_cast<MiniHttpd*>(obj)->requestCallback(req);
}

} // namespace common
} // namespace argus


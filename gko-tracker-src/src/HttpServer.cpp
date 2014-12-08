#include "bbts/HttpServer.h"

#include <boost/bind.hpp>
#include "minihttpd/eventloop.h"
#include "minihttpd/minihttpd.h"

#include "bbts/StatusManager.h"

namespace bbts {

using std::string;
using ::argus::common::EventLoop;
using ::argus::common::MiniHttpd;

bool HttpServer::start(uint16_t port) {
  boost::thread t(boost::bind(&HttpServer::Run, this, port));
  thread_.swap(t);
  return g_pStatusManager->RegisterItem("monitor_status_queries", StatusManager::COUNTING_ITEM);
}

bool HttpServer::SetCallback(const string &path, callback_fn cb) {
  // detect status
  const timespec unit = { 1, 0 };
  for (int i = 0; !httpd_ && i < 10; ++i) {
      nanosleep(&unit, NULL);
  }

  if (!httpd_) {
    LOG(WARNING) << "must set httpd callback after it started";
    return false;
  }

  string retmsg;
  bool succ = httpd_->setCallBack(path, cb, &retmsg);
  if (!succ) {
    LOG(WARNING) << "set minihttpd callback " << path << " fail: " << retmsg;
  } else {
    LOG(INFO) << "set minihttpd callback " << path << " success!";
  }
  return succ;
}

void HttpServer::Run(uint16_t port) {
  loop_ = new EventLoop();
  httpd_ = new MiniHttpd(loop_, port);
  LOG(INFO) << "Httpd init, port:" << port;
  bool succ = httpd_->setCallBack("monitor_status", QueryMonitorStatusCallBack);
  assert(succ);
  LOG(INFO) << "Set CallBack(QueryMonitorStatusCallBack) for request monitor_status";
  loop_->loop();
  LOG(INFO) << "Httpd thread exit";

}

string HttpServer::QueryMonitorStatusCallBack(const string& query) {
  string result;
  LOG(INFO) << "get query monitor status request";
  g_pStatusManager->ShowAllStatistics(&result);
  g_pStatusManager->IncreaseItemCounting("monitor_status_queries");
  return result;
}

} // namespace bbts

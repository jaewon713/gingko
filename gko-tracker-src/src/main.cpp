#include <signal.h>

#include <sstream>

#include <boost/asio/ip/host_name.hpp>
#include <boost/format.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/thread.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/PosixThreadFactory.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TNonblockingServer.h>
#include <thrift/Thrift.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TSocket.h>

#include "bbts/HttpServer.h"
#include "bbts/KeyTypeRWLock.hpp"
#include "bbts/tracker/redis_conf.pb.h"
#include "bbts/RedisManager.h"
#include "bbts/StatusManager.h"
#include "Announce.h"
#include "bbts/tracker/AnnounceHandler.h"
#include "bbts/tracker/CommonConf.h"
#include "bbts/tracker/InfoHashGarbageCleaner.h"
#include "bbts/tracker/PeerHandler.h"
#include "bbts/tracker/RemotePeersSyncronizer.h"
#include "bbts/tracker/tracker_conf.pb.h"

using std::string;
using boost::bind;
using boost::scoped_ptr;
using boost::shared_ptr;
using boost::system::error_code;
using boost::thread;
using boost::thread_group;
using apache::thrift::concurrency::ThreadFactory;
using apache::thrift::concurrency::ThreadManager;
using apache::thrift::concurrency::PosixThreadFactory;
using apache::thrift::protocol::TBinaryProtocol;
using apache::thrift::protocol::TBinaryProtocolFactory;
using apache::thrift::protocol::TProtocol;
using apache::thrift::protocol::TProtocolFactory;
using apache::thrift::server::TNonblockingServer;
using apache::thrift::transport::TTransportFactory;
using apache::thrift::transport::TBufferedTransportFactory;
using apache::thrift::TException;
using apache::thrift::TProcessor;

using namespace bbts;
using namespace bbts::tracker;

#ifdef BBTS_TRACKER_VERSION
static const char VERSIONID[] = BBTS_TRACKER_VERSION;
#else
static const char VERSIONID[] = "unknown";
#endif

TNonblockingServer *g_my_server;
PeerHandler *peer_handler = NULL;

DEFINE_string(dir, "../conf", "conf file dir");
DEFINE_string(file, "tracker.conf", "conf file");
DEFINE_string(log, "../log/tracker.log", "log file name");
DEFINE_string(redis_file, "redis.conf", "redis conf file");
//DEFINE_bool(logtostderr, true, "log to std err");

static void ProcessTerminateSig(int sigNum) {
  g_my_server->stop();
}

static string ForceQuit(const string &query) {
  g_my_server->stop();
  return "have send stop singal\n";
}

static string ShowInfohashs(const string &query) {
  return peer_handler->ShowInfohashs(query);
}

static string ShowPeers(const string &query) {
  return peer_handler->ShowPeers(query);
}

static string GenerateTrackerId(int port) {
  string hostname;
  error_code ec;
  hostname = boost::asio::ip::host_name(ec);
  if (ec) {
    LOG(WARNING) << "get host name failed";
    hostname = "unknow";
  } else {
    string::size_type pos = hostname.rfind(".baidu.com");
    hostname = hostname.substr(0, pos);
  }
  DLOG(INFO) << "hostname is:" << hostname;
  return (boost::format("%s:%d") % hostname % port).str();
}

static void StartStatusItems(int32_t update_cycle_second) {
  g_pStatusManager->RegisterItem("request", StatusManager::COUNTING_ITEM);
  g_pStatusManager->RegisterItem("valid_request", StatusManager::COUNTING_ITEM);
  g_pStatusManager->RegisterItem("redis_to_write_num_master", StatusManager::NUM_ITEM);
  g_pStatusManager->RegisterItem("redis_to_write_num_slave", StatusManager::NUM_ITEM);
  g_pStatusManager->RegisterItem("redis_to_syncronize_num", StatusManager::NUM_ITEM);
  g_pStatusManager->Start(update_cycle_second);
}

static bool StartRedisManager() {
  RedisConf redis_conf;
  if (!LoadConf(FLAGS_dir + "/" + FLAGS_redis_file, &redis_conf)) {
    exit(1);
  }
  if (!g_pRedisManager->Start(redis_conf)) {
    LOG(ERROR) << "can't initialize redis";
    exit(2);
  }
  return true;
}

static void StartHttpServer(HttpServer *http_server, int httpd_port) {
  http_server->start(httpd_port);
  http_server->SetCallback("infohashs", &ShowInfohashs);
  http_server->SetCallback("peers", &ShowPeers);
  http_server->SetCallback("forcequit", &ForceQuit);
}

inline static void InitLogging(string log_path) {
  google::InitGoogleLogging(FLAGS_log.c_str());
  //FLAGS_logtostderr = 1;
  google::SetLogDestination(google::INFO, (log_path + "info_").c_str());
  google::SetLogDestination(google::WARNING, (log_path + "warning_").c_str());
  google::SetLogDestination(google::ERROR, (log_path + "error_").c_str());
  google::SetLogDestination(google::FATAL, (log_path + "fatal_").c_str());
}

inline static void SetSingalProcess() {
  signal(SIGINT,  ProcessTerminateSig);
  signal(SIGTERM, ProcessTerminateSig);
  signal(SIGQUIT, ProcessTerminateSig);
  signal(SIGHUP,  ProcessTerminateSig);
  signal(SIGPIPE, SIG_IGN);
}

int main(int argc, char ** argv) {
  google::SetVersionString(VERSIONID);
  google::ParseCommandLineFlags(&argc, &argv, true);

  TrackerConf tracker_conf;
  if (!LoadConf(FLAGS_dir + "/" + FLAGS_file, &tracker_conf)) {
    return 1;
  }

  InitLogging(tracker_conf.log_path());
  PeerInfo::set_tracker_id(GenerateTrackerId(tracker_conf.port()));

  shared_ptr<InfoHashMap> local_map(new InfoHashMap());
  shared_ptr<InfoHashMap> remote_map(new InfoHashMap());
  shared_ptr<InfoHashQueue> queue_to_syncronize(new InfoHashQueue());
  assert(local_map && remote_map && queue_to_syncronize);
  InfoHashGarbageCleaner garbage_cleaner;
  garbage_cleaner.Initialize(tracker_conf.info_hash_expire_time(), local_map, remote_map);

  StartStatusItems(tracker_conf.monitor_cycle_second());
  HttpServer http_server;
  StartHttpServer(&http_server, tracker_conf.httpd_port());
  StartRedisManager();
  thread garbage_cleaner_thread(bind(&InfoHashGarbageCleaner::ThreadFunc, &garbage_cleaner));
  thread_group remote_peers_syncronizers;
  for (int i = 0; i < tracker_conf.remote_peers_syncronizer_num(); ++i) {
    remote_peers_syncronizers.create_thread(
        bind(RemotePeersSyncronizer::ThreadFunc, queue_to_syncronize, remote_map));
  }
  LOG(INFO) << "remote_peers_syncronizers started";

  shared_ptr<AnnounceHandler> handler(new AnnounceHandler());
  peer_handler = &handler->get_peer_handler();
  peer_handler->SetInfoHashMaps(local_map, remote_map, queue_to_syncronize, tracker_conf.info_hash_expire_time());
  shared_ptr<TProcessor> processor(new AnnounceProcessor(handler));
  shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());
  shared_ptr<ThreadManager> threadManager = ThreadManager::newSimpleThreadManager(tracker_conf.thread_num());
  shared_ptr<ThreadFactory> threadFactory(new PosixThreadFactory());
  threadManager->threadFactory(threadFactory);
  g_my_server = new TNonblockingServer(processor, protocolFactory, tracker_conf.port(), threadManager);
  SetSingalProcess();

  try {
      threadManager->start();
      g_my_server->serve();
  } catch (std::exception &e) {
      LOG(ERROR) << "catch exception: " << e.what();
      return 9;
  }
  return 0;
}

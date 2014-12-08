#include "bbts/RedisManager.h"

#include <glog/logging.h>

#include "bbts/hash_crc32.h"
#include "bbts/tracker/redis_conf.pb.h"
#include "bbts/StatusManager.h"

using std::string;
using boost::bind;

typedef boost::mutex::scoped_lock ScopedLock;

namespace bbts {

RedisManager::RedisManager()
  : redis_num_(0)
  , timeout_(3)
  , terminated_(false) {}

RedisManager::~RedisManager() {}

bool RedisManager::Start(const RedisConf &redis_conf) {
  timeout_  = redis_conf.timeout();
  database_ = redis_conf.database();
  passwd_   = redis_conf.passwd();

  redis_num_ = redis_conf.host_pair_size();
  if (redis_num_ <= 0) {
    LOG(WARNING) << "No redis server found";
    return false;
  }

  for (int i = 0; i < redis_num_; ++i) {
    redis_servers_[0][i].hostname = redis_conf.host_pair(i).master_hostname();
    redis_servers_[0][i].port = redis_conf.host_pair(i).master_port();
    redis_servers_[1][i].hostname = redis_conf.host_pair(i).slave_hostname();
    redis_servers_[1][i].port = redis_conf.host_pair(i).slave_port();
  }

  DoConnection();
  threads_.create_thread(bind(&RedisManager::CheckAndConnectRedis, this));
  for (int j = 0; j < 2; ++j) {
    for (int i = 0; i < redis_num_; ++i) {
      threads_.create_thread(bind(&RedisManager::SendToRedis, this, &context_managers_[j][i]));
    }
  }

  LOG(INFO) << "RedisManager initialized";
  return true;
}

void RedisManager::Disconnect(redisContext **c) {
  redisFree(*c);
  *c = NULL;
}

bool RedisManager::ConnectRedis(const RedisServer& server, redisContext** context) {
  struct timeval  timeout = {timeout_ / 1000, timeout_ % 1000 * 1000};
  redisContext *c = redisConnectWithTimeout(server.hostname.c_str(), server.port, timeout);
  if (c->err) {
    LOG(WARNING) << "Connection error with " << server.hostname << ": " << c->errstr;
    redisFree(c);
    return false;
  }

  bool ret = false;
  redisReply* reply = reinterpret_cast<redisReply *>(redisCommand(c, "AUTH %s", passwd_.c_str()));
  if (reply != NULL) {
    if (reply->type == REDIS_REPLY_STATUS && strncmp(reply->str, "OK", 2) == 0) {
      ret = true;
    }
    freeReplyObject(reply);
  }
  if (!ret) {
    LOG(WARNING) << "Auth host:" << server.hostname << ", " <<  passwd_.c_str() << " failed";
    redisFree(c);
    return false;
  }
  LOG(INFO) << "Auth host:" << server.hostname << ", " <<  passwd_.c_str() << " pass";

  if (0 == database_.length()) {
    LOG(INFO) << "No database specified, use default 0";
    *context = c;
    return true;
  }

  ret = false;
  reply = reinterpret_cast<redisReply *>(redisCommand(c, "SELECT %s", database_.c_str()));
  if (reply != NULL) {
    if (reply->type == REDIS_REPLY_STATUS && strncmp(reply->str, "OK", 2) == 0) {
      ret = true;
    }
    freeReplyObject(reply);
  }

  if (!ret) {
    LOG(WARNING) << "select database " << database_ << " failed";
    redisFree(c);
    return false;
  }
  LOG(INFO) << "Select DB " << server.hostname << ":" << database_ << " success";
  *context = c;
  return true;
}

void RedisManager::Stop() {
  terminated_ = true;
}

uint32_t RedisManager::GetRedisIndex(const string &key) {
  uint32_t index = hash_crc32(key.c_str(), key.length(), NULL);
  return index % redis_num_;
}

bool RedisManager::AppendCommand(ContextManager *context, const string &command) {
  if (!context->flag) {
    return false;
  }
  ScopedLock lock(context->charge_mutex);
  redisAppendCommand(context->charge_context, command.c_str());
  ++context->charged_commands;
  return true;
}

bool RedisManager::PipelineCommand(const string &redis_key, const string &command) {
  LOG(INFO) << "Pipeline command: " << command;

  bool ret = false;
  uint32_t redis_index = GetRedisIndex(redis_key);
  ContextManager *context = &context_managers_[0][redis_index];
  if (AppendCommand(context, command)) {
    ret = true;
    g_pStatusManager->SetItemStatus("redis_to_write_num_master", context->charged_commands);
  } else {
    LOG(INFO) << "master redis fail, ignore command: " << command;
  }

  context = &context_managers_[1][redis_index];
  if (AppendCommand(context, command)) {
    ret = true;
    g_pStatusManager->SetItemStatus("redis_to_write_num_slave", context->charged_commands);
  } else {
    LOG(INFO) << "slave redis fail, ignore command: " << command;
  }
  return ret;
}

void RedisManager::DoConnection() {
  for (int j = 0; j < 2 ; ++j) {
    for (int i = 0; i < redis_num_; ++i) {
      ContextManager &context = context_managers_[j][i];
      if (context.flag) {
        continue;
      }

      RedisServer &server = redis_servers_[j][i];
      ScopedLock release_lock(context.release_mutex);
      ScopedLock charge_lock(context.charge_mutex);
      if (NULL != context.release_context) {
        Disconnect(&context.release_context);
      }
      if (NULL != context.charge_context) {
        Disconnect(&context.charge_context);
      }
      context.release_commands = 0;
      context.charged_commands = 0;

      if (ConnectRedis(server, &context.release_context)
          && ConnectRedis(server, &context.charge_context)) {
        context.flag = true;
        LOG(INFO) << "connect "<< server.hostname << " succes";
      } else {
        LOG(WARNING) << "Can not connect "<< server.hostname;
      }
    }
  }
}

void RedisManager::CheckAndConnectRedis() {
  LOG(INFO) << "Redis connection thread created";

  const timespec sleep_time = {2, 0};
  while (!terminated_) {
    DoConnection();
    nanosleep(&sleep_time, NULL);
  }
  LOG(INFO) << "Redis connection thread exit";
}


bool RedisManager::FlushPipeline(ContextManager* context) {
  redisReply* reply = NULL;
  //LOG(INFO) << "flush redis commands: " << manager->release_commands;
  for (int i = 0; i < context->release_commands; ++i) {
    redisGetReply(context->release_context, reinterpret_cast<void**>(&reply));
    if (!reply) {
      LOG(WARNING) << "Got NULL reply:" << context->release_context->errstr;
      return false;
    }
    if (REDIS_REPLY_ERROR == reply->type) {
      LOG(WARNING) << "Got reply error: " << reply->str;
      freeReplyObject(reply);
      return false;
    }
    freeReplyObject(reply);
  }
  return true;
}


void RedisManager::SendToRedis(ContextManager* context) {
  LOG(INFO) << "Redis pipeline thread created";
  const timespec sleep_time = {0, 250000000};
  while (!terminated_) {
    int release_commands = 0;
    if (context->flag) {
      ScopedLock lock(context->release_mutex);
      if (FlushPipeline(context)) {
        ScopedLock lock(context->charge_mutex);
        std::swap(context->release_context, context->charge_context);
        context->release_commands = context->charged_commands;
        context->charged_commands = 0;
        release_commands = context->release_commands;
      } else {
        context->flag = false;
      }
    }
    if (release_commands < 1) {
      nanosleep(&sleep_time, NULL);
    }
  }
  LOG(INFO) << "Redis pipeline thread exited";
}

struct SyncContextManagers {
 public:
  SyncContextManager* GetMasterContext(int index) {
    return &sync_context_managers_[0][index];
  }

  SyncContextManager* GetSlaveContext(int index) {
    return &sync_context_managers_[1][index];
  }

 private:
  // constructed from redis servers, muiltiple servers here
  // just in case redis server crash
  SyncContextManager sync_context_managers_[2][MAX_REDIS_SERVERS];
};

bool RedisManager::ExcuteCommandInternal(const RedisServer& server,
                                         SyncContextManager *context,
                                         const string &command,
                                         redisReply** reply) {
  if (!context->flag) {
    if (!ConnectRedis(server, &context->execute_context)) {
      return false;
    }
    context->flag = true;
  }

  *reply = reinterpret_cast<redisReply*>(redisCommand(context->execute_context, command.c_str()));
  if (*reply == NULL) {
    LOG(WARNING) << "Got NULL reply for " << command << ": " << context->execute_context->errstr;
    context->flag = false;
    return false;
  }
  if (REDIS_REPLY_ERROR == (*reply)->type) {
    LOG(WARNING) << "Got reply error: " << (*reply)->str << "for " << command;
    freeReplyObject(*reply);
    *reply = NULL;
    context->flag = false;
    return false;
  }
  return true;
}

bool RedisManager::ExecuteCommand(const string &redis_key,
                                  const string &command,
                                  redisReply** reply) {
  LOG(INFO) << "execute command: " << command;

  int index = GetRedisIndex(redis_key);
  SyncContextManagers *managers = ThreadSingleton<SyncContextManagers>::instance();
  if (ExcuteCommandInternal(redis_servers_[0][index],
                            managers->GetMasterContext(index),
                            command,
                            reply)) {
    return true;
  }

  // try slave redis
  if (ExcuteCommandInternal(redis_servers_[1][index],
                            managers->GetSlaveContext(index),
                            command,
                            reply)) {
    return true;
  }
  return false;
}

void RedisManager::FreeReplyObject(redisReply** reply) {
  freeReplyObject(*reply);
  *reply = NULL;
}

}  // namespace bbts

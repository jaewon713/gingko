#include "bbts/tracker/RemotePeersSyncronizer.h"

#include <boost/unordered_set.hpp>
#include <boost/weak_ptr.hpp>
#include <glog/logging.h>

#include "bbts/KeyTypeRWLock.hpp"
#include "bbts/RedisCommandCreator.h"
#include "bbts/RedisManager.h"
#include "bbts/tracker/PeerHandler.h"

using std::string;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::unordered_set;

namespace bbts {
namespace tracker {

/*
 * used to syncronize the random seeds to redis to be shared by other district
 */
void RemotePeersSyncronizer::ThreadFunc(shared_ptr<InfoHashQueue> queue_to_syncronize,
                                        shared_ptr<InfoHashMap> remote_map) {
  RedisManager *redis_manager = g_pRedisManager;
  assert(queue_to_syncronize && remote_map && redis_manager);
  unordered_set<string> remote_setdiff;
  while (true) {
    uint32_t info_hash_num = RemotePeersSyncronizer::MAX_HANDLE_INFO_HASHS;
    remote_setdiff.clear();
    {
      boost::mutex::scoped_lock lock(queue_to_syncronize->mutex);
      while (!queue_to_syncronize->have_query) {
        queue_to_syncronize->cond.wait(lock);
      }

      QueryMap::iterator it  = queue_to_syncronize->query_map.begin();
      for (; info_hash_num > 0 && it != queue_to_syncronize->query_map.end(); ++it) {
        if (it->second == true) {
            continue;
        }
        it->second = true;
        remote_setdiff.insert(it->first);
        --info_hash_num;
      }
      if (info_hash_num > 0) {
        queue_to_syncronize->have_query = false;
        continue;
      }
    }

    unordered_set<string>::iterator to_query_string_iterator = remote_setdiff.begin();
    for (; to_query_string_iterator != remote_setdiff.end(); ++to_query_string_iterator) {
      redisReply *reply = NULL;
      string command;
      string info_hash_key;
      string info_hash = *to_query_string_iterator;
      PeerHandler::ConstructInfoHashKey(*to_query_string_iterator, &info_hash_key);
      RedisCommandCreator::GetHashValueInfo(info_hash_key, &command);
      redis_manager->ExecuteCommand(info_hash, command, &reply);
      RemotePeersSyncronizer::UpdateInfoHashMapByReply(*to_query_string_iterator, reply, remote_map);
      if (reply) {
        redis_manager->FreeReplyObject(&reply);
      }
    }

    to_query_string_iterator = remote_setdiff.begin();
    {
      boost::mutex::scoped_lock lock(queue_to_syncronize->mutex);
      for (; to_query_string_iterator != remote_setdiff.end(); ++to_query_string_iterator) {
          queue_to_syncronize->query_map.erase(*to_query_string_iterator);
      }
    }
  }
}

void RemotePeersSyncronizer::UpdateInfoHashMapByReply(const string &key, redisReply *reply,
                                                      const shared_ptr<InfoHashMap> &info_hash_map) {
  if (reply == NULL) {
    LOG(ERROR)<<"reply NULL when UpdateInfoHashMapByReply";
    return;
  }

  if (reply->type != REDIS_REPLY_ARRAY) {
    LOG(ERROR)<<"reply is not REDIS_REPLY_ARRAY when UpdateInfoHashMapByReply";
    return;
  }
  LOG(INFO) << "reply peers num: " << reply->elements;

  shared_ptr<SingleInfoHashInfo> single_info_pointer(new SingleInfoHashInfo());
  assert(single_info_pointer);
  for (size_t i = 0; i < reply->elements; ++i) {
    string value_string = reply->element[i]->str;
    string decode_string;
    if (!Base64Decode(value_string, &decode_string)) {
      continue;
    }
    shared_ptr<PeerInfo> peer_info_pointer(new PeerInfo());
    assert(peer_info_pointer);
    try {
      peer_info_pointer->ConstructPeerInfoByString(decode_string);
    } catch (std::exception &e) {
      LOG(ERROR) << "construct peer from remote failed: " << e.what();
      continue;
    }
    single_info_pointer->peer_map[peer_info_pointer->GetBase64PeerId()] = peer_info_pointer;
    single_info_pointer->peer_vec.push_back(weak_ptr<PeerInfo>(peer_info_pointer));
    if (peer_info_pointer->IsASeed()) {
        single_info_pointer->peer_seed_vec.push_back(weak_ptr<PeerInfo>(peer_info_pointer));
    }
  }

  {
    KeyTypeRWLock<RemoteInfoHashMap> lock("remote_info_hash_map");
    // 用swap而不用erase使得老的远程peers在single_info_pointer析构时进行析构，减少锁的时间（single_info_pointer中的peers可能量很大）
    (*info_hash_map)[key].swap(single_info_pointer);
  }
}

}  // namespace tracker
}  // namespace bbts

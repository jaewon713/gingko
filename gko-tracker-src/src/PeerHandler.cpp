#include "bbts/tracker/PeerHandler.h"

#include <climits>

#include <boost/format.hpp>
#include <glog/logging.h>

#include "bbts/encode.h"
#include "bbts/KeyTypeRWLock.hpp"
#include "bbts/Random.h"
#include "bbts/RedisCommandCreator.h"
#include "bbts/RedisManager.h"
#include "bbts/StatusManager.h"
#include "tracker_types.h"
#include "bbts/tracker/TrackerErrorCategory.h"

using std::string;
using std::numeric_limits;
using boost::shared_ptr;
using boost::system::error_code;
using boost::weak_ptr;

namespace bbts {
namespace tracker {

PeerHandler::PeerHandler() : info_hash_expire_second_(3600) {
  //blank implement
}

void PeerHandler::ControlByInfohash(const string &infohash, int type, error_code &ec) {
  shared_ptr<SingleInfoHashInfo> single_info_pointer;
  KeyTypeRWLock<LocalInfoHashMap> lock("local_info_hash_map");
  InfoHashMap::iterator info_hash_map_iterator = local_info_hash_map_->find(infohash);
  if (info_hash_map_iterator == local_info_hash_map_->end()) {
    ec.assign(errors::INFOHASH_NOT_FOUND, get_error_category());
    return;
  }

  shared_ptr<SingleInfoHashInfo> &single_info = info_hash_map_iterator->second;
  single_info->timestamp = time(NULL);
  switch (type) {
    case STOP_DOWNLOADING:
      single_info->need_stop = true;
      break;

    case RESUME_DOWNLOAD:
      single_info->need_stop = false;
      break;

    default:
      ec.assign(errors::INVALID_OPERAT_TYPE, get_error_category());
      break;
  }
  return;
}

int PeerHandler::Handle(const PeerInfo &input_peer, PeerList *peer_list_output, bool &have_seed) {
  int ret = UpdatePeer(input_peer);
  if (ret) {
    return ret;
  }

  if (input_peer.CheckNeedMorePeers() == true) {
    ret = ChoosePeers(input_peer, peer_list_output, have_seed);
  }
  return ret;
}

void PeerHandler::SetInfoHashMaps(const shared_ptr<InfoHashMap> &local_map,
                                  const shared_ptr<InfoHashMap> &remote_map,
                                  const shared_ptr<InfoHashQueue> &queue,
                                  uint32_t expire_second) {
  local_info_hash_map_ = local_map;
  remote_info_hash_map_ = remote_map;
  info_hash_queue_ = queue;
  info_hash_expire_second_ = expire_second;
}

void PeerHandler::PreHandleInfoHashInfo(const PeerInfo &input_peer,
                                        shared_ptr<SingleInfoHashInfo> &single_info) {
  const string &info_hash = input_peer.GetBase64InfoHash();
  InfoHashMap::iterator info_hash_map_iterator = local_info_hash_map_->find(info_hash);
  if (info_hash_map_iterator == local_info_hash_map_->end()) {
    single_info.reset(new SingleInfoHashInfo());
    assert(single_info);
    (*local_info_hash_map_)[info_hash] = single_info;
  }else {
    single_info = info_hash_map_iterator->second;
    single_info->timestamp = time(NULL);
  }
}

void PeerHandler::PreHandlePeerInfo(const PeerInfo &input_peer,
                                    const shared_ptr<SingleInfoHashInfo> &single_info_pointer,
                                    boost::shared_ptr<PeerInfo> &peer_info,
                                    bool *peer_is_new) {
  assert(peer_is_new);
  *peer_is_new = false;
  SinglePeerMap &peer_map = single_info_pointer->peer_map;
  SinglePeerMap::iterator single_peer_map_iterator = peer_map.find(input_peer.GetBase64PeerId());
  if (single_peer_map_iterator == peer_map.end() && !input_peer.CheckToStop()) {
    *peer_is_new = true;
    peer_info.reset(new PeerInfo());
    assert(peer_info);
    peer_map[input_peer.GetBase64PeerId()] = peer_info;
  } else if (single_peer_map_iterator != peer_map.end()) {
    if (input_peer.CheckToStop()) {
      if (single_peer_map_iterator->second->GetIsSetRemote()) {// delete the peers in redis
        if (0 == DeletePeerRemote(input_peer)) {
          --single_info_pointer->remote_peer_count;
          LOG(INFO) << "start to delete seed remote:" << input_peer.GetBase64InfoHash().c_str()
                    << ", now: " << single_info_pointer->remote_peer_count;
        }
      }
      peer_map.erase(single_peer_map_iterator);
      ++single_info_pointer->complete_num;
      return;
    }
    peer_info = single_peer_map_iterator->second;
  }
}

int PeerHandler::UpdatePeer(const PeerInfo &input_peer) {

  shared_ptr<SingleInfoHashInfo> single_info_pointer;
  {
    // get infohash level
    KeyTypeRWLock<LocalInfoHashMap> lock("local_info_hash_map");
    PreHandleInfoHashInfo(input_peer, single_info_pointer);
  }

  // get peer info from infohash level
  shared_ptr<PeerInfo> peer_info;
  bool peer_is_new = false;
  {
    KeyTypeRWLock<LocalInfoHashMap> lock(input_peer.GetBase64InfoHash());
    PreHandlePeerInfo(input_peer, single_info_pointer, peer_info, &peer_is_new);
  }
  if (!peer_info) {// stop message
    return 0;
  }

  // if global need stop, return magic num to tell client stop download except seed client
  if (single_info_pointer->need_stop && !input_peer.IsASeed()) {
    return 26234;
  }

  {
    // we must lock vector because can't fetch this peer before it updated.
    KeyTypeRWLock<LocalInfoHashMap> lock(input_peer.GetBase64InfoHash() + "weak_vector");
    peer_info->UpdateInfo(input_peer);
    if (peer_is_new) {// new peer will push to vector
      string idc = input_peer.GetIdc();
      if (single_info_pointer->idc_to_peer_vector.find(idc)
          == single_info_pointer->idc_to_peer_vector.end()) {
        shared_ptr<SinglePeerVec> single_peer_vector(new SinglePeerVec());
        single_info_pointer->idc_to_peer_vector[idc] = single_peer_vector;
      }
      shared_ptr<SinglePeerVec> single_peer_vector = single_info_pointer->idc_to_peer_vector[idc];
      single_peer_vector->push_back(weak_ptr<PeerInfo>(peer_info));
      if (peer_info->IsASeed()) {
        if (single_info_pointer->idc_to_seed_vector.find(idc)
            == single_info_pointer->idc_to_seed_vector.end()) {
          shared_ptr<SinglePeerVec> single_seed_vector(new SinglePeerVec());
          single_info_pointer->idc_to_seed_vector[idc] = single_seed_vector;
        }
        shared_ptr<SinglePeerVec> single_seed_vector = single_info_pointer->idc_to_seed_vector[idc];
        single_seed_vector->push_back(weak_ptr<PeerInfo>(peer_info));
      }
    }
  }

  uint64_t probability = 1 << single_info_pointer->remote_peer_count;
  bool if_set_peer_remote = Random::GetRandomNumber(probability) <= 1;
  //printf("is_seed: %d; if_set_peer_remote: %d, is_seeding: %d\n", input_peer.IsASeed(), if_set_peer_remote, input_peer.IsSeeding());
  if (peer_info->GetIsSetRemote() == true || input_peer.IsASeed()
          || (input_peer.IsSeeding() && if_set_peer_remote == true)) {
    if (0 == SetPeerRemote(input_peer)) {
      KeyTypeRWLock<LocalInfoHashMap> lock(input_peer.GetBase64InfoHash().c_str());
      peer_info->SetIsSetRemote(true);
      if (peer_is_new) {
          ++single_info_pointer->remote_peer_count;
      }
      LOG(INFO) << "start to set seed remote:" << input_peer.GetBase64InfoHash().c_str()
                << ", now:" << single_info_pointer->remote_peer_count;
    }
  }
  return 0;
}

void PeerHandler::ConstructInfoHashKey(const string &base64_info_hash, string *key) {
  assert(key);
  key->append("info_hash_key_");
  key->append(base64_info_hash.c_str());
}

void PeerHandler::ConstructPeerKey(const string &base64_peer_id, string *key) {
  assert(key);
  key->append(base64_peer_id.c_str());
}

int PeerHandler::SetPeerRemote(const PeerInfo &input_peer) {
  int ret = 0;
  string serilized_peer_info;
  input_peer.ConstructStringByPeerInfo(&serilized_peer_info);
  string encode_string;
  if (!Base64Encode(serilized_peer_info, &encode_string)) {
    LOG(INFO)<<"base64 encode serialized peer info failed";
    return -1;
  }
try {
  string command;
  string info_hash_key, peer_id_key;
  ConstructInfoHashKey(input_peer.GetBase64InfoHash(), &info_hash_key);
  ConstructPeerKey(input_peer.GetBase64PeerId(), &peer_id_key);
  ret = RedisCommandCreator::SetHashOneItemInfo(info_hash_key,
                                                peer_id_key,
                                                encode_string,
                                                &command);
  g_pRedisManager->PipelineCommand(input_peer.GetBase64InfoHash(), command);
  ret = ret | RedisCommandCreator::SetExpire(info_hash_key,
                                             info_hash_expire_second_,
                                             &command);
  g_pRedisManager->PipelineCommand(input_peer.GetBase64InfoHash(), command);
} catch (std::exception &e) {
  LOG(WARNING) << "catch exception " << e.what();
}
  return ret;
}

int PeerHandler::DeletePeerRemote(const PeerInfo &input_peer) {
  string info_hash_key, peer_id_key;
  string command;
  ConstructInfoHashKey(input_peer.GetBase64InfoHash(), &info_hash_key);
  ConstructPeerKey(input_peer.GetBase64PeerId(), &peer_id_key);
  int ret = RedisCommandCreator::DeleteOneHashItem(info_hash_key.c_str(),
                                                   peer_id_key.c_str(),
                                                   &command);
  g_pRedisManager->PipelineCommand(input_peer.GetBase64InfoHash(), command);
  return ret;
}

int PeerHandler::ChoosePeers(const PeerInfo &input_peer,
                             PeerList *peer_list_output, bool &have_seed) {
  assert(peer_list_output);
  peer_list_output->clear();

  shared_ptr<SingleInfoHashInfo> info_pointer;
  string map_key = "local_info_hash_map";
  {
    KeyTypeRWLock<LocalInfoHashMap> lock(map_key.c_str(), 'r');
    InfoHashMap::const_iterator info_hash_map_iter = local_info_hash_map_->find(input_peer.GetBase64InfoHash());
    if (info_hash_map_iter != local_info_hash_map_->end()) {
      info_pointer = info_hash_map_iter->second;
    }
  }
  {
    KeyTypeRWLock<LocalInfoHashMap> lock(input_peer.GetBase64InfoHash() + "weak_vector");
    if (info_pointer) {
      VisitIdcToPeers(input_peer, info_pointer->idc_to_peer_vector, peer_list_output,
                      have_seed, map_key, input_peer.GetWantNumber());

      if (!have_seed && info_pointer->idc_to_seed_vector.size() > 0) {
        VisitIdcToPeers(input_peer, info_pointer->idc_to_seed_vector, peer_list_output,
                        have_seed, map_key, 1);
      }
    }
  }

  // if fetch seed peer in local, no need to fetch seed peer from remote
  if (input_peer.IsASeed() || (peer_list_output->size() > 0 && have_seed)) {
    return 0;
  }

  // fetch remote seed peer
  AddNeedInfoHash(input_peer);
  map_key = "remote_info_hash_map";
  info_pointer.reset();
  {
    KeyTypeRWLock<RemoteInfoHashMap> lock(map_key.c_str(), 'r');
    InfoHashMap::const_iterator info_hash_map_iter = remote_info_hash_map_->find(input_peer.GetBase64InfoHash());
    if (info_hash_map_iter != remote_info_hash_map_->end()) {
      info_pointer = info_hash_map_iter->second;
    }
  }
  {
    KeyTypeRWLock<RemoteInfoHashMap> lock(input_peer.GetBase64InfoHash() + "weak_vector");
    if (info_pointer) {
      if (peer_list_output->size() < 10) {
        ChoosePeersByMap(input_peer, peer_list_output, have_seed, info_pointer->peer_vec, map_key,
                         10 - peer_list_output->size());
      }
      if (!have_seed && !info_pointer->peer_seed_vec.empty()) {
        ChoosePeersByMap(input_peer, peer_list_output, have_seed, info_pointer->peer_seed_vec,
                         map_key, 1);
      }
    }
  }

  return 0;
}

int PeerHandler::VisitIdcToPeers(const PeerInfo &input_peer,
                                 IdcToSingleVector &idc_map,
                                 PeerList *peer_list_output,
                                 bool &if_seed_inserted,
                                 const string &map_key,
                                 int num_want) {
  assert(peer_list_output);

  shared_ptr<SinglePeerVec> single_peer_vector;
  PeerList local_output_list;
  int num_left = num_want;
  if (idc_map.find(input_peer.GetIdc()) != idc_map.end()) {
    single_peer_vector = idc_map[input_peer.GetIdc()];
    ChoosePeersByMap(input_peer, &local_output_list, if_seed_inserted, *single_peer_vector, map_key, num_left);
  }
  num_left = num_want - local_output_list.size();
  IdcToSingleVector::iterator iter = idc_map.begin();
  int random_index = Random::GetRandomNumber(idc_map.size() - 1);
  int now_index = -1;
  for (; num_left > 0 && iter != idc_map.end(); iter++) {
    now_index++;
    if (random_index < now_index) {
      continue;
    }
    string idc = static_cast<string>(iter->first);
    if (idc == input_peer.GetIdc()) {
      continue;
    }
    single_peer_vector = iter->second;
    ChoosePeersByMap(input_peer, &local_output_list, if_seed_inserted, *single_peer_vector, map_key, num_left);
    num_left = num_want - local_output_list.size();
  }
  PeerList::iterator vec_iter = local_output_list.begin();
  for (; vec_iter != local_output_list.end(); vec_iter++) {
    peer_list_output->push_back(*vec_iter);
  }
  return 0;
}

int PeerHandler::ChoosePeersByMap(const PeerInfo &input_peer,
                                  PeerList *peer_list_output,
                                  bool &if_seed_inserted,
                                  SinglePeerVec &peer_vec,
                                  const string &map_key,
                                  int num_want) {
  assert(peer_list_output);
  int peer_vec_size = peer_vec.size();
  int random_index = Random::GetRandomNumber(peer_vec_size - 1);
  for (int i = 0; i < peer_vec_size && num_want > 0;) {
    int peer_index = (i + random_index) % peer_vec_size;
    shared_ptr<PeerInfo> peer_info = peer_vec[peer_index].lock();
    if (peer_info) {
      ++i;
      if (peer_info->Initialized() == false) {
        LOG(WARNING)<<"peer_info is not initialized!!!";
        continue;
      }
      if (input_peer.GetBase64PeerId() == peer_info->GetBase64PeerId()) {
        //printf("find a peerid equal input: %s", input_peer.GetBase64PeerId().c_str());
        continue;
      }
      if (if_seed_inserted == false && peer_info->IsASeed()) {
        if_seed_inserted = true;
      }
      peer_list_output->push_back(*peer_info);
      --num_want;
    } else {
      SinglePeerVec::iterator peer_it = peer_vec.begin() + peer_index;
      peer_vec.erase(peer_it);
      peer_vec_size = peer_vec.size();
    }
  }
  return 0;
}

/* used to add need infohash to remote syncronizer */
void PeerHandler::AddNeedInfoHash(const PeerInfo &input_peer) {
  bool querying = false;
  boost::mutex::scoped_lock lock(info_hash_queue_->mutex);
  QueryMap::iterator query = info_hash_queue_->query_map.find(input_peer.GetBase64InfoHash());
  if (query == info_hash_queue_->query_map.end()) {
      info_hash_queue_->query_map[input_peer.GetBase64InfoHash()] = false;
      g_pStatusManager->SetItemStatus("redis_to_syncronize_num", info_hash_queue_->query_map.size());
  } else {
      querying = query->second;
  }

  if (!querying) {
      info_hash_queue_->have_query = true;
      info_hash_queue_->cond.notify_one();
  }
}

string PeerHandler::ShowInfohashs(const string &query) {
  int max_infohash_num = 100;
  if (!query.empty()) {
    max_infohash_num = atoi(query.c_str());
    max_infohash_num = max_infohash_num < 0 ? numeric_limits<int>::max() : max_infohash_num;
  }

  int remote_peers_num = 0;
  string hex_infohash;
  string ret = (boost::format("all infohashs: %d\n\n") % local_info_hash_map_->size()).str();
  ret.append("lpn:  local peers num\n"
             "rpn:  remote peers num\n"
             "srn:  set remote peers num\n"
             "s:    need stop\n"
             "cnum: complete peers num\n\n\n"
             "                infohash                    lpn    rpn  timestamp    srn s   cnum\n");
  {
    boost::format infohash_format("%40s %6d %6d %10d %6d %1d %6d\n");
    KeyTypeRWLock<LocalInfoHashMap> l_lock("local_info_hash_map", 'r');
    KeyTypeRWLock<RemoteInfoHashMap> r_lock("remote_info_hash_map", 'r');
    for(InfoHashMap::iterator local_it = local_info_hash_map_->begin();
        max_infohash_num > 0 && local_it != local_info_hash_map_->end(); ++local_it, --max_infohash_num) {
      InfoHashMap::iterator remote_it = remote_info_hash_map_->find(local_it->first);
      remote_peers_num = remote_it != remote_info_hash_map_->end() ? remote_it->second->peer_map.size() : 0;
      Base64ToHex(local_it->first, &hex_infohash);
      boost::shared_ptr<SingleInfoHashInfo> &infohash_ptr = local_it->second;
      infohash_format % hex_infohash
                      % infohash_ptr->peer_map.size()
                      % remote_peers_num
                      % infohash_ptr->timestamp
                      % infohash_ptr->remote_peer_count
                      % infohash_ptr->need_stop
                      % infohash_ptr->complete_num;
      ret.append(infohash_format.str());
    }
  }
  return ret;
}

inline static void TravelsPeerMap(const shared_ptr<SingleInfoHashInfo> &peers,
                                  int max_peers_num,
                                  const string &peers_tag,
                                  string *ret) {
  if (peers) {
    ret->append((boost::format("\ntotal %s peers num: %d\n") % peers_tag % peers->peer_map.size()).str());
    ret->append("                 peerid                         ip:port        d r t     uploaded   downloaded         left tracker\n");
    boost::format peer_format("%40s %15s:%-5d %1d %1d %1d %12d %12d %12d %s\n");
    for (SinglePeerMap::const_iterator it = peers->peer_map.begin()
        ; it != peers->peer_map.end() && max_peers_num > 0; ++it, --max_peers_num) {
      const shared_ptr<PeerInfo> &peer = it->second;
      string hex_peerid;
      BytesToHex(peer->GetPeerId(), &hex_peerid);
      peer_format % hex_peerid
                  % peer->GetIp()
                  % peer->GetPort()
                  % peer->GetIsSeed()
                  % peer->GetIsSetRemote()
                  % peer->GetStatus()
                  % peer->GetUploaded()
                  % peer->GetDownloaded()
                  % peer->GetLeft()
                  % peer->GetTrackerId();
      ret->append(peer_format.str());
    }
  } else {
    ret->append("\nnot find any" + peers_tag + " peers\n");
  }
}

string PeerHandler::ShowPeers(const string &query) {
  int max_peers_num = 100;
  string base64_infohash;
  string hex_infohash;
  {
    std::string::size_type pos = query.find('&');
    if (pos != query.npos) {
      hex_infohash = query.substr(0, pos);
      max_peers_num = atoi(query.substr(pos + 1).c_str());
      max_peers_num = max_peers_num < 0 ? numeric_limits<int>::max() : max_peers_num;
    } else {
      hex_infohash = query;
    }
    if (hex_infohash.empty() || !HexToBase64(hex_infohash, &base64_infohash)) {
      return "infohash is empty or invalid!";
    }
  }

  std::string ret("mpn:  max display peers num\n"
                  "srn:  set remote peers num\n"
                  "s:    need stop\n"
                  "cnum: complete peers num\n"
                  "d:    is seed\n"
                  "r:    is set remote\n"
                  "t:    status\n\n\n"
                  "                infohash                    mpn  timestamp    srn s   cnum\n");
  {
    KeyTypeRWLock<LocalInfoHashMap> l_lock("local_info_hash_map", 'r');
    InfoHashMap::iterator local_it = local_info_hash_map_->find(base64_infohash);
    if (local_it != local_info_hash_map_->end()) {
      boost::shared_ptr<SingleInfoHashInfo> &infohash_ptr = local_it->second;
      ret.append((boost::format("%40s %6d %10d %6d %1d %6d\n") % hex_infohash
                                                             % max_peers_num
                                                             % infohash_ptr->timestamp
                                                             % infohash_ptr->remote_peer_count
                                                             % infohash_ptr->need_stop
                                                             % infohash_ptr->complete_num).str());
      TravelsPeerMap(local_it->second, max_peers_num, "local", &ret);
    } else {
      ret.append(base64_infohash);
    }
  }

  {
    KeyTypeRWLock<RemoteInfoHashMap> r_lock("remote_info_hash_map", 'r');
    InfoHashMap::iterator remote_it = remote_info_hash_map_->find(base64_infohash);
    if (remote_it != remote_info_hash_map_->end()) {
      TravelsPeerMap(remote_it->second, max_peers_num, "remote", &ret);
    }
  }
  return ret;
}

}
}

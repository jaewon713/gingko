#ifndef OP_OPED_NOAH_BBTS_TRACKER_PEER_HANDLER_H_
#define OP_OPED_NOAH_BBTS_TRACKER_PEER_HANDLER_H_

#include <boost/shared_ptr.hpp>
#include <boost/system/error_code.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/unordered_map.hpp>
#include <boost/weak_ptr.hpp>

#include "bbts/tracker/PeerInfo.h"

namespace bbts {
namespace tracker {

typedef std::vector<PeerInfo> PeerList;
typedef boost::unordered_map<std::string, boost::shared_ptr<PeerInfo> > SinglePeerMap;
typedef std::vector<boost::weak_ptr<PeerInfo> > SinglePeerVec;
typedef boost::unordered_map<std::string, boost::shared_ptr<SinglePeerVec> > IdcToSingleVector;

struct SingleInfoHashInfo {
  SingleInfoHashInfo()
    : remote_peer_count(0),
      complete_num(0),
      timestamp(time(NULL)),
      need_stop(false) {}

  //~SingleInfoHashInfo() { printf("release SingleInfoHashInfo\n"); }

  bool HasSetRemotePeer() {
    return remote_peer_count == 0;
  }

  SinglePeerMap peer_map;
  IdcToSingleVector idc_to_peer_vector;
  IdcToSingleVector idc_to_seed_vector;
  SinglePeerVec peer_vec;
  SinglePeerVec peer_seed_vec;
  uint64_t remote_peer_count;
  uint64_t complete_num;
  uint64_t timestamp;
  bool need_stop;
};

typedef boost::unordered_map<std::string, boost::shared_ptr<SingleInfoHashInfo> > InfoHashMap;
typedef InfoHashMap LocalInfoHashMap;
typedef InfoHashMap RemoteInfoHashMap;
typedef boost::unordered_map<std::string, bool> QueryMap;

struct InfoHashQueue {
  InfoHashQueue() : have_query(false) {}

  QueryMap query_map;
  bool have_query;
  boost::mutex mutex;
  boost::condition_variable cond;
};

// main operations of handle peers
class PeerHandler {
 public:
  PeerHandler();

  int Handle(const PeerInfo &input_peer, PeerList *peer_list_output, bool &have_seed);

  void SetInfoHashMaps(const boost::shared_ptr<InfoHashMap> &local_map,
                       const boost::shared_ptr<InfoHashMap> &remote_map,
                       const boost::shared_ptr<InfoHashQueue> &queue,
                       uint32_t expire_second);

  enum {
    STOP_DOWNLOADING,
    RESUME_DOWNLOAD,
  };
  void ControlByInfohash(const std::string &infohash, int type, boost::system::error_code &ec);

  std::string ShowInfohashs(const std::string &query);
  std::string ShowPeers(const std::string &query);

  static void ConstructInfoHashKey(const std::string &info_hash, std::string *key);

  static void ConstructPeerKey(const std::string &peer_id, std::string *key);

 private:
  int UpdatePeer(const PeerInfo &input_peer);
  int DeletePeerRemote(const PeerInfo &input_peer);
  int SetPeerRemote(const PeerInfo &input_peer);
  int ChoosePeers(const PeerInfo &input_peer, PeerList *peer_list_output, bool &have_seed);

  void PreHandleInfoHashInfo(const PeerInfo &input_peer,
                             boost::shared_ptr<SingleInfoHashInfo> &single_info_pointer);

  void PreHandlePeerInfo(const PeerInfo &input_peer,
                         const boost::shared_ptr<SingleInfoHashInfo> &single_info_pointer,
                         boost::shared_ptr<PeerInfo> &peer_info,
                         bool *peer_is_new);

  int VisitIdcToPeers(const PeerInfo &input_peer,
                      IdcToSingleVector &idc_map,
                      PeerList *peer_list_output,
                      bool &if_seed_inserted,
                      const std::string &map_key,
                      int num_want);

  int ChoosePeersByMap(const PeerInfo &input_peer,
                       PeerList *peer_list_output,
                       bool &if_seed_inserted,
                       SinglePeerVec &peer_vec,
                       const std::string &map_key,
                       int num_want);

  void AddNeedInfoHash(const PeerInfo &input_peer);

 private:
  boost::shared_ptr<InfoHashMap> local_info_hash_map_;
  boost::shared_ptr<InfoHashMap> remote_info_hash_map_;
  boost::shared_ptr<InfoHashQueue> info_hash_queue_;
  uint32_t info_hash_expire_second_;
};

}
}

#endif// OP_OPED_NOAH_BBTS_TRACKER_PEER_HANDLER_H_

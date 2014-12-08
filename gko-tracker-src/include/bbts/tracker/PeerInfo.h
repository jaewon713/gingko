#ifndef OP_OPED_NOAH_BBTS_TRACKER_PEER_INFO_H_
#define OP_OPED_NOAH_BBTS_TRACKER_PEER_INFO_H_

#include <glog/logging.h>

#include "bbts/encode.h"
#include "inner_types.h"

namespace bbts {
namespace tracker {

class PeerInfo {
 public:
  const static int STATUS_NONE = 0;
  const static int STATUS_GETPEER = 1;
  const static int STATUS_STARTED = 2;
  const static int STATUS_SEEDING = 3;
  const static int STATUS_STOPPED = 5;

  PeerInfo();
  PeerInfo(const PeerInfo &to_copy);

  //~PeerInfo() {printf("peerinfo:%s released!\n", base64_info_hash.c_str());}// debug内存释放时开启

  void UpdateInfo(const PeerInfo &input_peer);

  bool Initialized() const {
    if (thrift_peer.__isset.info_hash && thrift_peer.__isset.peer_id) {
      return true;
    }
    return false;
  }

  bool CheckNeedMorePeers() const {
    if (GetWantNumber() <= 0 || CheckToStop()) {
      return false;
    } else {
      return true;
    }
  }

  bool CheckToStop() const {
    return GetStatus() == tracker::Status::STOPPED;
  }

  bool IsSeeding() const {
    return GetIsSeed() || GetStatus() == tracker::Status::SEEDING;
  }

  bool IsASeed() const {
    return GetIsSeed();
  }

  const tracker::StoredPeerInfo &GetInnerPeer() const;
  tracker::StoredPeerInfo &GetInnerPeerReference();

  const std::string &GetIp () const { return thrift_peer.ip; };
  const std::string &GetIdc () const { return thrift_peer.idc; };
  const std::string &GetInfoHash () const { return thrift_peer.info_hash; };
  const std::string &GetBase64InfoHash () const { return base64_info_hash; };
  const std::string &GetPeerId () const { return thrift_peer.peer_id; };
  const std::string &GetBase64PeerId() const { return base64_peer_id; };
  uint32_t GetPort () const { return thrift_peer.port; };
  tracker::Status::type GetStatus () const { return thrift_peer.status; };
  uint64_t GetDownloaded () const { return thrift_peer.downloaded; };
  uint64_t GetUploaded () const { return thrift_peer.uploaded; };
  uint64_t GetLeft () const { return thrift_peer.left; };
  uint64_t GetExpireTime () const { return thrift_peer.expire_time; };
  uint32_t GetWantNumber () const { return thrift_peer.want_number; };
  bool GetIsSetRemote() const { return is_set_remote; };
  bool GetIsSeed() const { return thrift_peer.is_seed; };
  const std::string &GetTrackerId () const { return thrift_peer.tracker_id; }
  void SetIp(const std::string &ip) { thrift_peer.__set_ip(ip); };
  void SetIdc(const std::string &idc) { thrift_peer.__set_idc(idc); };
  void SetInfoHash(const std::string &info_hash) {
    thrift_peer.__set_info_hash(info_hash);
    if (!Base64Encode(info_hash, &base64_info_hash)) {
      base64_info_hash = info_hash;
    }
  };
  void SetPeerId(const std::string &peer_id) {
    thrift_peer.__set_peer_id(peer_id);
    if (!Base64Encode(peer_id, &base64_peer_id)) {
      base64_peer_id = peer_id;
    }
  };
  void SetPort(uint32_t port) { thrift_peer.__set_port(port); };
  void SetStatus(tracker::Status::type status) { thrift_peer.__set_status(status); };
  void SetDownloaded(uint64_t downloaded) { thrift_peer.__set_downloaded(downloaded); };
  void SetUploaded(uint64_t uploaded) { thrift_peer.__set_uploaded(uploaded); };
  void SetLeft(uint64_t left) { thrift_peer.__set_left(left); };
  void SetExpireTime(uint64_t expire_time) { thrift_peer.__set_expire_time(expire_time); };
  void SetTimeStamp(uint64_t timestamp) { thrift_peer.__set_timestamp_start(timestamp); };
  void SetWantNumber(uint32_t want_number) { thrift_peer.__set_want_number(want_number); };
  void SetIsSetRemote(bool is_set_remote) { this->is_set_remote = is_set_remote; };
  void SetIsSeed(bool is_seed) { thrift_peer.__set_is_seed(is_seed); };
  void SetTrackerId(const std::string &tracker_id) { thrift_peer.__set_tracker_id(tracker_id); }

  void ConstructPeerInfoByString(const std::string &string_input);
  void ConstructStringByPeerInfo(std::string *string_input) const;

  static std::string &GetLocalTrackerId() { return tracker_id_; }

  static void set_tracker_id(std::string tracker_id) {
    tracker_id_ = tracker_id;
  }
 private:
   tracker::StoredPeerInfo thrift_peer;
   std::string base64_info_hash;
   std::string base64_peer_id;
   bool is_set_remote;
   static std::string tracker_id_;
};

}
}
#endif // OP_OPED_NOAH_BBTS_TRACKER_PEER_INFO_H_

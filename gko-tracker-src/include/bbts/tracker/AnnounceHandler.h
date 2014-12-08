#ifndef OP_OPED_NOAH_BBTS_TRACKER_ANNOUNCE_HANDLER_
#define OP_OPED_NOAH_BBTS_TRACKER_ANNOUNCE_HANDLER_

#include "Announce.h"
#include "bbts/tracker/PeerHandler.h"

namespace bbts {
namespace tracker {

class AnnounceHandler : virtual public AnnounceIf {
 public:
  AnnounceHandler() {
    // Your initialization goes here
  }

  virtual void announce(AnnounceResponse &_return, const AnnounceRequest &_request);

  virtual void StopByInfohash(BaseResponse& _return, const ControlByInfohashRequest& request);

  virtual void ResumeByInfohash(BaseResponse& _return, const ControlByInfohashRequest& request);

  PeerHandler& get_peer_handler() {
    return peer_handler_;
  }

 private:
  void AnnounceInfo(AnnounceResponse &_return,
                    int error_code,
                    const std::string &request_tag,
                    const std::string &message);

  void AnnounceSuccess(AnnounceResponse &_return, const std::string &request_tag);

  void AnnounceError(AnnounceResponse &_return,
                     const std::string &request_tag,
                     const std::string &error_message);

  bool PreHandleRequest(const AnnounceRequest &request, PeerInfo *peer_info);

  bool ComposeResponse(const PeerList &peer_list, bool have_seed, AnnounceResponse *response);

  void ControlByInfohash(BaseResponse& _return, const ControlByInfohashRequest& request, int type);

  PeerHandler peer_handler_;
  static int MIN_INTERVAL;
};

} // namespace tracker
} // namespace bbts


#endif // OP_OPED_NOAH_BBTS_TRACKER_ANNOUNCE_HANDLER_


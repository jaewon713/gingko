include "tracker.thrift"

namespace cpp bbts.tracker

service Announce {
  tracker.AnnounceResponse announce(1:tracker.AnnounceRequest announce_request);
  tracker.BaseResponse StopByInfohash(1:tracker.ControlByInfohashRequest request);
  tracker.BaseResponse ResumeByInfohash(1:tracker.ControlByInfohashRequest request);
}

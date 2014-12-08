#ifndef OP_OPED_NOAH_BBTS_TRACKER_INFO_HASH_GARBAGE_CLEANER_
#define OP_OPED_NOAH_BBTS_TRACKER_INFO_HASH_GARBAGE_CLEANER_

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include "bbts/LazySingleton.hpp"
#include "bbts/tracker/PeerHandler.h"

namespace bbts {
namespace tracker {

class InfoHashGarbageCleaner : private boost::noncopyable {
 public:
  InfoHashGarbageCleaner() : expire_interval_(3600) {};

  void Initialize(uint64_t expire_interval,
                  const boost::shared_ptr<InfoHashMap> &local_map,
                  const boost::shared_ptr<InfoHashMap> &remote_map) {
    expire_interval_ = expire_interval;
    local_map_ = local_map;
    remote_map_ = remote_map;
  }

  void ThreadFunc();

 private:
  const static uint32_t CYCLE = 60;
  uint64_t expire_interval_;
  boost::shared_ptr<InfoHashMap> local_map_;
  boost::shared_ptr<InfoHashMap> remote_map_;

  void PeriodSchedule();
};

} // namespace tracker
} // namespace bbts

#endif // OP_OPED_NOAH_BBTS_TRACKER_INFO_HASH_GARBAGE_CLEANER_


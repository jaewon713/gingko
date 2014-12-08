#include "bbts/tracker/InfoHashGarbageCleaner.h"

#include <vector>

#include <glog/logging.h>

#include "bbts/KeyTypeRWLock.hpp"

namespace bbts {
namespace tracker {

using std::string;
using std::vector;
using boost::shared_ptr;

/*
 * used to clean the expire infohash
 */
void InfoHashGarbageCleaner::PeriodSchedule() {
  vector<string> info_hashs_to_delete;
  vector<shared_ptr<SingleInfoHashInfo> > info_pointers_to_free;
  time_t now = time(NULL);
  {
    KeyTypeRWLock<LocalInfoHashMap> lock("local_info_hash_map");
    for (InfoHashMap::iterator to_delete_local_iterator = local_map_->begin()
        ; to_delete_local_iterator != local_map_->end();) {
      string info_hash_key = to_delete_local_iterator->first;
      shared_ptr<SingleInfoHashInfo> info = to_delete_local_iterator->second;
      if (now - info->timestamp > expire_interval_ || info->peer_map.size() == 0) {
        info_hashs_to_delete.push_back(info_hash_key);
        info_pointers_to_free.push_back(info);
        to_delete_local_iterator = local_map_->erase(to_delete_local_iterator);
      } else {
        ++to_delete_local_iterator;
      }
    }
  }
  if (info_hashs_to_delete.empty()) {
      return;
  }
  {
    KeyTypeRWLock<RemoteInfoHashMap> remote_lock("remote_info_hash_map");
    for (vector<string>::iterator to_delete_string_iterator = info_hashs_to_delete.begin()
        ; to_delete_string_iterator != info_hashs_to_delete.end(); ++to_delete_string_iterator) {
      string &info_hash_key = *to_delete_string_iterator;
      InfoHashMap::iterator to_delete_remote_iterator = remote_map_->find(info_hash_key);
      if (to_delete_remote_iterator != remote_map_->end()) {
        info_pointers_to_free.push_back(to_delete_remote_iterator->second);
        remote_map_->erase(to_delete_remote_iterator);
      }
    }
  }
  // 析构info_pointers_to_free的时候，会自动析构引用计数为0的所有指针
  // 放在队列里，释放锁后再析构，减少delete时加锁的时间
  // 原来的在本线程直接delete指针可能会导致announce的线程使用delete掉的野指针导致出core
}

void InfoHashGarbageCleaner::ThreadFunc() {
  LOG(INFO) << "garbage clean run.";
  while (true) {
    sleep(CYCLE);
    PeriodSchedule();
  }
}

}  // namespace tracker
}  // namespace bbts


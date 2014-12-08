/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file module_status_manager.cpp
 * @author huangwei(huangwei02@baidu.com)
 * @date 2013/06/13 15:56:17
 **/

#include "bbts/StatusManager.h"

#include <sys/time.h>
#include <time.h>

#include <iomanip>
#include <sstream>
#include <utility>

#include <boost/thread/mutex.hpp>
#include <glog/logging.h>

using std::string;
using std::make_pair;
using boost::shared_ptr;
using boost::static_pointer_cast;

typedef boost::unique_lock<boost::shared_mutex> unique_lock;
typedef boost::shared_lock<boost::shared_mutex> shared_lock;
typedef boost::mutex::scoped_lock scoped_lock;

namespace bbts {

namespace detail {

struct Item : private boost::noncopyable {
 public:
  virtual StatusManager::ItemType type() const = 0;

 protected:
  Item() {}
  virtual ~Item() {}

  mutable boost::mutex mutex_;
};

// struct for item with string type, eg. process-version
struct StringItem : public Item {
  friend class ::bbts::StatusManager;
  StringItem() : timestamp_(0) {}

  void Update(const string &value);

  bool GetValue(int update_period, string *value) const;

  string value() const {
    return value_;
  }

  StatusManager::ItemType type() const {
    return type_;
  }

 private:
  string value_;
  time_t timestamp_;
  static const StatusManager::ItemType type_;
};

const StatusManager::ItemType StringItem::type_ = StatusManager::STRING_ITEM;

void StringItem::Update(const string &value) {
  scoped_lock lock(mutex_);
  value_ = value;
  timestamp_ = time(NULL);
}

bool StringItem::GetValue(int update_period, string *value) const {
  scoped_lock lock(mutex_);
  if (time(NULL) - timestamp_ < update_period) {
    *value = value_;
    return true;
  }
  return false;
}

// struct a num-item, eg. thread_num
struct NumItem : public Item {
  friend class ::bbts::StatusManager;
  NumItem() : value_(0.), timestamp_(0) {}

  void Update(double value);

  bool GetValue(int update_period, double *value) const;

  double value() const {
    return value_;
  }

  StatusManager::ItemType type() const {
    return type_;
  }

 private:
  double value_;
  time_t timestamp_;
  static const StatusManager::ItemType type_;
};

const StatusManager::ItemType NumItem::type_ = StatusManager::NUM_ITEM;

void NumItem::Update(double value) {
  scoped_lock lock(mutex_);
  value_ = value;
  timestamp_ = time(NULL);
}

bool NumItem::GetValue(int update_period, double *value) const {
  scoped_lock lock(mutex_);
  if (time(NULL) - timestamp_ < update_period) {
    *value = value_;
    return true;
  }
  return false;
}

// struct for accumulating items, eg. query count
struct CountingItem : public Item {
  friend class ::bbts::StatusManager;
  CountingItem()
    : current_count_per_second_(0)
    , current_count_per_cycle_(0)
    , current_accumulated_count_(0)
    , accumulated_count_(0) {}

  void IncreaseCounting(int64_t increasement);

  void Update(int duration);

  StatusManager::ItemType type() const {
    return type_;
  }

 private:
  int64_t      current_count_per_second_;
  // per collect cycle
  int64_t      current_count_per_cycle_;
  int64_t      current_accumulated_count_;

  // counting from the last check to now, increase operation used upon this
  int64_t      accumulated_count_;
  static const StatusManager::ItemType type_;
};

const StatusManager::ItemType CountingItem::type_ = StatusManager::COUNTING_ITEM;

void CountingItem::IncreaseCounting(int64_t increasement) {
  scoped_lock lock(mutex_);
  accumulated_count_ += increasement;
  current_accumulated_count_ += increasement;
}

void CountingItem::Update(int duration) {
  scoped_lock lock(mutex_);
  current_count_per_cycle_ = accumulated_count_;
  current_count_per_second_ = accumulated_count_ / duration;
  accumulated_count_ = 0;
}

// struct for latency items, eg. query latency
struct LatencyItem : public Item {
  friend class ::bbts::StatusManager;
  LatencyItem()
    : current_count_per_second_(0)
    , current_count_per_cycle_(0)
    , current_accumulated_count_(0)
    , current_max_latency_(0)
    , current_average_latency_(0)
    , current_min_latency_(0)
    , accumulated_count_(0)
    , accumulated_latency_(0)
    , max_latency_(0)
    , min_latency_(0) {}

  void InsertLatency(double latency);

  void Update(int duration);

  StatusManager::ItemType type() const {
    return type_;
  }

 private:
  int64_t      current_count_per_second_;
  int64_t      current_count_per_cycle_;
  int64_t      current_accumulated_count_;
  double       current_max_latency_;      // in unit of millisecond
  double       current_average_latency_;  // in unit of millisecond
  double       current_min_latency_;      // in unit of millisecond

  // counting from the last check to now, increase operation used upon this
  int64_t      accumulated_count_;
  double       accumulated_latency_;
  double       max_latency_;
  double       min_latency_;
  static const StatusManager::ItemType type_;
};

const StatusManager::ItemType LatencyItem::type_ = StatusManager::LATENCY_ITEM;

void LatencyItem::InsertLatency(double latency) {
  scoped_lock lock(mutex_);
  if (accumulated_count_ == 0) {
    max_latency_ = latency;
    min_latency_ = latency;
  } else {
    if (max_latency_ < latency) {
      max_latency_ = latency;
    }
    if (min_latency_ > latency) {
      min_latency_ = latency;
    }
  }
  accumulated_latency_ += latency;
  ++accumulated_count_;
  ++current_accumulated_count_;
}

void LatencyItem::Update(int duration) {
  scoped_lock lock(mutex_);
  if (accumulated_count_ == 0) {
      return;
  }

  current_count_per_cycle_ = accumulated_count_;
  current_count_per_second_ = accumulated_count_ / duration;
  current_max_latency_ = max_latency_;
  current_min_latency_ = min_latency_;
  current_average_latency_ = accumulated_latency_ / accumulated_count_;
  accumulated_count_ = 0;
  accumulated_latency_ = 0;
}

} // namespace detail

StatusManager::StatusManager()
  : terminated_(false)
  , update_period_(10) {}

StatusManager::~StatusManager() {}

bool StatusManager::Start(int period) {
  update_period_ = period;
  boost::thread t(boost::bind(&StatusManager::UpdateData, this));
  thread_.swap(t);
  return true;
}

void StatusManager::UpdateData() {
  LOG(INFO) << "UpdateData thread created";
  const timespec  unit = {0, 500000000};
  time_t timestamp = time(NULL);
  time_t last_timestamp = 0;
  while (!terminated_) {
    timestamp = time(NULL);
    if (last_timestamp != timestamp && timestamp % update_period_ == 0) {
      last_timestamp = timestamp;
      UpdateItems(update_period_);
      LOG(INFO) << "Update monitor status: " << last_timestamp;
    }
    nanosleep(&unit, NULL);
  }
  LOG(INFO) << "UpdateData thread exit";
}

void StatusManager::Stop() {
  terminated_ = true;
  thread_.join();
}

bool StatusManager::RegisterItem(const string& name, ItemType type) {
  unique_lock lock(shared_mutex_);
  if (item_map_.find(name) != item_map_.end()) {
    LOG(WARNING) << name << " has already been registered";
    return false;
  }

  shared_ptr<detail::Item> item;
  switch (type) {
    case STRING_ITEM:
      item.reset(new detail::StringItem());
      break;

    case NUM_ITEM:
      item.reset(new detail::NumItem());
      break;

    case COUNTING_ITEM:
      item.reset(new detail::CountingItem());
      break;

    case LATENCY_ITEM:
      item.reset(new detail::LatencyItem());
      break;

    default:
      LOG(WARNING) << "type: " << type << " can't be recognized";
      return false;
  }
  item_map_.insert(make_pair(name, item));
  return true;
}

bool StatusManager::UnregisterItem(const string& name) {
  unique_lock lock(shared_mutex_);
  ItemMap::iterator item_it = item_map_.find(name);
  if (item_it == item_map_.end()) {
    LOG(WARNING) << name << " has not been registered";
    return false;
  }
  item_map_.erase(item_it);
  return true;
}

template <typename Item>
bool StatusManager::GetItem(const string &key, shared_ptr<Item> *item) const {
  shared_lock lock(shared_mutex_);
  ItemMap::const_iterator iter = item_map_.find(key);
  if (iter == item_map_.end()) {
    LOG(WARNING) << key << " not registered";
    return false;
  }
  if (iter->second->type() != Item::type_) {
    LOG(WARNING) << key << " doesn't have type " << Item::type_;
    return false;
  }
  *item = static_pointer_cast<Item>(iter->second);
  return true;
}

bool StatusManager::SetItemStatus(const string& key, const string& value) {
  shared_ptr<detail::StringItem> item;
  if (!GetItem(key, &item)) {
    return false;
  }

  item->Update(value);
  return true;
}


bool StatusManager::SetItemStatus(const string& key, double value) {
  shared_ptr<detail::NumItem> item;
  if (!GetItem(key, &item)) {
    return false;
  }

  item->Update(value);
  return true;
}


bool StatusManager::GetItemStatus(const string& key, string* value) const {
  shared_ptr<detail::StringItem> item;
  if (!GetItem(key, &item)) {
    return false;
  }

  if (!item->GetValue(update_period_, value)) {
    LOG(WARNING) << key << " expired";
    return false;
  }
  return true;
}


bool StatusManager::GetItemStatus(const string& key, double* value) const {
  shared_ptr<detail::NumItem> item;
  if (!GetItem(key, &item)) {
    return false;
  }

  if (!item->GetValue(update_period_, value)) {
    LOG(WARNING) << key << " expired";
    return false;
  }
  return true;
}


bool StatusManager::IncreaseItemCounting(const string& key) {
  return IncreaseItemCounting(key, 1);
}


bool StatusManager::IncreaseItemCounting(const string& key, int64_t increasement) {
  if (increasement < 1) {
    LOG(WARNING) << "Invalid increasement " << increasement;
    return false;
  }

  shared_ptr<detail::CountingItem> item;
  if (!GetItem(key, &item)) {
    return false;
  }
  item->IncreaseCounting(increasement);
  return true;
}


bool StatusManager::GetAverageCountingPerSecond(const string& key, int64_t* value) const {
  shared_ptr<detail::CountingItem> item;
  if (!GetItem(key, &item)) {
    return false;
  }
  *value = item->current_count_per_second_;
  return true;
}


bool StatusManager::GetTotalCountingPerCycle(const string& key, int64_t* value) const {
  shared_ptr<detail::CountingItem> item;
  if (!GetItem(key, &item)) {
    return false;
  }
  *value = item->current_count_per_cycle_;
  return true;
}


bool StatusManager::InsertLatency(const string& key, double latency) {
  shared_ptr<detail::LatencyItem> item;
  if (!GetItem(key, &item)) {
    return false;
  }
  item->InsertLatency(latency);
  return true;
}


bool StatusManager::GetAverageLatency(const string& key, double* latency) const {
  shared_ptr<detail::LatencyItem> item;
  if (!GetItem(key, &item)) {
    return false;
  }
  *latency = item->current_average_latency_;
  return true;
}


bool StatusManager::GetMaxLatency(const string& key, double* latency) const {
  shared_ptr<detail::LatencyItem> item;
  if (!GetItem(key, &item)) {
    return false;
  }
  *latency = item->current_max_latency_;
  return true;
}


bool StatusManager::GetMinLatency(const string& key, double* latency) const {
  shared_ptr<detail::LatencyItem> item;
  if (!GetItem(key, &item)) {
    return false;
  }
  *latency = item->current_min_latency_;
  return true;
}

void StatusManager::UpdateItems(int duration) {
  shared_lock lock(shared_mutex_);
  for (ItemMap::iterator iter = item_map_.begin(); iter != item_map_.end(); ++iter) {
    switch (iter->second->type()) {
      case COUNTING_ITEM:
        static_pointer_cast<detail::CountingItem>(iter->second)->Update(duration);
        break;

      case LATENCY_ITEM:
        static_pointer_cast<detail::LatencyItem>(iter->second)->Update(duration);
        break;

      case STRING_ITEM: // fall through
      case NUM_ITEM: // fall through
      default:
        continue;
    }
  }
}

void StatusManager::ShowAllStatistics(string* buffer) const {
  using std::setprecision;
  std::ostringstream os;
  os << std::setiosflags(std::ios::fixed);
  // show all string items
  os << "collect_cycle : " << update_period_ << "\n";

  shared_lock lock(shared_mutex_);
  for (ItemMap::const_iterator iter = item_map_.begin(); iter != item_map_.end(); ++iter) {
    switch (iter->second->type()) {
      case STRING_ITEM:
        os << iter->first << " : \"" << static_pointer_cast<detail::StringItem>(iter->second)->value_ << "\"\n";
        break;

      case NUM_ITEM:
        os << iter->first << " : \"" << setprecision(2)
           << static_pointer_cast<detail::NumItem>(iter->second)->value_ << "\"\n";
        break;

      case COUNTING_ITEM: {
        shared_ptr<detail::CountingItem> item =
            static_pointer_cast<detail::CountingItem>(iter->second);
        os << iter->first << "_per_second : " << item->current_count_per_second_ << "\n";
        os << iter->first << "_per_cycle : " << item->current_count_per_cycle_ << "\n";
        os << iter->first << "_count_total : " << item->current_accumulated_count_ << "\n";
        break;
      }

      case LATENCY_ITEM: {
        shared_ptr<detail::LatencyItem> item =
            static_pointer_cast<detail::LatencyItem>(iter->second);
        os << iter->first << "_count_per_second : " << item->current_count_per_second_ << "\n";
        os << iter->first << "_count_per_cycle : " << item->current_count_per_cycle_ << "\n";
        os << iter->first << "_count_total : " << item->current_accumulated_count_ << "\n";
        os << setprecision(2) << iter->first << "_latency_max : "
           << item->current_max_latency_ << "\n";
        os << setprecision(2) << iter->first << "_latency_min : "
           << item->current_min_latency_ << "\n";
        os << setprecision(2) << iter->first << "_latency_avg : "
           << item->current_average_latency_ << "\n";
        break;
      }

      default:
        continue;
    }
  }
  os.str().swap(*buffer);
}

}  // namespace bbts

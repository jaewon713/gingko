/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file module_status_manager.h
 * @author huangwei(huangwei02@baidu.com)
 * @date 2013/06/13 15:56:17
 * @brief
 *      maintains various status of warning-tracker
 **/

#ifndef  OP_OPED_NOAH_BBTS_STATUS_MANAGER_H_
#define  OP_OPED_NOAH_BBTS_STATUS_MANAGER_H_

#include <map>
#include <string>

#include <boost/noncopyable.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/thread.hpp>

#include "bbts/LazySingleton.hpp"

namespace bbts {

namespace detail {
class Item;
} // namespace detail

// maintains user-defined status
// one should first register item, call operations to these items.
// there's a backgroud thread periodically checking each item/status
class StatusManager : private boost::noncopyable {
 public:
  enum ItemType {
    // for this type of item, you get one interface
    // eg. SetItemStatus(const std::string& key, const std::string& value)
    //     GetItemStatus(const std::string& key, std::string* value)
    STRING_ITEM = 1,

    // for this type of item, you get one interface
    // eg. SetItemStatus(const std::string& key, const double& value)
    //     GetItemStatus(const std::string& key, double* value)
    NUM_ITEM = 2,

    // for this type of item, you get folling interfaces
    // eg. IncreaseItemCounting(const std::string& key), add by 1 automatically
    //     IncreaseItemCounting(const std::string& key, const int64_t& increment)
    //     GetAverageCountingPerSecond(const std::string& key, int64_t* value)
    //     GetTotalCountingPerCycle(const std::string& key, int64_t* value)
    COUNTING_ITEM = 3,

    // for this type of item, you get folling interfaces
    // eg. InsertLatency(const std::string& key, const double& latency)
    //     GetAverageLatency(const std::string& key, int64_t* value)
    //     GetMaxLatency(const std::string& key, int64_t* value)
    //     GetMinLatency(const std::string& key, int64_t* value)
    //     GetAverageCountingPerSecond()(const std::string& key, int64_t* value)
    //     GetTotalCountingPerCycle(const std::string& key, int64_t* value)
    LATENCY_ITEM = 4,
  };

  // default, cycle will be set to 10
  StatusManager();
  ~StatusManager();

  // call this function to start updating thread, if not called
  // all Get operations will fail
  bool Start(int period = 10);

  void Stop();

  // call this to register a item to be monitored, indentified by @name,
  // false whill be return if name is duplicated
  bool RegisterItem(const std::string& name, ItemType type);
  bool UnregisterItem(const std::string& name);

  // suitable for num/string value, if type not valid or item not exist
  // false is returned
  bool SetItemStatus(const std::string& key, double value);
  bool SetItemStatus(const std::string& key, const std::string& value);

  // suitable for num/string value, if type not valid or item not exist
  // false is returned
  bool GetItemStatus(const std::string& key, double* value) const;
  bool GetItemStatus(const std::string& key, std::string* value) const;

  // suitable for counting items, increase item by one
  bool IncreaseItemCounting(const std::string& key);
  // suitable for counting items, increase item by @increasement,
  // @increasement should be positive integers
  bool IncreaseItemCounting(const std::string& key, int64_t increasement);

  // suitable for counting/latency type items
  bool GetAverageCountingPerSecond(const std::string& key, int64_t* value) const;
  bool GetTotalCountingPerCycle(const std::string& key, int64_t* value) const;

  // only for latency items
  bool InsertLatency(const std::string& key, double latency);
  bool GetAverageLatency(const std::string& key, double* value) const;
  bool GetMaxLatency(const std::string& key, double* value) const;
  bool GetMinLatency(const std::string& key, double* value) const;

  bool UpdateLatencyItems(const int duration);

  // show all valid key-values
  void ShowAllStatistics(std::string* buffer) const;

  void UpdateData();

 private:
  typedef std::map<std::string, boost::shared_ptr<detail::Item> > ItemMap;

  template <typename Item> bool GetItem(const std::string &key, boost::shared_ptr<Item> *item) const;

  void UpdateItems(int duration);

  bool                terminated_;
  // period that statistics should be updated
  int                 update_period_;
  // take item name as key
  ItemMap             item_map_;
  mutable boost::shared_mutex shared_mutex_;
  boost::thread       thread_;
};

}  // namespace bbts

// way to get a singleton object of StatusManager
#define g_pStatusManager bbts::LazySingleton<bbts::StatusManager>::instance()

#endif  // OP_OPED_NOAH_BBTS_STATUS_MANAGER_H_

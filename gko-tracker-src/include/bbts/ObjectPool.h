/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   ObjectPool.h
 *
 * @author liuming03
 * @date   2014年6月30日
 * @brief 
 */

#ifndef OP_OPED_NOAH_BBTS_OBJECT_POOL_H_
#define OP_OPED_NOAH_BBTS_OBJECT_POOL_H_

#include <string>
#include <vector>

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/unordered_map.hpp>

namespace bbts {

/**
 * @brief
 */
template <typename T>
class ObjectPool {
 public:
  typedef std::vector<T *> NodeVector;

  ObjectPool(int alloc_size) : alloc_size_(alloc_size) {
    boost::mutex::scoped_lock lock(mutex_);
    AllocObjects(alloc_size_);
  }

  ~ObjectPool() {
    boost::mutex::scoped_lock lock(mutex_);
    for(typename NodeVector::iterator it = nodes_pool_.begin(); it != nodes_pool_.end(); ++it) {
      delete[] *it;
    }
    nodes_pool_.clear();
    nodes_.clear();
  }

  boost::shared_ptr<T> GetOneObject() {
    boost::mutex::scoped_lock lock(mutex_);
    if (nodes_.empty()) {
      AllocObjects(alloc_size_);
    }
    T* obj = nodes_.back();
    nodes_.pop_back();
    return boost::shared_ptr<T>(obj, boost::bind(&ObjectPool::PutOneObject, this, _1));
  }

 private:
  void AllocObjects(int n) {
    T* objs = new T[n];
    assert(objs);
    nodes_pool_.push_back(objs);
    for (int i = 0; i < n; ++i) {
      nodes_.push_back(objs + i);
    }
  }

  void PutOneObject(T* object) {
    boost::mutex::scoped_lock lock(mutex_);
    nodes_.push_back(object);
  }

  NodeVector nodes_;
  boost::mutex mutex_;
  int alloc_size_;
  NodeVector nodes_pool_;
};

}  // namespace bbts
#endif // OP_OPED_NOAH_BBTS_OBJECT_POOL_H_

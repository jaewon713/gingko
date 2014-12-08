/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   KeyTypeRWLock.hpp
 *
 * @author liuming03
 * @date   2014-3-24
 * @brief 
 */

#ifndef OP_OPED_NOAH_BBTS_KEY_TYPE_RWLOCK_HPP_
#define OP_OPED_NOAH_BBTS_KEY_TYPE_RWLOCK_HPP_

#include <string>

#include <boost/thread/shared_mutex.hpp>

#include "bbts/LazySingleton.hpp"

namespace bbts {

inline static uint32_t BKDRHash(const char *str, size_t size) {
  uint32_t seed = 13131;  //  31 131 1313 13131 131313 etc..
  uint32_t hash = 0;
  for (size_t i = 0; i < size; ++i) {
    hash = hash * seed + str[i];
  }
  return (hash & 0x7FFFFFFF);
}

template<typename Type>
class KeyTypeRWLock {
 public:
  KeyTypeRWLock(const std::string &key, char lock_type = 'w')
    : lock_type_(lock_type),
      current_shared_mutex_(NULL) {
    SharedMutexPoolType *mutex_pool = LazySingleton<SharedMutexPoolType>::instance();
    int index = BKDRHash(key.c_str(), key.length()) % mutex_pool->get_size();

    current_shared_mutex_ = mutex_pool->GetSharedMutexByIndex(index);
    if (lock_type_ == 'w') {
      current_shared_mutex_->lock();
    } else {
      current_shared_mutex_->lock_shared();
    }
  }

  ~KeyTypeRWLock() {
    if (lock_type_ == 'w') {
      current_shared_mutex_->unlock();
    } else {
      current_shared_mutex_->unlock_shared();
    }
  }

  static void SetMaxLockSize(int lock_size) {
    LazySingleton <SharedMutexPoolType> ::instance()->ChangeMaxSize(lock_size);
  }

 private:
  char lock_type_;
  boost::shared_mutex *current_shared_mutex_;

  class SharedMutexPoolType {
   public:
    SharedMutexPoolType() : shared_mutex_vector_size_(5000) {
      for (int i = 0; i < shared_mutex_vector_size_; ++i) {
        shared_mutex_vector_.push_back(new boost::shared_mutex());
      }
    }

    ~SharedMutexPoolType() {
      for (int i = 0; i < shared_mutex_vector_size_; ++i) {
        delete shared_mutex_vector_[i];
      }
    }

    boost::shared_mutex* GetSharedMutexByIndex(int index) {
      return shared_mutex_vector_[index];
    }

    void ChangeMaxSize(int size) {
      if (size <= shared_mutex_vector_size_) {
        return;
      }
      for (int i = shared_mutex_vector_size_; i < size; ++i) {
        shared_mutex_vector_.push_back(new boost::shared_mutex());
      }
      shared_mutex_vector_size_ = size;
    }

    int get_size() const {
      return shared_mutex_vector_size_;
    }

   private:
    int shared_mutex_vector_size_;
    std::vector<boost::shared_mutex *> shared_mutex_vector_;
  };
};

}  // namespace bbts

#endif // OP_OPED_NOAH_BBTS_KEY_TYPE_RWLOCK_HPP_

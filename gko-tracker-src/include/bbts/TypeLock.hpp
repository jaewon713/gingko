/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   TypeLock.hpp
 *
 * @author liuming03
 * @date   2014-3-24
 * @brief 
 */

#ifndef OP_OPED_NOAH_BBTS_TYPE_LOCK_HPP_
#define OP_OPED_NOAH_BBTS_TYPE_LOCK_HPP_

#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>

namespace bbts {

template<typename Type>
class TypeLock : private boost::noncopyable {
 public:
  // TypeLock lock;
  TypeLock() : lock_(mutex_) {}
  ~TypeLock() {}

  // TypeLock<T>::lock();
  static void lock() {
    mutex_.lock();
  }

  // TypeLock<T>::unlock();
  static void unlock() {
    mutex_.unlock();
  }

 private:
  boost::mutex::scoped_lock lock_;

  static boost::mutex mutex_;

  TypeLock* operator &();
  TypeLock* operator &() const;
};

template<typename Type>
boost::mutex TypeLock<Type>::mutex_;

}  // namespace bbts

#endif // OP_OPED_NOAH_BBTS_TYPE_LOCK_HPP_

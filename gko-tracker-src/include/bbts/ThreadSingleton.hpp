/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   ThreadSingleton.hpp
 *
 * @author liuming03
 * @date   2014年6月4日
 * @brief 
 */

#ifndef OP_OPED_NOAH_BBTS_THREAD_SINGLETON_HPP_
#define OP_OPED_NOAH_BBTS_THREAD_SINGLETON_HPP_

#include <boost/noncopyable.hpp>
#include <boost/thread/tss.hpp>

#include "bbts/LazySingleton.hpp"

namespace bbts {

template <typename Type>
class ThreadSingleton : private boost::noncopyable {
 public:
  static Type* instance() {
    if (!ptr_.get()) {
      ptr_.reset(new Type());
    }
    return ptr_.get();
  }

  template<typename Arg1>
  static Type* instance(const Arg1& arg1) {
    if (!ptr_.get()) {
      ptr_.reset(new Type(arg1));
    }
    return ptr_.get();
  }

  template<typename Arg1, typename Arg2>
  static Type* instance(const Arg1& arg1, const Arg2& arg2) {
    if (!ptr_.get()) {
      ptr_.reset(new Type(arg1, arg2));
    }
    return ptr_.get();
  }

  template<typename Arg1, typename Arg2, typename Arg3>
  static Type* instance(const Arg1& arg1, const Arg2& arg2, const Arg3& arg3) {
    if (!ptr_.get()) {
      ptr_.reset(new Type(arg1, arg2, arg3));
    }
    return ptr_.get();
  }

 private:
  static boost::thread_specific_ptr<Type> ptr_;

  ThreadSingleton();
  ~ThreadSingleton();
  ThreadSingleton* operator &();
  const ThreadSingleton* operator &() const;
};

template<typename Type>
boost::thread_specific_ptr<Type> ThreadSingleton<Type>::ptr_;

} // namespace bbts

#endif // OP_OPED_NOAH_BBTS_THREAD_SINGLETON_HPP_

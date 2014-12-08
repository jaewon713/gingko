/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   LazySingleton.hpp
 *
 * @author liuming03
 * @date   2014-3-24
 * @brief 
 */

#ifndef OP_OPED_NOAH_BBTS_LAZY_SINGLETON_HPP_
#define OP_OPED_NOAH_BBTS_LAZY_SINGLETON_HPP_

#include <assert.h>

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

#include "bbts/TypeLock.hpp"

namespace bbts {

template<typename Type>
class LazySingleton : private boost::noncopyable {
 public:
  static Type* instance() {
    if (!instance_) {
      TypeLock<LazySingleton<Type> > lock;
      if (!instance_) {
        instance_.reset(new Type());
      }
    }
    assert(instance_);
    return instance_.get();
  }

 private:
  static boost::scoped_ptr<Type> instance_;

  LazySingleton();
  ~LazySingleton();
};

template<typename Type>
boost::scoped_ptr<Type> LazySingleton<Type>::instance_;

}  // namespace bbts

#endif // OP_OPED_NOAH_BBTS_LAZY_SINGLETON_HPP_

/*
 * random.h
 *
 *  Created on: 2013-8-20
 *      Author: huxiaoxiao
 */

#ifndef OP_OPED_NOAH_BBTS_RANDOM_H_
#define OP_OPED_NOAH_BBTS_RANDOM_H_

#include <sys/time.h>

namespace bbts {

class Random {
 public:
  static int GetRandomNumber(int rand_max) {
    if (rand_max == 0) {
      return 0;
    }
    if (rand_max < 0) {
      return -1;
    }

    unsigned int seed = MakeRandomSeed();
    srand(seed);
    return rand_r(&seed) % rand_max;
  }

  static uint32_t MakeRandomSeed() {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    // reference uns
    const uint32_t kPrime1 = 61631;
    const uint32_t kPrime2 = 64997;
    const uint32_t kPrime3 = 111857;

    return   kPrime1 * static_cast<uint32_t>(getpid())
           + kPrime2 * static_cast<uint32_t>(tv.tv_sec)
           + kPrime3 * static_cast<uint32_t>(tv.tv_usec);
  }
};

}

#endif /* OP_OPED_NOAH_HERMES_BBTS_TRACKER_RANDOM_H_ */

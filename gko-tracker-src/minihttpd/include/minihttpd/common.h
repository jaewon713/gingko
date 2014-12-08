#ifndef ARGUS_COMMON_COMMON_H_
#define ARGUS_COMMON_COMMON_H_

#include "types.h"
#include <glog/logging.h>

#undef  ARGUS_DISALLOW_EVIL_CONSTRUCTORS
#define ARGUS_DISALLOW_EVIL_CONSTRUCTORS(TypeName)    \
    TypeName(const TypeName&);                        \
    void operator=(const TypeName&)

namespace argus {
namespace common {

}
}


#endif  // ARGUS_COMMON_COMMON_H_

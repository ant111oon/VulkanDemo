#ifndef COMMON_DBG_H
#define COMMON_DBG_H

#include "common/math/common.slang"


#ifndef ENV_RELEASE
    #define DBG_BIT_VALUE(BIT_MASK, BIT, RELEASE_VALUE) IS_BIT_SET(BIT_MASK, BIT)
    #define DBG_VALUE(VALUE, RELEASE_VALUE) (VALUE)
#else
    #define DBG_BIT_VALUE(BIT_MASK, BIT, RELEASE_VALUE) (RELEASE_VALUE)
    #define DBG_VALUE(VALUE, RELEASE_VALUE) (RELEASE_VALUE)
#endif

#endif
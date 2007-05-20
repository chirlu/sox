#include "soxconfig.h"

#ifdef HAVE_STDINT_H
  #include <stdint.h>
#else
  #ifdef HAVE_INTTYPES_H
    #include <inttypes.h>
  #else
    #ifdef _MSC_VER
      typedef __int64 int64_t;
      typedef unsigned __int64 uint64_t;
    #else
      typedef long long int64_t;
      typedef unsigned long long uint64_t;
    #endif
    typedef char int8_t;
    typedef long int32_t;
    typedef short int16_t;
    typedef unsigned char uint8_t;
    typedef unsigned long uint32_t;
    typedef unsigned short uint16_t;

    typedef int8_t  INT8;
    typedef int16_t INT16;
    typedef int32_t INT32;
    typedef int64_t INT64;
  #endif
#endif

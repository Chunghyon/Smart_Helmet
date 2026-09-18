/*!
\file       csrtypes.h
\brief      Host-test stub for the QCC ADK base types.

Only the subset used by the smart_helmet sources is provided.
*/
#ifndef SH_TEST_CSRTYPES_H
#define SH_TEST_CSRTYPES_H

#include <stdint.h>

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int8_t   int8;
typedef int16_t  int16;
typedef int32_t  int32;

#ifndef TRUE
#define TRUE  (1)
#endif
#ifndef FALSE
#define FALSE (0)
#endif

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

#endif /* SH_TEST_CSRTYPES_H */

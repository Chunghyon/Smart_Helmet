/*!
\file       vm.h
\brief      Host-test stub for the QCC ADK VM timer.

The harness drives time explicitly, so VmGetTimerTime() returns a value the
test program controls through sh_test_now_us.
*/
#ifndef SH_TEST_VM_H
#define SH_TEST_VM_H

#include <csrtypes.h>

typedef uint32 rtime_t;

extern uint32 sh_test_now_us;

static inline rtime_t VmGetTimerTime(void)
{
    return (rtime_t)sh_test_now_us;
}

#endif /* SH_TEST_VM_H */

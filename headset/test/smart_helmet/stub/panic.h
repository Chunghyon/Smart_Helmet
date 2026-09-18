/*!
\file       panic.h
\brief      Host-test stub for the QCC ADK panic interface.
*/
#ifndef SH_TEST_PANIC_H
#define SH_TEST_PANIC_H

#include <stdlib.h>

static inline void Panic(void)
{
    abort();
}

static inline void *PanicNull(void *p)
{
    if (!p)
    {
        abort();
    }
    return p;
}

#endif /* SH_TEST_PANIC_H */

/*!
\file       logging.h
\brief      Host-test stub for the QCC ADK logging macros.

CC_LOGN becomes a printf gated on the SH_TEST_LOG environment variable, so
the scenario runner stays quiet unless logs are explicitly requested. The
"enum:<type>:" prefix used by the real macro is left in the format string;
it renders harmlessly as literal text here.
*/
#ifndef SH_TEST_LOGGING_H
#define SH_TEST_LOGGING_H

#include <stdio.h>
#include <stdlib.h>

static inline int sh_test_log_enabled(void)
{
    static int cached = -1;
    if (cached < 0)
    {
        cached = (getenv("SH_TEST_LOG") != NULL);
    }
    return cached;
}

#define CC_LOGN(...)                                 \
    do {                                             \
        if (sh_test_log_enabled()) {                 \
            printf(__VA_ARGS__);                     \
            printf("\n");                            \
        }                                            \
    } while (0)

#define DEBUG_LOG_DEFINE_LEVEL_VAR
#define DEBUG_LOG_ERROR(...)   ((void)0)
#define DEBUG_LOG_WARN(...)    ((void)0)
#define DEBUG_LOG_INFO(...)    ((void)0)
#define DEBUG_LOG_VERBOSE(...) ((void)0)
#define DEBUG_LOG(...)         ((void)0)

#endif /* SH_TEST_LOGGING_H */

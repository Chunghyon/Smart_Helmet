/* Copyright (c) 2014 - 2022 Qualcomm Technologies International, Ltd. */

#ifndef GATT_APPLE_NOTIFICATION_CLIENT_DEBUG_H_
#define GATT_APPLE_NOTIFICATION_CLIENT_DEBUG_H_

#include <logging.h>

#ifdef GATT_APPLE_NOTIFICATION_DEBUG_LIB
#define DEBUG_PANIC(...) {DEBUG_LOG_ALWAYS(__VA_ARGS__); Panic();}
#else
#define DEBUG_PANIC(...) {DEBUG_LOG_ALWAYS(__VA_ARGS__);}
#endif

#define PANIC(...) {DEBUG_LOG_ALWAYS(__VA_ARGS__); Panic();}

#endif /* GATT_APPLE_NOTIFICATION_CLIENT_DEBUG_H_ */

/* Copyright (c) 2020 Qualcomm Technologies International, Ltd. */
/*  */

#ifndef _GATT_TMAS_SERVER_DEBUG_H_
#define _GATT_TMAS_SERVER_DEBUG_H_

#include <panic.h>

/* Macro used to generate debug version of this library */
#ifdef GATT_TMAS_SERVER_DEBUG_LIB

#ifndef DEBUG_PRINT_ENABLED
#define DEBUG_PRINT_ENABLED
#endif

#include <print.h>
#include <stdio.h>

#define GATT_TMAS_SERVER_DEBUG_INFO(x) {PRINT(("%s:%d - ", __FILE__, __LINE__)); PRINT(x);}
#define GATT_TMAS_SERVER_DEBUG_PANIC(x) {GATT_TMAS_SERVER_DEBUG_INFO(x); Panic();}
#define GATT_TMAS_SERVER_PANIC(x) {GATT_TMAS_SERVER_DEBUG_INFO(x); Panic();}

#else /* GATT_TMAS_SERVER_DEBUG_LIB */

#define GATT_TMAS_SERVER_DEBUG_INFO(x)
#define GATT_TMAS_SERVER_DEBUG_PANIC(x)
#define GATT_TMAS_SERVER_PANIC(x) {Panic();}

#endif /* GATT_TMAS_SERVER_DEBUG_LIB */

#endif /* _GATT_TMAS_SERVER_DEBUG_H_ */

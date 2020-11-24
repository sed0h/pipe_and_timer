#ifndef __PORTMACRO_H__
#define __PORTMACRO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define PRIVILEGED_FUNCTION()

#define portINTERRUPT_YIELD (0UL)
#define portINTERRUPT_TICK (1UL)

typedef long BaseType_t;
typedef unsigned long UBaseType_t;

typedef uint32_t TickType_t;
#define portMAX_DELAY ( TickType_t ) 0xffffffffUL

/* Hardware specifics. */
#define portSTACK_GROWTH (-1)
#define portTICK_PERIOD_MS ((TickType_t)1000 / configTICK_RATE_HZ)


#ifdef __cplusplus
}
#endif

#endif


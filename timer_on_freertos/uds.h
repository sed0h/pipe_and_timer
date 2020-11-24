#ifndef __UDS_H__
#define __UDS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "udsconfig.h"
#include "portmacro.h"
#include "portable.h"
#include <stdbool.h>

#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(xTimeInMs)                                                                   \
    ((TickType_t)(((TickType_t)(xTimeInMs) * (TickType_t)configTICK_RATE_HZ) / (TickType_t)1000))
#endif

#define pdFALSE ((BaseType_t)0)
#define pdTRUE ((BaseType_t)1)

#define pdPASS (pdTRUE)
#define pdFAIL (pdFALSE)

#ifndef mtCOVERAGE_TEST_MARKER
	#define mtCOVERAGE_TEST_MARKER()
#endif

#ifndef mtCOVERAGE_TEST_DELAY
	#define mtCOVERAGE_TEST_DELAY()
#endif

#ifndef traceTIMER_EXPIRED
	#define traceTIMER_EXPIRED(pxTimer)
#endif

#ifndef configASSERT
	#define configASSERT(x)
	#define configASSERT_DEFINED 0
#else
	#define configASSERT_DEFINED 1
#endif

#ifndef traceTIMER_CREATE
    #define traceTIMER_CREATE(pxNewTimer)
#endif

#ifndef traceQUEUE_CREATE
    #define traceQUEUE_CREATE(pxNewQueue)
#endif

#ifndef traceTIMER_COMMAND_RECEIVED
    #define traceTIMER_COMMAND_RECEIVED( pxTimer, xMessageID, xMessageValue )
#endif

#ifndef configSUPPORT_STATIC_ALLOCATION
    /* Defaults to 0 for backward compatibility. */
#define configSUPPORT_STATIC_ALLOCATION 0
#endif

#ifndef configSUPPORT_DYNAMIC_ALLOCATION
    /* Defaults to 1 for backward compatibility. */
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#endif

typedef void *QueueHandle_t;
typedef void *TaskHandle_t;

#ifndef traceTASK_INCREMENT_TICK
	#define traceTASK_INCREMENT_TICK(xTickCount)
#endif

struct xSTATIC_LIST_ITEM {
    TickType_t xDummy1;
    void *     pvDummy2[4];
};
typedef struct xSTATIC_LIST_ITEM StaticListItem_t;

typedef struct xSTATIC_TIMER {
    void *           pvDummy1;
    StaticListItem_t xDummy2;
    TickType_t       xDummy3;
    UBaseType_t      uxDummy4;
    void *           pvDummy5[2];
#if (configUSE_TRACE_FACILITY == 1)
    UBaseType_t uxDummy6;
#endif

#if ((configSUPPORT_STATIC_ALLOCATION == 1) && (configSUPPORT_DYNAMIC_ALLOCATION == 1))
    uint8_t ucDummy7;
#endif

} StaticTimer_t;



#ifdef __cplusplus
}
#endif



#endif 

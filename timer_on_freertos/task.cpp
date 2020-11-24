#include "uds.h"
#include "task.h"
#include <thread>

/*PRIVILEGED_DATA */ static volatile TickType_t  xTickCount    = (TickType_t)0U;
/*PRIVILEGED_DATA */ static volatile UBaseType_t uxPendedTicks = (UBaseType_t)0U;
/*PRIVILEGED_DATA */ static volatile TickType_t  xNextTaskUnblockTime =
    (TickType_t)0U; /* Initialised to portMAX_DELAY before the scheduler starts. */

/*PRIVILEGED_DATA */ static volatile UBaseType_t uxSchedulerSuspended = (UBaseType_t)pdFALSE;
//PRIVILEGED_DATA                                  TCB_t *volatile pxCurrentTCB = NULL;

TickType_t xTaskGetTickCount(void) {
    TickType_t xTicks;

    /* Critical section required if running on a 16 bit processor. */
    // portTICK_TYPE_ENTER_CRITICAL();
    { xTicks = xTickCount; }
    // portTICK_TYPE_EXIT_CRITICAL();

    return xTicks;
}

BaseType_t xTaskIncrementTick(void) {
    //TCB_t *    pxTCB;
    TickType_t xItemValue;
    BaseType_t xSwitchRequired = pdFALSE;
    
    traceTASK_INCREMENT_TICK(xTickCount);
    if (uxSchedulerSuspended == (UBaseType_t)pdFALSE) {
#ifdef _DEBUG
        //printf("Tick Value: %d", xTickCount++);
#endif // _DEBUG
        /* Minor optimisation.  The tick count cannot change in this
        block. */
        const TickType_t xConstTickCount = xTickCount + 1;

        /* Increment the RTOS tick, switching the delayed and overflowed
        delayed lists if it wraps to 0. */
        xTickCount = xConstTickCount;

        if (xConstTickCount == (TickType_t)0U)
        {
            //taskSWITCH_DELAYED_LISTS();
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
        /* See if this tick has made a timeout expire.  Tasks are stored in
        the	queue in the order of their wake time - meaning once one task
        has been found whose block time has not expired there is no need to
        look any further down the list. */
        if (xConstTickCount >= xNextTaskUnblockTime)
        {
        }
    } else {
        ++uxPendedTicks;
    }

    return xSwitchRequired;
}

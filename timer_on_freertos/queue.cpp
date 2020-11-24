#include "uds.h"
#include "list.h"
#include "queue.h"

/* Constants used with the cRxLock and cTxLock structure members. */
#define queueUNLOCKED ((int8_t)-1)

typedef struct QueueDefinition {
    int8_t *pcHead; /*< Points to the beginning of the queue storage area. */
    int8_t
        *pcTail; /*< Points to the byte at the end of the queue storage area.  Once more byte is
                    allocated than necessary to store the queue items, this is used as a marker. */
    int8_t *pcWriteTo; /*< Points to the free next place in the storage area. */

    union /* Use of a union is an exception to the coding standard to ensure two mutually exclusive
             structure members don't appear simultaneously (wasting RAM). */
    {
        int8_t *pcReadFrom; /*< Points to the last place that a queued item was read from when the
                               structure is used as a queue. */
        UBaseType_t uxRecursiveCallCount; /*< Maintains a count of the number of times a recursive
                                             mutex has been recursively 'taken' when the structure
                                             is used as a mutex. */
    } u;

    List_t xTasksWaitingToSend; /*< List of tasks that are blocked waiting to post onto this queue.
                                   Stored in priority order. */
    List_t xTasksWaitingToReceive; /*< List of tasks that are blocked waiting to read from this
                                      queue.  Stored in priority order. */

    volatile UBaseType_t uxMessagesWaiting; /*< The number of items currently in the queue. */
    UBaseType_t uxLength;   /*< The length of the queue defined as the number of items it will hold,
                               not the number of bytes. */
    UBaseType_t uxItemSize; /*< The size of each items that the queue will hold. */

    volatile int8_t cRxLock; /*< Stores the number of items received from the queue (removed from
                                the queue) while the queue was locked.  Set to queueUNLOCKED when
                                the queue is not locked. */
    volatile int8_t
        cTxLock; /*< Stores the number of items transmitted to the queue (added to the queue) while
                    the queue was locked.  Set to queueUNLOCKED when the queue is not locked. */

//#if ((configSUPPORT_STATIC_ALLOCATION == 1) && (configSUPPORT_DYNAMIC_ALLOCATION == 1))
    uint8_t ucStaticallyAllocated; /*< Set to pdTRUE if the memory used by the queue was statically
                                      allocated to ensure no attempt is made to free the memory. */
//#endif

#if (configUSE_QUEUE_SETS == 1)
    struct QueueDefinition *pxQueueSetContainer;
#endif

#if (configUSE_TRACE_FACILITY == 1)
    UBaseType_t uxQueueNumber;
    uint8_t     ucQueueType;
#endif

} xQUEUE;

/* The old xQUEUE name is maintained above then typedefed to the new Queue_t
name below to enable the use of older kernel aware debuggers. */
typedef xQUEUE Queue_t;

BaseType_t xQueueGenericReset(QueueHandle_t xQueue, BaseType_t xNewQueue) {
    Queue_t *const pxQueue = (Queue_t *)xQueue;

    configASSERT(pxQueue);

    //taskENTER_CRITICAL();
    {
        pxQueue->pcTail            = pxQueue->pcHead + (pxQueue->uxLength * pxQueue->uxItemSize);
        pxQueue->uxMessagesWaiting = (UBaseType_t)0U;
        pxQueue->pcWriteTo         = pxQueue->pcHead;
        pxQueue->u.pcReadFrom =
            pxQueue->pcHead + ((pxQueue->uxLength - (UBaseType_t)1U) * pxQueue->uxItemSize);
        pxQueue->cRxLock = queueUNLOCKED;
        pxQueue->cTxLock = queueUNLOCKED;

        if (xNewQueue == pdFALSE) {
            /* If there are tasks blocked waiting to read from the queue, then
            the tasks will remain blocked as after this function exits the queue
            will still be empty.  If there are tasks blocked waiting to write to
            the queue, then one should be unblocked as after this function exits
            it will be possible to write to it. */
            if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToSend)) == pdFALSE) {
                //if (xTaskRemoveFromEventList(&(pxQueue->xTasksWaitingToSend)) != pdFALSE) {
                //    queueYIELD_IF_USING_PREEMPTION();
                //} else {
                //    mtCOVERAGE_TEST_MARKER();
                //}
            } else {
                mtCOVERAGE_TEST_MARKER();
            }
        } else {
            /* Ensure the event queues start in the correct state. */
            vListInitialise(&(pxQueue->xTasksWaitingToSend));
            vListInitialise(&(pxQueue->xTasksWaitingToReceive));
        }
    }
    //taskEXIT_CRITICAL();

    /* A value is returned for calling semantic consistency with previous
    versions. */
    return pdPASS;
}

static void prvInitialiseNewQueue(const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize,
                                  uint8_t *pucQueueStorage, const uint8_t ucQueueType,
                                  Queue_t *pxNewQueue) {
    /* Remove compiler warnings about unused parameters should
    configUSE_TRACE_FACILITY not be set to 1. */
    (void)ucQueueType;

    if (uxItemSize == (UBaseType_t)0) {
        /* No RAM was allocated for the queue storage area, but PC head cannot
        be set to NULL because NULL is used as a key to say the queue is used as
        a mutex.  Therefore just set pcHead to point to the queue as a benign
        value that is known to be within the memory map. */
        pxNewQueue->pcHead = (int8_t *)pxNewQueue;
    } else {
        /* Set the head to the start of the queue storage area. */
        pxNewQueue->pcHead = (int8_t *)pucQueueStorage;
    }

    /* Initialise the queue members as described where the queue type is
    defined. */
    pxNewQueue->uxLength   = uxQueueLength;
    pxNewQueue->uxItemSize = uxItemSize;
    (void)xQueueGenericReset(pxNewQueue, pdTRUE);

#if (configUSE_TRACE_FACILITY == 1)
    { pxNewQueue->ucQueueType = ucQueueType; }
#endif /* configUSE_TRACE_FACILITY */

#if (configUSE_QUEUE_SETS == 1)
    { pxNewQueue->pxQueueSetContainer = NULL; }
#endif /* configUSE_QUEUE_SETS */

    traceQUEUE_CREATE(pxNewQueue);
}

QueueHandle_t xQueueGenericCreate(const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize,
                                  const uint8_t ucQueueType) {
    Queue_t *pxNewQueue;
    size_t   xQueueSizeInBytes;
    uint8_t *pucQueueStorage;

    configASSERT(uxQueueLength > (UBaseType_t)0);

    if (uxItemSize == (UBaseType_t)0) {
        /* There is not going to be a queue storage area. */
        xQueueSizeInBytes = (size_t)0;
    } else {
        /* Allocate enough space to hold the maximum number of items that
        can be in the queue at any time. */
        xQueueSizeInBytes =
            (size_t)(uxQueueLength * uxItemSize); /*lint !e961 MISRA exception as the casts are only
                                                     redundant for some ports. */
    }

    //pxNewQueue = (Queue_t *)pvPortMalloc(sizeof(Queue_t) + xQueueSizeInBytes);
    pxNewQueue = new Queue_t[(sizeof(Queue_t) + xQueueSizeInBytes)];

    if (pxNewQueue != NULL) {
        /* Jump past the queue structure to find the location of the queue
        storage area. */
        pucQueueStorage = ((uint8_t *)pxNewQueue) + sizeof(Queue_t);

#if (configSUPPORT_STATIC_ALLOCATION == 1)
        {
            /* Queues can be created either statically or dynamically, so
            note this task was created dynamically in case it is later
            deleted. */
            pxNewQueue->ucStaticallyAllocated = pdFALSE;
        }
#endif /* configSUPPORT_STATIC_ALLOCATION */

        prvInitialiseNewQueue(uxQueueLength, uxItemSize, pucQueueStorage, ucQueueType, pxNewQueue);
    }

    return pxNewQueue;
}

void vQueueWaitForMessageRestricted( QueueHandle_t xQueue, TickType_t xTicksToWait, const BaseType_t xWaitIndefinitely )
{
Queue_t * const pxQueue = ( Queue_t * ) xQueue;

    /* This function should not be called by application code hence the
    'Restricted' in its name.  It is not part of the public API.  It is
    designed for use by kernel code, and has special calling requirements.
    It can result in vListInsert() being called on a list that can only
    possibly ever have one item in it, so the list will be fast, but even
    so it should be called with the scheduler locked and not from a critical
    section. */

    /* Only do anything if there are no messages in the queue.  This function
    will not actually cause the task to block, just place it on a blocked
    list.  It will not block until the scheduler is unlocked - at which
    time a yield will be performed.  If an item is added to the queue while
    the queue is locked, and the calling task blocks on the queue, then the
    calling task will be immediately unblocked when the queue is unlocked. */
    //prvLockQueue( pxQueue );
    //if( pxQueue->uxMessagesWaiting == ( UBaseType_t ) 0U )
    //{
    //    /* There is nothing in the queue, block for the specified period. */
    //    vTaskPlaceOnEventListRestricted( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait, xWaitIndefinitely );
    //}
    //else
    //{
    //    mtCOVERAGE_TEST_MARKER();
    //}
    //prvUnlockQueue( pxQueue );
}

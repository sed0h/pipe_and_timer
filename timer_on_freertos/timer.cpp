#include "uds.h"
#include "pthread.h"
#include "task.h"
#include "list.h"
#include "timer.h"
#include "queue.h"
#include <thread>

/* Misc definitions. */
#define tmrNO_DELAY (TickType_t)0U

enum TimerState 
{
    STATE_EXECUTE_CALLBACK = ((BaseType_t) -1),
    STATE_START = 0,
    STATE_RESET,
    STATE_RUNNING ,
    STATE_STOPPED,
};

/* The definition of the timers themselves. */
typedef struct tmrTimerControl {
    const char				*pcTimerName;		/*<< Text name.  This is not used by the kernel, it is included simply to make debugging easier. */ /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
    ListItem_t xTimerListItem; /*<< Standard linked list item as used by all kernel features for
                                  event management. */
    TickType_t xTimerPeriodInTicks; /*<< How quickly and often the timer expires. */
    UBaseType_t
        uxAutoReload; /*<< Set to pdTRUE if the timer should be automatically restarted once
                         expired.  Set to pdFALSE if the timer is, in effect, a one-shot timer. */
    void *pvTimerID;  /*<< An ID to identify the timer.  This allows the timer to be identified when
                         the same callback is used for multiple timers. */
    TimerCallbackFunction_t
        pxCallbackFunction; /*<< The function that will be called when the timer expires. */
#if (configUSE_TRACE_FACILITY == 1)
    UBaseType_t uxTimerNumber; /*<< An ID assigned by trace tools such as FreeRTOS+Trace */
#endif

#if ((configSUPPORT_STATIC_ALLOCATION == 1) && (configSUPPORT_DYNAMIC_ALLOCATION == 1))
    uint8_t
        ucStaticallyAllocated; /*<< Set to pdTRUE if the timer was created statically so no attempt
                                  is made to free the memory again if the timer is later deleted. */
#endif
} xTIMER;

/* The old xTIMER name is maintained above then typedefed to the new Timer_t
name below to enable the use of older kernel aware debuggers. */
typedef xTIMER Timer_t;

typedef struct tmrTimerParameters {
    TickType_t xMessageValue; /*<< An optional value used by a subset of commands, for example, when
                                 changing the period of a timer. */
    Timer_t *pxTimer;         /*<< The timer to which the command will be applied. */
} TimerParameter_t;

typedef struct tmrCallbackParameters {
    PendedFunction_t pxCallbackFunction; /* << The callback function to execute. */
    void *pvParameter1; /* << The value that will be used as the callback functions first parameter.
                         */
    uint32_t ulParameter2; /* << The value that will be used as the callback functions second
                              parameter. */
} CallbackParameters_t;

/* The structure that contains the two message types, along with an identifier
that is used to determine which message type is valid. */
typedef struct tmrTimerQueueMessage {
    BaseType_t xMessageID; /*<< The command being sent to the timer service task. */
    union {
        TimerParameter_t xTimerParameters;

/* Don't include xCallbackParameters if it is not going to be used as
it makes the structure (and therefore the timer queue) larger. */
#if (INCLUDE_xTimerPendFunctionCall == 1)
        CallbackParameters_t xCallbackParameters;
#endif /* INCLUDE_xTimerPendFunctionCall */
    } u;
} DaemonTaskMessage_t;

/* The list in which active timers are stored.  
Timers are referenced in expire time order, with the nearest expiry time at the front of the list.  
Only the timer service task is allowed to access these lists. */
/*PRIVILEGED_DATA */static List_t xActiveTimerList1;
/*PRIVILEGED_DATA */static List_t xActiveTimerList2;
static List_t *pxCurrentTimerList;
static List_t *pxOverflowTimerList ;

/* A queue that is used to send commands to the timer service task. */
static QueueHandle_t xTimerQueue = NULL;
static TaskHandle_t xTimerTaskHandle = NULL;

static DaemonTaskMessage_t xMessage;

/*
 * Initialise the infrastructure used by the timer service task if it has not
 * been initialised already.
 */
static void prvCheckForValidListAndQueue(void) /*PRIVILEGED_FUNCTION*/;

/*
 * An active timer has reached its expire time.  Reload the timer if it is an
 * auto reload timer, then call its callback.
 */
static void prvProcessExpiredTimer(const TickType_t xNextExpireTime, const TickType_t xTimeNow)/* PRIVILEGED_FUNCTION*/;

/*
 * The tick count has overflowed.  Switch the timer lists after ensuring the
 * current timer list does not still reference some timers.
 */
static void prvSwitchTimerLists(void) /*PRIVILEGED_FUNCTION*/;

/*
 * Obtain the current tick count, setting *pxTimerListsWereSwitched to pdTRUE
 * if a tick count overflow occurred since prvSampleTimeNow() was last called.
 */
static TickType_t prvSampleTimeNow(BaseType_t *const pxTimerListsWereSwitched);

/*
 * If the timer list contains any active timers then return the expire time of
 * the timer that will expire first and set *pxListWasEmpty to false.  If the
 * timer list does not contain any timers then return 0 and set *pxListWasEmpty
 * to pdTRUE.
 */
static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty );

/*
 * If a timer has expired, process it.  Otherwise, block the timer service task
 * until either a timer does expire or a command is received.
 */
static void prvProcessTimerOrBlockTask( const TickType_t xNextExpireTime, BaseType_t xListWasEmpty );

/*
 * Called by the timer service task to interpret and process a command it
 * received on the timer queue.
 */
static void prvProcessReceivedCommands( void );

/*
 * Insert the timer into either xActiveTimerList1, or xActiveTimerList2,
 * depending on if the expire time causes a timer counter overflow.
 */
static BaseType_t prvInsertTimerInActiveList(Timer_t* const pxTimer, const TickType_t xNextExpiryTime, const TickType_t xTimeNow, const TickType_t xCommandTime) /*PRIVILEGED_FUNCTION*/;

static pthread_t thread_timers_manage;
static void TimersManageTask(void* args);
//volatile bool running;

static void prvInitialiseNewTimer(const char *const pcTimerName,
                                  const TickType_t  xTimerPeriodInTicks,
                                  const UBaseType_t uxAutoReload, void *const pvTimerID,
                                  TimerCallbackFunction_t pxCallbackFunction, Timer_t *pxNewTimer)
    /*PRIVILEGED_FUNCTION*/; /*lint !e971 Unqualified char types are allowed for strings and single
                            characters only. */


BaseType_t CreateTimerManageTask(void) {

    // if(!pthread_create(&thread_timers_manage, NULL, (void*)TimersManageTask, NULL)) {
    //	return true;
    //}
 //or
    prvCheckForValidListAndQueue();

    if (xTimerQueue != NULL) {
       std::thread  *timer_thread = new std::thread(TimersManageTask, nullptr);
        //xNextTaskUnblockTime = portMAX_DELAY;
    }
    
    return true;
}

#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

TimerHandle_t xTimerCreate(const char* const pcTimerName,
    const TickType_t xTimerPeriodInTicks,
    const UBaseType_t uxAutoReload,
    void* const pvTimerID,
    TimerCallbackFunction_t pxCallbackFunction) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
    Timer_t* pxNewTimer;

    //pxNewTimer = (Timer_t*)pvPortMalloc(sizeof(Timer_t));
    pxNewTimer = new Timer_t[sizeof(Timer_t)];

    if (pxNewTimer != NULL)
    {
        prvInitialiseNewTimer(pcTimerName, xTimerPeriodInTicks, uxAutoReload, pvTimerID, pxCallbackFunction, pxNewTimer);

#if( configSUPPORT_STATIC_ALLOCATION == 1 )
        {
            /* Timers can be created statically or dynamically, so note this
            timer was created dynamically in case the timer is later
            deleted. */
            pxNewTimer->ucStaticallyAllocated = pdFALSE;
        }
#endif /* configSUPPORT_STATIC_ALLOCATION */
    }

    return pxNewTimer;
}

#endif /* configSUPPORT_STATIC_ALLOCATION */

#if( configSUPPORT_STATIC_ALLOCATION == 1 )
TimerHandle_t
xTimerCreateStatic(const char *const pcTimerName, const TickType_t xTimerPeriodInTicks,
                   const UBaseType_t uxAutoReload, void *const pvTimerID,
                   TimerCallbackFunction_t pxCallbackFunction,
                   StaticTimer_t *pxTimerBuffer) /*lint !e971 Unqualified char types are allowed for
                                                    strings and single characters only. */
{
    Timer_t *pxNewTimer;

#if (configASSERT_DEFINED == 1)
    {
        /* Sanity check that the size of the structure used to declare a
        variable of type StaticTimer_t equals the size of the real timer
        structures. */
        volatile size_t xSize = sizeof(StaticTimer_t);
        configASSERT(xSize == sizeof(Timer_t));
    }
#endif /* configASSERT_DEFINED */

    /* A pointer to a StaticTimer_t structure MUST be provided, use it. */
    configASSERT(pxTimerBuffer);
    pxNewTimer = (Timer_t *)
        pxTimerBuffer; /*lint !e740 Unusual cast is ok as the structures are designed to have the
                          same alignment, and the size is checked by an assert. */

    if (pxNewTimer != NULL) {
        prvInitialiseNewTimer(pcTimerName, xTimerPeriodInTicks, uxAutoReload, pvTimerID,
                              pxCallbackFunction, pxNewTimer);

#if (configSUPPORT_DYNAMIC_ALLOCATION == 1)
        {
            /* Timers can be created statically or dynamically so note this
            timer was created statically in case it is later deleted. */
            pxNewTimer->ucStaticallyAllocated = pdTRUE;
        }
#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
    }

    return pxNewTimer;
}
#endif

static void
prvInitialiseNewTimer(const char *const pcTimerName, const TickType_t xTimerPeriodInTicks,
                      const UBaseType_t uxAutoReload, void *const pvTimerID,
                      TimerCallbackFunction_t pxCallbackFunction,
                      Timer_t *pxNewTimer) /*lint !e971 Unqualified char types are allowed for
                                              strings and single characters only. */
{
    /* 0 is not a valid value for xTimerPeriodInTicks. */
    configASSERT((xTimerPeriodInTicks > 0));

    if (pxNewTimer != NULL) {
        /* Ensure the infrastructure used by the timer service task has been
        created/initialised. */
        prvCheckForValidListAndQueue();

        /* Initialise the timer structure members using the function
        parameters. */
        pxNewTimer->pcTimerName         = pcTimerName;
        pxNewTimer->xTimerPeriodInTicks = xTimerPeriodInTicks;
        pxNewTimer->uxAutoReload        = uxAutoReload;
        pxNewTimer->pvTimerID           = pvTimerID;
        pxNewTimer->pxCallbackFunction  = pxCallbackFunction;
        vListInitialiseItem(&(pxNewTimer->xTimerListItem));
        traceTIMER_CREATE(pxNewTimer);
    }
}

static void prvProcessExpiredTimer(const TickType_t xNextExpireTime, const TickType_t xTimeNow)
{
    BaseType_t xResult;
    Timer_t* const pxTimer = (Timer_t*)listGET_OWNER_OF_HEAD_ENTRY(pxCurrentTimerList);

    /* Remove the timer from the list of active timers.  A check has already
    been performed to ensure the list is not empty. */
    (void)uxListRemove(&(pxTimer->xTimerListItem));
    traceTIMER_EXPIRED(pxTimer);

    /* If the timer is an auto reload timer then calculate the next
    expiry time and re-insert the timer in the list of active timers. */
    if (pxTimer->uxAutoReload == (UBaseType_t)pdTRUE)
    {
        /* The timer is inserted into a list using a time relative to anything
        other than the current time.  It will therefore be inserted into the
        correct list relative to the time this task thinks it is now. */
        if (prvInsertTimerInActiveList(pxTimer, (xNextExpireTime + pxTimer->xTimerPeriodInTicks), xTimeNow, xNextExpireTime) != pdFALSE)
        {
            /* The timer expired before it was added to the active timer
            list.  Reload it now.  */
            xResult = xTimerGenericCommand(pxTimer, tmrCOMMAND_START_DONT_TRACE, xNextExpireTime, NULL, tmrNO_DELAY);
            configASSERT(xResult);
            (void)xResult;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }

    /* Call the timer callback. */
    pxTimer->pxCallbackFunction((TimerHandle_t)pxTimer);
}

void TimersManageTask(void *args) {
	TickType_t xNextExpireTime;
	BaseType_t xListWasEmpty;

	while(1) {
		/* Query the timers list to see if it contains any timers, and if so,
		obtain the time at which the next timer will expire. */
		xNextExpireTime = prvGetNextExpireTime( &xListWasEmpty );

		/* If a timer has expired, process it.  Otherwise, block this task
		until either a timer does expire, or a command is received. */
		prvProcessTimerOrBlockTask( xNextExpireTime, xListWasEmpty );

		/* Empty the command queue. */
		prvProcessReceivedCommands();
	}
}

static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty ) {
    TickType_t xNextExpireTime;
	/* Timers are listed in expiry time order, with the head of the list referencing the task that will expire first.
	*/
	*pxListWasEmpty = listLIST_IS_EMPTY( pxCurrentTimerList );
	if( *pxListWasEmpty == false ) {
		xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList );
	} else {
		/* Ensure the task unblocks when the tick count rolls over. */
		xNextExpireTime = ( TickType_t ) 0U;
	}

	return xNextExpireTime;
}

static TickType_t prvSampleTimeNow( BaseType_t * const pxTimerListsWereSwitched ) {
TickType_t xTimeNow;
/* PRIVILEGED_DATA*/ static TickType_t xLastTime = ( TickType_t ) 0U; /*lint !e956 Variable is only accessible to one task. */

	xTimeNow = xTaskGetTickCount();

	if( xTimeNow < xLastTime ) {
		prvSwitchTimerLists();
		*pxTimerListsWereSwitched = true;
	} else {
		*pxTimerListsWereSwitched = false;
	}

	xLastTime = xTimeNow;

	return xTimeNow;
}

static BaseType_t prvInsertTimerInActiveList(Timer_t* const pxTimer, const TickType_t xNextExpiryTime, const TickType_t xTimeNow, const TickType_t xCommandTime)
{
    BaseType_t xProcessTimerNow = pdFALSE;

    listSET_LIST_ITEM_VALUE(&(pxTimer->xTimerListItem), xNextExpiryTime);
    listSET_LIST_ITEM_OWNER(&(pxTimer->xTimerListItem), pxTimer);

    if (xNextExpiryTime <= xTimeNow)
    {
        /* Has the expiry time elapsed between the command to start/reset a
        timer was issued, and the time the command was processed? */
        if (((TickType_t)(xTimeNow - xCommandTime)) >= pxTimer->xTimerPeriodInTicks) /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
        {
            /* The time between a command being issued and the command being
            processed actually exceeds the timers period.  */
            xProcessTimerNow = pdTRUE;
        }
        else
        {
            vListInsert(pxOverflowTimerList, &(pxTimer->xTimerListItem));
        }
    }
    else
    {
        if ((xTimeNow < xCommandTime) && (xNextExpiryTime >= xCommandTime))
        {
            /* If, since the command was issued, the tick count has overflowed
            but the expiry time has not, then the timer must have already passed
            its expiry time and should be processed immediately. */
            xProcessTimerNow = pdTRUE;
        }
        else
        {
            vListInsert(pxCurrentTimerList, &(pxTimer->xTimerListItem));
        }
    }

    return xProcessTimerNow;
}

static void prvProcessTimerOrBlockTask( const TickType_t xNextExpireTime, BaseType_t xListWasEmpty ) {
    TickType_t xTimeNow;
    BaseType_t xTimerListsWereSwitched;
    
    //vTaskSuspendAll();
    {
        /* Obtain the time now to make an assessment as to whether the timer
        has expired or not.  If obtaining the time causes the lists to switch
        then don't process this timer as any timers that remained in the list
        when the lists were switched will have been processed within the
        prvSampleTimeNow() function. */
        xTimeNow = prvSampleTimeNow(&xTimerListsWereSwitched);
        if (xTimerListsWereSwitched == pdFALSE)
        {
            /* The tick count has not overflowed, has the timer expired? */
            if ((xListWasEmpty == pdFALSE) && (xNextExpireTime <= xTimeNow))
            {
                //(void)xTaskResumeAll();
                prvProcessExpiredTimer(xNextExpireTime, xTimeNow);
            }
            else
            {
                /* The tick count has not overflowed, and the next expire
                time has not been reached yet.  This task should therefore
                block to wait for the next expire time or a command to be
                received - whichever comes first.  The following line cannot
                be reached unless xNextExpireTime > xTimeNow, except in the
                case when the current timer list is empty. */
                if (xListWasEmpty != pdFALSE)
                {
                    /* The current timer list is empty - is the overflow list
                    also empty? */
                    xListWasEmpty = listLIST_IS_EMPTY(pxOverflowTimerList);
                }

                //vQueueWaitForMessageRestricted(xTimerQueue, (xNextExpireTime - xTimeNow), xListWasEmpty);

                //if (xTaskResumeAll() == pdFALSE)
                //{
                //    /* Yield to wait for either a command to arrive, or the
                //    block time to expire.  If a command arrived between the
                //    critical section being exited and this yield then the yield
                //    will not cause the task to block. */
                //    portYIELD_WITHIN_API();
                //}
                //else
                //{
                //    mtCOVERAGE_TEST_MARKER();
                //}
            }
        }
        else
        {
            //(void)xTaskResumeAll();
        }
    }

}
static void prvProcessReceivedCommands( void ) {
    Timer_t* pxTimer;
    BaseType_t xTimerListsWereSwitched, xResult;
    TickType_t xTimeNow;
    /* Commands that are positive are timer commands rather than pended
function calls. */
    if (xMessage.xMessageID >= (BaseType_t)0)
    {
        /* The messages uses the xTimerParameters member to work on a
        software timer. */
        pxTimer = xMessage.u.xTimerParameters.pxTimer;

        if (listIS_CONTAINED_WITHIN(NULL, &(pxTimer->xTimerListItem)) == pdFALSE)
        {
            /* The timer is in a list, remove it. */
            (void)uxListRemove(&(pxTimer->xTimerListItem));
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }

        traceTIMER_COMMAND_RECEIVED(pxTimer, xMessage.xMessageID, xMessage.u.xTimerParameters.xMessageValue);

        /* In this case the xTimerListsWereSwitched parameter is not used, but
        it must be present in the function call.  prvSampleTimeNow() must be
        called after the message is received from xTimerQueue so there is no
        possibility of a higher priority task adding a message to the message
        queue with a time that is ahead of the timer daemon task (because it
        pre-empted the timer daemon task after the xTimeNow value was set). */
        xTimeNow = prvSampleTimeNow(&xTimerListsWereSwitched);

        switch (xMessage.xMessageID)
        {
        case tmrCOMMAND_START:
        case tmrCOMMAND_START_FROM_ISR:
        case tmrCOMMAND_RESET:
        case tmrCOMMAND_RESET_FROM_ISR:
        case tmrCOMMAND_START_DONT_TRACE:
            /* Start or restart a timer. */
            if (prvInsertTimerInActiveList(pxTimer, xMessage.u.xTimerParameters.xMessageValue + pxTimer->xTimerPeriodInTicks, xTimeNow, xMessage.u.xTimerParameters.xMessageValue) != pdFALSE)
            {
                /* The timer expired before it was added to the active
                timer list.  Process it now. */
                pxTimer->pxCallbackFunction((TimerHandle_t)pxTimer);
                traceTIMER_EXPIRED(pxTimer);

                if (pxTimer->uxAutoReload == (UBaseType_t)pdTRUE)
                {
                    xResult = xTimerGenericCommand(pxTimer, tmrCOMMAND_START_DONT_TRACE, xMessage.u.xTimerParameters.xMessageValue + pxTimer->xTimerPeriodInTicks, NULL, tmrNO_DELAY);
                    configASSERT(xResult);
                    (void)xResult;
                    memset(&xMessage, 0, sizeof(xMessage));
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
            
            break;

        case tmrCOMMAND_STOP:
        case tmrCOMMAND_STOP_FROM_ISR:
            /* The timer has already been removed from the active list.
            There is nothing to do here. */
            break;

        case tmrCOMMAND_CHANGE_PERIOD:
        case tmrCOMMAND_CHANGE_PERIOD_FROM_ISR:
            pxTimer->xTimerPeriodInTicks = xMessage.u.xTimerParameters.xMessageValue;
            configASSERT((pxTimer->xTimerPeriodInTicks > 0));

            /* The new period does not really have a reference, and can
            be longer or shorter than the old one.  The command time is
            therefore set to the current time, and as the period cannot
            be zero the next expiry time can only be in the future,
            meaning (unlike for the xTimerStart() case above) there is
            no fail case that needs to be handled here. */
            (void)prvInsertTimerInActiveList(pxTimer, (xTimeNow + pxTimer->xTimerPeriodInTicks), xTimeNow, xTimeNow);
            break;

        case tmrCOMMAND_DELETE:
            /* The timer has already been removed from the active list,
            just free up the memory if the memory was dynamically
            allocated. */
#if( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 0 ) )
        {
            /* The timer can only have been allocated dynamically -
            free it again. */
            //vPortFree(pxTimer);
            delete[] pxTimer;
        }
#elif( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
        {
            /* The timer could have been allocated statically or
            dynamically, so check before attempting to free the
            memory. */
            if (pxTimer->ucStaticallyAllocated == (uint8_t)pdFALSE)
            {
                vPortFree(pxTimer);
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
        }
#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
        break;

        default:
            /* Don't expect to get here. */
            break;
        }
    }

}

BaseType_t xTimerGenericCommand(TimerHandle_t xTimer, const BaseType_t xCommandID,
                                const TickType_t  xOptionalValue,
                                BaseType_t *const pxHigherPriorityTaskWoken,
                                const TickType_t  xTicksToWait) {
    BaseType_t          xReturn = pdFAIL;

    configASSERT(xTimer);

    /* Send a message to the timer service task to perform a particular action
    on a particular timer definition. */
    if (xTimerQueue != NULL) {
        /* Send a command to the timer service task to start the xTimer timer. */
        xMessage.xMessageID                       = xCommandID;
        xMessage.u.xTimerParameters.xMessageValue = xOptionalValue;
        xMessage.u.xTimerParameters.pxTimer       = (Timer_t *)xTimer;

        //if (xCommandID < tmrFIRST_FROM_ISR_COMMAND) {
        //    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        //        xReturn = xQueueSendToBack(xTimerQueue, &xMessage, xTicksToWait);
        //    } else {
        //        xReturn = xQueueSendToBack(xTimerQueue, &xMessage, tmrNO_DELAY);
        //    }
        //} else {
        //    xReturn = xQueueSendToBackFromISR(xTimerQueue, &xMessage, pxHigherPriorityTaskWoken);
        //}

        //traceTIMER_COMMAND_SEND(xTimer, xCommandID, xOptionalValue, xReturn);
    } else {
        mtCOVERAGE_TEST_MARKER();
    }
    xReturn = pdPASS;
    return xReturn;
}

void Start() {
  
}

static void prvSwitchTimerLists(void) {
    TickType_t xNextExpireTime, xReloadTime;
    List_t *   pxTemp;
    Timer_t *  pxTimer;
    BaseType_t xResult;

    /* The tick count has overflowed.  The timer lists must be switched.
    If there are any timers still referenced from the current timer list
    then they must have expired and should be processed before the lists
    are switched. */
    while (listLIST_IS_EMPTY(pxCurrentTimerList) == pdFALSE) {
        xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY(pxCurrentTimerList);

        /* Remove the timer from the list. */
        pxTimer = (Timer_t *)listGET_OWNER_OF_HEAD_ENTRY(pxCurrentTimerList);
        (void)uxListRemove(&(pxTimer->xTimerListItem));
        traceTIMER_EXPIRED(pxTimer);

        /* Execute its callback, then send a command to restart the timer if
        it is an auto-reload timer.  It cannot be restarted here as the lists
        have not yet been switched. */
        pxTimer->pxCallbackFunction((TimerHandle_t)pxTimer);

        if (pxTimer->uxAutoReload == (UBaseType_t)pdTRUE) {
            /* Calculate the reload value, and if the reload value results in
            the timer going into the same timer list then it has already expired
            and the timer should be re-inserted into the current list so it is
            processed again within this loop.  Otherwise a command should be sent
            to restart the timer to ensure it is only inserted into a list after
            the lists have been swapped. */
            xReloadTime = (xNextExpireTime + pxTimer->xTimerPeriodInTicks);
            if (xReloadTime > xNextExpireTime) {
                listSET_LIST_ITEM_VALUE(&(pxTimer->xTimerListItem), xReloadTime);
                listSET_LIST_ITEM_OWNER(&(pxTimer->xTimerListItem), pxTimer);
                vListInsert(pxCurrentTimerList, &(pxTimer->xTimerListItem));
            } else {
                xResult = xTimerGenericCommand(pxTimer, tmrCOMMAND_START_DONT_TRACE,
                                               xNextExpireTime, NULL, tmrNO_DELAY);
                configASSERT(xResult);
                (void)xResult;
            }
        } else {
            mtCOVERAGE_TEST_MARKER();
        }
    }

    pxTemp              = pxCurrentTimerList;
    pxCurrentTimerList  = pxOverflowTimerList;
    pxOverflowTimerList = pxTemp;
}

static void prvCheckForValidListAndQueue(void) {
    /* Check that the list from which active timers are referenced, and the
    queue used to communicate with the timer service, have been
    initialised. */
    //taskENTER_CRITICAL();
    {
        if (xTimerQueue == NULL) {
            vListInitialise(&xActiveTimerList1);
            vListInitialise(&xActiveTimerList2);
            pxCurrentTimerList  = &xActiveTimerList1;
            pxOverflowTimerList = &xActiveTimerList2;

//#if (configSUPPORT_STATIC_ALLOCATION == 1)
            //{
            //    /* The timer queue is allocated statically in case
            //    configSUPPORT_DYNAMIC_ALLOCATION is 0. */
            //    static StaticQueue_t xStaticTimerQueue;
            //    static uint8_t       ucStaticTimerQueueStorage[configTIMER_QUEUE_LENGTH *
            //                                             sizeof(DaemonTaskMessage_t)];

            //    xTimerQueue = xQueueCreateStatic(
            //        (UBaseType_t)configTIMER_QUEUE_LENGTH, sizeof(DaemonTaskMessage_t),
            //        &(ucStaticTimerQueueStorage[0]), &xStaticTimerQueue);
            //}
//#else
            {
                xTimerQueue = xQueueCreate((UBaseType_t)configTIMER_QUEUE_LENGTH,
                                           sizeof(DaemonTaskMessage_t));
            }
//#endif

#if (configQUEUE_REGISTRY_SIZE > 0)
            {
                if (xTimerQueue != NULL) {
                    vQueueAddToRegistry(xTimerQueue, "TmrQ");
                } else {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
#endif /* configQUEUE_REGISTRY_SIZE */
        } else {
            mtCOVERAGE_TEST_MARKER();
        }
    }
    //taskEXIT_CRITICAL();
}

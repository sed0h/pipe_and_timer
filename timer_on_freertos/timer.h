
#ifndef __TIMER_H__
#define __TIMER_H__

#include <stdint.h>

/* IDs for commands that can be sent/received on the timer queue.  These are to
be used solely through the macros that make up the public software timer API,
as defined below.  The commands that are sent from interrupts must use the
highest numbers as tmrFIRST_FROM_ISR_COMMAND is used to determine if the task
or interrupt version of the queue send function should be used. */
#define tmrCOMMAND_EXECUTE_CALLBACK_FROM_ISR ((BaseType_t)-2)
#define tmrCOMMAND_EXECUTE_CALLBACK ((BaseType_t)-1)
#define tmrCOMMAND_START_DONT_TRACE ((BaseType_t)0)
#define tmrCOMMAND_START					    ( ( BaseType_t ) 1 )
#define tmrCOMMAND_RESET						( ( BaseType_t ) 2 )
#define tmrCOMMAND_STOP							( ( BaseType_t ) 3 )
#define tmrCOMMAND_CHANGE_PERIOD				( ( BaseType_t ) 4 )
#define tmrCOMMAND_DELETE						( ( BaseType_t ) 5 )

#define tmrFIRST_FROM_ISR_COMMAND ((BaseType_t)6)
#define tmrCOMMAND_START_FROM_ISR				( ( BaseType_t ) 6 )
#define tmrCOMMAND_RESET_FROM_ISR				( ( BaseType_t ) 7 )
#define tmrCOMMAND_STOP_FROM_ISR				( ( BaseType_t ) 8 )
#define tmrCOMMAND_CHANGE_PERIOD_FROM_ISR		( ( BaseType_t ) 9 )

typedef void* TimerHandle_t;

/*
 * Defines the prototype to which functions used with the
 * xTimerPendFunctionCallFromISR() function must conform.
 */
typedef void (*PendedFunction_t)(void*, uint32_t);

typedef void (*ExpireCallBack)(void);

typedef  struct timer_s {
	uint32_t tick_count;
	ExpireCallBack callback;

} timer_t;

/*
 * Defines the prototype to which timer callback functions must conform.
 */
typedef void (*TimerCallbackFunction_t)(TimerHandle_t xTimer);

TimerHandle_t xTimerCreate(const char* const pcTimerName,
	const TickType_t xTimerPeriodInTicks,
	const UBaseType_t uxAutoReload,
	void* const pvTimerID,
	TimerCallbackFunction_t pxCallbackFunction);
TimerHandle_t xTimerCreateStatic(const char* const pcTimerName, const TickType_t xTimerPeriodInTicks,
	const UBaseType_t uxAutoReload, void* const pvTimerID,
	TimerCallbackFunction_t pxCallbackFunction,
	StaticTimer_t* pxTimerBuffer);

BaseType_t CreateTimerManageTask(void);

#define xTimerStart( xTimer, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_START, ( xTaskGetTickCount() ), NULL, ( xTicksToWait ) )

BaseType_t xTimerGenericCommand(TimerHandle_t xTimer, const BaseType_t xCommandID,
	const TickType_t  xOptionalValue,
	BaseType_t* const pxHigherPriorityTaskWoken,
	const TickType_t  xTicksToWait);

void Delete();
void Start();
void Stop();
void Reset();


#endif


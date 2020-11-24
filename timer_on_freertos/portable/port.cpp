#include "..\uds.h"
#include "..\task.h"
#include "portable.h"
#include <Windows.h>

#ifdef __GNUC__
    #include "mmsystem.h"
#else
    #pragma comment(lib, "winmm.lib")
#endif

#define portMAX_INTERRUPTS                                                                         \
    ((uint32_t)sizeof(uint32_t) * 8UL) /* The number of bits in an uint32_t. */
#define portNO_CRITICAL_NESTING ((uint32_t)0)

/* The priorities at which the various components of the simulation execute.
Priorities are higher when a soak test is performed to lessen the effect of
Windows interfering with the timing. */
#define portSOAK_TEST
#ifndef portSOAK_TEST
    #define portSIMULATED_INTERRUPTS_THREAD_PRIORITY THREAD_PRIORITY_NORMAL
    #define portSIMULATED_TIMER_THREAD_PRIORITY THREAD_PRIORITY_BELOW_NORMAL
#else
    #define portSIMULATED_INTERRUPTS_THREAD_PRIORITY THREAD_PRIORITY_HIGHEST
    #define portSIMULATED_TIMER_THREAD_PRIORITY THREAD_PRIORITY_ABOVE_NORMAL
#endif

/*
 * Created as a high priority thread, this function uses a timer to simulate
 * a tick interrupt being generated on an embedded target.  In this Windows
 * environment the timer does not achieve anything approaching real time
 * performance though.
 */
static DWORD WINAPI prvSimulatedPeripheralTimer(LPVOID lpParameter);

/*
 * Process all the simulated interrupts - each represented by a bit in
 * ulPendingInterrupts variable.
 */
static void prvProcessSimulatedInterrupts(void);


/*
 * Interrupt handlers used by the kernel itself.  These are executed from the
 * simulated interrupt handler thread.
 */
static uint32_t prvProcessYieldInterrupt(void);
static uint32_t prvProcessTickInterrupt(void);

/*
 * Called when the process exits to let Windows know the high timer resolution
 * is no longer required.
 */
static BOOL WINAPI prvEndProcess(DWORD dwCtrlType);

/* The WIN32 simulator runs each task in a thread.  The context switching is
managed by the threads, so the task stack does not have to be managed directly,
although the task stack is still used to hold an xThreadState structure this is
the only thing it will ever hold.  The structure indirectly maps the task handle
to a thread handle. */
typedef struct {
    /* Handle of the thread that executes the task. */
    void *pvThread;

} xThreadState;

/* Simulated interrupts waiting to be processed.  This is a bit mask where each
bit represents one interrupt, so a maximum of 32 interrupts can be simulated. */
static volatile uint32_t ulPendingInterrupts = 0UL;

/* An event used to inform the simulated interrupt processing thread (a high
priority thread that simulated interrupt processing) that an interrupt is
pending. */
static void *pvInterruptEvent = NULL;

/* Mutex used to protect all the simulated interrupt variables that are accessed
by multiple threads. */
static void *pvInterruptEventMutex = NULL;

static uint32_t ulCriticalNesting = 9999UL;

/* Handlers for all the simulated software interrupts.  The first two positions
are used for the Yield and Tick interrupts so are handled slightly differently,
all the other interrupts can be user defined. */
static uint32_t (*ulIsrHandler[portMAX_INTERRUPTS])(void) = {0};

/* Pointer to the TCB of the currently executing task. */
//extern void *pxCurrentTCB;

/* Used to ensure nothing is processed during the startup sequence. */
static BaseType_t xPortRunning = pdFALSE;

static DWORD WINAPI prvSimulatedPeripheralTimer(LPVOID lpParameter) {
    TickType_t xMinimumWindowsBlockTime;
    TIMECAPS   xTimeCaps;

    /* Set the timer resolution to the maximum possible. */
    if (timeGetDevCaps(&xTimeCaps, sizeof(xTimeCaps)) == MMSYSERR_NOERROR) {
        xMinimumWindowsBlockTime = (TickType_t)xTimeCaps.wPeriodMin;
        timeBeginPeriod(xTimeCaps.wPeriodMin);

        /* Register an exit handler so the timeBeginPeriod() function can be
        matched with a timeEndPeriod() when the application exits. */
        SetConsoleCtrlHandler(prvEndProcess, TRUE);
    } else {
        xMinimumWindowsBlockTime = (TickType_t)20;
    }

    /* Just to prevent compiler warnings. */
    (void)lpParameter;

    for (;;) {
        /* Wait until the timer expires and we can access the simulated interrupt
        variables.  *NOTE* this is not a 'real time' way of generating tick
        events as the next wake time should be relative to the previous wake
        time, not the time that Sleep() is called.  It is done this way to
        prevent overruns in this very non real time simulated/emulated
        environment. */
        if (portTICK_PERIOD_MS < xMinimumWindowsBlockTime) {
            Sleep(xMinimumWindowsBlockTime);
        } else {
            Sleep(portTICK_PERIOD_MS);
        }

        configASSERT(xPortRunning);

        WaitForSingleObject(pvInterruptEventMutex, INFINITE);

        /* The timer has expired, generate the simulated tick event. */
        ulPendingInterrupts |= (1 << portINTERRUPT_TICK);

        /* The interrupt is now pending - notify the simulated interrupt
        handler thread. */
        if (ulCriticalNesting == 0) {
            SetEvent(pvInterruptEvent);
        }

        /* Give back the mutex so the simulated interrupt handler unblocks
        and can	access the interrupt handler variables. */
        ReleaseMutex(pvInterruptEventMutex);
    }

#ifdef __GNUC__
    /* Should never reach here - MingW complains if you leave this line out,
    MSVC complains if you put it in. */
    return 0;
#endif
}

static BOOL WINAPI prvEndProcess(DWORD dwCtrlType) {
    TIMECAPS xTimeCaps;

    (void)dwCtrlType;

    if (timeGetDevCaps(&xTimeCaps, sizeof(xTimeCaps)) == MMSYSERR_NOERROR) {
        /* Match the call to timeBeginPeriod( xTimeCaps.wPeriodMin ) made when
        the process started with a timeEndPeriod() as the process exits. */
        timeEndPeriod(xTimeCaps.wPeriodMin);
    }

    return pdPASS;
}

BaseType_t StartPortScheduler(void) {
    void *        pvHandle;
    int32_t       lSuccess = pdPASS;
    xThreadState *pxThreadState;

    /* Install the interrupt handlers used by the scheduler itself. */
    vPortSetInterruptHandler(portINTERRUPT_YIELD, prvProcessYieldInterrupt);
    vPortSetInterruptHandler(portINTERRUPT_TICK, prvProcessTickInterrupt);

    /* Create the events and mutexes that are used to synchronise all the
    threads. */
    pvInterruptEventMutex = CreateMutex(NULL, FALSE, NULL);
    pvInterruptEvent      = CreateEvent(NULL, FALSE, FALSE, NULL);

    if ((pvInterruptEventMutex == NULL) || (pvInterruptEvent == NULL)) {
        lSuccess = pdFAIL;
    }

    /* Set the priority of this thread such that it is above the priority of
    the threads that run tasks.  This higher priority is required to ensure
    simulated interrupts take priority over tasks. */
    pvHandle = GetCurrentThread();
    if (pvHandle == NULL) {
        lSuccess = pdFAIL;
    }

    if (lSuccess == pdPASS) {
        if (SetThreadPriority(pvHandle, portSIMULATED_INTERRUPTS_THREAD_PRIORITY) == 0) {
            lSuccess = pdFAIL;
        }
        SetThreadPriorityBoost(pvHandle, TRUE);
        SetThreadAffinityMask(pvHandle, 0x01);
    }

    if (lSuccess == pdPASS) {
        /* Start the thread that simulates the timer peripheral to generate
        tick interrupts.  The priority is set below that of the simulated
        interrupt handler so the interrupt event mutex is used for the
        handshake / overrun protection. */
        pvHandle = CreateThread(NULL, 0, prvSimulatedPeripheralTimer, NULL, CREATE_SUSPENDED, NULL);
        if (pvHandle != NULL) {
            SetThreadPriority(pvHandle, portSIMULATED_TIMER_THREAD_PRIORITY);
            SetThreadPriorityBoost(pvHandle, TRUE);
            SetThreadAffinityMask(pvHandle, 0x01);
            ResumeThread(pvHandle);
        }

        /* Start the highest priority task by obtaining its associated thread
        state structure, in which is stored the thread handle. */
//pxThreadState     = (xThreadState *)*((size_t *)pxCurrentTCB);
        ulCriticalNesting = portNO_CRITICAL_NESTING;

        /* Bump up the priority of the thread that is going to run, in the
        hope that this will assist in getting the Windows thread scheduler to
        behave as an embedded engineer might expect. */
//ResumeThread(pxThreadState->pvThread);

        /* Handle all simulated interrupts - including yield requests and
        simulated ticks. */
        prvProcessSimulatedInterrupts();
    }

    /* Would not expect to return from prvProcessSimulatedInterrupts(), so should
    not get here. */
    return 0;
}
/*-----------------------------------------------------------*/

static uint32_t prvProcessYieldInterrupt(void) {
    return pdTRUE;
}
/*-----------------------------------------------------------*/

static uint32_t prvProcessTickInterrupt(void) {
    uint32_t ulSwitchRequired;

    /* Process the tick itself. */
    configASSERT(xPortRunning);
    ulSwitchRequired = (uint32_t)xTaskIncrementTick();
    ulSwitchRequired = 0;

    return ulSwitchRequired;
}

static void prvProcessSimulatedInterrupts(void) {
    uint32_t      ulSwitchRequired, i;
    xThreadState *pxThreadState;
    void *        pvObjectList[2];
    //CONTEXT       xContext;

    /* Going to block on the mutex that ensured exclusive access to the simulated
    interrupt objects, and the event that signals that a simulated interrupt
    should be processed. */
    pvObjectList[0] = pvInterruptEventMutex;
    pvObjectList[1] = pvInterruptEvent;

    /* Create a pending tick to ensure the first task is started as soon as
    this thread pends. */
    ulPendingInterrupts |= (1 << portINTERRUPT_TICK);
    SetEvent(pvInterruptEvent);

    xPortRunning = pdTRUE;

    for (;;) {
        WaitForMultipleObjects(sizeof(pvObjectList) / sizeof(void *), pvObjectList, TRUE, INFINITE);

        /* Used to indicate whether the simulated interrupt processing has
        necessitated a context switch to another task/thread. */
        ulSwitchRequired = pdFALSE;

        /* For each interrupt we are interested in processing, each of which is
        represented by a bit in the 32bit ulPendingInterrupts variable. */
        for (i = 0; i < portMAX_INTERRUPTS; i++) {
            /* Is the simulated interrupt pending? */
            if (ulPendingInterrupts & (1UL << i)) {
                /* Is a handler installed? */
                if (ulIsrHandler[i] != NULL) {
                    /* Run the actual handler. */
                    if (ulIsrHandler[i]() != pdFALSE) {
                        ulSwitchRequired |= (1 << i);
                    }
                }

                /* Clear the interrupt pending bit. */
                ulPendingInterrupts &= ~(1UL << i);
            }
        }

//if (ulSwitchRequired != pdFALSE) {
//    void *pvOldCurrentTCB;

//    pvOldCurrentTCB = pxCurrentTCB;

//    /* Select the next task to run. */
//    vTaskSwitchContext();

//    /* If the task selected to enter the running state is not the task
//    that is already in the running state. */
//    if (pvOldCurrentTCB != pxCurrentTCB) {
//        /* Suspend the old thread. */
//        pxThreadState = (xThreadState *)*((size_t *)pvOldCurrentTCB);
//        SuspendThread(pxThreadState->pvThread);

//        /* Ensure the thread is actually suspended by performing a
//        synchronous operation that can only complete when the thread is
//        actually suspended.  The below code asks for dummy register
//        data. */
//        xContext.ContextFlags = CONTEXT_INTEGER;
//        (void)GetThreadContext(pxThreadState->pvThread, &xContext);

//        /* Obtain the state of the task now selected to enter the
//        Running state. */
//        pxThreadState = (xThreadState *)(*(size_t *)pxCurrentTCB);
//        ResumeThread(pxThreadState->pvThread);
//    }
//}

        ReleaseMutex(pvInterruptEventMutex);
    }
}

void vPortSetInterruptHandler(uint32_t ulInterruptNumber, uint32_t (*pvHandler)(void)) {
    if (ulInterruptNumber < portMAX_INTERRUPTS) {
        if (pvInterruptEventMutex != NULL) {
            WaitForSingleObject(pvInterruptEventMutex, INFINITE);
            ulIsrHandler[ulInterruptNumber] = pvHandler;
            ReleaseMutex(pvInterruptEventMutex);
        } else {
            ulIsrHandler[ulInterruptNumber] = pvHandler;
        }
    }
}
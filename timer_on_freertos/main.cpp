#include <stdio.h>
#include "uds.h"
#include "timer.h"
#include "list.h"
#include "task.h"

/* The periods assigned to the one-shot and auto-reload timers respectively. */
#define mainONE_SHOT_TIMER_PERIOD (pdMS_TO_TICKS(3333UL))

static void prvOneShotTimerCallback(TimerHandle_t xTimer);

int main() {

    timer_t timer1;
    timer1.tick_count = 50;
    // timer1.callback = CB;

    List_t     list_test;
    ListItem_t list_item_1, list_item_2, list_item_3;

    // root node initialise
    vListInitialise(&list_test);
    // node 1 initialise
    vListInitialiseItem(&list_item_1);
    list_item_1.xItemValue = 1;
    vListInitialiseItem(&list_item_2);
    list_item_2.xItemValue = 2;
    vListInitialiseItem(&list_item_3);
    list_item_3.xItemValue = 3;
    vListInsert(&list_test, &list_item_2);
    vListInsert(&list_test, &list_item_1);
    vListInsert(&list_test, &list_item_3); 

    TimerHandle_t xOneShotTimer;
    BaseType_t xTimer1Started;
    //StaticTimer_t *timer_buffer = new StaticTimer_t[sizeof(StaticTimer_t)];
    xOneShotTimer = xTimerCreate(
        "OneShot",                 /* Text name for the software timer - not used by FreeRTOS. */
        mainONE_SHOT_TIMER_PERIOD, /* The software timer's period in ticks. */
        pdFALSE, /* Setting uxAutoRealod to pdFALSE creates a one-shot software timer. */
        0,       /* This example does not use the timer id. */
        prvOneShotTimerCallback); /* The callback function to be used by the software timer
                                     being created. */

    if (xOneShotTimer != NULL) {
        xTimer1Started = xTimerStart(xOneShotTimer, 0);
        if(xTimer1Started == pdPASS) {
            CreateTimerManageTask();
        }
    }

    StartPortScheduler();
    
    for (;;);
    return 0;
}

static void prvOneShotTimerCallback(TimerHandle_t xTimer) {
    static TickType_t xTimeNow;

    /* Obtain the current tick count. */
    xTimeNow = xTaskGetTickCount();

    /* Output a string to show the time at which the callback was executed. */
    printf("One-shot timer callback executing %d\n", xTimeNow);
}

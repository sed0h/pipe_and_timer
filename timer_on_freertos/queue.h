#pragma once

/**
 * Type by which queues are referenced.  For example, a call to xQueueCreate()
 * returns an QueueHandle_t variable that can then be used as a parameter to
 * xQueueSend(), xQueueReceive(), etc.
 */
typedef void *QueueHandle_t;

QueueHandle_t xQueueGenericCreate(const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize,
                                  const uint8_t ucQueueType) /*PRIVILEGED_FUNCTION*/;

/* For internal use only.  These definitions *must* match those in queue.c. */
#define queueQUEUE_TYPE_BASE ((uint8_t)0U)

#define xQueueCreate(uxQueueLength, uxItemSize)                                                    \
    xQueueGenericCreate((uxQueueLength), (uxItemSize), (queueQUEUE_TYPE_BASE))


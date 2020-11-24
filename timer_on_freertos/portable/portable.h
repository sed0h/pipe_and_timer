#ifndef __PORTABLE_H__
#define __PORTABLE_H__

void vPortSetInterruptHandler(uint32_t ulInterruptNumber, uint32_t (*pvHandler)(void));

/*
 * Setup the hardware ready for the scheduler to take control.  This generally
 * sets up a tick interrupt and sets timers for the correct tick frequency.
 */
BaseType_t StartPortScheduler( void ) /*PRIVILEGED_FUNCTION*/;

#endif
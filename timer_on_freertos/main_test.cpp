#if 0 
#include <stdio.h>
#include <Windows.h>

#pragma comment(lib, "winmm.lib")

static void* pvInterruptEventMutex = NULL;

int main() {

	pvInterruptEventMutex = CreateMutex(NULL, FALSE, NULL);
	if (pvInterruptEventMutex != NULL) {
		WaitForSingleObject(pvInterruptEventMutex, INFINITE);
	}

	return 0;
}
#endif
#include "platforms/lock.h"
#include "platforms/thread.h"
#include "types/error.h"
#include "math/math.h"

#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <Windows.h>

Error Lock_create(Lock *res) {

	if(!res)
		return Error_nullPointer(0, 0);

	*res = (Lock) { .data = CreateMutexA(NULL, FALSE, NULL) };

	if (!res->data) {
		*res = (Lock) { 0 };
		return Error_platformError(0, GetLastError());
	}

	return Error_none();
}

Bool Lock_free(Lock *res) {

	if(!res || !res->data)
		return true;

	if(Lock_isLocked(*res))
		return false;

	CloseHandle(res->data);
	*res = (Lock){ 0 };
	return true;
}

Bool Lock_lock(Lock *l, Ns maxTime) {

	if (l && !Lock_isLocked(*l)) {

		DWORD wait = WaitForSingleObject(
			l->data, (DWORD) U64_min((U64_min(maxTime, U64_MAX - MS) + MS - 1) / MS, U32_MAX)
		);

		if (wait == WAIT_OBJECT_0) {
			l->lockThread = Thread_getId();
			return true;
		}
	}

	return false;
}

Bool Lock_isLockedForThread(Lock l) { return l.lockThread == Thread_getId(); }

Bool Lock_unlock(Lock *l) {

	if (l && Lock_isLockedForThread(*l) && ReleaseMutex(l->data)) {
		l->lockThread = 0;
		return true;
	}

	return false;
}

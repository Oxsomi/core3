#include "platforms/lock.h"
#include "platforms/thread.h"
#include "types/error.h"
#include "math/math.h"

#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <Windows.h>

Error Lock_create(Lock *res) {

	if(!res)
		return (Error) { .genericError = GenericError_NullPointer };

	*res = (Lock) { .data = CreateMutexA(
		NULL, FALSE, NULL
	)};

	if(!res->data)
		return (Error) { .genericError = GenericError_InvalidOperation };

	return Error_none();
}

Error Lock_free(Lock *res) {

	if(!res || !res->data)
		return (Error) { .genericError = GenericError_NullPointer };

	if(Lock_isLocked(*res))
		return (Error) { .genericError = GenericError_InvalidOperation };

	CloseHandle(res->data);
	*res = (Lock){ 0 };
	return Error_none();
}

Bool Lock_lock(Lock *l, Ns maxTime) {

	if (l && !Lock_isLocked(*l)) {

		//TODO: this rolls over

		DWORD wait = WaitForSingleObject(
			l->data, (DWORD) U64_min((maxTime + ms - 1) / ms, U32_MAX)
		);

		if (wait == WAIT_OBJECT_0) {
			l->lockThread = Thread_getId();
			return true;
		}
	}

	return false;
}

Bool Lock_isLockedForThread(Lock l) {
	return l.lockThread == Thread_getId();
}

Bool Lock_unlock(Lock *l) {

	if (l && Lock_isLockedForThread(*l) && ReleaseMutex(l->data)) {
		l->lockThread = 0;
		return true;
	}

	return false;
}

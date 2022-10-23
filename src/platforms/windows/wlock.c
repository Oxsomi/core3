#include "platforms/lock.h"
#include "platforms/thread.h"
#include "types/error.h"
#include "math/math.h"

#include <Windows.h>

struct Error Lock_create(struct Lock *res) {

	if(!res)
		return (struct Error) { .genericError = GenericError_NullPointer };

	*res = (struct Lock) { .data = CreateMutexA(
		NULL, FALSE, NULL
	)};

	if(!res->data)
		return (struct Error) { .genericError = GenericError_InvalidOperation };

	return Error_none();
}

struct Error Lock_free(struct Lock *res) {

	if(!res || !res->data)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if(Lock_isLocked(*res))
		return (struct Error) { .genericError = GenericError_InvalidOperation };

	CloseHandle(res->data);
	*res = (struct Lock){ 0 };
	return Error_none();
}

Bool Lock_lock(struct Lock l, Ns maxTime) {

	if (l.data && !Lock_isLocked(l)) {

		//TODO: this rolls over

		DWORD wait = WaitForSingleObject(
			l.data, (DWORD) U64_min((maxTime + ms - 1) / ms, U32_MAX)
		);

		if (wait == WAIT_OBJECT_0) {
			l.lockThread = Thread_getId();
			return true;
		}
	}

	return false;
}

Bool Lock_isLockedForThread(struct Lock l) {
	return l.lockThread == Thread_getId();
}

Bool Lock_unlock(struct Lock l) {

	if (l.data && Lock_isLockedForThread(l) && ReleaseMutex(l.data)) {
		l.lockThread = 0;
		return true;
	}

	return false;
}

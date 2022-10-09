#include "platforms/lock.h"
#include "types/error.h"

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

	CloseHandle(res->data);
	*res = (struct Lock){ 0 };
	return Error_none();
}

Bool Lock_lock(struct Lock l, Ns maxTime) {

	if (l.data) {
		DWORD wait = WaitForSingleObject(l.data, (maxTime + ms - 1) / ms);
		return wait == WAIT_OBJECT_0;
	}

	return false;
}

Bool Lock_unlock(struct Lock l) {

	if (l.data)
		return ReleaseMutex(l.data);

	return false;
}
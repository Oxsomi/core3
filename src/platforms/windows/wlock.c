/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
*/

#include "platforms/lock.h"
#include "platforms/thread.h"
#include "types/error.h"
#include "math/math.h"

#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <Windows.h>

Error Lock_create(Lock *res) {

	if(!res)
		return Error_nullPointer(0);

	if(res->data)
		return Error_invalidOperation(0);

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

/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
*  
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*  
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*  
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
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

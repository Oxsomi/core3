/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#pragma once
#include "atomic.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Lock {

	AtomicI64 lockedThreadId;

	Bool active;
	U8 pad[7];

} Lock;

typedef struct Error Error;

Lock Lock_create();
Bool Lock_free(Lock *res);

//Even though maxTime is in Ns it may be interpreted
//As a different unit by the runtime.
//Ex. Windows uses ms so it'll round up to ms

typedef enum ELockAcquire {

	ELockAcquire_Invalid = -2,
	ELockAcquire_TimedOut = -1,
	ELockAcquire_Acquired = 0,
	ELockAcquire_AlreadyLocked = 1,

	ELockAcquire_Success = 0			//Anything geq this is success

} ELockAcquire;

ELockAcquire Lock_lock(Lock *l, Ns maxTime);

Bool Lock_unlock(Lock *l);

Bool Lock_isLockedForThread(Lock *l);

#ifdef __cplusplus
	}
#endif

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

#define UNICODE
#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#define NOMINMAX
#include <Windows.h>

#include "types/base/atomic.h"

I64 AtomicI64_and(AtomicI64 *ptr, I64 value) {
	return InterlockedAnd64(&ptr->atomic, value);
}

I64 AtomicI64_xor(AtomicI64 *ptr, I64 value) {
	return InterlockedXor64(&ptr->atomic, value);
}

I64 AtomicI64_or(AtomicI64 *ptr, I64 value) {
	return InterlockedOr64(&ptr->atomic, value);
}

I64 AtomicI64_load(AtomicI64 *ptr) {
	return AtomicI64_add(ptr, 0);
}

I64 AtomicI64_add(AtomicI64 *ptr, I64 value) {
	return InterlockedExchangeAdd64(&ptr->atomic, value);
}

I64 AtomicI64_store(AtomicI64 *ptr, I64 value) {
	return InterlockedExchange64(&ptr->atomic, value);
}

I64 AtomicI64_cmpStore(AtomicI64 *ptr, I64 compare, I64 value) {
	return InterlockedCompareExchange64(&ptr->atomic, value, compare);
}

I64 AtomicI64_sub(AtomicI64 *ptr, I64 value) {

	if(value == I64_MIN)
		value = 0;

	return AtomicI64_add(ptr, -value);
}

I64 AtomicI64_inc(AtomicI64 *ptr) {
	return InterlockedIncrement64(&ptr->atomic);
}

I64 AtomicI64_dec(AtomicI64 *ptr) {
	return InterlockedDecrement64(&ptr->atomic);
}

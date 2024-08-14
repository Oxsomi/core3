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
#include "types/types.h"

#if _PLATFORM_TYPE != PLATFORM_WINDOWS
	#ifndef __cplusplus
		#include <stdatomic.h>
	#else
		#include <atomic>
		#define _Atomic(X) std::atomic< X >
	#endif
#else
	#define _Atomic(T) T
#endif

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct AtomicI64 {
	volatile _Atomic(I64) atomic;		//Don't manually touch
} AtomicI64;

#undef Atomic

//Bitwise

impl I64 AtomicI64_xor(AtomicI64 *ptr, I64 value);
impl I64 AtomicI64_and(AtomicI64 *ptr, I64 value);
impl I64 AtomicI64_or(AtomicI64 *ptr, I64 value);
impl I64 AtomicI64_load(AtomicI64 *ptr);
impl I64 AtomicI64_store(AtomicI64 *ptr, I64 value);
impl I64 AtomicI64_cmpStore(AtomicI64 *ptr, I64 compare, I64 value);		//If value in atomic is compare set to value

//Return state of atomic before adding value.

impl I64 AtomicI64_add(AtomicI64 *ptr, I64 value);
impl I64 AtomicI64_sub(AtomicI64 *ptr, I64 value);		//I64 = I64_MIN becomes NO-OP (only fetches, doesn't increment)

//Return state of atomic after adding value.

impl I64 AtomicI64_inc(AtomicI64 *ptr);
impl I64 AtomicI64_dec(AtomicI64 *ptr);

#ifdef __cplusplus
	}
#endif

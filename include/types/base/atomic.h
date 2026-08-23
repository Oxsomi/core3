/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
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

//types/base/atomic.h

#pragma once
#include "types/base/constants.h"
#include "types/base/platform_types.h"

#if _PLATFORM_TYPE != PLATFORM_WINDOWS
	#ifndef __cplusplus
		#include <stdatomic.h>
	#else
		#include <atomic>
		#define _Atomic(X) std::atomic<X>
	#endif
#else
	#include <intrin.h>
	#define _Atomic(T) T
#endif

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct AtomicI64 {

	volatile _Atomic(I64) atomic;        //Don't manually touch

	//std::atomic deletes its copy constructor, which makes every struct holding a SpinLock non copyable in
	// C++ and every TList over one fail to compile, since ListT_at returns T by value.
	//Windows never had this: there _Atomic(T) is plain T, so the whole C++ layer was written against a
	// copyable AtomicI64 and only the other platforms disagreed.
	//Copying an atomic is not atomic with respect to anything else, but neither was it on Windows, and the
	// lists doing it are moving storage around rather than synchronising through it.

	#if defined(__cplusplus) && _PLATFORM_TYPE != PLATFORM_WINDOWS

		AtomicI64() = default;

		AtomicI64(const AtomicI64 &other) { atomic.store(other.atomic.load()); }

		AtomicI64 &operator=(const AtomicI64 &other) {

			if(this != &other)
				atomic.store(other.atomic.load());

			return *this;
		}

	#endif

} AtomicI64;

#undef Atomic

//Bitwise

impl static inline I64 AtomicI64_xor(AtomicI64 *ptr, I64 value);
impl static inline I64 AtomicI64_and(AtomicI64 *ptr, I64 value);
impl static inline I64 AtomicI64_or(AtomicI64 *ptr, I64 value);
impl static inline I64 AtomicI64_load(AtomicI64 *ptr);
impl static inline I64 AtomicI64_store(AtomicI64 *ptr, I64 value);
impl static inline I64 AtomicI64_cmpStore(AtomicI64 *ptr, I64 compare, I64 value);

//Return state of atomic before adding value.

impl static inline I64 AtomicI64_add(AtomicI64 *ptr, I64 value);
impl static inline I64 AtomicI64_sub(AtomicI64 *ptr, I64 value);     //I64 = I64_MIN becomes NO-OP (only fetches)

//Return state of atomic after adding value.

impl static inline I64 AtomicI64_inc(AtomicI64 *ptr);
impl static inline I64 AtomicI64_dec(AtomicI64 *ptr);

//Implementation

#if _PLATFORM_TYPE == PLATFORM_WINDOWS

	static inline I64 AtomicI64_and(AtomicI64 *ptr, I64 value) {
		return _InterlockedAnd64(&ptr->atomic, value);
	}

	static inline I64 AtomicI64_xor(AtomicI64 *ptr, I64 value) {
		return _InterlockedXor64(&ptr->atomic, value);
	}

	static inline I64 AtomicI64_or(AtomicI64 *ptr, I64 value) {
		return _InterlockedOr64(&ptr->atomic, value);
	}

	static inline I64 AtomicI64_load(AtomicI64 *ptr) {
		//Cheap acquire load instead of a locked read-modify-write.
		I64 v = (I64) __iso_volatile_load64((volatile __int64*)&ptr->atomic);
		#if defined(_M_ARM64)
			//A plain volatile load is NOT ordered on ARM64 under MSVC /volatile:iso.
			//Emit a load-acquire barrier after the load (inner shareable, load side).
			__dmb(_ARM64_BARRIER_ISHLD);
		#else
			//x86/x64 volatile load is already acquire under TSO; only block compiler reordering.
			_ReadWriteBarrier();
		#endif
		return v;
	}

	static inline I64 AtomicI64_add(AtomicI64 *ptr, I64 value) {
		return _InterlockedExchangeAdd64(&ptr->atomic, value);
	}

	static inline I64 AtomicI64_store(AtomicI64 *ptr, I64 value) {
		return _InterlockedExchange64(&ptr->atomic, value);
	}

	static inline I64 AtomicI64_cmpStore(AtomicI64 *ptr, I64 compare, I64 value) {
		return _InterlockedCompareExchange64(&ptr->atomic, value, compare);
	}

	static inline I64 AtomicI64_sub(AtomicI64 *ptr, I64 value) {

		if(value == I64_MIN)
			value = 0;

		return AtomicI64_add(ptr, -value);
	}

	static inline I64 AtomicI64_inc(AtomicI64 *ptr) {
		return _InterlockedIncrement64(&ptr->atomic);
	}

	static inline I64 AtomicI64_dec(AtomicI64 *ptr) {
		return _InterlockedDecrement64(&ptr->atomic);
	}

#else

	static inline I64 AtomicI64_and(AtomicI64 *ptr, I64 value) {
		return atomic_fetch_and(&ptr->atomic, value);
	}

	static inline I64 AtomicI64_xor(AtomicI64 *ptr, I64 value) {
		return atomic_fetch_xor(&ptr->atomic, value);
	}

	static inline I64 AtomicI64_or(AtomicI64 *ptr, I64 value) {
		return atomic_fetch_or(&ptr->atomic, value);
	}

	static inline I64 AtomicI64_load(AtomicI64 *ptr) {
		//Cheap acquire load instead of a locked read-modify-write.
		#ifdef __cplusplus
			//memory_order_acquire lives in namespace std in C++ (no ADL for a bare enumerator).
			return atomic_load_explicit(&ptr->atomic, std::memory_order_acquire);
		#else
			return atomic_load_explicit(&ptr->atomic, memory_order_acquire);
		#endif
	}

	static inline I64 AtomicI64_add(AtomicI64 *ptr, I64 value) {
		return atomic_fetch_add(&ptr->atomic, value);
	}

	static inline I64 AtomicI64_store(AtomicI64 *ptr, I64 value) {
		return atomic_exchange(&ptr->atomic, value);
	}

	static inline I64 AtomicI64_cmpStore(AtomicI64 *ptr, I64 compare, I64 value) {
		atomic_compare_exchange_strong(&ptr->atomic, &compare, value);
		return compare;
	}

	static inline I64 AtomicI64_sub(AtomicI64 *ptr, I64 value) {

		if(value == I64_MIN)
			value = 0;

		return AtomicI64_add(ptr, -value);
	}

	static inline I64 AtomicI64_inc(AtomicI64 *ptr) {
		return AtomicI64_add(ptr, 1) + 1;    //add returns old value, so correct for inc
	}

	static inline I64 AtomicI64_dec(AtomicI64 *ptr) {
		return AtomicI64_sub(ptr, 1) - 1;    //sub returns old value, so correct for dec
	}

#endif

#ifdef __cplusplus
	}
#endif

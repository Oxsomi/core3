/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
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

#include "types/atomic.h"

I64 AtomicI64_and(AtomicI64 *ptr, I64 value) {
	return atomic_fetch_and(&ptr->atomic, value);
}

I64 AtomicI64_xor(AtomicI64 *ptr, I64 value) {
	return atomic_fetch_xor(&ptr->atomic, value);
}

I64 AtomicI64_or(AtomicI64 *ptr, I64 value) {
	return atomic_fetch_or(&ptr->atomic, value);
}

I64 AtomicI64_load(AtomicI64 *ptr) {
	return AtomicI64_add(ptr, 0);
}

I64 AtomicI64_add(AtomicI64 *ptr, I64 value) {
	return atomic_fetch_add(&ptr->atomic, value);
}

I64 AtomicI64_store(AtomicI64 *ptr, I64 value) {
	return atomic_exchange(&ptr->atomic, value);
}

I64 AtomicI64_cmpStore(AtomicI64 *ptr, I64 compare, I64 value) {
	atomic_compare_exchange_strong(&ptr->atomic, &compare, value);
	return compare;
}

I64 AtomicI64_sub(AtomicI64 *ptr, I64 value) {

	if(value == I64_MIN)
		value = 0;

	return AtomicI64_add(ptr, -value);
}

I64 AtomicI64_inc(AtomicI64 *ptr) {
	return AtomicI64_add(ptr, 1) + 1;	//add returns old value, so correct for inc
}

I64 AtomicI64_dec(AtomicI64 *ptr) {
	return AtomicI64_sub(ptr, 1) - 1;	//sub returns old value, so correct for dec
}

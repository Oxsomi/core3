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

#include "types/pack.h"

U64 U64_pack21x3(U32 x, U32 y, U32 z) {

	if ((x >> 21) || (y >> 21) || (z >> 21))
		return U64_MAX;									//Returns U64_MAX on invalid

	return x | ((U64)y << 21) | ((U64)z << 42);
}

Bool U64_pack20x3u4(U64 *dst, U32 x, U32 y, U32 z, U8 u4) {

	if (!dst || (x >> 20) || (y >> 20) || (z >> 20) || (u4 >> 4))
		return false;

	*dst = x | ((U64)y << 20) | ((U64)z << 40) | ((U64)u4 << 60);
	return true;
}

U32 U64_unpack21x3(U64 packed, U8 off) {

	if ((packed >> 63) || (off >= 3))
		return U32_MAX;									//Returns U32_MAX on invalid

	return (U32)((packed >> (21 * off)) & ((1 << 21) - 1));
}

U32 U64_unpack20x3u4(U64 packed, U8 off) {

	if (off > 3)
		return U32_MAX;									//Returns U32_MAX on invalid

	if (off == 3)
		return packed >> 60;

	return (U32)((packed >> (20 * off)) & ((1 << 20) - 1));
}

Bool U64_setPacked20x3u4(U64 *packed, U8 off, U32 v) {

	if (off == 3) {

		if(!packed || v >> 4)
			return false;

		*packed &= ~((U64)0xF << 60);
		*packed |= (U64)v << 60;
		return true;
	}

	if (v >> 20 || off > 3 || !packed)
		return false;

	off *= 20;
	*packed &= ~((U64)((1 << 20) - 1) << off);		//Reset bits
	*packed |= (U64)v << off;						//Set bits
	return true;
}

Bool U64_setPacked21x3(U64 *packed, U8 off, U32 v) {

	if (v >> 21 || off >= 3 || !packed)
		return false;

	off *= 21;
	*packed &= ~((U64)((1 << 21) - 1) << off);		//Reset bits
	*packed |= (U64)v << off;						//Set bits
	return true;
}

//U32 packing

#define GET_BIT_IMPL(T)								\
													\
Bool T##_getBit(T packed, U8 off) {					\
													\
	if (off >= sizeof(T) * 8)						\
		return false;								\
													\
	return (packed >> off) & 1;						\
}													\
													\
Bool T##_setBit(T *packed, U8 off, Bool b) {		\
													\
	if (off >= sizeof(T) * 8)						\
		return false;								\
													\
	T shift = (T)1 << off;							\
													\
	if (b)											\
		*packed |= shift;							\
													\
	else *packed &= ~shift;							\
													\
	return true;									\
}

GET_BIT_IMPL(U64);
GET_BIT_IMPL(U32);
GET_BIT_IMPL(U16);
GET_BIT_IMPL(U8);

//Compressing quaternions

QuatF32 QuatS16_unpack(QuatS16 q) {
	const F32x4 v = F32x4_create4(q.arr[0], q.arr[1], q.arr[2], q.arr[3]);
	return F32x4_div(v, F32x4_xxxx4(I16_MAX));
}

QuatS16 QuatF32_pack(QuatF32 q) {

	q = QuatF32_normalize(q);
	const F32x4 asI16 = F32x4_mul(q, F32x4_xxxx4(I16_MAX));

	return (QuatS16) {
		{
			(I16) F32x4_x(asI16),
			(I16) F32x4_y(asI16),
			(I16) F32x4_z(asI16),
			(I16) F32x4_w(asI16)
		}
	};
}

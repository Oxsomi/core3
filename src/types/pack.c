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

U32 U64_unpack20x3(U64 packed, U8 off) {

	if (off >= 3)
		return U32_MAX;									//Returns U32_MAX on invalid

	return (U32)((packed >> (20 * off)) & ((1 << 20) - 1));
}

Bool U64_getBit(U64 packed, U8 off) {

	if (off >= 64)
		return false;

	return (packed >> off) & 1;
}

Bool U64_setPacked20x3(U64 *packed, U8 off, U32 v) {

	if (v >> 20 || off >= 3 || !packed)
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

Bool U64_setBit(U64 *packed, U8 off, Bool b) {

	if (off >= 64)
		return false;

	U64 shift = (U64)1 << off;

	if (b)
		*packed |= shift;

	else *packed &= ~shift;

	return true;
}

//U32 packing

Bool U32_getBit(U32 packed, U8 off) {

	if (off >= 32)
		return false;

	return (packed >> off) & 1;
}

Bool U32_setBit(U32 *packed, U8 off, Bool b) {

	if (off >= 32)
		return false;

	U32 shift = 1u << off;

	if (b)
		*packed |= shift;

	else *packed &= ~shift;

	return true;
}

//Compressing quaternions

Quat Quat_unpack(Quat16 q) {
	F32x4 v = F32x4_create4(q.arr[0], q.arr[1], q.arr[2], q.arr[3]);
	return F32x4_div(v, F32x4_xxxx4(I16_MAX));
}

Quat16 Quat_pack(Quat q) {

	q = Quat_normalize(q);
	F32x4 asI16 = F32x4_mul(q, F32x4_xxxx4(I16_MAX));

	return (Quat16) {
		{
			(I16) F32x4_x(asI16),
				(I16) F32x4_y(asI16),
				(I16) F32x4_z(asI16),
				(I16) F32x4_w(asI16)
		}
	};
}

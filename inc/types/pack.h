#pragma once
#include "types/types.h"

inline u64 u64_pack21x3(u32 x, u32 y, u32 z) { 

	if ((x >> 21) || (y >> 21) || (z >> 21))
		return u64_MAX;									//Returns u64_MAX on invalid

	return x | ((u64)y << 21) | ((u64)z << 42);
}

inline bool u64_pack20x3u4(u64 *dst, u32 x, u32 y, u32 z, u8 u4) {

	if (!dst || (x >> 20) || (y >> 20) || (z >> 20) || (u4 >> 4))
		return false;

	*dst = x | ((u64)y << 20) | ((u64)z << 40) | ((u64)u4 << 60);
	return true;
}

inline u32 u64_unpack21x3(u64 packed, u8 off) {

	if ((packed >> 63) || (off >= 3))
		return u32_MAX;									//Returns u32_MAX on invalid

	return (u32)((packed >> (21 * off)) & ((1 << 21) - 1));
}

inline u32 u64_unpack20x3(u64 packed, u8 off) {

	if (off >= 3)
		return u32_MAX;									//Returns u32_MAX on invalid

	return (u32)((packed >> (20 * off)) & ((1 << 20) - 1));
}

inline bool u64_getBit(u64 packed, u8 off) {

	if (off >= 64)
		return false;

	return (packed >> off) & 1;
}

inline bool u64_setPacked20x3(u64 *packed, u8 off, u32 v) {

	if (v >> 20 || off >= 3 || !packed)
		return false;

	off *= 20;
	*packed &= ~((u64)((1 << 20) - 1) << off);		//Reset bits
	*packed |= (u64)v << off;						//Set bits
	return true;
}

inline bool u64_setBit(u64 *packed, u8 off, bool b) {

	if (off >= 64)
		return false;

	u64 shift = (u64)1 << off;

	if (b)
		*packed |= shift;

	else *packed &= ~shift;

	return true;
}
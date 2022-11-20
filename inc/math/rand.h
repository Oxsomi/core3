#pragma once
#include "vec.h"

//TODO: Implement CSPRNG

//These are Psuedo RNG (PRNG), don't use these for critical uses!
//More info:
//	https://cheatsheetseries.owasp.org/cheatsheets/Cryptographic_Storage_Cheat_Sheet.html#secure-random-number-generation

U32 Random_seed(U16 x, U16 y, U16 w, U32 val1);

//Generate 'random' value [0, 1>
inline F32 Random_sample(U32 *seed) {
	*seed = (1664525u * *seed + 1013904223u);
	return (F32)(*seed & 0x00FFFFFF) / (F32)(0x01000000);
}

inline F32x4 Random_sample2(U32 *seed) {
	return F32x4_create2(Random_sample(seed), Random_sample(seed));
}
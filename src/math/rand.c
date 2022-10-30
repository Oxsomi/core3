#include "math/rand.h"

//Ported from Wisp:
//https://github.com/TeamWisp/WispRenderer/blob/master/resources/shaders/rand_util.hlsl

U32 Random_seed(U16 x, U16 y, U16 w, U32 val1) {

	U32 val0 = (U32)x + (U32)y * w;

	U32 v0 = val0, v1 = val1, s0 = 0;

	for (U32 n = 0; n < 16; n++) {
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}

	return v0;
}

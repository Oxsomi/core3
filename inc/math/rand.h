#pragma once
#include "vecf.h"

//Ported from Wisp:
//https://github.com/TeamWisp/WispRenderer/blob/master/resources/shaders/rand_util.hlsl

inline u32 Random_seed(u16 x, u16 y, u16 w, u32 val1) {

	u32 val0 = (u32)x + (u32)y * w;

	u32 v0 = val0, v1 = val1, s0 = 0;

	for (u32 n = 0; n < 16; n++) {
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}

	return v0;
}

//Generate 'random' value [0, 1>
inline f32 Random_sample(u32 *seed) {
	*seed = (1664525u * *seed + 1013904223u);
	return (f32)(*seed & 0x00FFFFFF) / (f32)(0x01000000);
}

inline f32x4 Random_sample2(u32 *seed) {
	return f32x4_init2(Random_sample(seed), Random_sample(seed));
}
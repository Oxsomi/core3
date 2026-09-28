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

//types/math/test/test_types_math_pack.c

#include "test_types_math_shared.h"
#include "types/math/pack.h"
#include "types/math/tonemap.h"
#include "types/math/quat.h"

void Test_pack21x3(Test *test) {

	Test_setModule(test, "U64_pack21x3");

	U32 x = 0x1FFFFF, y = 0x0, z = 0x123456;
	U64 packed = U64_pack21x3(x, y, z);

	Test_assert(test, "U64_unpack21x3", U64_unpack21x3(packed, 0) == x);
	Test_assert(test, "U64_unpack21x3", U64_unpack21x3(packed, 1) == y);
	Test_assert(test, "U64_unpack21x3", U64_unpack21x3(packed, 2) == z);

	//Out of bounds indices return U32_MAX

	Test_assert(test, "U64_unpack21x3", U64_unpack21x3(U64_MAX, 0) == U32_MAX);
	Test_assert(test, "U64_unpack21x3", U64_unpack21x3(U64_MAX, 3) == U32_MAX);

	//setPacked

	Test_assert(test, "U64_setPacked21x3", U64_setPacked21x3(&packed, 1, 0xABCDE));
	Test_assert(test, "U64_setPacked21x3", U64_unpack21x3(packed, 1) == 0xABCDE);

	//Invalid offset

	Test_assert(test, "U64_setPacked21x3", !U64_setPacked21x3(&packed, 3, 1));
}

void Test_pack20x3u4(Test *test) {

	Test_setModule(test, "U64_pack20x3u4");

	U64 packed = 0;
	U32 x = 0xFFFFF, y = 0xABCDE, z = 0x12345;
	U8 u4 = 0xF;

	Test_assert(test, "U64_pack20x3u4",   U64_pack20x3u4(&packed, x, y, z, u4));
	Test_assert(test, "U64_unpack20x3u4", U64_unpack20x3u4(packed, 0) == x);
	Test_assert(test, "U64_unpack20x3u4", U64_unpack20x3u4(packed, 1) == y);
	Test_assert(test, "U64_unpack20x3u4", U64_unpack20x3u4(packed, 2) == z);
	Test_assert(test, "U64_unpack20x3u4", U64_unpack20x3u4(packed, 3) == u4);

	//Out of bounds index returns U32_MAX

	Test_assert(test, "U64_unpack20x3u4", U64_unpack20x3u4(U64_MAX, 4) == U32_MAX);

	//setPacked

	Test_assert(test, "U64_setPacked20x3u4", U64_setPacked20x3u4(&packed, 2, 0x54321));
	Test_assert(test, "U64_setPacked20x3u4", U64_unpack20x3u4(packed, 2) == 0x54321);

	//Value too large for u4 slot

	Test_assert(test, "U64_setPacked20x3u4", !U64_setPacked20x3u4(&packed, 3, 0x10));
}

void Test_packBit(Test *test) {

	Test_setModule(test, "U32 bit set/get");

	U32 val = 0;

	Test_assert(test, "U32_setBit", U32_setBit(&val, 5, true));
	Test_assert(test, "U32_getBit", U32_getBit(val, 5));
	Test_assert(test, "U32_setBit", U32_setBit(&val, 5, false));
	Test_assert(test, "U32_getBit", !U32_getBit(val, 5));

	//Other bits unaffected

	Test_assert(test, "U32_getBit", !U32_getBit(val, 4));
	Test_assert(test, "U32_getBit", !U32_getBit(val, 6));

	//Set multiple bits independently

	Test_assert(test, "U32_setBit", U32_setBit(&val, 0, true));
	Test_assert(test, "U32_setBit", U32_setBit(&val, 31, true));
	Test_assert(test, "U32_getBit", U32_getBit(val, 0));
	Test_assert(test, "U32_getBit", U32_getBit(val, 31));
	Test_assert(test, "U32_getBit", !U32_getBit(val, 1));

	//Out of bounds offset should fail

	Test_assert(test, "U32_setBit", !U32_setBit(&val, 32, true));
}

void Test_packQuat(Test *test) {

	Test_setModule(test, "QuatF32 pack/unpack");

	const QuatF32 quats[] = {
		QuatF32_create(0.3f,  0.5f,  0.4f,  0.7f),
		QuatF32_create(-0.3f,  0.5f,  0.4f,  0.7f),
		QuatF32_create(0.3f, -0.5f,  0.4f,  0.7f),
		QuatF32_create(-0.3f, -0.5f,  0.4f,  0.7f),
		QuatF32_create(0.3f,  0.5f, -0.4f,  0.7f),
		QuatF32_create(-0.3f,  0.5f, -0.4f,  0.7f),
		QuatF32_create(0.3f, -0.5f, -0.4f,  0.7f),
		QuatF32_create(-0.3f, -0.5f, -0.4f,  0.7f),
		QuatF32_create(0.3f,  0.5f,  0.4f, -0.7f),
		QuatF32_create(-0.3f,  0.5f,  0.4f, -0.7f),
		QuatF32_create(0.3f, -0.5f,  0.4f, -0.7f),
		QuatF32_create(-0.3f, -0.5f,  0.4f, -0.7f),
		QuatF32_create(0.3f,  0.5f, -0.4f, -0.7f),
		QuatF32_create(-0.3f,  0.5f, -0.4f, -0.7f),
		QuatF32_create(0.3f, -0.5f, -0.4f, -0.7f),
		QuatF32_create(-0.3f, -0.5f, -0.4f, -0.7f),

		//Axis-aligned unit quaternions

		QuatF32_create(1,  0,  0,  0),
		QuatF32_create(0,  1,  0,  0),
		QuatF32_create(0,  0,  1,  0),
		QuatF32_create(0,  0,  0,  1),
		QuatF32_create(-1,  0,  0,  0),
		QuatF32_create(0, -1,  0,  0),
		QuatF32_create(0,  0, -1,  0),
		QuatF32_create(0,  0,  0, -1)
	};

	//Due to re-normalization and floating point precision we lose some bits

	const F32 maxDelta = 256.f / (1 << 20);
	const F32 maxDeltaW = 1.f / 16.f;
	const F32x4 threshold = F32x4_create4(maxDelta, maxDelta, maxDelta, maxDeltaW);

	for (U64 i = 0; i < sizeof(quats) / sizeof(quats[0]); ++i) {
		const QuatF32 quat = QuatF32_normalize(quats[i]);
		const QuatS16 q16 = QuatF32_pack(quat);
		const QuatF32 qf = QuatS16_unpack(q16);
		const F32x4 delta = F32x4_abs(F32x4_sub(quat, qf));
		Test_assert(test, "QuatF32 pack/unpack", !F32x4_any(F32x4_gt(delta, threshold)));
	}
}

void Test_packRGB10A2(Test *test) {

	Test_setModule(test, "RGB10A2 pack/unpack");

	//The ends are exact, and the channels land where the format puts them.

	const U32 one = U32_packRGB10A2(F32x4_create4(1, 1, 1, 1));
	Test_assert(test, "ones", one == U32_MAX);
	Test_assert(test, "zeros", U32_packRGB10A2(F32x4_zero()) == 0);

	const U32 layout = U32_packRGB10A2(F32x4_create4(1, 0, 0, 0)) | U32_packRGB10A2(F32x4_create4(0, 0, 1, 0));
	Test_assert(test, "layout", layout == (0x3FFu | (0x3FFu << 20)));
	Test_assert(test, "alpha", U32_packRGB10A2(F32x4_create4(0, 0, 0, 2.f / 3)) == 2u << 30);

	const F32x4 back = F32x4_unpackRGB10A2(one);
	Test_assert(
		test, "ones round trip",
		F32x4_x(back) == 1 && F32x4_y(back) == 1 && F32x4_z(back) == 1 && F32x4_w(back) == 1
	);

	//Out of range clamps rather than wrapping into the neighboring channel.

	Test_assert(
		test, "clamps",
		U32_packRGB10A2(F32x4_create4(-1, 2, 0.5f, 7)) == ((0x3FFu << 10) | (512u << 20) | (3u << 30))
	);

	//Rounded to nearest: every value comes back within half a step.

	Bool within = true;

	for(U32 i = 0; i <= 1000; ++i) {
		const F32 v = (F32) i / 1000;
		const F32 r = F32x4_x(F32x4_unpackRGB10A2(U32_packRGB10A2(F32x4_create4(v, 0, 0, 0))));
		within &= F32_abs(r - v) <= 0.5f / 1023 + 1e-6f;
	}

	Test_assert(test, "half step", within);
}

void Test_tonemap(Test *test) {

	Test_setModule(test, "Tonemap");

	//Grey at 0, mid grey and a hot 4, against values computed independently from each curve's published formula.

	static const F32 grey[3] = { 0, 0.18f, 4 };

	static const F32 expected[ETonemap_Count][3] = {
		{ 0, 0.461356f, 1 },
		{ 0, 0.426946f, 0.906332f },
		{ 0, 0.358457f, 0.958888f },
		{ 0, 0.496676f, 0.934048f },
		{ 0, 0.410021f, 0.992603f }
	};

	Bool matches = true, neutral = true, monotonic = true, bounded = true;

	for(U32 op = 0; op < ETonemap_Count; ++op) {

		for(U32 i = 0; i < 3; ++i) {
			const F32x4 v = F32x4_tonemap(F32x4_create4(grey[i], grey[i], grey[i], 1), (ETonemap) op);
			matches &= F32_abs(F32x4_x(v) - expected[op][i]) <= 1e-4f;

			//Grey stays grey to a quarter of an 8 bit step: AgX's published matrices drift it by 2.3e-4 at most.

			neutral &= F32_abs(F32x4_x(v) - F32x4_z(v)) <= 1e-3f;
		}

		//Brighter never displays darker, and nothing leaves [0, 1], down to negative input and up past the sun.

		F32 prev = -1;

		for(F32 x = -1; x < 1e5f; x = x < 0.01f ? x + 0.01f : x * 1.25f) {

			const F32 y = F32x4_y(F32x4_tonemap(F32x4_create4(x, x, x, 1), (ETonemap) op));

			monotonic &= y >= prev - 1e-6f;
			bounded &= y >= 0 && y <= 1;
			prev = y;
		}
	}

	Test_assert(test, "reference values", matches);
	Test_assert(test, "grey stays neutral", neutral);
	Test_assert(test, "monotonic in grey", monotonic);
	Test_assert(test, "bounded to [0, 1]", bounded);

	//The sRGB pair inverts itself.

	Bool roundTrip = true;

	for(U32 i = 0; i <= 255; ++i) {
		const F32 v = (F32) i / 255;
		const F32x4 back = F32x4_srgbEncode(F32x4_srgbDecode(F32x4_xxxx4(v)));
		roundTrip &= F32_abs(F32x4_x(back) - v) <= 1e-5f && F32x4_w(back) == v;
	}

	Test_assert(test, "srgb round trip", roundTrip);
}

void Test_pack(Test *test) {
	Test_pack21x3(test);
	Test_pack20x3u4(test);
	Test_packBit(test);
	Test_packQuat(test);
	Test_packRGB10A2(test);
	Test_tonemap(test);
}

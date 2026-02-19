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

#include "types/math/pack.h"
#include "types/math/quat.h"
#include "shared.h"

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

void Test_pack(Test *test) {
	Test_pack21x3(test);
	Test_pack20x3u4(test);
	Test_packBit(test);
	Test_packQuat(test);
}

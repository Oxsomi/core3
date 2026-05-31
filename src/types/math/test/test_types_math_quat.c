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

//types/math/test/test_types_math_quat.c

#include "test_types_math_shared.h"
#include "types/math/quat.h"

void Test_quatBasic(Test *test) {

	Test_setModule(test, "QuatF32 basic");

	QuatF32 q = QuatF32_create(1, 2, 3, 4);

	Test_assert(test, "QuatF32_x", QuatF32_x(q) == 1);
	Test_assert(test, "QuatF32_y", QuatF32_y(q) == 2);
	Test_assert(test, "QuatF32_z", QuatF32_z(q) == 3);
	Test_assert(test, "QuatF32_w", QuatF32_w(q) == 4);
	Test_assert(test, "QuatF32_neq", !QuatF32_neq(q, QuatF32_create(1, 2, 3, 4)));

	const F32 invLen = 1 / F32_sqrt(F32_pow2(1) + F32_pow2(2) + F32_pow2(3) + F32_pow2(4));
	const QuatF32 norm = QuatF32_create(1 * invLen, 2 * invLen, 3 * invLen, 4 * invLen);

	Test_assert(test, "QuatF32_normalize", !QuatF32_neq(QuatF32_normalize(q), norm));
}

void Test_quatAngleAxis(Test *test) {

	Test_setModule(test, "QuatF32_angleAxis");

	const F32 isq2  = 1 / F32_sqrt(2);
	const F32 isq3  = 1 / F32_sqrt(3);
	const F32 norm3 = isq2 * isq3;

	const F32x4 axes[] = {
		F32x4_create3(0, 0, 1),
		F32x4_create3(0, 1, 0),
		F32x4_create3(0, 1, 1),
		F32x4_create3(1, 0, 0),
		F32x4_create3(1, 0, 1),
		F32x4_create3(1, 1, 0),
		F32x4_create3(1, 1, 1)
	};

	static const U64 axisCount = sizeof(axes) / sizeof(axes[0]);

	const F32 angles[] = {
		0,
		F32_PI * 0.5f,
		F32_PI,
		F32_PI * 1.5f,
		F32_PI * 2.0f
	};

	const QuatF32 expected[] = {

		//angle = 0
		QuatF32_identity(),
		QuatF32_identity(),
		QuatF32_identity(),
		QuatF32_identity(),
		QuatF32_identity(),
		QuatF32_identity(),
		QuatF32_identity(),

		//angle = 90deg
		QuatF32_create(0,        0,        isq2,    isq2),
		QuatF32_create(0,        isq2,    0,        isq2),
		QuatF32_create(0,        0.5f,    0.5f,    isq2),
		QuatF32_create(isq2,    0,        0,        isq2),
		QuatF32_create(0.5f,    0,        0.5f,    isq2),
		QuatF32_create(0.5f,    0.5f,    0,        isq2),
		QuatF32_create(norm3,    norm3,    norm3,    isq2),

		//angle = 180deg
		QuatF32_create(0,        0,        1,        0),
		QuatF32_create(0,        1,        0,        0),
		QuatF32_create(0,        isq2,    isq2,    0),
		QuatF32_create(1,        0,        0,        0),
		QuatF32_create(isq2,    0,        isq2,    0),
		QuatF32_create(isq2,    isq2,    0,        0),
		QuatF32_create(isq3,    isq3,    isq3,    0),

		//angle = 270deg
		QuatF32_create(0,        0,        isq2,    -isq2),
		QuatF32_create(0,        isq2,    0,        -isq2),
		QuatF32_create(0,        0.5f,    0.5f,    -isq2),
		QuatF32_create(isq2,    0,        0,        -isq2),
		QuatF32_create(0.5f,    0,        0.5f,    -isq2),
		QuatF32_create(0.5f,    0.5f,    0,        -isq2),
		QuatF32_create(norm3,    norm3,    norm3,    -isq2),

		//angle = 360deg
		F32x4_negate(QuatF32_identity()),
		F32x4_negate(QuatF32_identity()),
		F32x4_negate(QuatF32_identity()),
		F32x4_negate(QuatF32_identity()),
		F32x4_negate(QuatF32_identity()),
		F32x4_negate(QuatF32_identity()),
		F32x4_negate(QuatF32_identity()),
	};

	for (U64 i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i)
		Test_assert(test, "QuatF32_angleAxis",
			!QuatF32_neq(QuatF32_angleAxis(axes[i % axisCount], angles[i / axisCount]), expected[i]));
}

void Test_quatFromEuler(Test *test) {

	Test_setModule(test, "QuatF32_fromEuler");

	const F32 isq2 = 1.0f / F32_sqrt(2);

	const F32x4 eulerAngles[] = {
		F32x4_create3(-120, 10, 270),
		F32x4_create3(45, 30, 60),
		F32x4_create3(0, 0, 0),
		F32x4_create3(90, 0, 0),
		F32x4_create3(0, 90, 0),
		F32x4_create3(0, 0, 90),
		F32x4_create3(0, 90, 90),
		F32x4_create3(90, 90, 0),
		F32x4_create3(90, 0, 90),
		F32x4_create3(90, 90, 90)
	};

	const QuatF32 expected[] = {
		QuatF32_create( 0.6408564f,  0.579228f,   0.4055798f, -0.2988362f),
		QuatF32_create( 0.4396797f,  0.02226f,    0.5319757f,  0.7233174f),
		QuatF32_identity(),
		QuatF32_create( isq2,  0,     0,    isq2),
		QuatF32_create( 0,     isq2,  0,    isq2),
		QuatF32_create( 0,     0,     isq2, isq2),
		QuatF32_create( 0.5f,  0.5f,  0.5f, 0.5f),
		QuatF32_create( 0.5f,  0.5f,  0.5f, 0.5f),
		QuatF32_create( 0.5f, -0.5f,  0.5f, 0.5f),
		QuatF32_create( isq2,  0,     isq2, 0),
	};

	for (U64 i = 0; i < sizeof(eulerAngles) / sizeof(eulerAngles[0]); ++i)
		Test_assert(test, "QuatF32_fromEuler",
			!QuatF32_neq(QuatF32_fromEuler(eulerAngles[i]), expected[i]));
}

void Test_quatToEuler(Test *test) {

	Test_setModule(test, "QuatF32_toEuler");

	const F32 isq2 = 1.0f / F32_sqrt(2);

	const QuatF32 quats[] = {
		QuatF32_create( 0.6408564f,  0.579228f,   0.4055798f, -0.2988362f),
		QuatF32_create( 0.4396797f,  0.02226f,    0.5319757f,  0.7233174f),
		QuatF32_identity(),
		QuatF32_create( isq2,  0,     0,    isq2),
		QuatF32_create( 0,     isq2,  0,    isq2),
		QuatF32_create( 0,     0,     isq2, isq2),
		QuatF32_create( 0.5f,  0.5f,  0.5f, 0.5f),
		QuatF32_create( 0.5f,  0.5f,  0.5f, 0.5f),
		QuatF32_create( 0.5f, -0.5f,  0.5f, 0.5f),
		QuatF32_create( isq2,  0,     isq2, 0),
	};

	for (U64 i = 0; i < sizeof(quats) / sizeof(quats[0]); ++i) {

		const QuatF32 backTest = QuatF32_fromEuler(QuatF32_toEuler(quats[i]));

		F32x4 backMat[3], checkMat[3];
		QuatF32_toOrientation(backTest,  &backMat[0],  &backMat[1],  &backMat[2]);
		QuatF32_toOrientation(quats[i],  &checkMat[0], &checkMat[1], &checkMat[2]);

		for (U8 j = 0; j < 3; ++j)
			Test_assert(test, "QuatF32_toEuler",
				!F32x4_neqApproxAdv4(backMat[j], checkMat[j], 2e-2f, 2e-2f));
	}
}

void Test_quatMul(Test *test) {

	Test_setModule(test, "QuatF32_mul");

	const F32 isq2 = 1.0f / F32_sqrt(2);

	const QuatF32 mulA[] = {
		QuatF32_identity(),
		QuatF32_create(isq2, 0,    0,    isq2),        //90deg X
		QuatF32_create(0,    isq2, 0,    isq2),        //90deg Y
		QuatF32_create(0,    0,    isq2, isq2),        //90deg Z
	};

	const QuatF32 mulB[] = {
		QuatF32_create(isq2, 0,    0,    isq2),        //90deg X
		QuatF32_create(0,    isq2, 0,    isq2),        //90deg Y
		QuatF32_create(0,    0,    isq2, isq2),        //90deg Z
		QuatF32_identity(),
	};

	const QuatF32 expected[] = {
		QuatF32_create(isq2, 0,    0,    isq2),        //identity * X90 = X90
		QuatF32_create(0.5f, 0.5f, 0.5f, 0.5f),        //X90 * Y90
		QuatF32_create(0.5f, 0.5f, 0.5f, 0.5f),        //Y90 * Z90
		QuatF32_create(0,    0,    isq2, isq2),        //Z90 * identity = Z90
	};

	for (U32 i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i)
		Test_assert(test, "QuatF32_mul",
			!QuatF32_neq(QuatF32_mul(mulA[i], mulB[i]), expected[i]));
}

void Test_quatApplyToNormal(Test *test) {

	Test_setModule(test, "QuatF32_applyToNormal");

	const F32 isq2 = 1.0f / F32_sqrt(2);

	const QuatF32 rots[] = {
		QuatF32_identity(),                            //identity x3
		QuatF32_identity(),
		QuatF32_identity(),
		QuatF32_create(isq2, 0,    0,    isq2),        //90deg X x3
		QuatF32_create(isq2, 0,    0,    isq2),
		QuatF32_create(isq2, 0,    0,    isq2),
		QuatF32_create(0,    isq2, 0,    isq2),        //90deg Y x3
		QuatF32_create(0,    isq2, 0,    isq2),
		QuatF32_create(0,    isq2, 0,    isq2),
		QuatF32_create(0,    0,    isq2, isq2),        //90deg Z x3
		QuatF32_create(0,    0,    isq2, isq2),
		QuatF32_create(0,    0,    isq2, isq2),
	};

	const F32x4 normals[] = {
		F32x4_create3(1, 0, 0), F32x4_create3(0, 1, 0), F32x4_create3(0, 0, 1),
		F32x4_create3(1, 0, 0), F32x4_create3(0, 1, 0), F32x4_create3(0, 0, 1),
		F32x4_create3(1, 0, 0), F32x4_create3(0, 1, 0), F32x4_create3(0, 0, 1),
		F32x4_create3(1, 0, 0), F32x4_create3(0, 1, 0), F32x4_create3(0, 0, 1),
	};

	const F32x4 expected[] = {
		F32x4_create3( 1,  0,  0), F32x4_create3(0,  1,  0), F32x4_create3( 0,  0,  1),    //identity
		F32x4_create3( 1,  0,  0), F32x4_create3(0,  0, -1), F32x4_create3( 0,  1,  0),    //90deg X
		F32x4_create3( 0,  0,  1), F32x4_create3(0,  1,  0), F32x4_create3(-1,  0,  0),    //90deg Y
		F32x4_create3( 0, -1,  0), F32x4_create3(1,  0,  0), F32x4_create3( 0,  0,  1),    //90deg Z
	};

	for (U32 i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i)
		Test_assert(test, "QuatF32_applyToNormal",
			!F32x4_neqApprox4(QuatF32_applyToNormal(rots[i], normals[i]), expected[i]));
}

void Test_quatSlerp(Test *test) {

	Test_setModule(test, "QuatF32_slerp");

	const F32 isq2 = 1.0f / F32_sqrt(2);

	const QuatF32 slerpA[] = {
		QuatF32_identity(),
		QuatF32_create(isq2, 0,    0,    isq2),        //90deg X
		QuatF32_create(0,    isq2, 0,    isq2),        //90deg Y
		QuatF32_create(0.5f, 0.5f, 0.5f, 0.5f),        //X90 * Y90
	};

	const QuatF32 slerpB[] = {
		QuatF32_create(isq2, 0,    0,    isq2),        //90deg X
		QuatF32_create(0,    isq2, 0,    isq2),        //90deg Y
		QuatF32_identity(),
		QuatF32_create(isq2, 0,    0,    isq2),        //90deg X
	};

	const QuatF32 expected50[] = {
		QuatF32_create(0.382683f,     0,            0,     0.923879f),
		QuatF32_create(0.408248276f,  0.408248276f, 0,     0.816496551f),
		QuatF32_create(0,             0.382683f,    0,     0.923879f),
		QuatF32_create(0.65f,         0.27f,        0.27f, 0.65f),
	};

	for (U64 i = 0; i < sizeof(slerpA) / sizeof(slerpA[0]); ++i) {
		Test_assert(test, "QuatF32_slerp t=0",  !QuatF32_neq(QuatF32_slerp(slerpA[i], slerpB[i], 0.0f),  slerpA[i]));
		Test_assert(test, "QuatF32_slerp t=1",  !QuatF32_neq(QuatF32_slerp(slerpA[i], slerpB[i], 1.0f),  slerpB[i]));
		Test_assert(test, "QuatF32_slerp t=.5", !QuatF32_neq(QuatF32_slerp(slerpA[i], slerpB[i], 0.5f), expected50[i]));
	}
}

void Test_quatTargetDirection(Test *test) {

	Test_setModule(test, "QuatF32_targetDirection");

	const F32 isq2 = 1.0f / F32_sqrt(2);

	const F32x4 fwd[] = {
		F32x4_create3( 0,  0,  1),
		F32x4_create3( 1,  0,  0),
		F32x4_create3( 0,  1,  0),
		F32x4_create3( 1,  1,  0),
		F32x4_create3(-1,  0,  1),
		F32x4_create3( 0, -1,  0),
		F32x4_create3( 1,  0,  0),
		F32x4_create3( 0,  0, -1),
		F32x4_create3( 0,  0, -1),
	};

	const F32x4 up[] = {
		F32x4_create3(0,  1,  0),
		F32x4_create3(0,  1,  0),
		F32x4_create3(0,  0,  1),
		F32x4_create3(0,  1,  0),
		F32x4_create3(0,  1,  0),
		F32x4_create3(0,  0,  1),
		F32x4_create3(0,  1,  0),
		F32x4_create3(0, -1,  0),
		F32x4_create3(0,  1,  0),
	};

	const QuatF32 expected[] = {
		QuatF32_create( 0,     0,     0,    1),
		QuatF32_create( 0,    -isq2,  0,    isq2),
		QuatF32_create( 0,     isq2,  isq2, 0),
		QuatF32_create( 0.27f,-0.65f,-0.27f,0.65f),
		QuatF32_create( 0,     0.38f, 0,    0.93f),
		QuatF32_create(-isq2,  0,     0,    isq2),
		QuatF32_create( 0,    -isq2,  0,    isq2),
		QuatF32_create( 1,     0,     0,    0),
		QuatF32_create( 0,     1,     0,    0),
	};

	for (U64 i = 0; i < sizeof(fwd) / sizeof(fwd[0]); ++i)
		Test_assert(test, "QuatF32_targetDirection",
			!QuatF32_neq(QuatF32_targetDirection(F32x4_zero(), fwd[i], up[i]), expected[i]));
}

void Test_quat(Test *test) {
	Test_quatBasic(test);
	Test_quatAngleAxis(test);
	Test_quatFromEuler(test);
	Test_quatToEuler(test);
	Test_quatMul(test);
	Test_quatApplyToNormal(test);
	Test_quatSlerp(test);
	Test_quatTargetDirection(test);
}

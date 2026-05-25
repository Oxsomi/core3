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

//types/math/test/test_types_math_rand.c

#include "test_types_math_shared.h"
#include "types/math/rand.h"

void Test_rand(Test *test) {

	Test_setModule(test, "Random");

	//Output is in [0, 1>

	U32 seed = Random_seed(123, 456);

	static const U32 sampleCount = 16;

	for (U32 i = 0; i < sampleCount; ++i) {
		const F32 r = Random_sample(&seed);
		Test_assert(test, "Random_sample >= 0", r >= 0.0f);
		Test_assert(test, "Random_sample < 1", r < 1.0f);
	}

	//Determinism

	U32 seedA = Random_seed(123, 456);
	U32 seedB = Random_seed(123, 456);

	for (U32 i = 0; i < sampleCount; ++i)
		Test_assert(test, "Random determinism", Random_sample(&seedA) == Random_sample(&seedB));

	//Different seeds

	U32 seedC = Random_seed(123, 456);
	U32 seedD = Random_seed(789, 101);

	Bool anyDiffer = false;

	for (U32 i = 0; i < sampleCount; ++i)
		if (Random_sample(&seedC) != Random_sample(&seedD))
			anyDiffer = true;

	Test_assert(test, "Random seeds", anyDiffer);

	//Seed state advances after each sample

	U32 seedE = Random_seed(123, 456);
	const U32 stateBefore = seedE;
	Random_sample(&seedE);
	Test_assert(test, "Random_sample advances", seedE != stateBefore);

	//Same seed inputs with swapped params produce different seeds

	const U32 seedXY = Random_seed(111, 222);
	const U32 seedYX = Random_seed(222, 111);
	Test_assert(test, "Random_seed", seedXY != seedYX);
}

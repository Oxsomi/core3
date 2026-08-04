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

//types/math/test/test_types_math_main.c

#include "types/test/test.h"
#include "test_types_math_shared.h"

OXC3_TEST_MAIN(types_math) {

	Test t = (Test) { 0 };

	Test_floatType(&t);
	Test_flpF16(&t);
	Test_flpF32(&t);
	Test_flpF64(&t);
	Test_flpRoundTrip(&t);

	Test_mathU128(&t);

	Test_quat(&t);
	Test_vec2f(&t);
	Test_vec2i(&t);
	Test_vec4i(&t);
	Test_vec4f(&t);
	Test_hppVec(&t);
	Test_mat(&t);

	Test_pack(&t);
	Test_rand(&t);

	Test_typeCast(&t);

	return Test_end(&t);
}

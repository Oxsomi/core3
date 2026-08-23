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

//types/math/test/test_types_math_hpp_vec4i.cpp
//
//Type check for the C++ oxc::I32x4 wrapper.
//types/math/vec4i.hpp only ever reached a translation unit through vec_cvt.hpp, which includes vec4f.hpp first.
//That hid whether the header stands on its own, so this TU includes it alone and includes nothing else.
//
//The body below is deliberately never CALLED.
//It names every member so the compiler has to typecheck each one against the current C headers.
//test_types_math_hpp.cpp owns the behavioural coverage; a link time reference is enough to keep this honest.

#include "types/math/vec4i.hpp"

//Never invoked. See the file comment: this is a compile time check, not a test module.

extern "C" void Test_hppVec4iTypeCheck(const void *raw, oxc::c::I32x4 *sink) {

	using namespace oxc;

	//Every constructor, including the implicit one from the C I32x4 that the class wraps.
	//The two argument form leans on the z and w defaults.

	const I32x4 defaulted;
	const I32x4 fromC(*sink);
	const I32x4 broadcast(7);
	const I32x4 xyzw(1, 2, 3, 4);
	const I32x4 xyOnly(1, 2);

	//Statics.

	I32x4 acc = I32x4::zero();
	acc = I32x4::one();
	acc = I32x4::load(raw);

	//Compound assignment.
	//This is also what keeps every constructed value above read rather than merely set.

	acc += defaulted;
	acc -= fromC;
	acc *= broadcast;
	acc /= xyzw;
	acc &= xyOnly;
	acc |= xyzw;
	acc ^= broadcast;

	//Component access, by name and by index.

	acc.setX(acc.x());
	acc.setY(acc.y());
	acc.setZ(acc.z());
	acc.setW(acc.w());
	acc.set(0, acc[3]);

	//Arithmetic and bitwise operators, plus the two whole vector shifts.

	acc = acc + xyzw;
	acc = acc - xyzw;
	acc = acc * xyzw;
	acc = acc / xyzw;
	acc = acc % xyzw;
	acc = -acc;
	acc = acc & xyzw;
	acc = acc | xyzw;
	acc = acc ^ xyzw;
	acc = acc.lsh128(1);
	acc = acc.rsh128(1);

	//Component wise helpers.

	acc = acc.abs();
	acc = acc.sign();
	acc = acc.min(xyzw);
	acc = acc.max(xyzw);
	acc = acc.clamp(I32x4::zero(), I32x4::one());
	acc = acc.trunc2();
	acc = acc.trunc3();
	acc = acc.xxxx();
	acc = acc.wzyx();

	//Comparison.
	//The results are folded back in rather than dropped, so nothing here reads as unused.

	const bool compared = (acc == xyzw) || (acc != xyzw) || acc.all() || acc.any();
	acc.set(0, (c::I32) compared);

	//Interop: the const handle() feeds the mutable one, which names both overloads.

	acc.handle() = xyzw.handle();
	*sink = acc.handle();
}

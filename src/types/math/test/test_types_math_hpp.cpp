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

//types/math/test/test_types_math_hpp.cpp
//
//Exercises oxc::F32x4 / oxc::I32x4 / oxc::F32x4x4.
//These are pure syntax over the C calls, so the point of the coverage is that the operators map to the
// function the name promises (and in the right operand order), not that the arithmetic itself works;
// test_types_math_vec4f.c and _mat.c own that.

#include "types/math/vec_cvt.hpp"
#include "types/math/mat.hpp"

namespace oxc { namespace c {
	#include "types/test/test.h"
}}

extern "C" void Test_hppVec(oxc::c::Test *t) {

	using namespace oxc;

	Test_setModule(t, "hppVec");

	//========================= oxc::F32x4 =========================

	{
		const F32x4 a(1, 2, 3, 4);
		const F32x4 b(2.f);                                 //broadcast

		Test_assert(t, "F32x4: components", a.x() == 1 && a.y() == 2 && a.z() == 3 && a.w() == 4);
		Test_assert(t, "F32x4: indexing", a[0] == 1 && a[3] == 4);
		Test_assert(t, "F32x4: broadcast", b.x() == 2 && b.w() == 2);
		Test_assert(t, "F32x4: zero", F32x4::zero() == F32x4(0, 0, 0, 0));
		Test_assert(t, "F32x4: one", F32x4::one() == F32x4(1, 1, 1, 1));

		Test_assert(t, "F32x4: add", (a + b) == F32x4(3, 4, 5, 6));
		Test_assert(t, "F32x4: sub", (a - b) == F32x4(-1, 0, 1, 2));
		Test_assert(t, "F32x4: mul", (a * b) == F32x4(2, 4, 6, 8));
		Test_assert(t, "F32x4: div", (a / b) == F32x4(0.5f, 1, 1.5f, 2));
		Test_assert(t, "F32x4: negate", (-a) == F32x4(-1, -2, -3, -4));

		//Order matters for the non-commutative ones
		Test_assert(t, "F32x4: sub is lhs - rhs", (b - a) == F32x4(1, 0, -1, -2));
		Test_assert(t, "F32x4: div is lhs / rhs", (b / a) == F32x4(2, 1, 2.f / 3, 0.5f));

		Test_assert(t, "F32x4: scalar mul", (a * 2.f) == F32x4(2, 4, 6, 8));
		Test_assert(t, "F32x4: scalar mul commutes", (2.f * a) == (a * 2.f));
		Test_assert(t, "F32x4: scalar div", (a / 2.f) == F32x4(0.5f, 1, 1.5f, 2));

		{
			F32x4 acc = a;
			acc += b;  Test_assert(t, "F32x4: +=", acc == F32x4(3, 4, 5, 6));
			acc -= b;  Test_assert(t, "F32x4: -=", acc == a);
			acc *= b;  Test_assert(t, "F32x4: *=", acc == F32x4(2, 4, 6, 8));
			acc /= b;  Test_assert(t, "F32x4: /=", acc == a);
			acc *= 3.f; Test_assert(t, "F32x4: scalar *=", acc == F32x4(3, 6, 9, 12));
		}

		{
			F32x4 s = a;
			s.setX(9); s.setW(8); s.set(1, 7);
			Test_assert(t, "F32x4: setters", s == F32x4(9, 7, 3, 8));
		}

		Test_assert(t, "F32x4: fma", F32x4::fma(a, b, F32x4::one()) == F32x4(3, 5, 7, 9));

		//Geometry; dot3 must ignore w, which is the usual source of confusion
		Test_assert(t, "F32x4: dot4", a.dot4(b) == 20);
		Test_assert(t, "F32x4: dot3 ignores w", a.dot3(b) == 12);
		Test_assert(t, "F32x4: sqLen3", F32x4(3, 4, 0, 99).sqLen3() == 25);
		Test_assert(t, "F32x4: len3", F32x4(3, 4, 0, 99).len3() == 5);

		Test_assert(t, "F32x4: cross3", F32x4(1, 0, 0, 0).cross3(F32x4(0, 1, 0, 0)).eqApprox(F32x4(0, 0, 1, 0)));

		//normalize* goes through F32x4_rsqrt, which is the hardware's fast reciprocal sqrt (_mm_rsqrt_ps
		// is ~12 bits), so the default 1e-5 relative tolerance is tighter than the operation can deliver
		Test_assert(t, "F32x4: normalize3", F32x4(0, 5, 0, 0).normalize3().eqApprox(F32x4(0, 1, 0, 0), 1e-3f, 1e-4f));

		Test_assert(t, "F32x4: trunc3 zeroes w", F32x4(1, 2, 3, 4).trunc3() == F32x4(1, 2, 3, 0));
		Test_assert(t, "F32x4: trunc2 zeroes zw", F32x4(1, 2, 3, 4).trunc2() == F32x4(1, 2, 0, 0));

		Test_assert(t, "F32x4: abs", F32x4(-1, 2, -3, 4).abs() == F32x4(1, 2, 3, 4));
		Test_assert(t, "F32x4: min", a.min(b) == F32x4(1, 2, 2, 2));
		Test_assert(t, "F32x4: max", a.max(b) == F32x4(2, 2, 3, 4));
		Test_assert(t, "F32x4: clamp", a.clamp(F32x4(2.f), F32x4(3.f)) == F32x4(2, 2, 3, 3));
		Test_assert(t, "F32x4: floor", F32x4(1.7f, -1.2f, 0, 0).floor() == F32x4(1, -2, 0, 0));
		Test_assert(t, "F32x4: ceil", F32x4(1.2f, -1.7f, 0, 0).ceil() == F32x4(2, -1, 0, 0));
		Test_assert(t, "F32x4: sqrt", F32x4(4, 9, 16, 25).sqrt() == F32x4(2, 3, 4, 5));
		Test_assert(t, "F32x4: saturate", F32x4(-1, 0.5f, 2, 1).saturate() == F32x4(0, 0.5f, 1, 1));
		Test_assert(t, "F32x4: lerp", F32x4::lerp(F32x4::zero(), F32x4(10.f), 0.5f).eqApprox(F32x4(5.f)));

		Test_assert(t, "F32x4: swizzle xxxx", a.xxxx() == F32x4(1, 1, 1, 1));
		Test_assert(t, "F32x4: swizzle wzyx", a.wzyx() == F32x4(4, 3, 2, 1));

		Test_assert(t, "F32x4: equality", a == F32x4(1, 2, 3, 4));
		Test_assert(t, "F32x4: inequality", a != b);
		Test_assert(t, "F32x4: eqApprox tolerates error", a.eqApprox(a + F32x4(1e-8f)));
		Test_assert(t, "F32x4: eqApprox rejects real difference", !a.eqApprox(b));

		Test_assert(t, "F32x4: any", F32x4(0, 0, 1, 0).any() && !F32x4::zero().any());
		Test_assert(t, "F32x4: all", F32x4::one().all() && !F32x4(1, 1, 0, 1).all());

		//Conversions: cast reinterprets bits, to* converts values
		Test_assert(t, "F32x4: toI32x4 converts", toI32x4(F32x4(1.9f, -2.9f, 3, 4)) == I32x4(1, -2, 3, 4));
		Test_assert(t, "F32x4: cast round trip", castF32x4(castI32x4(a)) == a);
	}

	//========================= oxc::I32x4 =========================

	{
		const I32x4 a(1, 2, 3, 4);
		const I32x4 b(2);

		Test_assert(t, "I32x4: components", a.x() == 1 && a.w() == 4);
		Test_assert(t, "I32x4: broadcast", b == I32x4(2, 2, 2, 2));

		Test_assert(t, "I32x4: add", (a + b) == I32x4(3, 4, 5, 6));
		Test_assert(t, "I32x4: sub", (a - b) == I32x4(-1, 0, 1, 2));
		Test_assert(t, "I32x4: mul", (a * b) == I32x4(2, 4, 6, 8));
		Test_assert(t, "I32x4: div", (a / b) == I32x4(0, 1, 1, 2));
		Test_assert(t, "I32x4: mod", (a % b) == I32x4(1, 0, 1, 0));
		Test_assert(t, "I32x4: negate", (-a) == I32x4(-1, -2, -3, -4));

		Test_assert(t, "I32x4: and", (I32x4(0xF, 0xF0, 0, 0xFF) & I32x4(0x3)) == I32x4(0x3, 0, 0, 0x3));
		Test_assert(t, "I32x4: or", (I32x4(0xF0, 0, 0, 0) | I32x4(0x0F, 0, 0, 0)) == I32x4(0xFF, 0, 0, 0));
		Test_assert(t, "I32x4: xor", (a ^ a) == I32x4::zero());

		{
			I32x4 acc = a;
			acc += b;  Test_assert(t, "I32x4: +=", acc == I32x4(3, 4, 5, 6));
			acc -= b;  Test_assert(t, "I32x4: -=", acc == a);
			acc &= I32x4(1); Test_assert(t, "I32x4: &=", acc == I32x4(1, 0, 1, 0));
		}

		{
			I32x4 s = a;
			s.setZ(9); s.set(0, 8);
			Test_assert(t, "I32x4: setters", s == I32x4(8, 2, 9, 4));
		}

		Test_assert(t, "I32x4: abs", I32x4(-1, 2, -3, 4).abs() == I32x4(1, 2, 3, 4));
		Test_assert(t, "I32x4: min", a.min(b) == I32x4(1, 2, 2, 2));
		Test_assert(t, "I32x4: max", a.max(b) == I32x4(2, 2, 3, 4));
		Test_assert(t, "I32x4: trunc3 zeroes w", a.trunc3() == I32x4(1, 2, 3, 0));
		Test_assert(t, "I32x4: swizzle wzyx", a.wzyx() == I32x4(4, 3, 2, 1));

		Test_assert(t, "I32x4: any", I32x4(0, 0, 1, 0).any() && !I32x4::zero().any());
		Test_assert(t, "I32x4: all", I32x4::one().all() && !I32x4(1, 1, 0, 1).all());

		Test_assert(t, "I32x4: toF32x4 converts", toF32x4(I32x4(1, 2, 3, 4)) == F32x4(1, 2, 3, 4));
	}

	//========================= oxc::F32x4x4 =========================

	//The maths is covered by test_types_math_mat.c; this checks the operators forward to the right call.

	{
		const F32x4x4 id = F32x4x4::identity();
		const F32x4x4 tr = F32x4x4::translate(10, 20, 30);
		const F32x4x4 sc = F32x4x4::scale(2, 2, 2);

		Test_assert(t, "F32x4x4: default is identity", F32x4x4() == id);
		Test_assert(t, "F32x4x4: zero", F32x4x4::zero()[0] == F32x4::zero());
		Test_assert(t, "F32x4x4: row", tr[3] == F32x4(10, 20, 30, 1));
		Test_assert(t, "F32x4x4: column", tr.column(3) == F32x4(0, 0, 0, 1));
		Test_assert(t, "F32x4x4: get is (row, col)", tr.get(3, 0) == 10);

		{
			F32x4x4 m = F32x4x4::zero();
			m.set(2, 1, 9);
			Test_assert(t, "F32x4x4: set", m.get(2, 1) == 9);
			m.setRow(0, F32x4(1, 2, 3, 4));
			Test_assert(t, "F32x4x4: setRow", m[0] == F32x4(1, 2, 3, 4));
			m.setColumn(3, F32x4(5, 6, 7, 8));
			Test_assert(t, "F32x4x4: setColumn", m.column(3) == F32x4(5, 6, 7, 8));
		}

		//operator* is the matrix product and keeps the C's ordering: lhs applies first
		Test_assert(t, "F32x4x4: operator* applies lhs first", ((tr * sc) * F32x4(1, 1, 1, 1)).eqApprox(
			sc * (tr * F32x4(1, 1, 1, 1))
		));

		Test_assert(t, "F32x4x4: m * v transforms", (tr * F32x4(1, 2, 3, 1)).eqApprox(F32x4(11, 22, 33, 1)));
		Test_assert(t, "F32x4x4: transformPoint", tr.transformPoint(F32x4(1, 2, 3, 0)).eqApprox(F32x4(11, 22, 33, 1)));
		Test_assert(t, "F32x4x4: transformDirection ignores translation", tr.transformDirection(
			F32x4(1, 0, 0, 1)
		).eqApprox(F32x4(1, 0, 0, 0)));

		Test_assert(t, "F32x4x4: identity is neutral", (tr * id) == tr && (id * tr) == tr);
		Test_assert(t, "F32x4x4: add", (id + id) == id.mulScalar(2));
		Test_assert(t, "F32x4x4: sub", (tr - tr) == F32x4x4::zero());

		{
			F32x4x4 acc = tr;
			acc *= sc;
			Test_assert(t, "F32x4x4: *=", acc == (tr * sc));
			acc = tr; acc += tr;
			Test_assert(t, "F32x4x4: +=", acc == tr.mulScalar(2));
		}

		Test_assert(t, "F32x4x4: transpose", tr.transpose().column(3) == F32x4(10, 20, 30, 1));
		Test_assert(t, "F32x4x4: transpose twice", tr.transpose().transpose() == tr);

		Test_assert(t, "F32x4x4: determinant of identity", c::F32_abs(id.determinant() - 1) < 1e-5f);
		Test_assert(t, "F32x4x4: determinant of scale", c::F32_abs(F32x4x4::scale(2, 3, 4).determinant() - 24) < 1e-4f);

		{
			F32x4x4 inv;
			Test_assert(t, "F32x4x4: inverse", tr.inverse(inv));
			Test_assert(t, "F32x4x4: m * inv == identity", (tr * inv).eqApprox(id, 1e-3f, 1e-4f));

			//A singular matrix must be reported, and must leave the output alone
			F32x4x4 untouched = id;
			Test_assert(t, "F32x4x4: inverse rejects singular", !F32x4x4::zero().inverse(untouched));
			Test_assert(t, "F32x4x4: failed inverse left result alone", untouched == id);
		}

		//Builders forward to the C, which owns the conventions
		Test_assert(t, "F32x4x4: rotateZ", F32x4x4::rotateZ(c::F32_PI / 2).transformDirection(
			F32x4(1, 0, 0, 0)
		).eqApprox(F32x4(0, 1, 0, 0), 1e-4f, 1e-5f));

		Test_assert(t, "F32x4x4: lookAt puts eye at origin", F32x4x4::lookAt(
			F32x4(0, 0, -5, 1), F32x4(0, 0, 0, 1), F32x4(0, 1, 0, 0)
		).transformPoint(F32x4(0, 0, -5, 1)).trunc3().eqApprox(F32x4::zero(), 1e-3f, 1e-4f));

		//Reverse Z: the near plane lands on depth 1 after the perspective divide

		const F32x4 atNear = F32x4x4::perspective(c::F32_PI / 2, 1, 0.1f, 100).transformPoint(F32x4(0, 0, 0.1f, 1));
		Test_assert(t, "F32x4x4: perspective near maps to 1", c::F32_abs(atNear.z() / atNear.w() - 1) < 1e-4f);

		Test_assert(t, "F32x4x4: orthoSize matches ortho", F32x4x4::orthoSize(4, 2, 0, 10).eqApprox(
			F32x4x4::ortho(-2, 2, -1, 1, 0, 10)
		));

		Test_assert(t, "F32x4x4: eqApprox tolerates error", tr.eqApprox(tr * F32x4x4::scale(
			F32x4(1 + 1e-8f, 1, 1, 1)
		)));

		//Rows in, rows out; the ctor takes them in the HLSL constructor's order
		Test_assert(t, "F32x4x4: row constructor", F32x4x4(
			F32x4(1, 0, 0, 0), F32x4(0, 1, 0, 0), F32x4(0, 0, 1, 0), F32x4(0, 0, 0, 1)
		) == id);

		{
			c::C8 buf[256];
			const c::U64 n = tr.format(buf, sizeof(buf));
			Test_assert(t, "F32x4x4: format", n > 0 && buf[n] == '\0');

			c::C8 tiny[8];
			Test_assert(t, "F32x4x4: format rejects small buffer", !tr.format(tiny, sizeof(tiny)));
		}

		//The wrapper has to be nothing but the C matrix
		F32x4x4 v = id;
		c::F32x4x4 &raw = v.handle();
		raw.v[3] = c::F32x4_create4(1, 2, 3, 1);
		Test_assert(t, "F32x4x4: handle aliases the wrapped value", v[3] == F32x4(1, 2, 3, 1));
	}

	//========================= interop =========================

	//The wrapper must be nothing but the C vector, so it can live inside a C struct unchanged
	{
		F32x4 v(1, 2, 3, 4);
		c::F32x4 &raw = v.handle();
		raw = c::F32x4_add(raw, c::F32x4_one());
		Test_assert(t, "F32x4: handle aliases the wrapped value", v == F32x4(2, 3, 4, 5));

		F32x4 fromRaw = c::F32x4_xxxx4(3);
		Test_assert(t, "F32x4: implicit from c::F32x4", fromRaw == F32x4(3.f));
	}
}

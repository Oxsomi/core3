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

//types/math/test/test_types_math_mat.c

#include "test_types_math_shared.h"
#include "types/math/mat.h"

#include <string.h>

//The transpose has three separate implementations (SSE shuffles, NEON trn/combine, scalar),
// so it gets checked against an explicitly written out expectation rather than only against its own inverse.

static F32x4x4 Test_matCounting() {
	F32x4x4 m;
	m.v[0] = F32x4_create4( 1,  2,  3,  4);
	m.v[1] = F32x4_create4( 5,  6,  7,  8);
	m.v[2] = F32x4_create4( 9, 10, 11, 12);
	m.v[3] = F32x4_create4(13, 14, 15, 16);
	return m;
}

void Test_mat(Test *test) {

	Test_setModule(test, "mat");

	const F32x4x4 id = F32x4x4_identity();
	const F32x4x4 counting = Test_matCounting();

	//========================= constants and access =========================

	Test_assert(test, "mat identity diagonal", (
		F32x4x4_get(id, 0, 0) == 1 && F32x4x4_get(id, 1, 1) == 1 &&
		F32x4x4_get(id, 2, 2) == 1 && F32x4x4_get(id, 3, 3) == 1
	));

	Test_assert(test, "mat identity off diagonal", F32x4x4_get(id, 0, 1) == 0 && F32x4x4_get(id, 3, 0) == 0);

	//Held in a local rather than indexing the temporary's array member directly;
	//MSVC rejects that under WX as C4223 (non-lvalue array converted to pointer)
	const F32x4x4 zero = F32x4x4_zero();
	Test_assert(test, "mat zero", F32x4_eqExact4(zero.v[0], F32x4_zero()));
	Test_assert(test, "mat eq", F32x4x4_eq(id, F32x4x4_identity()));
	Test_assert(test, "mat neq", F32x4x4_neq(id, F32x4x4_zero()));

	Test_assert(test, "mat row", F32x4_eqExact4(F32x4x4_row(counting, 1), F32x4_create4(5, 6, 7, 8)));
	Test_assert(test, "mat column", F32x4_eqExact4(F32x4x4_column(counting, 1), F32x4_create4(2, 6, 10, 14)));
	Test_assert(test, "mat get is (row, column)", F32x4x4_get(counting, 1, 0) == 5);

	{
		F32x4x4 m = F32x4x4_zero();
		F32x4x4_set(&m, 2, 1, 9);
		Test_assert(test, "mat set", F32x4x4_get(m, 2, 1) == 9);

		F32x4x4_setRow(&m, 0, F32x4_create4(1, 2, 3, 4));
		Test_assert(test, "mat setRow", F32x4_eqExact4(F32x4x4_row(m, 0), F32x4_create4(1, 2, 3, 4)));

		F32x4x4_setColumn(&m, 3, F32x4_create4(5, 6, 7, 8));
		Test_assert(test, "mat setColumn", F32x4_eqExact4(F32x4x4_column(m, 3), F32x4_create4(5, 6, 7, 8)));
	}

	//========================= transpose =========================

	{
		const F32x4x4 t = F32x4x4_transpose(counting);

		Test_assert(test, "mat transpose row 0", F32x4_eqExact4(t.v[0], F32x4_create4(1, 5,  9, 13)));
		Test_assert(test, "mat transpose row 1", F32x4_eqExact4(t.v[1], F32x4_create4(2, 6, 10, 14)));
		Test_assert(test, "mat transpose row 2", F32x4_eqExact4(t.v[2], F32x4_create4(3, 7, 11, 15)));
		Test_assert(test, "mat transpose row 3", F32x4_eqExact4(t.v[3], F32x4_create4(4, 8, 12, 16)));

		Test_assert(test, "mat transpose is an involution", F32x4x4_eq(F32x4x4_transpose(t), counting));
		Test_assert(test, "mat transpose identity", F32x4x4_eq(F32x4x4_transpose(id), id));

		//Row i of the transpose must be column i of the original, by definition
		Bool matchesColumns = true;

		for(U8 i = 0; i < 4; ++i)
			matchesColumns &= F32x4_eqExact4(t.v[i], F32x4x4_column(counting, i));

		Test_assert(test, "mat transpose row i == column i", matchesColumns);
	}

	//========================= component-wise arithmetic =========================

	{
		Test_assert(test, "mat add", F32x4x4_eq(
			F32x4x4_add(counting, counting), F32x4x4_mulScalar(counting, 2)
		));

		Test_assert(test, "mat sub", F32x4x4_eq(F32x4x4_sub(counting, counting), F32x4x4_zero()));
	}

	//========================= multiplication =========================

	{
		Test_assert(test, "mat mul by identity (rhs)", F32x4x4_eq(F32x4x4_mul(counting, id), counting));
		Test_assert(test, "mat mul by identity (lhs)", F32x4x4_eq(F32x4x4_mul(id, counting), counting));

		//The defining property of the convention: mul(a, b) applies a first
		const F32x4x4 a = F32x4x4_translate(F32x4_create4(1, 2, 3, 1));
		const F32x4x4 b = F32x4x4_scale3(2, 2, 2);
		const F32x4 p = F32x4_create4(1, 1, 1, 1);

		Test_assert(test, "mat mul applies lhs first", F32x4_eqApprox4(
			F32x4x4_transform(F32x4x4_mul(a, b), p),
			F32x4x4_transform(b, F32x4x4_transform(a, p))
		));

		//...and that is not the other order, otherwise the test above proves nothing
		Test_assert(test, "mat mul is not commutative here", !F32x4x4_eqApprox(
			F32x4x4_mul(a, b), F32x4x4_mul(b, a)
		));

		//(ab)c == a(bc)
		const F32x4x4 c = F32x4x4_rotateZ(0.5f);
		Test_assert(test, "mat mul associative", F32x4x4_eqApprox(
			F32x4x4_mul(F32x4x4_mul(a, b), c), F32x4x4_mul(a, F32x4x4_mul(b, c))
		));
	}

	//========================= transforms =========================

	{
		const F32x4x4 tr = F32x4x4_translate3(10, 20, 30);
		const F32x4 p = F32x4_create4(1, 2, 3, 1);

		Test_assert(test, "mat translate moves a point", F32x4_eqApprox4(
			F32x4x4_transformPoint(tr, p), F32x4_create4(11, 22, 33, 1)
		));

		//A direction has w = 0, so translation must not touch it; this is the classic normal bug
		Test_assert(test, "mat translate leaves a direction alone", F32x4_eqApprox4(
			F32x4x4_transformDirection(tr, F32x4_create4(1, 0, 0, 0)), F32x4_create4(1, 0, 0, 0)
		));

		Test_assert(test, "mat translation lives in row 3", F32x4_eqExact4(
			F32x4x4_row(tr, 3), F32x4_create4(10, 20, 30, 1)
		));

		const F32x4x4 sc = F32x4x4_scale3(2, 3, 4);

		Test_assert(test, "mat scale scales a point", F32x4_eqApprox4(
			F32x4x4_transformPoint(sc, p), F32x4_create4(2, 6, 12, 1)
		));

		Test_assert(test, "mat scale scales a direction", F32x4_eqApprox4(
			F32x4x4_transformDirection(sc, F32x4_create4(1, 1, 1, 0)), F32x4_create4(2, 3, 4, 0)
		));
	}

	//========================= rotation =========================

	{
		const F32 quarter = F32_PI / 2;

		//Rotating X onto Y about Z
		Test_assert(test, "mat rotateZ quarter turn", F32x4_eqApproxAdv4(
			F32x4x4_transformDirection(F32x4x4_rotateZ(quarter), F32x4_create4(1, 0, 0, 0)),
			F32x4_create4(0, 1, 0, 0), 1e-4f, 1e-5f
		));

		Test_assert(test, "mat rotateX quarter turn", F32x4_eqApproxAdv4(
			F32x4x4_transformDirection(F32x4x4_rotateX(quarter), F32x4_create4(0, 1, 0, 0)),
			F32x4_create4(0, 0, 1, 0), 1e-4f, 1e-5f
		));

		Test_assert(test, "mat rotateY quarter turn", F32x4_eqApproxAdv4(
			F32x4x4_transformDirection(F32x4x4_rotateY(quarter), F32x4_create4(0, 0, 1, 0)),
			F32x4_create4(1, 0, 0, 0), 1e-4f, 1e-5f
		));

		//A full turn is identity, and a rotation preserves length
		Test_assert(test, "mat rotate full turn is identity", F32x4x4_eqApproxAdv(
			F32x4x4_rotateZ(F32_PI * 2), id, 1e-4f, 1e-5f
		));

		const F32x4 d = F32x4_normalize3(F32x4_create4(1, 2, 3, 0));
		const F32x4 rotated = F32x4x4_transformDirection(F32x4x4_rotate(F32x4_create4(0.3f, 0.7f, 1.1f, 0)), d);
		Test_assert(test, "mat rotate preserves length", F32_abs(F32x4_len3(rotated) - F32x4_len3(d)) < 1e-3f);

		//A rotation is orthonormal, so its inverse is its transpose
		const F32x4x4 rot = F32x4x4_rotate(F32x4_create4(0.3f, 0.7f, 1.1f, 0));
		F32x4x4 rotInv = F32x4x4_zero();
		Test_assert(test, "mat rotate is invertible", F32x4x4_inverse(rot, &rotInv));
		Test_assert(test, "mat rotate inverse == transpose", F32x4x4_eqApproxAdv(
			rotInv, F32x4x4_transpose(rot), 1e-3f, 1e-4f
		));
	}

	//========================= determinant =========================

	{
		Test_assert(test, "mat determinant identity", F32_abs(F32x4x4_determinant(id) - 1) < 1e-5f);
		Test_assert(test, "mat determinant zero", F32x4x4_determinant(F32x4x4_zero()) == 0);

		//det(scale) is the product of the diagonal
		Test_assert(test, "mat determinant scale", F32_abs(
			F32x4x4_determinant(F32x4x4_scale3(2, 3, 4)) - 24
		) < 1e-4f);

		//A rotation preserves volume
		Test_assert(test, "mat determinant rotation is 1", F32_abs(
			F32x4x4_determinant(F32x4x4_rotate(F32x4_create4(0.3f, 0.7f, 1.1f, 0))) - 1
		) < 1e-3f);

		//The counting matrix has linearly dependent rows, so it's singular
		Test_assert(test, "mat determinant singular", F32_abs(F32x4x4_determinant(counting)) < 1e-3f);

		//det(transpose) == det
		Test_assert(test, "mat determinant of transpose", F32_abs(
			F32x4x4_determinant(F32x4x4_transpose(counting)) - F32x4x4_determinant(counting)
		) < 1e-3f);
	}

	//========================= inverse =========================

	{
		const F32x4x4 m = F32x4x4_transformSRT(
			F32x4_create4(2, 3, 4, 1), F32x4_create4(0.3f, 0.7f, 1.1f, 0), F32x4_create4(5, 6, 7, 1)
		);

		F32x4x4 inv = F32x4x4_zero();
		Test_assert(test, "mat inverse succeeds", F32x4x4_inverse(m, &inv));

		Test_assert(test, "mat m * inv == identity", F32x4x4_eqApproxAdv(
			F32x4x4_mul(m, inv), id, 1e-3f, 1e-4f
		));

		Test_assert(test, "mat inv * m == identity", F32x4x4_eqApproxAdv(
			F32x4x4_mul(inv, m), id, 1e-3f, 1e-4f
		));

		//Round tripping a point through both must land where it started
		const F32x4 p = F32x4_create4(1, -2, 3, 1);
		Test_assert(test, "mat inverse round trips a point", F32x4_eqApproxAdv4(
			F32x4x4_transformPoint(inv, F32x4x4_transformPoint(m, p)), p, 1e-3f, 1e-3f
		));

		//A singular matrix must be reported, not silently filled with inf
		F32x4x4 bad = F32x4x4_identity();
		Test_assert(test, "mat inverse rejects singular", !F32x4x4_inverse(counting, &bad));
		Test_assert(test, "mat inverse leaves result alone on failure", F32x4x4_eq(bad, id));
		Test_assert(test, "mat inverse rejects null result", !F32x4x4_inverse(m, NULL));

		Test_assert(test, "mat inverse of identity is identity", (
			F32x4x4_inverse(id, &inv) && F32x4x4_eqApprox(inv, id)
		));
	}

	//========================= projections =========================

	//Left handed, reverse Z: near lands on depth 1, far on 0.
	//What matters is where the near and far planes land after the divide.

	{
		const F32 nearPlane = 0.1f, farPlane = 100;
		const F32x4x4 proj = F32x4x4_perspective(F32_PI / 2, 16.f / 9, nearPlane, farPlane);

		const F32x4 atNear = F32x4x4_transformPoint(proj, F32x4_create4(0, 0, nearPlane, 1));
		const F32x4 atFar = F32x4x4_transformPoint(proj, F32x4_create4(0, 0, farPlane, 1));

		Test_assert(test, "mat perspective near maps to 1", F32_abs(
			F32x4_z(atNear) / F32x4_w(atNear) - 1
		) < 1e-4f);

		Test_assert(test, "mat perspective far maps to 0", F32_abs(
			F32x4_z(atFar) / F32x4_w(atFar)
		) < 1e-4f);

		//w carries view space z, which is what makes the perspective divide work
		Test_assert(test, "mat perspective w is view z", F32_abs(F32x4_w(atFar) - farPlane) < 1e-3f);

		//An aspect > 1 must squash x relative to y
		Test_assert(test, "mat perspective applies aspect", F32x4x4_get(proj, 0, 0) < F32x4x4_get(proj, 1, 1));
	}

	{
		const F32x4x4 ortho = F32x4x4_ortho(-2, 2, -1, 1, 0, 10);

		Test_assert(test, "mat ortho near maps to 1", F32_abs(
			F32x4_z(F32x4x4_transformPoint(ortho, F32x4_create4(0, 0, 0, 1))) - 1
		) < 1e-5f);

		Test_assert(test, "mat ortho far maps to 0", F32_abs(
			F32x4_z(F32x4x4_transformPoint(ortho, F32x4_create4(0, 0, 10, 1)))
		) < 1e-5f);

		Test_assert(test, "mat ortho right maps to 1", F32_abs(
			F32x4_x(F32x4x4_transformPoint(ortho, F32x4_create4(2, 0, 0, 1))) - 1
		) < 1e-5f);

		Test_assert(test, "mat ortho top maps to 1", F32_abs(
			F32x4_y(F32x4x4_transformPoint(ortho, F32x4_create4(0, 1, 0, 1))) - 1
		) < 1e-5f);

		//w stays 1, i.e. no perspective divide
		Test_assert(test, "mat ortho keeps w", F32x4_w(
			F32x4x4_transformPoint(ortho, F32x4_create4(1, 1, 5, 1))
		) == 1);

		Test_assert(test, "mat orthoSize matches centered ortho", F32x4x4_eqApprox(
			F32x4x4_orthoSize(4, 2, 0, 10), ortho
		));
	}

	//========================= lookAt =========================

	{
		const F32x4 eye = F32x4_create4(0, 0, -5, 1);
		const F32x4 center = F32x4_create4(0, 0, 0, 1);
		const F32x4 up = F32x4_create4(0, 1, 0, 0);

		const F32x4x4 view = F32x4x4_lookAt(eye, center, up);

		//The camera sits at the origin of view space
		Test_assert(test, "mat lookAt puts eye at origin", F32x4_eqApproxAdv4(
			F32x4_trunc3(F32x4x4_transformPoint(view, eye)), F32x4_zero(), 1e-3f, 1e-4f
		));

		//And looks down +Z (left handed)
		const F32x4 target = F32x4x4_transformPoint(view, center);
		Test_assert(test, "mat lookAt looks down +Z", F32x4_z(target) > 0);
		Test_assert(test, "mat lookAt target is centered", (
			F32_abs(F32x4_x(target)) < 1e-3f && F32_abs(F32x4_y(target)) < 1e-3f
		));

		//A view matrix is rigid, so it preserves distances
		const F32x4 a = F32x4_create4(1, 2, 3, 1), b = F32x4_create4(-4, 0, 2, 1);
		const F32 before = F32x4_len3(F32x4_sub(a, b));
		const F32 after = F32x4_len3(F32x4_sub(
			F32x4x4_transformPoint(view, a), F32x4x4_transformPoint(view, b)
		));

		Test_assert(test, "mat lookAt preserves distance", F32_abs(before - after) < 1e-3f);

		//lookDir with the same direction has to agree with lookAt
		Test_assert(test, "mat lookDir agrees with lookAt", F32x4x4_eqApproxAdv(
			F32x4x4_lookDir(eye, F32x4_sub(center, eye), up), view, 1e-4f, 1e-5f
		));
	}

	//========================= view / transformSRT =========================

	{
		//A view matrix must undo the transform that placed the camera
		const F32x4 pos = F32x4_create4(3, -2, 7, 1);
		const F32x4 euler = F32x4_create4(0.2f, 0.4f, 0.6f, 0);

		const F32x4x4 view = F32x4x4_view(pos, euler);

		Test_assert(test, "mat view puts camera at origin", F32x4_eqApproxAdv4(
			F32x4_trunc3(F32x4x4_transformPoint(view, pos)), F32x4_zero(), 1e-3f, 1e-4f
		));

		//SRT: scale is applied before the translation, so the translation isn't scaled
		const F32x4x4 srt = F32x4x4_transformSRT(
			F32x4_create4(2, 2, 2, 1), F32x4_zero(), F32x4_create4(10, 0, 0, 1)
		);

		Test_assert(test, "mat SRT scales then translates", F32x4_eqApprox4(
			F32x4x4_transformPoint(srt, F32x4_create4(1, 0, 0, 1)), F32x4_create4(12, 0, 0, 1)
		));
	}

	//========================= quaternion rotation =========================

	{
		const QuatF32 q = QuatF32_angleAxis(F32x4_create4(0, 0, 1, 0), F32_PI / 2);
		const F32x4 d = F32x4_create4(1, 0, 0, 0);

		Test_assert(test, "mat rotateQuat matches the quaternion", F32x4_eqApproxAdv4(
			F32x4x4_transformDirection(F32x4x4_rotateQuat(q), d),
			F32x4_setWCopy(QuatF32_applyToNormal(q, d), 0),
			1e-3f, 1e-4f
		));
	}

	//========================= format =========================

	{
		C8 buf[256];
		const U64 n = F32x4x4_format(id, buf, sizeof(buf));

		Test_assert(test, "mat format writes something", n > 0 && n < sizeof(buf));
		Test_assert(test, "mat format is null terminated", buf[n] == '\0');
		Test_assert(test, "mat format has 4 lines", (
			buf[n - 1] == '\n' &&
			(U64)(strchr(buf, '\n') - buf) < n
		));

		//Too small a buffer must report failure rather than write a partial matrix
		C8 tiny[8];
		Test_assert(test, "mat format rejects a small buffer", !F32x4x4_format(id, tiny, sizeof(tiny)));
		Test_assert(test, "mat format handles null", !F32x4x4_format(id, NULL, 16));
	}
}

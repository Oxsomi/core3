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

//types/math/mat.hpp

#pragma once
#include "types/math/vec4f.hpp"

//Operator sugar over the C F32x4x4; same deal as vec4f.hpp, a value with no ownership that compiles away to the same calls.
//
//  F32x4x4_mul(F32x4x4_scale(s), F32x4x4_mul(F32x4x4_rotate(r), F32x4x4_translate(t)))
//
//reads as
//
//  scale(s) * rotate(r) * translate(t)
//
//operator* is the matrix product, so it keeps the C's ordering: a * b applies a first, then b, and m * v transforms v by m.
//The conventions themselves (row major, row vectors, left handed, [0,1] depth) are documented on the C side in mat.h
// and are not restated here so they can't drift.

namespace oxc {

	namespace c {
		#include "types/math/mat.h"
	}

	class F32x4x4 {

		c::F32x4x4 m;

	public:

		F32x4x4() noexcept : m(c::F32x4x4_identity()) {}
		F32x4x4(const c::F32x4x4 &raw) noexcept : m(raw) {}

		//Rows, in the order the HLSL F32x4x4(...) constructor takes them
		F32x4x4(const F32x4 &r0, const F32x4 &r1, const F32x4 &r2, const F32x4 &r3) noexcept {
			m.v[0] = r0.handle();
			m.v[1] = r1.handle();
			m.v[2] = r2.handle();
			m.v[3] = r3.handle();
		}

		[[nodiscard]] static F32x4x4 zero() noexcept { return c::F32x4x4_zero(); }
		[[nodiscard]] static F32x4x4 identity() noexcept { return c::F32x4x4_identity(); }

		//Access

		[[nodiscard]] F32x4 row(c::U8 i) const noexcept { return c::F32x4x4_row(m, i); }
		[[nodiscard]] F32x4 column(c::U8 j) const noexcept { return c::F32x4x4_column(m, j); }
		[[nodiscard]] c::F32 get(c::U8 row, c::U8 col) const noexcept { return c::F32x4x4_get(m, row, col); }

		//operator[] indexes a row, matching HLSL's m[i]
		[[nodiscard]] F32x4 operator[](c::U8 i) const noexcept { return c::F32x4x4_row(m, i); }

		void setRow(c::U8 i, const F32x4 &v) noexcept { c::F32x4x4_setRow(&m, i, v.handle()); }
		void setColumn(c::U8 j, const F32x4 &v) noexcept { c::F32x4x4_setColumn(&m, j, v.handle()); }
		void set(c::U8 row, c::U8 col, c::F32 f) noexcept { c::F32x4x4_set(&m, row, col, f); }

		//Arithmetic. * is the matrix product; use mulScalar for a component-wise scale.

		[[nodiscard]] F32x4x4 operator+(const F32x4x4 &o) const noexcept { return c::F32x4x4_add(m, o.m); }
		[[nodiscard]] F32x4x4 operator-(const F32x4x4 &o) const noexcept { return c::F32x4x4_sub(m, o.m); }
		[[nodiscard]] F32x4x4 operator*(const F32x4x4 &o) const noexcept { return c::F32x4x4_mul(m, o.m); }

		F32x4x4 &operator+=(const F32x4x4 &o) noexcept { m = c::F32x4x4_add(m, o.m); return *this; }
		F32x4x4 &operator-=(const F32x4x4 &o) noexcept { m = c::F32x4x4_sub(m, o.m); return *this; }
		F32x4x4 &operator*=(const F32x4x4 &o) noexcept { m = c::F32x4x4_mul(m, o.m); return *this; }

		[[nodiscard]] F32x4x4 mulScalar(c::F32 s) const noexcept { return c::F32x4x4_mulScalar(m, s); }

		//Transforms.
		//transform() takes w as given; point/direction set it for you,
		// which is the difference between "translation applies" and "it doesn't".

		[[nodiscard]] F32x4 transform(const F32x4 &v) const noexcept {
			return c::F32x4x4_transform(m, v.handle());
		}

		[[nodiscard]] F32x4 transformPoint(const F32x4 &p) const noexcept {
			return c::F32x4x4_transformPoint(m, p.handle());
		}

		[[nodiscard]] F32x4 transformDirection(const F32x4 &d) const noexcept {
			return c::F32x4x4_transformDirection(m, d.handle());
		}

		//m * v reads as "apply m to v"
		[[nodiscard]] F32x4 operator*(const F32x4 &v) const noexcept { return transform(v); }

		[[nodiscard]] F32x4x4 transpose() const noexcept { return c::F32x4x4_transpose(m); }
		[[nodiscard]] c::F32 determinant() const noexcept { return c::F32x4x4_determinant(m); }

		//False for a singular matrix, leaving result untouched
		[[nodiscard]] bool inverse(F32x4x4 &result) const noexcept {
			return c::F32x4x4_inverse(m, &result.m);
		}

		//Builders

		[[nodiscard]] static F32x4x4 scale(const F32x4 &s) noexcept { return c::F32x4x4_scale(s.handle()); }

		[[nodiscard]] static F32x4x4 scale(c::F32 x, c::F32 y, c::F32 z) noexcept {
			return c::F32x4x4_scale3(x, y, z);
		}

		[[nodiscard]] static F32x4x4 translate(const F32x4 &t) noexcept {
			return c::F32x4x4_translate(t.handle());
		}

		[[nodiscard]] static F32x4x4 translate(c::F32 x, c::F32 y, c::F32 z) noexcept {
			return c::F32x4x4_translate3(x, y, z);
		}

		[[nodiscard]] static F32x4x4 rotateX(c::F32 rad) noexcept { return c::F32x4x4_rotateX(rad); }
		[[nodiscard]] static F32x4x4 rotateY(c::F32 rad) noexcept { return c::F32x4x4_rotateY(rad); }
		[[nodiscard]] static F32x4x4 rotateZ(c::F32 rad) noexcept { return c::F32x4x4_rotateZ(rad); }

		[[nodiscard]] static F32x4x4 rotate(const F32x4 &eulerRad) noexcept {
			return c::F32x4x4_rotate(eulerRad.handle());
		}

		[[nodiscard]] static F32x4x4 rotate(const c::QuatF32 &q) noexcept { return c::F32x4x4_rotateQuat(q); }

		[[nodiscard]] static F32x4x4 transformSRT(
			const F32x4 &scale, const F32x4 &eulerRad, const F32x4 &translate
		) noexcept {
			return c::F32x4x4_transformSRT(scale.handle(), eulerRad.handle(), translate.handle());
		}

		[[nodiscard]] static F32x4x4 view(const F32x4 &position, const F32x4 &eulerRad) noexcept {
			return c::F32x4x4_view(position.handle(), eulerRad.handle());
		}

		[[nodiscard]] static F32x4x4 perspective(c::F32 fovYRad, c::F32 aspect, c::F32 near, c::F32 far) noexcept {
			return c::F32x4x4_perspective(fovYRad, aspect, near, far);
		}

		[[nodiscard]] static F32x4x4 ortho(
			c::F32 left, c::F32 right, c::F32 bottom, c::F32 top, c::F32 near, c::F32 far
		) noexcept {
			return c::F32x4x4_ortho(left, right, bottom, top, near, far);
		}

		[[nodiscard]] static F32x4x4 orthoSize(c::F32 width, c::F32 height, c::F32 near, c::F32 far) noexcept {
			return c::F32x4x4_orthoSize(width, height, near, far);
		}

		[[nodiscard]] static F32x4x4 construct(
			const F32x4 &x, const F32x4 &y, const F32x4 &z, const F32x4 &eye
		) noexcept {
			return c::F32x4x4_construct(x.handle(), y.handle(), z.handle(), eye.handle());
		}

		[[nodiscard]] static F32x4x4 lookDir(const F32x4 &eye, const F32x4 &dir, const F32x4 &up) noexcept {
			return c::F32x4x4_lookDir(eye.handle(), dir.handle(), up.handle());
		}

		[[nodiscard]] static F32x4x4 lookAt(const F32x4 &eye, const F32x4 &center, const F32x4 &up) noexcept {
			return c::F32x4x4_lookAt(eye.handle(), center.handle(), up.handle());
		}

		//Comparison; use eqApprox for anything that went through arithmetic

		[[nodiscard]] bool operator==(const F32x4x4 &o) const noexcept { return c::F32x4x4_eq(m, o.m); }
		[[nodiscard]] bool operator!=(const F32x4x4 &o) const noexcept { return c::F32x4x4_neq(m, o.m); }

		[[nodiscard]] bool eqApprox(const F32x4x4 &o) const noexcept { return c::F32x4x4_eqApprox(m, o.m); }

		[[nodiscard]] bool eqApprox(const F32x4x4 &o, c::F32 relEps, c::F32 absEps) const noexcept {
			return c::F32x4x4_eqApproxAdv(m, o.m, relEps, absEps);
		}

		//Debug dump; 0 if it didn't fit
		[[nodiscard]] c::U64 format(c::C8 *buffer, c::U64 bufferSize) const noexcept {
			return c::F32x4x4_format(m, buffer, bufferSize);
		}

		//Interop

		[[nodiscard]] c::F32x4x4 &handle() noexcept { return m; }
		[[nodiscard]] const c::F32x4x4 &handle() const noexcept { return m; }
	};

	//Has to stay a pure value so it can sit inside a C struct or an upload buffer unchanged
	static_assert(sizeof(F32x4x4) == sizeof(c::F32x4x4), "oxc::F32x4x4 must stay layout compatible");
	static_assert(std::is_trivially_copyable<F32x4x4>::value, "oxc::F32x4x4 must stay trivially copyable");
}

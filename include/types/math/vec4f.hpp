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

//types/math/vec4f.hpp

#pragma once
#include <type_traits>

//Pre-include system headers used by the C headers below at global scope;
//they must not be pulled in for the first time inside a namespace.

#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#ifdef __APPLE__
	#include <TargetConditionals.h>            //platform_types.h reaches for this on apple
#endif

//platform_types.h is what decides _SIMD, so pull it into oxc::c first - its *types* belong there like
//every other C header's. The macros it defines are global regardless (macros don't see namespaces),
//which is what lets the intrinsic header be picked below, at the global scope intrinsics require.

namespace oxc { namespace c {
	#include "types/base/platform_types.h"
} }

#if _SIMD == SIMD_SSE
	#include <emmintrin.h>
	#include <smmintrin.h>
	#include <immintrin.h>
#elif _SIMD == SIMD_NEON
	#include <arm_neon.h>
#endif

//Operator sugar over the C F32x4.
//
//This is a value, not a resource: no allocator, no ownership, trivially copyable, and it compiles away
//to the same intrinsics the C code emits. The point is only that
//
//  F32x4_add(F32x4_mul(a, b), F32x4_xxxx4(2))
//
//reads as
//
//  a * b + 2
//
//Split from I32x4 the same way vec4f.h is split from vec4i.h: neither needs the other, and the
//conversions between them live in vec_cvt.hpp, mirroring vec_cvt.h.
//
//Everything the C API has stays reachable through handle(); this deliberately doesn't mirror the ~300
//generated swizzles, only the handful that come up constantly.

namespace oxc {

	namespace c {
		#include "types/math/vec4f.h"
		#include "types/math/vec4f_swizzle.h"
	}

	class F32x4 {

		c::F32x4 v;

	public:

		F32x4() noexcept : v(c::F32x4_zero()) {}
		F32x4(const c::F32x4 &raw) noexcept : v(raw) {}
		explicit F32x4(c::F32 all) noexcept : v(c::F32x4_xxxx4(all)) {}
		F32x4(c::F32 x, c::F32 y, c::F32 z = 0, c::F32 w = 0) noexcept : v(c::F32x4_create4(x, y, z, w)) {}

		[[nodiscard]] static F32x4 zero() noexcept { return c::F32x4_zero(); }
		[[nodiscard]] static F32x4 one() noexcept { return c::F32x4_one(); }

		//Reads 4 contiguous floats
		[[nodiscard]] static F32x4 load(const void *ptr) noexcept { return c::F32x4_load4(ptr); }

		//Components

		[[nodiscard]] c::F32 x() const noexcept { return c::F32x4_x(v); }
		[[nodiscard]] c::F32 y() const noexcept { return c::F32x4_y(v); }
		[[nodiscard]] c::F32 z() const noexcept { return c::F32x4_z(v); }
		[[nodiscard]] c::F32 w() const noexcept { return c::F32x4_w(v); }
		[[nodiscard]] c::F32 operator[](c::U8 i) const noexcept { return c::F32x4_get(v, i); }

		void setX(c::F32 f) noexcept { c::F32x4_setXRef(&v, f); }
		void setY(c::F32 f) noexcept { c::F32x4_setYRef(&v, f); }
		void setZ(c::F32 f) noexcept { c::F32x4_setZRef(&v, f); }
		void setW(c::F32 f) noexcept { c::F32x4_setWRef(&v, f); }
		void set(c::U8 i, c::F32 f) noexcept { c::F32x4_setRef(&v, i, f); }

		//Arithmetic

		[[nodiscard]] F32x4 operator+(const F32x4 &o) const noexcept { return c::F32x4_add(v, o.v); }
		[[nodiscard]] F32x4 operator-(const F32x4 &o) const noexcept { return c::F32x4_sub(v, o.v); }
		[[nodiscard]] F32x4 operator*(const F32x4 &o) const noexcept { return c::F32x4_mul(v, o.v); }
		[[nodiscard]] F32x4 operator/(const F32x4 &o) const noexcept { return c::F32x4_div(v, o.v); }
		[[nodiscard]] F32x4 operator-() const noexcept { return c::F32x4_negate(v); }

		F32x4 &operator+=(const F32x4 &o) noexcept { v = c::F32x4_add(v, o.v); return *this; }
		F32x4 &operator-=(const F32x4 &o) noexcept { v = c::F32x4_sub(v, o.v); return *this; }
		F32x4 &operator*=(const F32x4 &o) noexcept { v = c::F32x4_mul(v, o.v); return *this; }
		F32x4 &operator/=(const F32x4 &o) noexcept { v = c::F32x4_div(v, o.v); return *this; }

		//Scalar forms, so `a * 2` doesn't need an explicit broadcast

		[[nodiscard]] F32x4 operator*(c::F32 s) const noexcept { return c::F32x4_mul(v, c::F32x4_xxxx4(s)); }
		[[nodiscard]] F32x4 operator/(c::F32 s) const noexcept { return c::F32x4_div(v, c::F32x4_xxxx4(s)); }
		F32x4 &operator*=(c::F32 s) noexcept { v = c::F32x4_mul(v, c::F32x4_xxxx4(s)); return *this; }
		F32x4 &operator/=(c::F32 s) noexcept { v = c::F32x4_div(v, c::F32x4_xxxx4(s)); return *this; }

		//a * b + c, single rounding where the hardware has it
		[[nodiscard]] static F32x4 fma(const F32x4 &a, const F32x4 &b, const F32x4 &add) noexcept {
			return c::F32x4_fma(a.v, b.v, add.v);
		}

		//Geometry. The 2/3/4 suffix is the component count taken part in, matching the C names.

		[[nodiscard]] c::F32 dot2(const F32x4 &o) const noexcept { return c::F32x4_dot2(v, o.v); }
		[[nodiscard]] c::F32 dot3(const F32x4 &o) const noexcept { return c::F32x4_dot3(v, o.v); }
		[[nodiscard]] c::F32 dot4(const F32x4 &o) const noexcept { return c::F32x4_dot4(v, o.v); }

		[[nodiscard]] c::F32 sqLen2() const noexcept { return c::F32x4_sqLen2(v); }
		[[nodiscard]] c::F32 sqLen3() const noexcept { return c::F32x4_sqLen3(v); }
		[[nodiscard]] c::F32 sqLen4() const noexcept { return c::F32x4_sqLen4(v); }

		[[nodiscard]] c::F32 len2() const noexcept { return c::F32x4_len2(v); }
		[[nodiscard]] c::F32 len3() const noexcept { return c::F32x4_len3(v); }
		[[nodiscard]] c::F32 len4() const noexcept { return c::F32x4_len4(v); }

		[[nodiscard]] F32x4 normalize2() const noexcept { return c::F32x4_normalize2(v); }
		[[nodiscard]] F32x4 normalize3() const noexcept { return c::F32x4_normalize3(v); }
		[[nodiscard]] F32x4 normalize4() const noexcept { return c::F32x4_normalize4(v); }

		[[nodiscard]] F32x4 cross3(const F32x4 &o) const noexcept { return c::F32x4_cross3(v, o.v); }

		//Component-wise helpers

		[[nodiscard]] F32x4 abs() const noexcept { return c::F32x4_abs(v); }
		[[nodiscard]] F32x4 sign() const noexcept { return c::F32x4_sign(v); }
		[[nodiscard]] F32x4 floor() const noexcept { return c::F32x4_floor(v); }
		[[nodiscard]] F32x4 ceil() const noexcept { return c::F32x4_ceil(v); }
		[[nodiscard]] F32x4 round() const noexcept { return c::F32x4_round(v); }
		[[nodiscard]] F32x4 sqrt() const noexcept { return c::F32x4_sqrt(v); }
		[[nodiscard]] F32x4 rsqrt() const noexcept { return c::F32x4_rsqrt(v); }
		[[nodiscard]] F32x4 saturate() const noexcept { return c::F32x4_saturate(v); }

		[[nodiscard]] F32x4 min(const F32x4 &o) const noexcept { return c::F32x4_min(v, o.v); }
		[[nodiscard]] F32x4 max(const F32x4 &o) const noexcept { return c::F32x4_max(v, o.v); }

		[[nodiscard]] F32x4 clamp(const F32x4 &lo, const F32x4 &hi) const noexcept {
			return c::F32x4_clamp(v, lo.v, hi.v);
		}

		[[nodiscard]] static F32x4 lerp(const F32x4 &a, const F32x4 &b, c::F32 perc) noexcept {
			return c::F32x4_lerp(a.v, b.v, perc);
		}

		//Zeroes the components past the given count
		[[nodiscard]] F32x4 trunc2() const noexcept { return c::F32x4_trunc2(v); }
		[[nodiscard]] F32x4 trunc3() const noexcept { return c::F32x4_trunc3(v); }

		//The swizzles that actually come up; everything else is reachable through handle()
		[[nodiscard]] F32x4 xxxx() const noexcept { return c::F32x4_xxxx(v); }
		[[nodiscard]] F32x4 yyyy() const noexcept { return c::F32x4_yyyy(v); }
		[[nodiscard]] F32x4 zzzz() const noexcept { return c::F32x4_zzzz(v); }
		[[nodiscard]] F32x4 wwww() const noexcept { return c::F32x4_wwww(v); }
		[[nodiscard]] F32x4 wzyx() const noexcept { return c::F32x4_wzyx(v); }

		//Comparison. == is exact on all four lanes; use eqApprox for anything that went through arithmetic.

		[[nodiscard]] bool operator==(const F32x4 &o) const noexcept { return c::F32x4_eqExact4(v, o.v); }
		[[nodiscard]] bool operator!=(const F32x4 &o) const noexcept { return c::F32x4_neqExact4(v, o.v); }

		[[nodiscard]] bool eqApprox(const F32x4 &o) const noexcept { return c::F32x4_eqApprox4(v, o.v); }

		[[nodiscard]] bool eqApprox(const F32x4 &o, c::F32 relEps, c::F32 absEps) const noexcept {
			return c::F32x4_eqApproxAdv4(v, o.v, relEps, absEps);
		}

		//Per-lane compares produce a 0/1 mask vector, matching the C behaviour
		[[nodiscard]] F32x4 lt(const F32x4 &o) const noexcept { return c::F32x4_lt(v, o.v); }
		[[nodiscard]] F32x4 gt(const F32x4 &o) const noexcept { return c::F32x4_gt(v, o.v); }

		[[nodiscard]] bool all() const noexcept { return c::F32x4_all(v); }
		[[nodiscard]] bool any() const noexcept { return c::F32x4_any(v); }

		//Interop

		[[nodiscard]] c::F32x4 &handle() noexcept { return v; }
		[[nodiscard]] const c::F32x4 &handle() const noexcept { return v; }
	};

	[[nodiscard]] inline F32x4 operator*(c::F32 s, const F32x4 &v) noexcept { return v * s; }

	//These have to stay pure values so they can sit inside a struct, a TList or a matrix without the C
	//side ever knowing a wrapper was involved.
	static_assert(sizeof(F32x4) == sizeof(c::F32x4), "oxc::F32x4 must stay layout compatible with c::F32x4");
	static_assert(std::is_trivially_copyable<F32x4>::value, "oxc::F32x4 must stay trivially copyable");
}

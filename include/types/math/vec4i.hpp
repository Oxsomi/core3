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

//types/math/vec4i.hpp

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

//platform_types.h is what decides _SIMD,
// so pull it into oxc::c first, its *types* belong there like every other C header's.
//The macros it defines are global regardless (macros don't see namespaces),
// which is what lets the intrinsic header be picked below, at the global scope intrinsics require.

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

//Operator sugar over the C I32x4; see vec4f.hpp for the rationale.
//Conversions to and from F32x4 live in vec_cvt.hpp, mirroring vec_cvt.h.

namespace oxc {

	namespace c {
		#include "types/math/vec4i.h"
		#include "types/math/vec4i_swizzle.h"
	}

	class I32x4 {

		c::I32x4 v;

	public:

		I32x4() noexcept : v(c::I32x4_zero()) {}
		I32x4(const c::I32x4 &raw) noexcept : v(raw) {}
		explicit I32x4(c::I32 all) noexcept : v(c::I32x4_xxxx4(all)) {}
		I32x4(c::I32 x, c::I32 y, c::I32 z = 0, c::I32 w = 0) noexcept : v(c::I32x4_create4(x, y, z, w)) {}

		[[nodiscard]] static I32x4 zero() noexcept { return c::I32x4_zero(); }
		[[nodiscard]] static I32x4 one() noexcept { return c::I32x4_one(); }

		[[nodiscard]] static I32x4 load(const void *ptr) noexcept { return c::I32x4_load4(ptr); }

		//Components

		[[nodiscard]] c::I32 x() const noexcept { return c::I32x4_x(v); }
		[[nodiscard]] c::I32 y() const noexcept { return c::I32x4_y(v); }
		[[nodiscard]] c::I32 z() const noexcept { return c::I32x4_z(v); }
		[[nodiscard]] c::I32 w() const noexcept { return c::I32x4_w(v); }
		[[nodiscard]] c::I32 operator[](c::U8 i) const noexcept { return c::I32x4_get(v, i); }

		void setX(c::I32 i) noexcept { c::I32x4_setXRef(&v, i); }
		void setY(c::I32 i) noexcept { c::I32x4_setYRef(&v, i); }
		void setZ(c::I32 i) noexcept { c::I32x4_setZRef(&v, i); }
		void setW(c::I32 i) noexcept { c::I32x4_setWRef(&v, i); }
		void set(c::U8 i, c::I32 val) noexcept { c::I32x4_setRef(&v, i, val); }

		//Arithmetic

		[[nodiscard]] I32x4 operator+(const I32x4 &o) const noexcept { return c::I32x4_add(v, o.v); }
		[[nodiscard]] I32x4 operator-(const I32x4 &o) const noexcept { return c::I32x4_sub(v, o.v); }
		[[nodiscard]] I32x4 operator*(const I32x4 &o) const noexcept { return c::I32x4_mul(v, o.v); }
		[[nodiscard]] I32x4 operator/(const I32x4 &o) const noexcept { return c::I32x4_div(v, o.v); }
		[[nodiscard]] I32x4 operator%(const I32x4 &o) const noexcept { return c::I32x4_mod(v, o.v); }
		[[nodiscard]] I32x4 operator-() const noexcept { return c::I32x4_negate(v); }

		I32x4 &operator+=(const I32x4 &o) noexcept { v = c::I32x4_add(v, o.v); return *this; }
		I32x4 &operator-=(const I32x4 &o) noexcept { v = c::I32x4_sub(v, o.v); return *this; }
		I32x4 &operator*=(const I32x4 &o) noexcept { v = c::I32x4_mul(v, o.v); return *this; }
		I32x4 &operator/=(const I32x4 &o) noexcept { v = c::I32x4_div(v, o.v); return *this; }

		//Bitwise

		[[nodiscard]] I32x4 operator&(const I32x4 &o) const noexcept { return c::I32x4_and(v, o.v); }
		[[nodiscard]] I32x4 operator|(const I32x4 &o) const noexcept { return c::I32x4_or(v, o.v); }
		[[nodiscard]] I32x4 operator^(const I32x4 &o) const noexcept { return c::I32x4_xor(v, o.v); }

		I32x4 &operator&=(const I32x4 &o) noexcept { v = c::I32x4_and(v, o.v); return *this; }
		I32x4 &operator|=(const I32x4 &o) noexcept { v = c::I32x4_or(v, o.v); return *this; }
		I32x4 &operator^=(const I32x4 &o) noexcept { v = c::I32x4_xor(v, o.v); return *this; }

		//Whole-vector (128 bit) shifts, matching I32x4_lsh128 / I32x4_rsh128
		[[nodiscard]] I32x4 lsh128(c::U8 bits) const noexcept { return c::I32x4_lsh128(v, bits); }
		[[nodiscard]] I32x4 rsh128(c::U8 bits) const noexcept { return c::I32x4_rsh128(v, bits); }

		//Component-wise helpers

		[[nodiscard]] I32x4 abs() const noexcept { return c::I32x4_abs(v); }
		[[nodiscard]] I32x4 sign() const noexcept { return c::I32x4_sign(v); }
		[[nodiscard]] I32x4 min(const I32x4 &o) const noexcept { return c::I32x4_min(v, o.v); }
		[[nodiscard]] I32x4 max(const I32x4 &o) const noexcept { return c::I32x4_max(v, o.v); }

		[[nodiscard]] I32x4 clamp(const I32x4 &lo, const I32x4 &hi) const noexcept {
			return c::I32x4_clamp(v, lo.v, hi.v);
		}

		[[nodiscard]] I32x4 trunc2() const noexcept { return c::I32x4_trunc2(v); }
		[[nodiscard]] I32x4 trunc3() const noexcept { return c::I32x4_trunc3(v); }

		[[nodiscard]] I32x4 xxxx() const noexcept { return c::I32x4_xxxx(v); }
		[[nodiscard]] I32x4 wzyx() const noexcept { return c::I32x4_wzyx(v); }

		//Comparison

		[[nodiscard]] bool operator==(const I32x4 &o) const noexcept { return c::I32x4_eq4(v, o.v); }
		[[nodiscard]] bool operator!=(const I32x4 &o) const noexcept { return c::I32x4_neq4(v, o.v); }

		[[nodiscard]] bool all() const noexcept { return c::I32x4_all(v); }
		[[nodiscard]] bool any() const noexcept { return c::I32x4_any(v); }

		//Interop

		[[nodiscard]] c::I32x4 &handle() noexcept { return v; }
		[[nodiscard]] const c::I32x4 &handle() const noexcept { return v; }
	};

	static_assert(sizeof(I32x4) == sizeof(c::I32x4), "oxc::I32x4 must stay layout compatible with c::I32x4");
	static_assert(std::is_trivially_copyable<I32x4>::value, "oxc::I32x4 must stay trivially copyable");
}

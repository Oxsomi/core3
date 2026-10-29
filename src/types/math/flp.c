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

//types/math/flp.c

#include "types/base/platform_types.h"
#include "types/math/flp.h"
#include "types/math/type_cast.h"
#include "types/base/mathi.h"

#if !_FORCE_FLOAT_FALLBACK && _SIMD == SIMD_SSE
	#include "types/math/vec4i.h"
	#include "types/math/vec4f.h"
	#include <immintrin.h>
#elif !_FORCE_FLOAT_FALLBACK && _SIMD == SIMD_NEON
	#include <arm_neon.h>
#endif

//The generic path stays out of line on purpose:
// it is too big to copy into every cast helper, and only the dispatcher in EFloatType_convert needs to inline.
//That way LTCG folds the constant types of each X_castY into a single instruction or a direct call to this.

#if _MSC_VER
	#define EFloatType_noinline __declspec(noinline)
#else
	#define EFloatType_noinline __attribute__((noinline))
#endif

static U64 EFloatType_roundRight(U64 value, I32 shift) {

	if(shift <= 0)
		return value << -shift;

	if(shift >= 64)
		return 0;

	const U64 half = (U64) 1 << (shift - 1);
	const U64 discarded = value & ((half << 1) - 1);
	const U64 rounded = value >> shift;

	return rounded + (discarded > half || (discarded == half && (rounded & 1)));
}

static EFloatType_noinline U64 EFloatType_convertSoftware(EFloatType type, U64 v, EFloatType conversionType) {

	//A negative going to an unsigned type has to CLAMP:
	// signMask is 0 there, so without this the magnitude would convert cleanly and -3.5 would arrive as +3.5.

	if (!EFloatType_hasSign(conversionType) && EFloatType_sign(type, v))
		return 0;

	const U64 sign = EFloatType_sign(type, v) ? EFloatType_signMask(conversionType) : 0;

	if (EFloatType_isZero(type, v))
		return sign;

	const U8 srcMantissaBits = EFloatType_mantissaBits(type);
	const U8 dstMantissaBits = EFloatType_mantissaBits(conversionType);
	const U64 dstExponentMask = EFloatType_exponentMask(conversionType);
	const U64 mantissa = EFloatType_mantissa(type, v);
	const U64 inf = sign | (dstExponentMask << dstMantissaBits);     //+-infinity in the destination format

	if(!EFloatType_isFinite(type, v)) {

		U64 payload = srcMantissaBits > dstMantissaBits ? mantissa >> (srcMantissaBits - dstMantissaBits) :
			mantissa << (dstMantissaBits - srcMantissaBits);

		//Set the quiet bit: keeps a NaN from collapsing into inf if narrowing shifted out its whole payload,
		//and turns signaling NaNs into quiet ones as IEEE 754 requires for conversions.

		if(mantissa)
			payload |= (U64) 1 << (dstMantissaBits - 1);

		return inf | payload;
	}

	const U64 srcExponent = EFloatType_exponent(type, v);
	const U64 significand = mantissa | (srcExponent ? (U64) 1 << srcMantissaBits : 0);
	const I32 srcPower = (I32) (srcExponent ? srcExponent : 1) -
		(I32) (EFloatType_exponentMask(type) >> 1) - srcMantissaBits;

	const I32 dstBias = (I32) (dstExponentMask >> 1);
	const I32 unbiased = srcPower + U64_bitScan(significand);
	const I32 exponent = unbiased + dstBias;

	if(exponent >= (I32) dstExponentMask)
		return inf;

	//Round the full significand once at the destination's spacing, including its subnormal range.
	const I32 dstPower = (exponent > 0 ? unbiased : 1 - dstBias) - dstMantissaBits;
	const U64 rounded = EFloatType_roundRight(significand, dstPower - srcPower);

	if(exponent <= 0)
		return sign | rounded;

	//rounded still carries the implicit bit at dstMantissaBits, which adds the missing 1 back to exponent - 1.
	//A rounding carry into bit dstMantissaBits + 1 bumps the exponent once more, and a carry out of the largest
	// finite value lands on all-ones exponent with zero mantissa, which is exactly inf.

	return sign | (((U64) (exponent - 1) << dstMantissaBits) + rounded);
}

U64 EFloatType_convert(EFloatType type, U64 v, EFloatType conversionType) {

	#if !_FORCE_FLOAT_FALLBACK

		//Hardware support.
		//These follow the FP environment (MXCSR on x86, FPCR on ARM):
		// with flush-to-zero, denormals-are-zero or default-NaN mode enabled they stop matching the software path.

		if (type == EFloatType_F32 && conversionType == EFloatType_F64) {

			const F32 f32 = F32_fromU32Bits((U32) v);
			const F64 f64 = (F64) f32;

			return U64_fromF64Bits(f64);
		}

		if (type == EFloatType_F64 && conversionType == EFloatType_F32) {

			const F64 f64 = F64_fromU64Bits(v);
			const F32 f32 = (F32) f64;

			return U32_fromF32Bits(f32);
		}

		//F16 <-> F32 in hardware, and F16 -> F64 through F32 (exact, F32 holds every F16).
		//F64 -> F16 is NOT routed through F32: that rounds twice and can be 1 ULP off on ties,
		// so it goes to the software path, which rounds once.

		#if _SIMD == SIMD_SSE

			//SSE builds require F16C at startup, so conversion needs no per-call dispatch.

			if (type == EFloatType_F16 && (conversionType == EFloatType_F32 || conversionType == EFloatType_F64)) {

				const I32x4 expandedi = I32x4_create1((I32) v);
				const F32 expanded = F32x4_x(_mm_cvtph_ps(expandedi));

				if(conversionType == EFloatType_F64)
					return U64_fromF64Bits((F64) expanded);

				return U32_fromF32Bits(expanded);
			}

			if (type == EFloatType_F32 && conversionType == EFloatType_F16) {

				const F32 truncated = F32_fromU32Bits((U32) v);
				const I32x4 converted = _mm_cvtps_ph(F32x4_create1(truncated), _MM_FROUND_TO_NEAREST_INT);

				return (F16) I32x4_x(converted);        //Low 16 bits of lane 0 hold element 0
			}

		#elif _SIMD == SIMD_NEON

			//FCVTL / FCVTN are base AArch64, no extension check needed.

			if (type == EFloatType_F16 && (conversionType == EFloatType_F32 || conversionType == EFloatType_F64)) {

				const float16x4_t half = vreinterpret_f16_u16(vdup_n_u16((U16) v));
				const F32 expanded = vgetq_lane_f32(vcvt_f32_f16(half), 0);

				if(conversionType == EFloatType_F64)
					return U64_fromF64Bits((F64) expanded);

				return U32_fromF32Bits(expanded);
			}

			if (type == EFloatType_F32 && conversionType == EFloatType_F16) {
				const float16x4_t half = vcvt_f16_f32(vdupq_n_f32(F32_fromU32Bits((U32) v)));
				return vget_lane_u16(vreinterpret_u16_f16(half), 0);
			}

		#endif

	#endif

	return EFloatType_convertSoftware(type, v, conversionType);
}

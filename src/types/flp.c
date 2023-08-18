/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/flp.h"

U8 EFloatType_bytes(EFloatType type) {
	return (U8)(type >> 16);
}

U8 EFloatType_exponentBits(EFloatType type) {
	return (U8)(type >> 8);
}

U8 EFloatType_mantissaBits(EFloatType type) {
	return (U8) type;
}

U64 EFloatType_mantissaShift(EFloatType type) { 
	type;
	return 0;
}

U64 EFloatType_exponentShift(EFloatType type) {
	return EFloatType_mantissaShift(type) + EFloatType_mantissaBits(type);
}

U64 EFloatType_signShift(EFloatType type) {
	return EFloatType_exponentShift(type) + EFloatType_exponentBits(type);
}

U64 EFloatType_signMask(EFloatType type) {
	return (U64)1 << EFloatType_signShift(type);
}

U64 EFloatType_exponentMask(EFloatType type) {
	return ((U64)1 << EFloatType_exponentBits(type)) - 1;
}

U64 EFloatType_mantissaMask(EFloatType type) {
	return ((U64)1 << EFloatType_mantissaBits(type)) - 1;
}

bool EFloatType_sign(EFloatType type, U64 v) {
	return v & EFloatType_signMask(type);
}

U64 EFloatType_abs(EFloatType type, U64 v) {
	return v &~ EFloatType_signMask(type);
}

U64 EFloatType_negate(EFloatType type, U64 v) {
	return v ^ EFloatType_signMask(type);
}

U64 EFloatType_exponent(EFloatType type, U64 v) {
	return (v >> EFloatType_exponentShift(type)) & EFloatType_exponentMask(type);
}

U64 EFloatType_mantissa(EFloatType type, U64 v) {
	return (v >> EFloatType_mantissaShift(type)) & EFloatType_mantissaMask(type);
}

U64 EFloatType_isFinite(EFloatType type, U64 v) {
	return EFloatType_exponent(type, v) != EFloatType_exponentMask(type);
}

U64 EFloatType_isDeN(EFloatType type, U64 v) {
	return !EFloatType_exponent(type, v);
}

U64 EFloatType_isNaN(EFloatType type, U64 v) {
	return !EFloatType_isFinite(type, v) && EFloatType_mantissa(type, v);
}

U64 EFloatType_isInf(EFloatType type, U64 v) {
	return !EFloatType_isFinite(type, v) && !EFloatType_mantissa(type, v);
}

U64 EFloatType_isZero(EFloatType type, U64 v) {
	return !EFloatType_abs(type, v);
}

U64 EFloatType_getNaN(EFloatType type) {
	return EFloatType_getInf(type) | ((U64)1 << EFloatType_mantissaShift(type));
}

U64 EFloatType_getInf(EFloatType type) { 
	return EFloatType_exponentMask(type) << EFloatType_exponentShift(type);
}

U64 EFloatType_convertMantissa(EFloatType type1, U64 v, EFloatType type2, Bool *carry) {

	U8 mbit1 = EFloatType_mantissaBits(type1);
	U8 mbit2 = EFloatType_mantissaBits(type2);
	U64 mantissa = EFloatType_mantissa(type1, v);

	if (mbit1 == mbit2)
		return mantissa;

	if (mbit2 > mbit1)
		return mantissa << (mbit2 - mbit1);

	U64 shiftedMantissa = mantissa >> (mbit1 - mbit2);
	U64 discardedMantissa = mantissa & (((U64)1 << (mbit1 - mbit2)) - 1);
	U64 halfMantissa = (U64)1 << (mbit1 - mbit2 - 1);

	U8 round = discardedMantissa > halfMantissa;	//Yes, rounding for some reason ignores 0.5 and only works >0.5

	if (!EFloatType_isFinite(type1, v))				//Rounding is only for real numbers
		round = 0;

	U64 res = (shiftedMantissa + round) & EFloatType_mantissaMask(type2);

	if (!res && round)
		*carry = true;		//Increments exponent

	return res;
}

U64 EFloatType_convertExponent(
	EFloatType type1,
	U64 v,
	EFloatType type2,
	U64 *convertedMantissa,
	Bool carry
) {

	U8 ebit1 = EFloatType_exponentBits(type1);
	U8 ebit2 = EFloatType_exponentBits(type2);

	U8 mbit1 = EFloatType_mantissaBits(type1);
	U8 mbit2 = EFloatType_mantissaBits(type2);

	U64 exponent = EFloatType_exponent(type1, v);

	if (ebit1 == ebit2)
		return exponent;

	//Handle NaN/Inf

	if (!EFloatType_isFinite(type1, v)) {

		//NaN sets first mantissa bit to avoid accidental Inf

		if (EFloatType_isNaN(type1, v))
			*convertedMantissa |= (U64)1 << (EFloatType_mantissaBits(type2) - 1);

		//Inf or NaN always have all exp bits set to 1

		return EFloatType_exponentMask(type2);
	}

	//Expand

	if (ebit2 > ebit1) {

		//In expansion, the first on bit in the mantissa will be the new exponent.
		//The remainder of the mantissa is then shifted back to match the exponent.
		//I call this "renormalization"

		if (EFloatType_isDeN(type1, v)) {

			//To find the first on bit, we do a binary bit search.
			//This allows us to traverse up to 64 bits in 6 steps.

			U8 left = 0, right = EFloatType_mantissaBits(type1);

			U64 m = EFloatType_mantissa(type1, v);

			for (U8 i = 0; i < 6; ++i) {

				U8 bits = right - left;
				U8 center = left + (bits >> 1);
				U64 mask = (((U64)1 << (bits - (bits >> 1))) - 1) << center;

				//Left side

				if (m & mask)
					left = center;

				//Right side

				else right = center;

				if (bits == 2)
					break;
			}

			//Now we can shift mantissa by the first place to shift the first 1 out.
			//But keep the rest of the mantissa in.
			//The exponent then needs to be compensated.

			m = m << (mbit1 - left);
			m &= EFloatType_mantissaMask(type1);

			//Correct mantissa

			if (mbit2 >= mbit1)
				*convertedMantissa = m << (mbit2 - mbit1);

			else *convertedMantissa = m >> (mbit1 - mbit2);

			//Make exponent. Difference between the two exponents but adding the shift.

			U64 exp = (EFloatType_exponentMask(type2) >> 1) - (EFloatType_exponentMask(type1) >> 1);
			exp -= mbit1 - left - 1;
			return exp;
		}

		//Normal conversion of exponent

		I64 cvt = (I64)exponent - (EFloatType_exponentMask(type1) >> 1);
		cvt += EFloatType_exponentMask(type2) >> 1;
		return (U64)cvt;
	}

	//Truncate

	//Normal values can collapse to Inf or DeN,
	//Needs to be handled too.

	I64 cvt = (I64)exponent - (EFloatType_exponentMask(type1) >> 1);
	cvt += EFloatType_exponentMask(type2) >> 1;

	//Convert to DeN

	if (cvt <= 0) {

		//We can only get the lowest possible number by taking exponent 0 (2^-(b/2)).
		//However, this requires us to reconfigure the mantissa to approximate the original value.
		//This can mean sometimes a small number is collapsed to 0 (not enough mantissa bits).
		//We do this by simply moving around the prev exponent by our new exponent bits.
		//The remaining bits is simply how much our mantissa has to be shifted to get the same value.

		U64 missingBits = (U64)-cvt;

		if (missingBits > mbit2) {		//Collapse to zero
			*convertedMantissa = 0;
			return 0;
		}

		U64 m = EFloatType_mantissa(type1, v);

		if(missingBits == mbit2) {
			*convertedMantissa = m != 0;
			return 0;
		}

		U64 mantissaDiscardShift = missingBits + (mbit1 - mbit2) + 1;
		U64 mantissaDiscardMask = ((U64)1 << mantissaDiscardShift) - 1;
		U64 mantissaDiscarded = m & mantissaDiscardMask;
		U64 mantissaDiscardHalf = (U64)1 << (mantissaDiscardShift - 1);

		U64 round = mantissaDiscarded > mantissaDiscardHalf;

		m >>= mantissaDiscardShift;					//Correct to correct exponent
		m |= (U64)1 << (mbit2 - missingBits - 1);	//Shift the 1.x into the DeN
		m += round;									//Ensure correct rounding

		//Special case; round causes exponent to increment

		if (!m && round) {
			*convertedMantissa = 0;
			return 1;
		}

		*convertedMantissa = m;
		return 0;
	}

	cvt += carry;

	//Generates Inf (exponent is too high)

	if((U64)cvt >= EFloatType_exponentMask(type2)) {
		*convertedMantissa = 0;
		cvt = EFloatType_exponentMask(type2);
	}

	//Otherwise our exponent is in the correct bounds.

	return (U64)cvt;
}

U64 EFloatType_convert(EFloatType type, U64 v, EFloatType conversionType) {

	#ifndef _FORCE_FLOAT_FALLBACK

		//Hardware support

		if (type == EFloatType_F32 && conversionType == EFloatType_F64) {

			U32 v32 = (U32)v;
			F32 f32 = *(const F32*)&v32;
			F64 f64 = f32;

			return *(const U64*)&f64;
		}

		if (type == EFloatType_F64 && conversionType == EFloatType_F32) {

			F64 f64 = *(const F64*)&v;
			F32 f32 = f64;

			return *(const U32*)&f32;
		}

	#endif

	U64 sign = EFloatType_sign(type, v) ? EFloatType_signMask(conversionType) : 0;

	if (EFloatType_isZero(type, v))
		return sign;

	Bool carry = false;
	U64 mantissa = EFloatType_convertMantissa(type, v, conversionType, &carry);
	U64 exponent = EFloatType_convertExponent(type, v, conversionType, &mantissa, carry);

	return 
		sign | 
		(exponent << EFloatType_exponentShift(conversionType)) | 
		(mantissa << EFloatType_mantissaShift(conversionType));
}

#undef _EFloatType_cast1
#undef _EFloatType_cast

#define _EFloatType_cast1(a, b)										\
a b##_cast##a(b v) {												\
																	\
	U64 v64;														\
																	\
	switch (EFloatType_bytes(EFloatType_##b)) {						\
		case 2:		v64 = *(const U16*) &v;		break;				\
		case 4:		v64 = *(const U32*) &v;		break;				\
		case 8:		v64 = *(const U64*) &v;		break;				\
		default:	v64 = *(const U8*) &v;		break;				\
	}																\
																	\
	v64 = EFloatType_convert(EFloatType_##b, v64, EFloatType_##a);	\
																	\
	return *(const a*)&v64;											\
}

#define _EFloatType_cast(a)		\
_EFloatType_cast1(F8, a);		\
_EFloatType_cast1(F16, a);		\
_EFloatType_cast1(F32, a);		\
_EFloatType_cast1(F64, a);		\
_EFloatType_cast1(BF16, a);		\
_EFloatType_cast1(TF19, a);		\
_EFloatType_cast1(PXR24, a);	\
_EFloatType_cast1(FP24, a);

_EFloatType_cast(F8); 
_EFloatType_cast(F16); 
_EFloatType_cast(F32); 
_EFloatType_cast(F64); 
_EFloatType_cast(BF16); 
_EFloatType_cast(TF19); 
_EFloatType_cast(PXR24); 
_EFloatType_cast(FP24);

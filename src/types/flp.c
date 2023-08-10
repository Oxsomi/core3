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

U64 EFloatType_exponent(EFloatType type, U64 v) {
	return (v >> EFloatType_exponentShift(type)) & EFloatType_exponentMask(type);
}

U64 EFloatType_mantissa(EFloatType type, U64 v) {
	return (v >> EFloatType_mantissaShift(type)) & EFloatType_mantissaMask(type);
}

U64 EFloatType_isFinite(EFloatType type, U64 v);
U64 EFloatType_isDeN(EFloatType type, U64 v);
U64 EFloatType_isNaN(EFloatType type, U64 v);
U64 EFloatType_isInf(EFloatType type, U64 v);
U64 EFloatType_isZero(EFloatType type, U64 v) {
	return !EFloatType_abs(type, v);
}

U64 EFloatType_getNaN(EFloatType type) {
	return EFloatType_getInf(type) | ((U64)1 << EFloatType_mantissaShift(type));
}

U64 EFloatType_getInf(EFloatType type) { 
	return EFloatType_exponentMask(type) << EFloatType_exponentShift(type);
}

U64 EFloatType_convertMantissa(EFloatType type1, U64 v, EFloatType type2);
U64 EFloatType_convertExponent(
	EFloatType type1, 
	U64 v, 
	EFloatType type2, 
	U64 convertedMantissa
);

U64 EFloatType_convert(EFloatType type, U64 v, EFloatType conversionType);

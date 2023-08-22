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

#pragma once
#include "types/types.h"

#define _EFloatType(exponentBits, mantissaBits, bytes) \
(bytes << 16) | (exponentBits << 8) | (mantissaBits << 0)

typedef enum EFloatType {

	EFloatType_F8			= _EFloatType( 4,   3,  1),
	EFloatType_F16			= _EFloatType( 5,  10,  2),
	EFloatType_F32			= _EFloatType( 8,  23,  4),
	EFloatType_F64			= _EFloatType(11,  52,  8),
	//EFloatType_Quadruple	= _EFloatType(15, 112, 16),
	//EFloatType_Octuple	= _EFloatType(19, 236, 32),

	EFloatType_BF16			= _EFloatType( 8,   7,  2),			//BFloat
	EFloatType_TF19			= _EFloatType( 8,  10,  4),			//TensorFloat
	EFloatType_PXR24		= _EFloatType( 8,  15,  4),
	EFloatType_FP24			= _EFloatType( 7,  16,  4)

} EFloatType;

U8 EFloatType_bytes(EFloatType type);
U8 EFloatType_exponentBits(EFloatType type);
U8 EFloatType_mantissaBits(EFloatType type);

U64 EFloatType_signShift(EFloatType type);
U64 EFloatType_exponentShift(EFloatType type);
U64 EFloatType_mantissaShift(EFloatType type);

U64 EFloatType_signMask(EFloatType type);			//Shifted left
U64 EFloatType_exponentMask(EFloatType type);		//Not shifted
U64 EFloatType_mantissaMask(EFloatType type);		//Not shifted

Bool EFloatType_sign(EFloatType type, U64 v);
U64 EFloatType_abs(EFloatType type, U64 v);
U64 EFloatType_negate(EFloatType type, U64 v);
U64 EFloatType_exponent(EFloatType type, U64 v);
U64 EFloatType_mantissa(EFloatType type, U64 v);

U64 EFloatType_isFinite(EFloatType type, U64 v);
U64 EFloatType_isDeN(EFloatType type, U64 v);
U64 EFloatType_isNaN(EFloatType type, U64 v);
U64 EFloatType_isInf(EFloatType type, U64 v);
U64 EFloatType_isZero(EFloatType type, U64 v);

U64 EFloatType_getNaN(EFloatType type);
U64 EFloatType_getInf(EFloatType type);

U64 EFloatType_convert(EFloatType type, U64 v, EFloatType conversionType);

//Auto-generated software float casts

typedef U8 F8;
typedef U16 F16;
typedef U16 BF16;
typedef U32 TF19;
typedef U32 PXR24;
typedef U32 FP24;

#define _EFloatType_cast(a)		\
a F8_cast##a(F8 v);				\
a F16_cast##a(F16 v);			\
a F32_cast##a(F32 v);			\
a F64_cast##a(F64 v);			\
a BF16_cast##a(BF16 v);			\
a TF19_cast##a(TF19 v);			\
a PXR24_cast##a(PXR24 v);		\
a FP24_cast##a(FP24 v);

_EFloatType_cast(F8); 
_EFloatType_cast(F16); 
_EFloatType_cast(F32); 
_EFloatType_cast(F64); 
_EFloatType_cast(BF16); 
_EFloatType_cast(TF19); 
_EFloatType_cast(PXR24); 
_EFloatType_cast(FP24);

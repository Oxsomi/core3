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

#define _EFloatType(exponentBits, mantissaBits, bytes) (bytes << 16) | (exponentBits << 8) | (mantissaBits << 0)

typedef enum EFloatType {

	EFloatType_Minifloat	= _EFloatType( 4,   3,  1),
	EFloatType_Half			= _EFloatType( 5,  10,  2),
	EFloatType_Single		= _EFloatType( 8,  23,  4),
	EFloatType_Double		= _EFloatType(11,  52,  8),
	//EFloatType_Quadruple	= _EFloatType(15, 112, 16),
	//EFloatType_Octuple	= _EFloatType(19, 236, 32),

	EFloatType_BF16			= _EFloatType( 8,   7,  2),			//BFloat
	EFloatType_TF			= _EFloatType( 8,  10,  4),			//TensorFloat
	EFloatType_PXR24		= _EFloatType( 8,  15,  4),
	EFloatType_FP24			= _EFloatType( 7,  16,  4)

} EFloatType;

U8 EFloatType_bytes(EFloatType type);
U8 EFloatType_exponentBits(EFloatType type);
U8 EFloatType_mantissaBits(EFloatType type);

U64 EFloatType_signShift(EFloatType type);
U64 EFloatType_exponentShift(EFloatType type);
U64 EFloatType_mantissaShift(EFloatType type);

U64 EFloatType_signMask(EFloatType type);			//Not shifted
U64 EFloatType_exponentMask(EFloatType type);		//Already shifted
U64 EFloatType_mantissaMask(EFloatType type);		//Already shifted

bool EFloatType_sign(EFloatType type, U64 v);
U64 EFloatType_abs(EFloatType type, U64 v);
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

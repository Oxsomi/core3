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

#include "types/math.h"
#include <float.h>

#define CONST_IMPL(T, suffix)								\
const T T##_E				= 2.718281828459045##suffix;	\
const T T##_PI				= 3.141592653589793##suffix;	\
const T T##_RAD_TO_DEG		= 57.2957795131##suffix;		\
const T T##_DEG_TO_RAD		= 0.01745329251##suffix;

CONST_IMPL(F32, f);
CONST_IMPL(F64, );

const U64 KIBI			= 1 << 10;
const U64 MIBI			= 1 << 20;
const U64 GIBI			= 1 << 30;
const U64 TIBI			= (U64)1 << 40;
const U64 PEBI			= (U64)1 << 50;

const U64 KILO			= 1'000;
const U64 MEGA			= 1'000'000;
const U64 GIGA			= 1'000'000'000;
const U64 TERA			= 1'000'000'000'000;
const U64 PETA			= 1'000'000'000'000'000;

const Ns MU				= 1'000;
const Ns MS				= 1'000'000;
const Ns SECOND			= 1'000'000'000;
const Ns MIN			= 60'000'000'000;
const Ns HOUR			= 3'600'000'000'000;
const Ns DAY			= 86'400'000'000'000;
const Ns WEEK			= 604'800'000'000'000;

const U8 U8_MIN			= 0;
const U16 U16_MIN		= 0;
const U32 U32_MIN		= 0;
const U64 U64_MIN		= 0;

const I8  I8_MIN		= 0x80;
const C8 C8_MIN			= 0x80;
const I16 I16_MIN		= 0x8000;
const I32 I32_MIN		= 0x80000000;
const I64 I64_MIN		= 0x8000000000000000;

const U8  U8_MAX		= 0xFF;
const U16 U16_MAX		= 0xFFFF;
const U32 U32_MAX		= 0xFFFFFFFF;
const U64 U64_MAX		= 0xFFFFFFFFFFFFFFFF;

const C8  C8_MAX		= 0x7F;
const I8  I8_MAX		= 0x7F;
const I16 I16_MAX		= 0x7FFF;
const I32 I32_MAX		= 0x7FFFFFFF;
const I64 I64_MAX		= 0x7FFFFFFFFFFFFFFF;

const F32 F32_MAX		= FLT_MAX;
const F32 F32_MIN		= -FLT_MAX;

const F64 F64_MAX		= DBL_MAX;
const F64 F64_MIN		= -DBL_MAX;

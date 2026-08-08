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

//types/base/constants.h

#pragma once
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif
		
//Constants

extern const U64 KIBI;
extern const U64 MIBI;
extern const U64 GIBI;
extern const U64 TIBI;
extern const U64 PEBI;

static const U64 KILO = 1000;
static const U64 MEGA = 1000000;
static const U64 GIGA = 1000000000;
static const U64 TERA = 1000000000000;
static const U64 PETA = 1000000000000000;

static const Ns MU = 1000;
static const Ns MS = 1000000;
static const Ns SECOND = 1000000000;
static const Ns MIN = 60000000000;
static const Ns HOUR = 3600000000000;
static const Ns DAY = 86400000000000;
static const Ns WEEK = 604800000000000;

static const U8 U8_MIN = 0;
static const U16 U16_MIN = 0;
static const U32 U24_MIN = 0;
static const U32 U32_MIN = 0;
static const U64 U64_MIN = 0;

static const I8  I8_MIN = -128;
extern const C8  C8_MIN;
static const I16 I16_MIN = -32768;
static const U32 I24_MIN = 0x800000;
static const I32 I32_MIN = 0x80000000;
static const I64 I64_MIN = 0x8000000000000000;

static const U8  U8_MAX = 0xFF;
static const U16 U16_MAX = 0xFFFF;
static const U32 U24_MAX = 0xFFFFFF;
static const U32 U32_MAX = 0xFFFFFFFF;
static const U64 U64_MAX = 0xFFFFFFFFFFFFFFFF;

static const C8  C8_MAX = 0x7F;
static const I8  I8_MAX = 0x7F;
static const I16 I16_MAX = 0x7FFF;
static const U32 I24_MAX = 0x7FFFFF;
static const I32 I32_MAX = 0x7FFFFFFF;
static const I64 I64_MAX = 0x7FFFFFFFFFFFFFFF;

extern const F32 F32_MIN;
extern const F32 F32_MAX;

extern const F64 F64_MIN;
extern const F64 F64_MAX;

#ifdef __cplusplus
	}
#endif

/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

extern const U64 KILO;
extern const U64 MEGA;
extern const U64 GIGA;
extern const U64 TERA;
extern const U64 PETA;

extern const Ns MU;
extern const Ns MS;
extern const Ns SECOND;
extern const Ns MIN;
extern const Ns HOUR;
extern const Ns DAY;
extern const Ns WEEK;

extern const U8 U8_MIN;
extern const C8 C8_MIN;
extern const U16 U16_MIN;
extern const U32 U24_MIN;
extern const U32 U32_MIN;
extern const U64 U64_MIN;

extern const I8  I8_MIN;
extern const I16 I16_MIN;
extern const U32 I24_MIN;
extern const I32 I32_MIN;
extern const I64 I64_MIN;

extern const U8  U8_MAX;
extern const C8  C8_MAX;
extern const U16 U16_MAX;
extern const U32 U24_MAX;
extern const U32 U32_MAX;
extern const U64 U64_MAX;

extern const I8  I8_MAX;
extern const I16 I16_MAX;
extern const U32 I24_MAX;
extern const I32 I32_MAX;
extern const I64 I64_MAX;

extern const F32 F32_MIN;
extern const F32 F32_MAX;

extern const F64 F64_MIN;
extern const F64 F64_MAX;

#ifdef __cplusplus
	}
#endif

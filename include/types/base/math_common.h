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

//types/base/math_common.h

#pragma once
#include "types/base/types.h"

#define ARIT_OP(T)                                                                                    \
static inline T T##_min(T v0, T v1) { return v0 <= v1 ? v0 : v1; }                                    \
static inline T T##_max(T v0, T v1) { return v0 >= v1 ? v0 : v1; }                                    \
static inline T T##_clamp(T v, T mi, T ma) { return T##_max(mi, T##_min(ma, v)); }                    \
static inline T T##_pow2(T v) { return v * v; }

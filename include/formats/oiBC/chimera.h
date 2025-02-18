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
#include "fidi_a.h"
#include "types/math/flp.h"
#include "types/math/vec.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Chimera {

	union {
		F32x4 v4f[8], vf[8];
		F32x2 v2f[8];
	};

	union {
		I32x4 v4i[8], vi[8];
		I32x2 v2i[8];
	};

	union {
		F64 d[8];
		F32 f[8];
		F16 h[8];
	};

	union {

		I64 l[8];
		I32 i[8];
		I16 s[8];
		I8 b[8];

		U64 u[8];
		U32 ui[8];
		U16 us[8];
		U8 ub[8];
	};

	//Such as &3 = comparison 0: eq, 1: lt, 2: gt
	//constantOffset = (>>2) & 0xFF
	//constantCursor (>>10) & 3
	U64 effects;

	U64 padding;

} Chimera;

void Chimera_stepFidiA(Chimera *chim, const EFidiA op);
ECompareResult Chimera_getLastCompare(const Chimera *chim);

#ifdef __cplusplus
	}
#endif

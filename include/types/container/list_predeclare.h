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

//types/container/list_predeclare.h

#pragma once
#include "types/container/generic_list_predeclare.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Buffer Buffer;
typedef struct CharString CharString;

TListDefinition(U8, ListU8); TListDefinition(U16, ListU16); TListDefinition(U32, ListU32);
TListDefinition(I8, ListI8); TListDefinition(I16, ListI16); TListDefinition(I32, ListI32); TListDefinition(I64, ListI64);
TListDefinition(F32, ListF32); TListDefinition(F64, ListF64);

TListDefinition(Buffer, ListBuffer);

TListDefinition(ListU8, ListListU8);
TListDefinition(ListU16, ListListU16);
TListDefinition(ListU32, ListListU32);
TListDefinition(ListU64, ListListU64);

#ifdef __cplusplus
	}
#endif

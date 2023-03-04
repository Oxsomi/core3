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

typedef struct Error Error;
typedef struct List List;

Error List_createx(U64 length, U64 stride, List *result);
Error List_createRepeatedx(U64 length, U64 stride, Buffer data, List *result);
Error List_createCopyx(List list, List *result);

Error List_createSubsetReversex(List list, U64 index, U64 length, List *result);
Error List_createReversex(List list, List *result);

Error List_findx(List list, Buffer buf, List *result);

Error List_eraseAllx(List *list, Buffer buf);
Error List_insertx(List *list, U64 index, Buffer buf);
Error List_pushAllx(List *list, List other);
Error List_insertAllx(List *list, List other, U64 offset);

Error List_reservex(List *list, U64 capacity);
Error List_resizex(List *list, U64 size);
Error List_shrinkToFitx(List *list);

Error List_pushBackx(List *list, Buffer buf);

Bool List_freex(List *result);

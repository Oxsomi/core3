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

//types/base/string_base.c

#include "types/base/string_base.h"
#include "types/base/algorithm.h"

CharString CharString_createRefAutoConst(const C8 *ptr, U64 maxSize) {

	if (!ptr)
		return CharString_createNull();

	const U64 strl = CharString_calcStrLen(ptr, maxSize);

	return (CharString) {
		.lenAndNullTerminated = strl | ((U64)(strl != maxSize) << 63),
		.ptr = ptr,
		.capacityAndRefInfo = U64_MAX        //Flag as const
	};
}

CharString CharString_createRefSizedConst(const C8 *ptr, U64 size, Bool isNullTerminated) {

	if (!ptr || (size >> 48))
		return CharString_createNull();

	if (isNullTerminated && ptr[size])    //Invalid!
		return CharString_createNull();

	if (!isNullTerminated && size) {

		isNullTerminated = !ptr[size - 1];

		if (isNullTerminated)
			--size;
	}

	return (CharString) {
		.lenAndNullTerminated = size | ((U64)isNullTerminated << 63),
		.ptr = ptr,
		.capacityAndRefInfo = U64_MAX        //Flag as const
	};
}

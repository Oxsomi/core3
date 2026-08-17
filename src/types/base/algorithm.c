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

//types/base/algorithm.c

#include "types/base/algorithm.h"

//See CompareInvoke in the header for why a comparator from another language is reached through here.

ECompareResult CompareWrapper_compare(const void *aPtr, const void *bPtr, void *context) {

	const CompareWrapper *wrapper = (const CompareWrapper*) context;

	if(!wrapper || !wrapper->invoke)
		return ECompareResult_Eq;

	const I8 res = wrapper->invoke(aPtr, bPtr, wrapper->context);

	//Compared rather than cast: an out of range result would otherwise become an invalid enum value.

	return res < 0 ? ECompareResult_Lt : (res > 0 ? ECompareResult_Gt : ECompareResult_Eq);
}

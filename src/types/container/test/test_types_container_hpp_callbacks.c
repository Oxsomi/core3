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

//types/container/test/test_types_container_hpp_callbacks.c

//Callbacks the hpp wrapper test hands to the C containers, kept in C on purpose.
//test_types_container_hpp.cpp includes the C headers inside namespace oxc::c, so anything it defines returns
// oxc::c::ECompareResult, which is a different type to the C sort that ends up calling it even though the two
// are identical at the ABI level.
//-fsanitize=function compares the callee's recorded signature against the call site's and reports exactly
// that mismatch as a call through an incorrect function type, so the definition lives here where the enum is
// the one the caller expects; the C++ side only takes its address.

#include "types/base/algorithm.h"
#include "types/base/types.h"

ECompareResult cmpU32Desc(const void *a, const void *b) {
	const U32 x = *(const U32*) a, y = *(const U32*) b;
	return x > y ? ECompareResult_Lt : (x < y ? ECompareResult_Gt : ECompareResult_Eq);
}

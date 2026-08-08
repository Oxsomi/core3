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

//types/container/test/stream_harness.h

#pragma once
#include "types/base/types.h"
#include "types/base/string_base.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct RefPtr RefPtr;
typedef struct RefPtrType RefPtrType;
typedef struct Test Test;

//Extra abstraction over OxStream that allows us to easily unit test different types of streams.
typedef struct StreamHarness {

	//Create a writable stream of exactly 'size' logical bytes.
	//isResizable controls whether writes past size are allowed.
	Bool (*create)(
		const struct StreamHarness *h,
		U64    size,
		Bool   isResizable,
		RefPtr **out,
		Test   *t
	);

	const RefPtrType *type;

	ShortString name;

	LongString currModule;

} StreamHarness;

void Test_setModuleH(Test *t, StreamHarness *h, const C8 *str);    //str = ShortString (limit of 31)

void StreamHarness_testStream(StreamHarness *h, Test *t);
void StreamHarness_testCursor(StreamHarness *h, Test *t);

#ifdef __cplusplus
	}
#endif

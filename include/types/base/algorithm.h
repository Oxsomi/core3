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

//types/base/algorithm.h

#pragma once
#include "types/base/types.h"

typedef enum ECompareResult {
	ECompareResult_Lt,
	ECompareResult_Eq,
	ECompareResult_Gt
} ECompareResult;

//context is whatever was handed to the sort and is passed through untouched, so a comparator can carry state
// (an external key table, a locale, a language wrapper's own callable) instead of reaching for a global.
//Comparators that don't need it take it and ignore it.

typedef ECompareResult (*CompareFunction)(const void *aPtr, const void *bPtr, void *context);
typedef Bool (*EqualsFunction)(const void *aPtr, const void *bPtr);        //Passing NULL as func indicates raw buffer compare
typedef U64 (*HashFunction)(const void *aPtr);                            //Passing NULL as func indicates raw buffer hash

//A comparator coming from another language, whose signature mentions no struct or enum type on purpose.
//Such a comparator is compiled in its own language, where a type like ECompareResult is only reachable under
// a namespace and is therefore a DIFFERENT type to the C sort that ends up calling it, which
// -fsanitize=function reports as a call through an incorrect function type (see JobInvoke in job_queue.h for
// the same reasoning on the job queue side).
//Returns the usual three way answer: negative if a sorts before b, 0 if they tie, positive if after.

typedef I8 (*CompareInvoke)(const void *aPtr, const void *bPtr, void *context);

//Hand one of these to a sort as its context, together with CompareWrapper_compare as its CompareFunction.

typedef struct CompareWrapper {
	CompareInvoke invoke;
	void *context;              //Passed on to invoke; the wrapper's own payload rather than this struct
} CompareWrapper;

#ifdef __cplusplus
	extern "C" {
#endif

//A CompareFunction that forwards to the CompareInvoke of the CompareWrapper it receives as context.
//Deliberately a real function rather than static inline: a C++ caller has to end up calling the copy that
// was compiled as C, which is the entire point of routing through it.

ECompareResult CompareWrapper_compare(const void *aPtr, const void *bPtr, void *context);

#ifdef __cplusplus
	}
#endif

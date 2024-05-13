/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "list.h"

#ifdef __cplusplus
	extern "C" {
#endif

//A cdf (Cumulative Distribution Function) is essentially an efficient way to map a probability to an element or index.
//It allows appending or dealing with an entire cdf manually (with threading).
//A cdf can also be standalone, meaning there's no underlying struct it points too.
//It still maintains an id, and this allows it to be used however the user sees fit.
//For example; it can be remapped by the user to their own list so it doesn't consume any extra memory.

typedef enum ECdfListFlags {
	ECdfListFlags_None				= 0,
	ECdfListFlags_IsFinalized		= 1 << 0			//If the total is known
} ECdfListFlags;

typedef struct CdfValue {
	F32 self, predecessors;
} CdfValue;

TList(CdfValue);

typedef struct CdfList {
	ListCdfValue cdf;
	GenericList elements;
	F32 total;
	ECdfListFlags flags;
	U64 totalElements;
} CdfList;

//Create a CdfList. If isReserved is true, the maxElements are inaccessible until they're appended.
//If it's false, they're already accessible but a probability has to be set manually through CdfList_setProbability.

Error CdfList_create(
	U64 maxElements,
	Bool isReserved,
	U64 elementSize,
	Allocator allocator,
	CdfList *result
);

//Create a subset from a pre-allocated list.
//For example;	Creating a CDF of a row of a linear texture.
//				And then building a CDF out of that to get a 2D CDF.
//This doesn't allow for any pushing/popping afterward.
//Keep in mind that since the probabilities aren't available, you'd have to CdfList_setProbability.

Error CdfList_createSubset(
	GenericList preAllocated,
	U64 elementsOffset,
	U64 elementCount,
	Allocator allocator,
	CdfList *result
);

Bool CdfList_free(CdfList *list, Allocator allocator);

//Setting probability and/or element.

Bool CdfList_setProbability(CdfList *list, U64 i, F32 value, F32 *oldValue);
Bool CdfList_setElement(CdfList *list, U64 i, Buffer element);

Bool CdfList_set(CdfList *list, U64 i, F32 value, Buffer element);

//Pushing/popping is only available if the CFDList is linear.
//It allows pushing/popping back without breaking the CDF (push & pop at other locations will require a finalize though).
//Shouldn't be used when multi-threading.

Error CdfList_pushBack(CdfList *list, F32 value, Buffer element, Allocator allocator);
Error CdfList_pushFront(CdfList *list, F32 value, Buffer element, Allocator allocator);
Error CdfList_pushIndex(CdfList *list, U64 i, F32 value, Buffer element, Allocator allocator);

Error CdfList_popFront(CdfList *list, Buffer elementValue);
Error CdfList_popBack(CdfList *list, Buffer elementValue);
Error CdfList_popIndex(CdfList *list, U64 i, Buffer elementValue);

//Should be called if the total was ignored by set/setProbability (for multi threading reasons).

Error CdfList_finalize(CdfList *list);

//Finding a CDF by value.
//Secure means it uses CSPRNG to generate a random number instead of PRNG.
//These functions require a CdfList to be finalized.

typedef struct CdfListElement {
	Buffer value;
	U64 id;
	F32 chance;
} CdfListElement;

Error CdfList_getRandomElementSecure(CdfList *list, CdfListElement *elementValue);
Error CdfList_getRandomElementFast(CdfList *list, CdfListElement *elementValue, U32 *seed);
Error CdfList_getElementAtOffset(CdfList *list, F32 offset, CdfListElement *elementValue);

#ifdef __cplusplus
	}
#endif

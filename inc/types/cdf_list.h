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
#include "list.h"

//A cdf (Cumulative Distribution Function) is essentially an efficient way to map a probability to an element or index.
//It allows appending or dealing with an entire cdf manually (with threading).
//A cdf can also be elementless, meaning there's no underlying struct it points too.
//It still maintains an id, and this allows it to be used however the user sees fit.
//For example; it can be remapped by the user to their own list so it doesn't consume any extra memory.

typedef enum ECDFListFlags {
	ECDFListFlags_None				= 0,
	ECDFListFlags_IsFinalized		= 1 << 0			//If the total is known
} ECDFListFlags;

typedef struct CDFList {
	List cdf, elements;
	F32 total;
	U8 flags;
	U64 totalElements;
} CDFList;

//Create a CDFList. If isReserved is true, the maxElements are inacessible until they're appended.
//If it's false, they're already accessible but a probability has to be set manually through CDFList_setProbability.

Error CDFList_create(
	U64 maxElements,
	Bool isReserved,
	U64 elementSize,
	Allocator allocator,
	CDFList *result
);

//Create a subset from a preallocated list.
//For example;	Creating a CDF of a row of a linear texture.
//				And then building a CDF out of that to get a 2D CDF.
//This doesn't allow for any pushing/popping afterwards.
//Keep in mind that since the probabilities aren't available, you'd have to CDFList_setProbability.

Error CDFList_createSubset(
	List preallocated,
	U64 elementsOffset,
	U64 elementCount,
	Allocator allocator,
	CDFList *result
);

//

Bool CDFList_free(CDFList *list, Allocator allocator);

//Setting probability and/or element.
//updateTotal should be false if the total calculation process is multi-threaded, as this will cause threading issues.

Bool CDFList_setProbability(CDFList *list, U64 i, F32 value, Bool updateTotal, F32 *oldValue);
Bool CDFList_setElement(CDFList *list, U64 i, Buffer element);

Bool CDFList_set(CDFList *list, U64 i, F32 value, Buffer element, Bool updateTotal);

//Pushing/popping is only available if the CFDList is linear.
//It allows pushing/popping back without breaking the CDF (push & pop at other locations will require a finalize though).
//Shouldn't be used when multi-threading.

Error CDFList_pushBack(CDFList *list, F32 value, Buffer element, Allocator allocator);
Error CDFList_pushFront(CDFList *list, F32 value, Buffer element, Allocator allocator);
Error CDFList_pushIndex(CDFList *list, U64 i, F32 value, Buffer element, Allocator allocator);

Error CDFList_popFront(CDFList *list, Buffer elementValue);
Error CDFList_popBack(CDFList *list, Buffer elementValue);
Error CDFList_popIndex(CDFList *list, U64 i, Buffer elementValue);

//Should be called if the total was ignored by set/setProbability (for multi threading reasons).

Error CDFList_finalize(CDFList *list);

//Finding a CDF by value.
//Secure means it uses CSPRNG to generate a random number instead of PRNG.
//These functions require a CDFList to be finalized.

typedef struct CDFListElement {
	Buffer value;
	U64 id;
	F32 chance;
} CDFListElement;

Error CDFList_getRandomElement(CDFList *list, CDFListElement *elementValue);
Error CDFList_getRandomElementFast(CDFList *list, CDFListElement *elementValue, U32 *seed);
Error CDFList_getElementAtOffset(CDFList *list, F32 offset, CDFListElement *elementValue);

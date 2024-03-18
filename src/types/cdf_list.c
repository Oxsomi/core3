/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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

#include "types/list_impl.h"
#include "types/cdf_list.h"
#include "types/allocator.h"
#include "types/rand.h"

TListImpl(CdfValue);

Error CdfList_create(
	U64 maxElements,
	Bool isReserved,
	U64 elementSize,
	Allocator allocator,
	CdfList *result
) {

	if(!result)
		return Error_nullPointer(4, "CdfList_create()::result is required");

	if(result->cdf.ptr)
		return Error_invalidOperation(0, "CdfList_create()::result wasn't empty, indicating a possible memleak");

	Error err = Error_none();

	result->flags |= ECdfListFlags_IsFinalized;

	if(maxElements) {

		if(isReserved)
			gotoIfError(clean, ListCdfValue_reserve(&result->cdf, maxElements, allocator))

		else {
			gotoIfError(clean, ListCdfValue_resize(&result->cdf, maxElements, allocator));
			result->totalElements = maxElements;
		}
	}

	if (elementSize) {

		result->elements = GenericList_createEmpty(elementSize);

		if(maxElements) {

			if(isReserved)
				gotoIfError(clean, GenericList_reserve(&result->elements, maxElements, allocator))

			else gotoIfError(clean, GenericList_resize(&result->elements, maxElements, allocator))
		}
	}

	return Error_none();

clean:

	ListCdfValue_free(&result->cdf, allocator);
	GenericList_free(&result->elements, allocator);
	*result = (CdfList) { 0 };
	return err;
}

Error CdfList_createSubset(
	GenericList preallocated,
	U64 elementsOffset,
	U64 elementCount,
	Allocator allocator,
	CdfList *result
) {

	if(!result)
		return Error_nullPointer(4, "CdfList_createSubset()::result is required");

	if(result->cdf.ptr)
		return Error_invalidOperation(0, "CdfList_createSubset()::result isn't empty, this could indicate possible memleak");

	if(!preallocated.stride || !elementCount)
		return Error_invalidOperation(1, "CdfList_createSubset()::elementCount or preallocated.stride can't be 0");

	Error err = GenericList_createSubset(preallocated, elementsOffset, elementCount, &result->elements);

	if(err.genericError)
		return err;

	result->flags |= ECdfListFlags_IsFinalized;

	gotoIfError(clean, ListCdfValue_resize(&result->cdf, elementCount, allocator));
	result->totalElements = elementCount;

	return Error_none();

clean:
	*result = (CdfList) { 0 };
	return err;
}

Bool CdfList_free(CdfList *list, Allocator allocator) {

	if(!list || !list->cdf.ptr)
		return true;

	Bool success = ListCdfValue_free(&list->cdf, allocator);
	success &= GenericList_free(&list->elements, allocator);

	*list = (CdfList) { 0 };
	return success;
}

U64 CDFList_getLinearIndex(CdfList *list, U64 i) {			//We might override this later if we want to swizzle it around
	(void)list;
	return i;
}

Bool CdfList_setProbability(CdfList *list, U64 i, F32 value, F32 *oldValue) {

	if(!list || i >= list->totalElements || value < 0)
		return false;

	const U64 j = CDFList_getLinearIndex(list, i);

	CdfValue *f = list->cdf.ptrNonConst;
	const F32 v = f[j].self;

	if(v == value)
		return true;

	if(oldValue)
		*oldValue = v;

	list->flags &= ~ECdfListFlags_IsFinalized;
	f[j].self = value;

	return true;

}

Bool CdfList_setElement(CdfList *list, U64 i, Buffer element) {
	return list && !GenericList_isConstRef(list->elements)  && !GenericList_set(list->elements, i, element).genericError;
}

Bool CdfList_set(CdfList *list, U64 i, F32 value, Buffer element) {

	if(
		!list || (
			(
				Buffer_length(element) != list->elements.stride &&
				!(GenericList_isConstRef(list->elements) && !Buffer_length(element))
			) || (
				GenericList_isConstRef(list->elements) && Buffer_length(element)
			)
		)
	)
		return false;

	F32 oldValue;

	if(!CdfList_setProbability(list, i, value, &oldValue))
		return false;

	if(list && !list->elements.length)		//If elements aren't available
		return true;

	if(GenericList_isConstRef(list->elements)) {

		const Bool b = CdfList_setElement(list, i, element);

		if (!b) {
			CdfList_setProbability(list, i, oldValue, NULL);
			return false;
		}
	}

	return true;
}

Error CdfList_pushIndex(CdfList *list, U64 i, F32 value, Buffer element, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0, "CdfList_pushIndex()::list is required");

	if(GenericList_isRef(list->elements))
		return Error_invalidOperation(0, "CdfList_pushIndex()::list can't be a ref");

	if(value < 0)
		return Error_invalidParameter(2, 0, "CdfList_pushIndex()::value can't be <0");

	if(Buffer_length(element) != list->elements.stride)
		return Error_invalidParameter(3, 0, "CdfList_pushIndex()::element can't mismatch list->elements.stride");

	const Bool isLast = list->totalElements == i;

	Error err = ListCdfValue_insert(&list->cdf, i, (CdfValue) { value, 0 }, allocator);

	if(err.genericError)
		return err;

	if (list->elements.stride && (err = GenericList_insert(&list->elements, i, element, allocator)).genericError) {
		ListCdfValue_popLocation(&list->cdf, i, NULL);
		return err;
	}

	++list->totalElements;
	list->total += value;

	if(!isLast)
		list->flags &= ~ECdfListFlags_IsFinalized;		//If it's inserted anywhere else, it marked other parts dirty

	return Error_none();
}

Error CdfList_popIndex(CdfList *list, U64 i, Buffer element) {

	if(!list)
		return Error_nullPointer(0, "CdfList_popIndex()::list is required");

	if(GenericList_isRef(list->elements))
		return Error_invalidOperation(0, "CdfList_popIndex()::list can't be a ref");

	if(!list->totalElements)
		return Error_invalidOperation(1, "CdfList_popIndex()::list can't be empty");

	if(Buffer_length(element) && Buffer_length(element) != list->elements.stride)
		return Error_invalidParameter(2, 0, "CdfList_popIndex()::element can't mismatch list->elements.stride");

	CdfValue prevProbability = (CdfValue) { 0 };
	const Bool isLast = (list->totalElements - 1) == i;

	Error err = ListCdfValue_popLocation(&list->cdf, i, &prevProbability);

	if(err.genericError)
		return err;

	if (list->elements.stride && (err = GenericList_popLocation(&list->elements, i, element)).genericError) {
		ListCdfValue_insert(&list->cdf, i, prevProbability, (Allocator) { 0 });
		return err;
	}

	--list->totalElements;
	list->total -= prevProbability.self;

	if(!isLast)
		list->flags &= ~ECdfListFlags_IsFinalized;		//If it's popped anywhere else, it marked other parts dirty

	return Error_none();
}

Error CdfList_pushBack(CdfList *list, F32 value, Buffer element, Allocator allocator) {
	return CdfList_pushIndex(list, list ? list->totalElements : 0, value, element, allocator);
}

Error CdfList_pushFront(CdfList *list, F32 value, Buffer element, Allocator allocator) {
	return CdfList_pushIndex(list, 0, value, element, allocator);
}

Error CdfList_popFront(CdfList *list, Buffer elementValue) {
	return CdfList_popIndex(list, 0, elementValue);
}

Error CdfList_popBack(CdfList *list, Buffer elementValue) {
	return CdfList_popIndex(list, list && list->totalElements ? list->totalElements - 1 : 0, elementValue);
}

Error CdfList_finalize(CdfList *list) {

	if(!list || !list->totalElements)
		return Error_nullPointer(0, "CdfList_finalize()::list is required and should have at least 1 element");

	if(list->flags & ECdfListFlags_IsFinalized)
		return Error_none();

	CdfValue *f = list->cdf.ptrNonConst;

	F32 total = 0;

	for(U64 i = 0, j = list->totalElements; i < j; ++i) {
		f[CDFList_getLinearIndex(list, i)].predecessors = total;
		total += f[CDFList_getLinearIndex(list, i)].self;
	}

	list->total = total;
	list->flags |= ECdfListFlags_IsFinalized;
	return Error_none();
}

Error CdfList_getRandomElementSecure(CdfList *list, CdfListElement *elementValue) {

	if(!list || !elementValue)
		return Error_nullPointer(
			elementValue ? 1 : 0, "CdfList_getRandomElementSecure()::list and elementValue are required"
		);

	U32 v;
	const Buffer b = Buffer_createRef(&v, sizeof(v));

	if(!Buffer_csprng(b))
		return Error_invalidOperation(0, "");

	const F32 f = (v << 8 >> 8) / 16777215.f * list->total;
	return CdfList_getElementAtOffset(list, f, elementValue);
}

Error CdfList_getRandomElementFast(CdfList *list, CdfListElement *elementValue, U32 *seed) {

	if(!seed)
		return Error_nullPointer(2, "CdfList_getRandomElementFast()::seed is required");

	return CdfList_getElementAtOffset(list, Random_sample(seed), elementValue);
}

Error CdfList_getElementAtOffset(CdfList *list, F32 offset, CdfListElement *elementValue) {

	if(!list || !elementValue)
		return Error_nullPointer(list ? 2 : 0, "CdfList_getElementAtOffset()::list and elementValue are required");

	if(offset < 0 || offset >= list->total)
		return Error_outOfBounds(
			1, *(const U32*)&offset, *(const U32*)&list->total,
			"CdfList_getElementAtOffset()::offset is out of bounds"
		);

	if(!(list->flags & ECdfListFlags_IsFinalized))
		return Error_invalidOperation(0, "CdfList_getElementAtOffset()::list isn't finalized");

	if(!list->totalElements || !list->total)
		return Error_invalidOperation(1, "CdfList_getElementAtOffset()::list doesn't have any elements");

	U64 startRegion = 0, endRegion = list->totalElements;
	U64 j = (startRegion + endRegion) / 2;
	const CdfValue *f = list->cdf.ptrNonConst;

	for (U64 i = 0; i < 64; ++i) {

		const CdfValue curr = f[j];

		if (offset >= curr.predecessors && offset < curr.predecessors + curr.self) {

			*elementValue = (CdfListElement) { .chance = curr.self / list->total, .id = j };

			if(list->elements.length)
				elementValue->value = Buffer_createRefConst(GenericList_ptrConst(list->elements, j), list->elements.stride);

			return Error_none();
		}

		if (offset < curr.predecessors)
			endRegion = j;

		if (offset >= curr.predecessors + curr.self)
			startRegion = j + 1;

		const U64 oldJ = j;
		j = (startRegion + endRegion) / 2;

		//Should never happen, it should always find a match before this

		if(j == oldJ || j >= list->totalElements)
			return Error_notFound(0, 0, "CdfList_getElementAtOffset() couldn't find value");
	}

	return Error_none();
}

/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
*/

#include "types/cdf_list.h"
#include "types/allocator.h"
#include "types/error.h"
#include "types/buffer.h"
#include "math/rand.h"

typedef struct CDFValue {
	F32 self, predecessors;
} CDFValue;

Error CDFList_create(
	U64 maxElements, 
	Bool isReserved,
	U64 elementSize,
	Allocator allocator,
	CDFList *result
) {

	if(!result)
		return Error_nullPointer(4, 0);

	if(result->cdf.ptr)
		return Error_invalidOperation(0);

	Error err = Error_none();

	result->flags |= ECDFListFlags_IsFinalized;

	if(maxElements) {

		result->cdf = List_createEmpty(sizeof(CDFValue));

		if(isReserved) {
			_gotoIfError(clean, List_reserve(&result->cdf, maxElements, allocator));
		}
		
		else {
			_gotoIfError(clean, List_resize(&result->cdf, maxElements, allocator));
			result->totalElements = maxElements;
		}
	}

	if (elementSize) {

		result->elements = List_createEmpty(elementSize);

		if(maxElements) {

			if(isReserved) {
				_gotoIfError(clean, List_reserve(&result->elements, maxElements, allocator));
			}

			else {
				_gotoIfError(clean, List_resize(&result->elements, maxElements, allocator));
			}
		}
	}

	return Error_none();

clean:
	
	List_free(&result->cdf, allocator);
	List_free(&result->elements, allocator);
	*result = (CDFList) { 0 };
	return err;
}

Error CDFList_createSubset(
	List preallocated,
	U64 elementsOffset,
	U64 elementCount,
	Allocator allocator,
	CDFList *result
) {

	if(!result)
		return Error_nullPointer(4, 0);

	if(result->cdf.ptr)
		return Error_invalidOperation(0);

	if(!preallocated.stride || !elementCount)
		return Error_invalidOperation(1);

	Error err = List_createSubset(preallocated, elementsOffset, elementCount, &result->elements);

	if(err.genericError)
		return err;

	result->flags |= ECDFListFlags_IsFinalized;

	result->cdf = List_createEmpty(sizeof(CDFValue));

	_gotoIfError(clean, List_resize(&result->cdf, elementCount, allocator));
	result->totalElements = elementCount;

	return Error_none();

clean:
	*result = (CDFList) { 0 };
	return err;
}

Bool CDFList_free(CDFList *list, Allocator allocator) {

	if(!list || !list->cdf.ptr)
		return true;

	Bool success = List_free(&list->cdf, allocator);
	success &= List_free(&list->elements, allocator);

	*list = (CDFList) { 0 };
	return success;
}

U64 CDFList_getLinearIndex(CDFList *list, U64 i) {			//We might override this later if we want to swizzle it around
	list;
	return i;
}

Bool CDFList_setProbability(CDFList *list, U64 i, F32 value, Bool updateTotal, F32 *oldValue) {

	if(!list || i >= list->totalElements || value < 0)
		return false;

	U64 j = CDFList_getLinearIndex(list, i);

	CDFValue *f = (CDFValue*) list->cdf.ptr;
	F32 v = f[j].self;

	if(v == value)
		return true;

	if(oldValue)
		*oldValue = v;

	if(!updateTotal || i != list->totalElements - 1)
		list->flags &= ~ECDFListFlags_IsFinalized;

	f[j].self = value;

	if(updateTotal && (list->flags & ECDFListFlags_IsFinalized)) {
		f[j].predecessors = list->total - v;
		list->total = f[j].predecessors + value;
	}
		
	return true;

}

Bool CDFList_setElement(CDFList *list, U64 i, Buffer element) {
	return list && !List_isConstRef(list->elements)  && !List_set(list->elements, i, element).genericError;
}

Bool CDFList_set(CDFList *list, U64 i, F32 value, Buffer element, Bool updateTotal) {

	if(
		!list || (
			list && ((
				Buffer_length(element) != list->elements.stride && 
				!(List_isConstRef(list->elements) && !Buffer_length(element))
			) || (
				List_isConstRef(list->elements) && Buffer_length(element)
			))
		)
	)
		return false;

	F32 oldValue;

	if(!CDFList_setProbability(list, i, value, updateTotal, &oldValue))
		return false;

	if(list && !list->elements.length)		//If elements aren't available
		return true;

	if(List_isConstRef(list->elements)) {

		Bool b = CDFList_setElement(list, i, element);

		if (!b) {
			CDFList_setProbability(list, i, oldValue, updateTotal, NULL);
			return false;
		}
	}
	
	return true;
}

Error CDFList_pushIndex(CDFList *list, U64 i, F32 value, Buffer element, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0, 0);

	if(List_isRef(list->elements))
		return Error_invalidOperation(0);

	if(value < 0)
		return Error_invalidParameter(2, 0, 0);

	if(Buffer_length(element) != list->elements.stride)
		return Error_invalidParameter(3, 0, 0);

	Bool isLast = list->totalElements == i;

	Error err = List_insert(&list->cdf, i, Buffer_createConstRef(&value, sizeof(value)), allocator);

	if(err.genericError)
		return err;

	if (list->elements.stride && (err = List_insert(&list->elements, i, element, allocator)).genericError) {
		List_popLocation(&list->cdf, i, Buffer_createNull());
		return err;
	}

	++list->totalElements;
	list->total += value;

	if(!isLast)
		list->flags &= ~ECDFListFlags_IsFinalized;		//If it's inserted anywhere else, it marked other parts dirty

	return Error_none();
}

Error CDFList_popIndex(CDFList *list, U64 i, Buffer element) {

	if(!list)
		return Error_nullPointer(0, 0);

	if(List_isRef(list->elements))
		return Error_invalidOperation(0);

	if(!list->totalElements)
		return Error_invalidOperation(1);

	if(Buffer_length(element) != list->elements.stride)
		return Error_invalidParameter(2, 0, 0);

	CDFValue prevProbability = (CDFValue) { 0 };
	Bool isLast = (list->totalElements - 1) == i;

	Error err = List_popLocation(&list->cdf, i, Buffer_createRef(&prevProbability, sizeof(prevProbability)));

	if(err.genericError)
		return err;

	if (list->elements.stride && (err = List_popLocation(&list->elements, i, element)).genericError) {
		List_insert(&list->cdf, i, Buffer_createConstRef(&prevProbability, sizeof(prevProbability)), (Allocator) { 0 });
		return err;
	}

	--list->totalElements;
	list->total -= prevProbability.self;

	if(!isLast)
		list->flags &= ~ECDFListFlags_IsFinalized;		//If it's popped anywhere else, it marked other parts dirty

	return Error_none();
}

Error CDFList_pushBack(CDFList *list, F32 value, Buffer element, Allocator allocator) {
	return CDFList_pushIndex(list, list ? list->totalElements : 0, value, element, allocator);
}

Error CDFList_pushFront(CDFList *list, F32 value, Buffer element, Allocator allocator) {
	return CDFList_pushIndex(list, 0, value, element, allocator);
}

Error CDFList_popFront(CDFList *list, Buffer elementValue) {
	return CDFList_popIndex(list, 0, elementValue);
}

Error CDFList_popBack(CDFList *list, Buffer elementValue) {
	return CDFList_popIndex(list, list && list->totalElements ? list->totalElements - 1 : 0, elementValue);
}

Error CDFList_finalize(CDFList *list) {

	if(!list || !list->totalElements)
		return Error_nullPointer(0, 0);

	if(list->flags & ECDFListFlags_IsFinalized)
		return Error_none();

	CDFValue *f = (CDFValue*) list->cdf.ptr;

	F32 total = 0;

	for(U64 i = 0, j = list->totalElements; i < j; ++i) {
		f[CDFList_getLinearIndex(list, i)].predecessors = total;
		total += f[CDFList_getLinearIndex(list, i)].self;
	}

	list->total = total;
	list->flags |= ECDFListFlags_IsFinalized;
	return Error_none();
}

Error CDFList_getRandomElement(CDFList *list, CDFListElement *elementValue) {

	if(!list || !elementValue)
		return Error_nullPointer(elementValue ? 1 : 0, 0);

	U32 v;
	Buffer b = Buffer_createRef(&v, sizeof(v));

	if(!Buffer_csprng(b))
		return Error_invalidOperation(0);

	F32 f = (v << 8 >> 8) / 16'777'215.f * list->total;
	return CDFList_getElementAtOffset(list, f, elementValue);
}

Error CDFList_getRandomElementFast(CDFList *list, CDFListElement *elementValue, U32 *seed) {

	if(!seed)
		return Error_nullPointer(2, 0);

	return CDFList_getElementAtOffset(list, Random_sample(seed), elementValue);
}

Error CDFList_getElementAtOffset(CDFList *list, F32 randomValue, CDFListElement *elementValue) {

	if(!list || !elementValue)
		return Error_nullPointer(list ? 2 : 0, 0);

	if(randomValue < 0 || randomValue >= list->total)
		return Error_outOfBounds(1, 0, *(const U32*)&randomValue, *(const U32*)&list->total);

	if(!(list->flags & ECDFListFlags_IsFinalized))
		return Error_invalidOperation(0);

	if(!list->totalElements || !list->total)
		return Error_invalidOperation(1);

	U64 startRegion = 0, endRegion = list->totalElements;
	U64 j = (startRegion + endRegion) / 2;
	CDFValue *f = (CDFValue*) list->cdf.ptr;

	for (U64 i = 0; i < 64; ++i) {

		CDFValue curr = f[j];

		if (randomValue >= curr.predecessors && randomValue < curr.predecessors + curr.self) {

			*elementValue = (CDFListElement) { .chance = curr.self / list->total, .id = j };

			if(list->elements.length)
				elementValue->value = Buffer_createConstRef(List_ptrConst(list->elements, j), list->elements.stride);

			return Error_none();
		}

		if (randomValue < curr.predecessors)
			endRegion = j;

		if (randomValue >= curr.predecessors + curr.self)
			startRegion = j + 1;

		U64 oldJ = j;
		j = (startRegion + endRegion) / 2;

		if(j == oldJ || j >= list->totalElements)
			return Error_notFound(0, 0, 0);		//Should never happen, it should always find a match before this
	}

	return Error_none();
}

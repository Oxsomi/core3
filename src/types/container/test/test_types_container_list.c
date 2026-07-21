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

//types/container/test/test_types_container_list.c
//
//Covers the C TList API (ListU32) that the C++ oxc::List wrapper delegates to.

#include "test_types_container_shared.h"
#include "types/container/list_basic_types.h"
#include "types/base/algorithm.h"

//Descending comparator for sortCustom (sortCustom orders ascending by comparator).

static ECompareResult cmpU32Desc(const void *a, const void *b) {
	const U32 x = *(const U32*) a, y = *(const U32*) b;
	return x > y ? ECompareResult_Lt : (x < y ? ECompareResult_Gt : ECompareResult_Eq);
}

void Test_list(Test *t) {

	const Allocator *alloc = t->alloc;
	Error *e_rr = &t->err;

	Test_setModule(t, "List");

	ListU32 list = (ListU32) { 0 };

	//pushBack + indexing + length

	Bool ok = true;
	for (U32 i = 0; i < 16; ++i)
		ok &= ListU32_pushBack(&list, i * 3, alloc, e_rr);

	Test_assert(t, "pushBack succeeded", ok);
	Test_assert(t, "length after 16 pushBack", list.length == 16);

	Bool values = true;
	for (U32 i = 0; i < 16; ++i)
		values &= list.ptr[i] == i * 3;
	Test_assert(t, "values stored in insertion order", values);

	//pushFront + insert

	Test_assert(t, "pushFront", ListU32_pushFront(&list, 999, alloc, e_rr));
	Test_assert(t, "pushFront placed at index 0", list.ptr[0] == 999 && list.length == 17);

	Test_assert(t, "insert at index 2", ListU32_insert(&list, 2, 777, alloc, e_rr));
	Test_assert(t, "insert shifted elements", list.ptr[2] == 777 && list.length == 18);

	//findFirst + contains (NULL eq = raw compare)

	Test_assert(t, "findFirst existing", ListU32_findFirst(list, 777, 0, NULL) == 2);
	Test_assert(t, "findFirst missing returns U64_MAX", ListU32_findFirst(list, 0xDEAD, 0, NULL) == U64_MAX);
	Test_assert(t, "contains existing", ListU32_contains(list, 999, 0, NULL));
	Test_assert(t, "contains missing is false", !ListU32_contains(list, 0xDEAD, 0, NULL));

	//erase (index) + eraseAll (value)

	Test_assert(t, "erase index 2 (the 777)", ListU32_erase(&list, 2, e_rr));
	Test_assert(t, "erase removed value", !ListU32_contains(list, 777, 0, NULL) && list.length == 17);

	Test_assert(t, "pushBack duplicate for eraseAll", ListU32_pushBack(&list, 999, alloc, e_rr));
	Test_assert(t, "eraseAll removes every match", ListU32_eraseAll(&list, 999, alloc, NULL, e_rr));
	Test_assert(t, "eraseAll removed all", !ListU32_contains(list, 999, 0, NULL));

	//popBack + popFront

	U32 popped = 0;
	Test_assert(t, "popBack", ListU32_popBack(&list, &popped, e_rr) && popped == 45);
	Test_assert(t, "popFront", ListU32_popFront(&list, &popped, e_rr) && popped == 0);

	//reserve keeps length; resize grows (zero fill) then shrinks

	const U64 lenBefore = list.length;
	Test_assert(t, "reserve", ListU32_reserve(&list, 128, alloc, e_rr));
	Test_assert(t, "reserve keeps length", list.length == lenBefore);

	Test_assert(t, "resize grow", ListU32_resize(&list, 32, alloc, e_rr));
	Test_assert(t, "length after grow", list.length == 32);
	Test_assert(t, "grown region is zeroed", list.ptr[31] == 0);
	Test_assert(t, "resize shrink", ListU32_resize(&list, 4, alloc, e_rr));
	Test_assert(t, "length after shrink", list.length == 4);

	ListU32_clear(&list, e_rr);
	Test_assert(t, "length after clear", list.length == 0);

	//sort (default ascending) + sortCustom (descending) + reverse + swap

	ok = true;
	const U32 unsorted[5] = { 5, 1, 4, 2, 3 };
	for (U32 i = 0; i < 5; ++i)
		ok &= ListU32_pushBack(&list, unsorted[i], alloc, e_rr);
	Test_assert(t, "pushBack for sort", ok);

	Test_assert(t, "sort (default ascending)", ListU32_sort(list));
	Bool asc = list.ptr[0] == 1 && list.ptr[1] == 2 && list.ptr[2] == 3 && list.ptr[3] == 4 && list.ptr[4] == 5;
	Test_assert(t, "sorted ascending", asc);

	Test_assert(t, "sortCustom (descending)", ListU32_sortCustom(list, cmpU32Desc));
	Bool desc = list.ptr[0] == 5 && list.ptr[4] == 1;
	Test_assert(t, "sortCustom descending", desc);

	Test_assert(t, "reverse", ListU32_reverse(list));
	Test_assert(t, "reversed back to ascending", list.ptr[0] == 1 && list.ptr[4] == 5);

	Test_assert(t, "swap ends", ListU32_swap(list, 0, 4, e_rr));
	Test_assert(t, "swapped", list.ptr[0] == 5 && list.ptr[4] == 1);

	//createRepeated + eq/neq

	ListU32 repeated = (ListU32) { 0 };
	Test_assert(t, "createRepeated", ListU32_createRepeated(4, 7, alloc, &repeated, e_rr));
	Test_assert(t, "createRepeated length + values", repeated.length == 4 && repeated.ptr[0] == 7 && repeated.ptr[3] == 7);

	ListU32 repeated2 = (ListU32) { 0 };
	Test_assert(t, "createRepeated second", ListU32_createRepeated(4, 7, alloc, &repeated2, e_rr));
	Test_assert(t, "eq of identical lists", ListU32_eq(repeated, repeated2));
	Test_assert(t, "neq after mutation", (repeated2.ptrNonConst[0] = 8, ListU32_neq(repeated, repeated2)));

	ListU32_free(&repeated, alloc);
	ListU32_free(&repeated2, alloc);

	//createSubset (non-owning ref view into 'list')

	ListU32 subset = (ListU32) { 0 };
	Test_assert(t, "createSubset", ListU32_createSubset(list, 1, 3, &subset, e_rr));
	Test_assert(t, "subset length", subset.length == 3);
	Test_assert(t, "subset aliases parent data", subset.ptr == list.ptr + 1);
	ListU32_free(&subset, alloc);        //ref: frees nothing, must not touch parent data

	ListU32_free(&list, alloc);
	Test_assert(t, "free resets ptr and length", list.ptr == NULL && list.length == 0);
}

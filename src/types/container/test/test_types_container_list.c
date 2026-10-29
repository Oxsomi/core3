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
#include "types/container/list_impl.h"
#include "types/base/algorithm.h"
#include "types/base/allocator.h"
#include "types/base/buffer_base.h"

//A 64 byte element, so GenericList_alignment derives 64 from the stride and the list goes through the aligned
// allocator instead of the plain one.
//That path is worth its own element type because a GenericList keeps a raw pointer rather than the Buffer it
// was given, so growing or freeing one rebuilds the Buffer and has to restore the aligned marking itself.
//Getting that wrong hands the allocator an interior pointer, which is heap corruption rather than a failed
// assert, so nothing above this line would notice.

typedef struct OverAligned {
	U64 v[8];
} OverAligned;

TList(OverAligned);
TListImpl(OverAligned);

//Descending comparator for sortCustom (sortCustom orders ascending by comparator).
//Deliberately not static: the hpp wrapper test sorts through the same C API and shares this comparator,
// because a comparator it defined itself would return oxc::c::ECompareResult (its C headers live in that
// namespace), which is a different type to the C sort calling it even though the two are identical at the
// ABI level, and -fsanitize=function reports exactly that as a call through an incorrect function type.

ECompareResult cmpU32Desc(const void *a, const void *b, void *context) {
	(void) context;
	const U32 x = *(const U32*) a, y = *(const U32*) b;
	return x > y ? ECompareResult_Lt : (x < y ? ECompareResult_Gt : ECompareResult_Eq);
}

//Sorts by a key looked up in a table the caller passes as context, which is the whole point of having one:
//the ordering lives outside the elements, so it can't be expressed by comparing them.

static ECompareResult cmpU32ByKey(const void *a, const void *b, void *context) {
	const U32 *keys = (const U32*) context;
	const U32 x = keys[*(const U32*) a], y = keys[*(const U32*) b];
	return x < y ? ECompareResult_Lt : (x > y ? ECompareResult_Gt : ECompareResult_Eq);
}

//Counts the allocations that are still alive, so a test can tell that a free left something behind
// without depending on the leak report at the end of the suite.

typedef struct CountingAllocator {
	const Allocator *inner;
	I64 live;
} CountingAllocator;

static Bool CountingAllocator_alloc(void *allocator, U64 length, Buffer *output, Error *e_rr) {

	CountingAllocator *counting = (CountingAllocator*) allocator;

	if(!counting->inner->alloc(counting->inner->ptr, length, output, e_rr))
		return false;

	++counting->live;
	return true;
}

static void CountingAllocator_free(void *allocator, Buffer buf) {
	CountingAllocator *counting = (CountingAllocator*) allocator;
	--counting->live;
	counting->inner->free(counting->inner->ptr, buf);
}

void Test_list(Test *t) {

	const Allocator *alloc = t->alloc;
	Error *e_rr = &t->err;

	Test_setModule(t, "List");

	{

		U8 large[2][2049] = { { 1 }, { 2 } };

		large[0][2048] = 3;
		large[1][2048] = 4;

		GenericList largeRef = { 0 };

		Test_assert(t, "large stride list reference", GenericList_createRef(large, 2, sizeof(large[0]), &largeRef, e_rr));

		GenericList_reverse(largeRef);

		Test_assert(
			t, "large stride reverse",
			large[0][0] == 2 && large[1][0] == 1 && large[0][2048] == 4 && large[1][2048] == 3
		);

		ListU32 alias = { 0 }, source = { 0 };

		Test_assert(t, "aliased list create", ListU32_create(3, alloc, &alias, e_rr));

		alias.ptrNonConst[0] = 10;
		alias.ptrNonConst[1] = 20;
		alias.ptrNonConst[2] = 30;

		Test_assert(t, "aliased list append", ListU32_pushAll(&alias, alias, alloc, e_rr));
		Test_assert(t, "aliased list append contents", alias.length == 6 && alias.ptr[3] == 10 && alias.ptr[5] == 30);
		Test_assert(t, "aliased list reserve", ListU32_reserve(&alias, 20, alloc, e_rr));
		Test_assert(t, "aliased subset reference", ListU32_createRefConst(alias.ptr + 2, 3, &source, e_rr));
		Test_assert(t, "aliased list insertion", ListU32_insertAll(&alias, source, 1, alloc, e_rr));
		Test_assert(
			t, "aliased insertion contents",
			alias.length == 9 && alias.ptr[1] == 30 && alias.ptr[2] == 10 && alias.ptr[3] == 20
		);

		ListU32_free(&alias, alloc);
	}

	{

		const U32 values[] = { 10, 20, 30, 40 };
		const U64 prefix[] = { 0, 1 };

		ListU32 valuesRef = { 0 }, erased = { 0 };
		ListU64 prefixRef = { 0 }, indices = { 0 };

		Test_assert(
			t, "erase prefix setup",
			ListU32_createRefConst(values, 4, &valuesRef, e_rr) && ListU32_createCopy(valuesRef, alloc, &erased, e_rr) &&
			ListU64_createRefConst(prefix, 2, &prefixRef, e_rr) && ListU64_createCopy(prefixRef, alloc, &indices, e_rr)
		);

		if(erased.ptr && indices.ptr) {

			Test_assert(t, "erase adjacent indices starting at zero", ListU32_eraseAllIndices(&erased, &indices, e_rr));
			Test_assert(t, "erased prefix contents", erased.length == 2 && erased.ptr[0] == 30 && erased.ptr[1] == 40);
			Test_assert(t, "erase remaining indices", ListU32_eraseAllIndices(&erased, &indices, e_rr) && !erased.length);
		}

		ListU64_free(&indices, alloc);
		ListU32_free(&erased, alloc);

		U32 borrowed[2] = { 41, 42 };
		ListU32 ref = { 0 };

		Test_assert(t, "empty const list setup", ListU32_createRefConst(borrowed, 2, &ref, e_rr));
		Test_assert(t, "clear borrowed list", ListU32_clear(&ref, e_rr));
		Test_assert(t, "grow cleared const list", ListU32_resize(&ref, 2, alloc, e_rr));
		Test_assert(t, "cleared const list gets storage", ref.ptr != borrowed && !ListU32_isRef(ref));
		Test_assert(t, "borrowed list unchanged", borrowed[0] == 41 && borrowed[1] == 42);

		ListU32_free(&ref, alloc);
	}

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

	//What a shrink left behind is zeroed again when the list grows back over it, within its capacity and past it.

	for(U64 i = 0; i < list.length; ++i)
		list.ptrNonConst[i] = U32_MAX;

	Test_assert(t, "resize regrow", ListU32_resize(&list, 3, alloc, e_rr) && ListU32_resize(&list, 4, alloc, e_rr));
	Test_assert(t, "regrown element is zeroed", list.ptr[3] == 0 && list.ptr[2] == U32_MAX);

	const U64 beyond = ListU32_allocatedBytes(list) / sizeof(U32) + 7;
	Test_assert(t, "resize past capacity", ListU32_resize(&list, beyond, alloc, e_rr) && list.length == beyond);

	Bool zeroed = true;

	for(U64 i = 4; i < beyond; ++i)
		zeroed &= list.ptr[i] == 0;

	Test_assert(t, "grown past capacity is zeroed", zeroed && list.ptr[3] == 0);
	Test_assert(t, "shrink after growing", ListU32_resize(&list, 4, alloc, e_rr));

	//The Uninitialized forms: an exact allocation for create, resize's growth for resize, and the elements kept.

	ListU32 raw = (ListU32) { 0 };
	Test_assert(t, "createUninitialized", ListU32_createUninitialized(1000, alloc, &raw, e_rr));
	Test_assert(t, "createUninitialized is exact", raw.length == 1000 && ListU32_allocatedBytes(raw) == 1000 * sizeof(U32));

	for(U64 i = 0; i < raw.length; ++i)
		raw.ptrNonConst[i] = (U32) i * 7;

	Test_assert(t, "resizeUninitialized grow", ListU32_resizeUninitialized(&raw, 3000, alloc, e_rr) && raw.length == 3000);

	Bool kept = true;

	for(U64 i = 0; i < 1000; ++i)
		kept &= raw.ptr[i] == (U32) i * 7;

	Test_assert(t, "resizeUninitialized keeps the elements", kept);
	Test_assert(t, "resizeUninitialized shrink", ListU32_resizeUninitialized(&raw, 10, alloc, e_rr) && raw.length == 10);

	ListU32 empty = (ListU32) { 0 };
	Test_assert(t, "createUninitialized empty", ListU32_createUninitialized(0, alloc, &empty, e_rr) && !empty.length);
	Test_assert(t, "createUninitialized refuses a filled list", !ListU32_createUninitialized(4, alloc, &raw, NULL));

	ListU32_free(&raw, alloc);
	ListU32_free(&empty, alloc);

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

	Test_assert(t, "sortCustom (descending)", ListU32_sortCustom(list, cmpU32Desc, NULL));
	Bool desc = list.ptr[0] == 5 && list.ptr[4] == 1;
	Test_assert(t, "sortCustom descending", desc);

	//Its own list, so the checks after this one still see the 1..5 they expect.
	//It holds indices 0..4 and the context table says what each index is worth, so the result is the table's
	// order rather than the elements' own.
	//keys[] ranks index 3 first and index 1 last, which no comparison of the values alone could produce.

	{
		static const U32 keys[5] = { 40, 50, 20, 10, 30 };

		ListU32 keyed = (ListU32) { 0 };
		Bool keyedOk = true;

		for (U32 i = 0; i < 5; ++i)
			keyedOk &= ListU32_pushBack(&keyed, i, alloc, e_rr);

		Test_assert(t, "pushBack for sortCustom context", keyedOk);

		Test_assert(t, "sortCustom (context table)", ListU32_sortCustom(keyed, cmpU32ByKey, (void*) (const void*) keys));

		Test_assert(
			t, "sortCustom followed the context table",
			keyed.ptr[0] == 3 && keyed.ptr[1] == 2 && keyed.ptr[2] == 4 && keyed.ptr[3] == 0 && keyed.ptr[4] == 1
		);

		ListU32_free(&keyed, alloc);
	}

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

	//Ref semantics: a ref points at someone else's memory, so it can't be resized or freed, and a const ref can't be written
	//through either.
	//That's the safety story the whole TList API leans on.

	Test_setModule(t, "ListRefs");

	{
		U32 backing[4] = { 1, 2, 3, 4 };

		ListU32 mutRef = (ListU32) { 0 };
		Test_assert(t, "createRef", ListU32_createRef(backing, 4, &mutRef, e_rr));
		Test_assert(t, "createRef isRef", ListU32_isRef(mutRef));
		Test_assert(t, "createRef not const", !ListU32_isConstRef(mutRef));
		Test_assert(t, "createRef aliases", mutRef.ptr == backing && mutRef.length == 4);

		Test_assert(t, "set through mut ref", ListU32_set(mutRef, 0, 42, e_rr));
		Test_assert(t, "set landed in backing", backing[0] == 42);

		//Growing a ref would have to reallocate memory it doesn't own

		Test_assert(t, "pushBack on ref rejected", !ListU32_pushBack(&mutRef, 5, alloc, NULL));
		Test_assert(t, "resize on ref rejected", !ListU32_resize(&mutRef, 8, alloc, NULL));

		ListU32 constRef = (ListU32) { 0 };
		Test_assert(t, "createRefConst", ListU32_createRefConst(backing, 4, &constRef, e_rr));
		Test_assert(t, "createRefConst isConstRef", ListU32_isConstRef(constRef));
		Test_assert(t, "set through const ref rejected", !ListU32_set(constRef, 0, 1, NULL));

		//Freeing a ref is a no-op that must leave the backing store alone

		ListU32_free(&mutRef, alloc);
		ListU32_free(&constRef, alloc);
		Test_assert(t, "backing survives ref free", backing[0] == 42 && backing[3] == 4);
	}

	//empty/any, and get/set bounds

	Test_setModule(t, "ListAccess");

	{
		ListU32 l = (ListU32) { 0 };
		Test_assert(t, "empty on null list", ListU32_empty(l) && !ListU32_any(l));

		Test_assert(t, "create", ListU32_create(3, alloc, &l, e_rr));
		Test_assert(t, "any after create", ListU32_any(l) && !ListU32_empty(l));
		Test_assert(t, "create zeroes", l.ptr[0] == 0 && l.ptr[1] == 0 && l.ptr[2] == 0);

		Test_assert(t, "set", ListU32_set(l, 1, 55, e_rr));

		U32 got = 0;
		Test_assert(t, "get", ListU32_get(l, 1, &got, e_rr) && got == 55);

		Test_assert(t, "get out of bounds", !ListU32_get(l, 3, &got, NULL));
		Test_assert(t, "set out of bounds", !ListU32_set(l, 3, 1, NULL));

		ListU32_free(&l, alloc);
	}

	//Copies: createCopy is a deep copy, copy() moves a range between existing lists

	Test_setModule(t, "ListCopy");

	{
		ListU32 src = (ListU32) { 0 };

		for(U32 i = 0; i < 5; ++i)
			ListU32_pushBack(&src, i, alloc, e_rr);

		ListU32 deep = (ListU32) { 0 };
		Test_assert(t, "createCopy", ListU32_createCopy(src, alloc, &deep, e_rr));
		Test_assert(t, "createCopy equal", ListU32_eq(src, deep));
		Test_assert(t, "createCopy owns its own memory", deep.ptr != src.ptr && !ListU32_isRef(deep));

		src.ptrNonConst[0] = 99;
		Test_assert(t, "copy is independent", deep.ptr[0] == 0);

		ListU32 sub = (ListU32) { 0 };
		Test_assert(t, "createCopySubset", ListU32_createCopySubset(src, 1, 3, alloc, &sub, e_rr));
		Test_assert(t, "createCopySubset content", sub.length == 3 && sub.ptr && sub.ptr[0] == 1 && sub.ptr[2] == 3);

		ListU32 rev = (ListU32) { 0 };
		Test_assert(t, "createReverse", ListU32_createReverse(sub, alloc, &rev, e_rr));
		Test_assert(t, "createReverse content", rev.length == 3 && rev.ptr && rev.ptr[0] == 3 && rev.ptr[2] == 1);

		//copy() writes into an existing list rather than allocating

		Test_assert(t, "copy range", ListU32_copy(sub, 0, deep, 0, 3, e_rr));
		Test_assert(t, "copy landed", deep.ptr[0] == 1 && deep.ptr[2] == 3 && deep.ptr[3] == 3);
		Test_assert(t, "copy past end rejected", !ListU32_copy(sub, 0, deep, 4, 3, NULL));

		ListU32_free(&src, alloc);
		ListU32_free(&deep, alloc);
		ListU32_free(&sub, alloc);
		ListU32_free(&rev, alloc);
	}

	//Bulk insert, targeted erase and shrink

	Test_setModule(t, "ListBulk");

	{
		ListU32 l = (ListU32) { 0 }, other = (ListU32) { 0 };

		for(U32 i = 0; i < 3; ++i)
			ListU32_pushBack(&l, i, alloc, e_rr);            //0 1 2

		for(U32 i = 10; i < 13; ++i)
			ListU32_pushBack(&other, i, alloc, e_rr);        //10 11 12

		Test_assert(t, "pushAll", ListU32_pushAll(&l, other, alloc, e_rr));
		Test_assert(t, "pushAll appended", l.length == 6 && l.ptr[3] == 10 && l.ptr[5] == 12);

		Test_assert(t, "insertAll", ListU32_insertAll(&l, other, 1, alloc, e_rr));
		Test_assert(t, "insertAll spliced", l.length == 9 && l.ptr[1] == 10 && l.ptr[3] == 12 && l.ptr[4] == 1);

		//eraseFirst/eraseLast remove one occurrence from the given side; NULL means raw compare

		Test_assert(t, "eraseFirst", ListU32_eraseFirst(&l, 10, 0, NULL, e_rr));
		Test_assert(t, "eraseFirst removed the first", l.length == 8 && l.ptr[1] == 11);

		Test_assert(t, "eraseLast", ListU32_eraseLast(&l, 12, 0, NULL, e_rr));
		Test_assert(t, "eraseLast removed the last", l.length == 7);

		//find returns every index of a value; the list holds 11 twice by this point

		ListU64 found = (ListU64) { 0 };
		Test_assert(t, "find", ListU32_find(l, 11, NULL, alloc, &found, e_rr));
		Test_assert(t, "find count", found.length == 2);

		Test_assert(t, "eraseAllIndices", ListU32_eraseAllIndices(&l, &found, e_rr));

		//find rejects a non-empty result (it would leak), so the previous one has to go first

		ListU64_free(&found, alloc);
		Test_assert(t, "eraseAllIndices removed both", ListU32_find(l, 11, NULL, alloc, &found, e_rr) && !found.length);
		ListU64_free(&found, alloc);

		//popLocation hands the removed element back

		const U64 before = l.length;
		U32 tail = 0;
		Test_assert(t, "popLocation", ListU32_popLocation(&l, 0, &tail, e_rr));
		Test_assert(t, "popLocation shrank", l.length == before - 1);
		Test_assert(t, "popLocation out of bounds", !ListU32_popLocation(&l, l.length, &tail, NULL));

		//shrinkToFit drops spare capacity without changing contents

		ListU32_reserve(&l, 128, alloc, e_rr);
		ListU32 snapshot = (ListU32) { 0 };
		ListU32_createCopy(l, alloc, &snapshot, e_rr);

		Test_assert(t, "shrinkToFit", ListU32_shrinkToFit(&l, alloc, e_rr));
		Test_assert(t, "shrinkToFit keeps contents", ListU32_eq(l, snapshot));

		ListU32_free(&l, alloc);
		ListU32_free(&other, alloc);
		ListU32_free(&snapshot, alloc);
	}

	// -- Over-aligned elements ------------------------------------------------------------------------

	Test_setModule(t, "List/OverAligned");

	{
		ListOverAligned l = (ListOverAligned) { 0 };

		Test_assert(t, "stride is over-aligned", sizeof(OverAligned) == 64);

		//Grown one at a time so reserve reallocates repeatedly, which is the alloc-new then free-old pair.
		//The old buffer is rebuilt from the list's raw pointer at that point, so this is the case that broke.

		Bool alignedThroughout = true;

		for (U64 i = 0; i < 64; ++i) {

			const OverAligned e = (OverAligned) { .v = { i } };

			if(!ListOverAligned_pushBack(&l, e, alloc, e_rr))
				break;

			alignedThroughout &= !((U64) l.ptr & 63);
		}

		Test_assert(t, "grew to full length", l.length == 64);
		Test_assert(t, "stays aligned across reallocation", alignedThroughout);
		Test_assert(t, "contents survived", l.length == 64 && l.ptr[0].v[0] == 0 && l.ptr[63].v[0] == 63);

		//The free that has to walk back to the real base, rather than handing over the aligned pointer

		ListOverAligned_free(&l, alloc);
		Test_assert(t, "freed clean", !l.ptr && !l.length);
	}

	// -- Freeing the elements of a list of lists ------------------------------------------------------

	Test_setModule(t, "List/FreeUnderlying");

	//More entries than a 16 bit index can count: a list longer than 65535 has to be walked with a full width index.
	//The counting allocator checks that every element list is freed.

	{
		CountingAllocator counting = (CountingAllocator) { .inner = alloc };
		const Allocator countingAlloc = (Allocator) {
			.ptr = &counting, .alloc = CountingAllocator_alloc, .free = CountingAllocator_free
		};

		ListListU32 lists = (ListListU32) { 0 };
		const U64 count = 70000;

		Test_assert(t, "resize", ListListU32_resize(&lists, count, &countingAlloc, e_rr));

		Bool created = lists.length == count;

		for(U64 i = 0; created && i < count; ++i)
			created &= ListU32_create(1, &countingAlloc, &lists.ptrNonConst[i], e_rr);

		Test_assert(t, "elements created", created && counting.live == (I64) count + 1);

		ListListU32_freeUnderlying(&lists, &countingAlloc);
		Test_assert(t, "freeUnderlying empties", !lists.length && !lists.ptr);
		Test_assert(t, "freeUnderlying frees every element", counting.live == 0);
	}

	Test_setModule(t, NULL);
}

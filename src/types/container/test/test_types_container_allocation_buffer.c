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

//types/container/test/test_types_container_allocation_buffer.c

#include "test_types_container_shared.h"
#include "types/container/allocation_buffer.h"

static Bool Test_createAllocBuffer(Test *t, U64 size, U64 nonLinearAlignment, AllocationBuffer *ab) {

	const AllocationBufferCreate create = {
		.size               = size,
		.nonLinearAlignment = nonLinearAlignment,
		.alloc              = t->alloc,
		.allocationBuffer   = ab
	};

	if (!AllocationBuffer_create(&create, false, &t->err)) {
		Test_assert(t, "Test_createAllocBuffer", false);
		return false;
	}

	return true;
}

static const U8 *Test_allocFromAllocBuffer(
	Test *t,
	const C8 *label,
	AllocationBuffer *ab,
	U64 size,
	U64 alignment,
	Bool nonLinear
) {
	const AllocationBufferAllocate spec = {
		.allocationBuffer    = ab,
		.alignment           = alignment,
		.isNonLinearResource = nonLinear,
		.alloc               = t->alloc
	};

	const U8 *result = NULL;

	if (!AllocationBuffer_allocateBlock(&spec, size, &result, &t->err)) {
		Test_assert(t, label, false);
		return NULL;
	}

	if (!result) {
		Test_assert(t, label, false);
		return NULL;
	}

	//Verify the returned pointer is inside the buffer and correctly aligned

	const U64 off = (U64)(result - ab->buffer.ptr);
	Test_assert(t, label, off + size <= Buffer_length(ab->buffer));
	Test_assert(t, label, !(off % alignment));
	return result;
}

static inline U64 Block_start(AllocationBufferBlock b) { return b.startAndNonLinearAndFree << 2 >> 2; }
static inline Bool Block_isFree(AllocationBufferBlock b) { return (Bool)(b.startAndNonLinearAndFree >> 63); }
static inline Bool Block_isNonLinear(AllocationBufferBlock b) { return (Bool)((b.startAndNonLinearAndFree >> 62) & 1); }

void Test_allocationBufferCreate(Test *t) {

	Test_setModule(t, "AllocationBuffer create/free");

	AllocationBuffer ab = { 0 };

	Test_assert(t, "Create 1 KiB", Test_createAllocBuffer(t, 1024, 0, &ab));
	Test_assert(t, "Buffer non-null", ab.buffer.ptr);
	Test_assert(t, "Buffer length",   Buffer_length(ab.buffer) == 1024);
	Test_assert(t, "No allocations",  !ab.allocations.length);
	AllocationBuffer_free(&ab, t->alloc);
	Test_assert(t, "Free clears ptr", !ab.buffer.ptr);
}

void Test_allocationBufferSingleAlloc(Test *t) {

	Test_setModule(t, "AllocationBuffer single alloc");

	AllocationBuffer ab = { 0 };

	if (!Test_createAllocBuffer(t, 256, 0, &ab))
		return;

	//First alloc always starts at offset 0

	const U8 *p = Test_allocFromAllocBuffer(t, "First alloc", &ab, 64, 1, false);
	Test_assert(t, "Starts at base", p == ab.buffer.ptr);

	//One occupied block, start=0, end=64

	Test_assert(t, "Block count",      ab.allocations.length == 1);
	Test_assert(t, "Block start",      Block_start(ab.allocations.ptr[0]) == 0);
	Test_assert(t, "Block end",        ab.allocations.ptr[0].end == 64);
	Test_assert(t, "Block not free",   !Block_isFree(ab.allocations.ptr[0]));
	Test_assert(t, "Block not nonlin", !Block_isNonLinear(ab.allocations.ptr[0]));

	AllocationBuffer_free(&ab, t->alloc);
}

void Test_allocationBufferMultipleAllocs(Test *t) {

	Test_setModule(t, "AllocationBuffer multiple allocs");

	AllocationBuffer ab = { 0 };

	if (!Test_createAllocBuffer(t, 1024, 0, &ab))
		return;

	const U8 *a = Test_allocFromAllocBuffer(t, "Alloc a", &ab, 128, 1,   false);
	const U8 *b = Test_allocFromAllocBuffer(t, "Alloc b", &ab, 128, 1,   false);
	const U8 *c = Test_allocFromAllocBuffer(t, "Alloc c", &ab, 256, 256, false);

	Test_assert(t, "a/b no overlap", a + 128 <= b || b + 128 <= a);
	Test_assert(t, "b/c no overlap", b + 128 <= c || c + 256 <= b);
	Test_assert(t, "c alignment", !((U64)(c - ab.buffer.ptr) % 256));

	Test_assert(t, "Block count", ab.allocations.length == 3);

	for (U64 i = 0; i < ab.allocations.length; ++i)
		Test_assert(t, "Block not free", !Block_isFree(ab.allocations.ptr[i]));

	for (U64 i = 1; i < ab.allocations.length; ++i)
		Test_assert(t, "Blocks ordered", Block_start(ab.allocations.ptr[i]) >= ab.allocations.ptr[i - 1].end);

	AllocationBuffer_free(&ab, t->alloc);
}

void Test_allocationBufferFreeAndReuse(Test *t) {

	Test_setModule(t, "AllocationBuffer free and reuse");

	AllocationBuffer ab = { 0 };

	if (!Test_createAllocBuffer(t, 512, 0, &ab))
		return;

	const U8 *a = Test_allocFromAllocBuffer(t, "Alloc a", &ab, 128, 1, false);
	const U8 *b = Test_allocFromAllocBuffer(t, "Alloc b", &ab, 128, 1, false);
	const U8 *c = Test_allocFromAllocBuffer(t, "Alloc c", &ab, 256, 1, false);		//Force take up the rest of the buffer

	if (a && b && c) {

		//Try freeing and reoccupying middle block

		AllocationBuffer_freeBlock(&ab, b);

		Test_assert(t, "Freed b correctly", Block_isFree(ab.allocations.ptr[1]));

		const AllocationBufferAllocate allocOne = {
			.allocationBuffer = &ab,
			.alignment = 1,
			.isNonLinearResource = false,
			.alloc = t->alloc
		};

		const U8 *b2 = Test_allocFromAllocBuffer(t, "Force reuse b", &ab, 128, 1, false);
		Test_assert(t, "Needs to occupy same space", b2 == b);
		Test_assert(t, "Fully occupied", !AllocationBuffer_allocateBlock(&allocOne, 1, &b, NULL));

		//Reoccupy front block

		AllocationBuffer_freeBlock(&ab, a);

		Test_assert(
			t, "Freed a correctly", ab.allocations.length == 2 && ab.allocations.ptr[0].startAndNonLinearAndFree == 128
		);

		const U8 *a2 = Test_allocFromAllocBuffer(t, "Force reuse a", &ab, 128, 1, false);
		Test_assert(t, "Needs to occupy same space", a2 == a);
		Test_assert(t, "Fully occupied", !AllocationBuffer_allocateBlock(&allocOne, 1, &a, NULL));

		//Reoccupy back block

		AllocationBuffer_freeBlock(&ab, c);

		Test_assert(
			t, "Freed c correctly", ab.allocations.length == 2 && ab.allocations.ptr[1].startAndNonLinearAndFree == 128
		);

		const U8 *c2 = Test_allocFromAllocBuffer(t, "Force reuse c", &ab, 256, 1, false);
		Test_assert(t, "Needs to occupy same space", c2 == c);
		Test_assert(t, "Fully occupied", !AllocationBuffer_allocateBlock(&allocOne, 1, &c, NULL));
	}

	AllocationBuffer_free(&ab, t->alloc);
}

void Test_allocationBufferFreeAll(Test *t) {

	Test_setModule(t, "AllocationBuffer_freeAll");

	AllocationBuffer ab = { 0 };

	if (!Test_createAllocBuffer(t, 512, 0, &ab))
		return;

	Test_allocFromAllocBuffer(t, "Alloc 1", &ab, 64, 1, false);
	Test_allocFromAllocBuffer(t, "Alloc 2", &ab, 64, 1, false);
	Test_allocFromAllocBuffer(t, "Alloc 3", &ab, 64, 1, false);

	AllocationBuffer_freeAll(&ab);
	Test_assert(t, "All cleared", !ab.allocations.length);

	//After freeAll the full buffer should be re-usable

	const U8 *big = Test_allocFromAllocBuffer(t, "Alloc full", &ab, 512, 1, false);
	Test_assert(t, "Full alloc succeeds", big != NULL);

	//Single occupied block covering the whole buffer

	Test_assert(t, "Block count",    ab.allocations.length == 1);
	Test_assert(t, "Block start",    Block_start(ab.allocations.ptr[0]) == 0);
	Test_assert(t, "Block end",      ab.allocations.ptr[0].end == 512);
	Test_assert(t, "Block occupied", !Block_isFree(ab.allocations.ptr[0]));

	AllocationBuffer_free(&ab, t->alloc);
}

void Test_allocationBufferOutOfMemory(Test *t) {

	Test_setModule(t, "AllocationBuffer out of memory");

	AllocationBuffer ab = { 0 };

	if (!Test_createAllocBuffer(t, 128, 0, &ab))
		return;

	Test_allocFromAllocBuffer(t, "Fill", &ab, 128, 1, false);

	const AllocationBufferAllocate spec = {		//No more space
		.allocationBuffer    = &ab,
		.alignment           = 1,
		.isNonLinearResource = false,
		.alloc               = t->alloc
	};

	const U8 *overflow = NULL;
	Bool ok = AllocationBuffer_allocateBlock(&spec, 1, &overflow, NULL);

	Test_assert(t, "Over-alloc fails",    !ok);
	Test_assert(t, "Over-alloc ptr null", !overflow);

	AllocationBuffer_free(&ab, t->alloc);
}

//Fragmentation, free every other block so there is plenty of combined free space,
// but no single contiguous allocation is large enough.
void Test_allocationBufferFragmentedOOM(Test *t) {

	Test_setModule(t, "AllocationBuffer fragmented OOM");

	AllocationBuffer ab = { 0 };

	if (!Test_createAllocBuffer(t, 512, 0, &ab))
		return;

	const U8 *ptrs[8] = { 0 };		//8 allocations of 64 to fill 512 bytes

	for (U64 i = 0; i < 8; ++i)
		ptrs[i] = Test_allocFromAllocBuffer(t, "Fill", &ab, 64, 1, false);

	//_X_X_X_X -> X_X_X_X

	for (U64 i = 0; i < 8; i += 2)
		if (ptrs[i])
			AllocationBuffer_freeBlock(&ab, ptrs[i]);

	//Verify fragmented

	U64 mask = 0;

	for (U64 i = 0; i < ab.allocations.length; ++i)
		mask |= (U64)!Block_isFree(ab.allocations.ptr[i]) << i;

	Test_assert(t, "Fragmented state", ab.allocations.ptr[0].startAndNonLinearAndFree == 0x40);		//First block got popped
	Test_assert(t, "Fragmented state", mask == 0b1010101);

	//128-byte allocation must fail, no space left.

	const AllocationBufferAllocate spec = {
		.allocationBuffer    = &ab,
		.alignment           = 1,
		.isNonLinearResource = false,
		.alloc               = t->alloc
	};

	const U8 *big = NULL;
	Bool ok = AllocationBuffer_allocateBlock(&spec, 128, &big, NULL);

	Test_assert(t, "Fragmented OOM fails",    !ok);
	Test_assert(t, "Fragmented OOM ptr null", !big);

	t->err = Error_none();

	//64-byte allocation (fits in one empty spot) must still succeed

	const U8 *small = NULL;
	Test_assert(t, "small alloc in hole ok", AllocationBuffer_allocateBlock(&spec, 64, &small, &t->err));
	Test_assert(t, "small alloc non-null",   small);

	AllocationBuffer_free(&ab, t->alloc);
}

void Test_allocationBufferAlignmentPadding(Test *t) {

	Test_setModule(t, "AllocationBuffer alignment padding");

	AllocationBuffer ab = { 0 };

	//4 KiB; allocate a 1-byte block first to push the cursor off alignment,
	//Then ask for a 256-aligned block and verify the pointer is still correct.

	if (!Test_createAllocBuffer(t, 4096, 0, &ab))
		return;

	Test_allocFromAllocBuffer(t, "Misalign", &ab, 1, 1, false);
	const U8 *aligned = Test_allocFromAllocBuffer(t, "Aligned", &ab, 64, 256, false);

	if (aligned)
		Test_assert(t, "256-aligned", !((U64)(aligned - ab.buffer.ptr) % 256));

	//The first block covers [0,1) and the second starts at 1 despite alignment.
	//The real placed allocation starts at block[1].start alignas alignment.
	//There should be exactly 2 blocks; block[1].start must equal 256.

	Test_assert(t, "Two blocks", ab.allocations.length == 2);

	if (ab.allocations.length == 2) {
		Test_assert(t, "Padded start", Block_start(ab.allocations.ptr[1]) == 1);
		Test_assert(t, "Padded end",   ab.allocations.ptr[1].end == 256 + 64);
	}

	AllocationBuffer_free(&ab, t->alloc);
}

void Test_allocationBufferNonLinear(Test *t) {

	Test_setModule(t, "AllocationBuffer non-linear separation");

	AllocationBuffer ab = { 0 };

	//nonLinearAlignment = 256: linear and non-linear blocks must be separated by a multiple of 256.

	if (!Test_createAllocBuffer(t, 8192, 256, &ab))
		return;

	const U8 *lin    = Test_allocFromAllocBuffer(t, "Linear",     &ab, 128, 1, false);
	const U8 *nonlin = Test_allocFromAllocBuffer(t, "Non-linear", &ab, 128, 1, true);

	if (lin && nonlin) {
		const U64 linEnd  = (U64)(lin    - ab.buffer.ptr) + 128;
		const U64 nlStart = (U64)(nonlin - ab.buffer.ptr);
		Test_assert(t, "Non-linear gap aligned",           !(nlStart % 256));
		Test_assert(t, "Non-linear starts after linear end", nlStart >= linEnd);
	}

	Test_assert(t, "Two blocks", ab.allocations.length == 2);

	if (ab.allocations.length == 2) {
		Test_assert(t, "Block[0] linear",     !Block_isNonLinear(ab.allocations.ptr[0]));
		Test_assert(t, "Block[1] non-linear",  Block_isNonLinear(ab.allocations.ptr[1]));
	}

	AllocationBuffer_free(&ab, t->alloc);
}

void Test_allocationBufferVirtual(Test *t) {

	Test_setModule(t, "AllocationBuffer virtual");

	AllocationBuffer ab = { 0 };

	const AllocationBufferCreate create = {
		.size               = 65536,
		.nonLinearAlignment = 0,
		.alloc              = t->alloc,
		.allocationBuffer   = &ab
	};

	if (!AllocationBuffer_create(&create, true, &t->err)) {
		Test_assert(t, "AllocationBuffer_create virtual", false);
		return;
	}

	Test_assert(t, "AllocationBuffer_create virtual ptr null", !ab.buffer.ptr);
	Test_assert(t, "AllocationBuffer_create virtual length",   Buffer_length(ab.buffer) == 65536);

	const AllocationBufferAllocate spec = {
		.allocationBuffer    = &ab,
		.alignment           = 64,
		.isNonLinearResource = false,
		.alloc               = t->alloc
	};

	const U8 *p = NULL;
	Test_assert(t, "AllocationBuffer_allocateBlock",            AllocationBuffer_allocateBlock(&spec, 256, &p, &t->err));
	Test_assert(t, "AllocationBuffer_allocateBlock result start of virtual alloc", !p);

	Test_assert(t, "Virtual block count", ab.allocations.length == 1);

	if (ab.allocations.length) {
		Test_assert(t, "Virtual block start", Block_start(ab.allocations.ptr[0]) == 0);
		Test_assert(t, "Virtual block end",   ab.allocations.ptr[0].end == 256);
	}

	AllocationBuffer_free(&ab, t->alloc);
}

void Test_allocationBufferRefFromRegion(Test *t) {

	Test_setModule(t, "AllocationBuffer_createRefFromRegion");

	U8 data[1024];
	Buffer buf = Buffer_createRef(data, sizeof(data));

	AllocationBuffer ab = { 0 };

	const AllocationBufferCreate create = {
		.size               = 512,
		.nonLinearAlignment = 0,
		.alloc              = t->alloc,
		.allocationBuffer   = &ab
	};

	if (!AllocationBuffer_createRefFromRegion(&create, buf, 128, &t->err)) {
		Test_assert(t, "AllocationBuffer_createRefFromRegion", false);
		return;
	}

	Test_assert(t, "Ref ptr",    ab.buffer.ptr == buf.ptr + 128);
	Test_assert(t, "Ref length", Buffer_length(ab.buffer) == 512);

	const U8 *p = Test_allocFromAllocBuffer(t, "Alloc in ref", &ab, 64, 1, false);

	if (p)
		Test_assert(t, "Test_allocFromAllocBuffer", p == data + 128);

	ListAllocationBufferBlock_free(&ab.allocations, t->alloc);
}

void Test_allocationBufferAllocateAndFill(Test *t) {

	Test_setModule(t, "AllocationBuffer_allocateAndFillBlock");

	AllocationBuffer ab = { 0 };

	if (!Test_createAllocBuffer(t, 512, 0, &ab))
		return;

	const U8 src[8] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE };

	const AllocationBufferAllocate spec = {
		.allocationBuffer    = &ab,
		.alignment           = 1,
		.isNonLinearResource = false,
		.alloc               = t->alloc
	};

	//Allocate twice

	U8 *dst = NULL;
	Test_assert(t, "AllocationBuffer_allocateAndFillBlock",
		AllocationBuffer_allocateAndFillBlock(&spec, Buffer_createRefConst(src, sizeof(src)), &dst, &t->err)
	);

	if (dst)
		Test_assert(t, "Buffer eq", Buffer_eq(
			Buffer_createRefConst(dst, sizeof(src)),
			Buffer_createRefConst(src, sizeof(src))
		));

	U8 *ogDst = dst;
	dst = NULL;
	Test_assert(t, "AllocationBuffer_allocateAndFillBlock",
		AllocationBuffer_allocateAndFillBlock(&spec, Buffer_createRefConst(src, sizeof(src)), &dst, &t->err)
	);

	if (dst)
		Test_assert(t, "Buffer eq", Buffer_eq(
			Buffer_createRefConst(dst, sizeof(src)),
			Buffer_createRefConst(src, sizeof(src))
		));

	Test_assert(t, "Allocated offsets", ogDst == ab.buffer.ptr && dst == ogDst + sizeof(src));

	Test_assert(t, "Check 2 blocks", ab.allocations.length == 2);

	if (ab.allocations.length == 2) {
		Test_assert(t, "Block[0] start", Block_start(ab.allocations.ptr[0]) == 0);
		Test_assert(t, "Block[0] end",   ab.allocations.ptr[0].end == sizeof(src));
		Test_assert(t, "Block[1] start", Block_start(ab.allocations.ptr[1]) == sizeof(src));
		Test_assert(t, "Block[1] end",   ab.allocations.ptr[1].end == 2 * sizeof(src));
	}

	//Realloc the first, new alloc should land at the tail (ring buffer behavior)

	AllocationBuffer_freeBlock(&ab, ogDst);

	ogDst = dst;
	dst = NULL;
	Test_assert(t, "AllocationBuffer_allocateAndFillBlock",
		AllocationBuffer_allocateAndFillBlock(&spec, Buffer_createRefConst(src, sizeof(src)), &dst, &t->err)
	);

	if (dst)
		Test_assert(t, "AllocationBuffer_allocateAndFillBlock eq", !Buffer_neq(
			Buffer_createRefConst(dst, sizeof(src)),
			Buffer_createRefConst(src, sizeof(src))
		));

	Test_assert(t, "AllocationBuffer_allocateAndFillBlock offsets", dst == ogDst + sizeof(src));

	Test_assert(t, "Check 2 blocks", ab.allocations.length == 2);

	for (U64 i = 0; i < ab.allocations.length; ++i)
		Test_assert(t, "No free blocks left", !Block_isFree(ab.allocations.ptr[i]));

	AllocationBuffer_free(&ab, t->alloc);
}

void Test_allocationBufferMerge(Test *t) {		//Freeing adjacent blocks must collapse them into a single merged block

	Test_setModule(t, "AllocationBuffer merge");

	AllocationBuffer ab = { 0 };

	if (!Test_createAllocBuffer(t, 512, 0, &ab))
		return;

	const U8 *a = Test_allocFromAllocBuffer(t, "Alloc a", &ab, 64, 1, false);
	const U8 *b = Test_allocFromAllocBuffer(t, "Alloc b", &ab, 64, 1, false);
	const U8 *c = Test_allocFromAllocBuffer(t, "Alloc c", &ab, 64, 1, false);
	const U8 *d = Test_allocFromAllocBuffer(t, "Alloc d", &ab, 64, 1, false);

	if (!(a && b && c && d)) {
		AllocationBuffer_free(&ab, t->alloc);
		return;
	}

	//Freeing the middle will leave us with a big merged block in the middle.
	//Doing this for a or d will pop their blocks for us.

	AllocationBuffer_freeBlock(&ab, b);
	AllocationBuffer_freeBlock(&ab, c);

	Test_assert(t, "b+c merged into one free block", Block_isFree(ab.allocations.ptr[1]));
	Test_assert(t, "b+c merged into one bigger block", ab.allocations.ptr[1].end == 64 * 3);

	//Free last block to the right, should result in 1 block leftover (a)

	AllocationBuffer_freeBlock(&ab, d);
	Test_assert(t, "Popped last right block (d)", ab.allocations.length == 1 && ab.allocations.ptr[0].end == 64);

	AllocationBuffer_freeBlock(&ab, a);
	Test_assert(t, "Popped last block (a)", ab.allocations.length == 0);

	AllocationBuffer_free(&ab, t->alloc);
}

void Test_allocationBufferFrontAlloc(Test *t) {

	Test_setModule(t, "AllocationBuffer front alloc");

	AllocationBuffer ab = { 0 };

	if (!Test_createAllocBuffer(t, 384, 0, &ab))
		return;

	const U8 *a = Test_allocFromAllocBuffer(t, "Alloc a", &ab, 128, 1, false);
	const U8 *b = Test_allocFromAllocBuffer(t, "Alloc b", &ab, 128, 1, false);
	const U8 *c = Test_allocFromAllocBuffer(t, "Alloc c", &ab, 128, 1, false);

	if (!(a && b && c)) {
		AllocationBuffer_free(&ab, t->alloc);
		return;
	}

	//Free start and check if allocations at the start are possible
	//It allocates backwards from the first block to ensure it can allocate from the front multiple times.

	AllocationBuffer_freeBlock(&ab, a);

	const U8 *front = Test_allocFromAllocBuffer(t, "Front alloc", &ab, 64, 1, false);
	Test_assert(t, "Front alloc at offset 64", front == ab.buffer.ptr + 64);

	Test_assert(t, "B unaffected", ab.allocations.ptr[1].end == 256);

	AllocationBuffer_free(&ab, t->alloc);
}

void Test_allocationBufferFreeBlockInvalid(Test *t) {

	Test_setModule(t, "AllocationBuffer freeBlock invalid ptr");

	AllocationBuffer ab = { 0 };

	if (!Test_createAllocBuffer(t, 256, 0, &ab))
		return;

	const U8 *p = Test_allocFromAllocBuffer(t, "Alloc", &ab, 64, 1, false);

	if (!p) {
		AllocationBuffer_free(&ab, t->alloc);
		return;
	}

	const U64 countBefore = ab.allocations.length;

	AllocationBuffer_freeBlock(&ab, ab.buffer.ptr - 1);
	Test_assert(t, "Underflow ptr no-op", ab.allocations.length == countBefore);

	AllocationBuffer_freeBlock(&ab, ab.buffer.ptr + Buffer_length(ab.buffer));
	Test_assert(t, "Overflow ptr no-op", ab.allocations.length == countBefore);

	AllocationBuffer_freeBlock(&ab, p);

	const U64 countAfterFree = ab.allocations.length;
	AllocationBuffer_freeBlock(&ab, p);
	Test_assert(t, "Double-free no-op", ab.allocations.length == countAfterFree);

	AllocationBuffer_free(&ab, t->alloc);
}

void Test_allocationBufferNonLinearReverse(Test *t) {

	Test_setModule(t, "AllocationBuffer non-linear reverse order");

	AllocationBuffer ab = { 0 };

	if (!Test_createAllocBuffer(t, 8192, 256, &ab))
		return;

	const U8 *nonLin = Test_allocFromAllocBuffer(t, "Non-linear", &ab, 128, 1, true);
	const U8 *lin    = Test_allocFromAllocBuffer(t, "Linear",     &ab, 128, 1, false);

	if (nonLin && lin) {
		const U64 nonLinOff = nonLin - ab.buffer.ptr;
		const U64 linOff = lin  - ab.buffer.ptr;
		Test_assert(t, "Non-linear offset", nonLinOff == 0);
		Test_assert(t, "Linear offset", linOff == 256);
	}

	Test_assert(t, "Two blocks", ab.allocations.length == 2);

	if (ab.allocations.length == 2) {
		Test_assert(t, "Block[0] non-linear",  Block_isNonLinear(ab.allocations.ptr[0]));
		Test_assert(t, "Block[1] linear",      !Block_isNonLinear(ab.allocations.ptr[1]));
	}

	AllocationBuffer_free(&ab, t->alloc);
}

void Test_allocationBufferSplitThreshold(Test *t) {

	Test_setModule(t, "AllocationBuffer split threshold");

	AllocationBuffer ab = { 0 };

	if (!Test_createAllocBuffer(t, 384, 0, &ab))
		return;

	const U8 *anchor = Test_allocFromAllocBuffer(t, "Anchor", &ab, 128, 1, false);
	const U8 *hole   = Test_allocFromAllocBuffer(t, "Hole",   &ab, 128, 1, false);
	const U8 *tail   = Test_allocFromAllocBuffer(t, "Tail",   &ab, 128, 1, false);

	if (!anchor || !hole || !tail) {
		AllocationBuffer_free(&ab, t->alloc);
		return;
	}

	AllocationBuffer_freeBlock(&ab, hole);

	const U64 countAfterFree = ab.allocations.length;

	//Request 100 bytes into a 128-byte hole (100 >= 128*3/4 = 96), results in a full absorb for the allocation

	const AllocationBufferAllocate spec = {
		.allocationBuffer    = &ab,
		.alignment           = 1,
		.isNonLinearResource = false,
		.alloc               = t->alloc
	};

	const U8 *absorb = NULL;
	Test_assert(t, "Absorb alloc ok", AllocationBuffer_allocateBlock(&spec, 100, &absorb, &t->err));
	Test_assert(t, "Absorb: no extra block", ab.allocations.length == countAfterFree);

	if (absorb) AllocationBuffer_freeBlock(&ab, absorb);

	//Request 32 bytes into the 128-byte hole (32 < 128*3/4 = 96), results in a split block

	const U8 *split = NULL;
	Test_assert(t, "Split alloc ok", AllocationBuffer_allocateBlock(&spec, 32, &split, &t->err));

	Test_assert(t, "Split: extra free block", ab.allocations.length == countAfterFree + 1);

	AllocationBuffer_free(&ab, t->alloc);
}

void Test_allocationBuffer(Test *t) {
	Test_allocationBufferCreate(t);
	Test_allocationBufferSingleAlloc(t);
	Test_allocationBufferMultipleAllocs(t);
	Test_allocationBufferFreeAndReuse(t);
	Test_allocationBufferFreeAll(t);
	Test_allocationBufferOutOfMemory(t);
	Test_allocationBufferFragmentedOOM(t);
	Test_allocationBufferMerge(t);
	Test_allocationBufferFrontAlloc(t);
	Test_allocationBufferFreeBlockInvalid(t);
	Test_allocationBufferNonLinearReverse(t);
	Test_allocationBufferSplitThreshold(t);
	Test_allocationBufferAlignmentPadding(t);
	Test_allocationBufferNonLinear(t);
	Test_allocationBufferVirtual(t);
	Test_allocationBufferRefFromRegion(t);
	Test_allocationBufferAllocateAndFill(t);
}

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

//formats/oiSH/test/test_oiSH_review.c

#include "test_oiSH_shared.h"
#include "formats/oiSH/sh_headers.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_entry.h"
#include "types/container/memory_stream.h"
#include "types/container/list_basic_types.h"
#include "types/container/buffer.h"
#include "types/container/test/basic_alloc.h"
#include "types/container/test/failing_stream.h"
#include "types/base/string_read_helper.h"
#include "types/base/constants.h"

#include <stddef.h>

static Bool writeFile(const SHFile *sh, const RefPtrType *msType, MemoryStreamRef **ms, U64 *size, Test *t, Bool report) {

	U64 off = 0;
	Error *e_rr = report ? &t->err : NULL;

	if(!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, msType, ms, e_rr))
		return false;

	if(!SHFile_write((StreamRef*) *ms, &off, sh, t->alloc, e_rr))
		return false;

	*size = off;
	return true;
}

//prefix followed by the decimal digits of value

static void numberToString(C8 *dst, const C8 *prefix, U32 value) {

	U64 len = 0;

	for(; prefix[len]; ++len)
		dst[len] = prefix[len];

	C8 digits[10];
	U8 count = 0;

	do {
		digits[count++] = (C8) ('0' + value % 10);
		value /= 10;
	} while(value);

	while(count)
		dst[len++] = digits[--count];

	dst[len] = 0;
}

static SHEntry makeEntry(const C8 *name, EGfxPipelineStage stage, const U16 *binaryId, Test *t) {

	SHEntry e = (SHEntry) { .name = CharString_createRefCStrConst(name), .stage = stage };

	if(stage == EGfxPipelineStage_Compute)
		e.groupX = e.groupY = e.groupZ = 1;

	ListU16_createRefConst(binaryId, 1, &e.binaryIds, &t->err);
	return e;
}

static GfxBindings bindingFor(EGfxBinaryType type, U32 space, U32 binding) {

	GfxBindings b = GfxBindings_dummy();

	b.arr[type] = (GfxBinding) { .space = space, .binding = binding };
	return b;
}

//SHFile_combine: an entry in only one input must share a's / b's semantic names by reference.

void Test_SHFileCombineSemanticsSingleSided(Test *t) {

	Test_setModule(t, "SHFile combine: entries only in one input keep semantic names (no double free)");

	SHFile a = (SHFile) { 0 }, b = (SHFile) { 0 }, out = (SHFile) { 0 };

	Test_assert(t, "create a", Test_SHFileCreate(t, &a));
	Test_assert(t, "create b", Test_SHFileCreate(t, &b));

	SHBinaryInfo ba = makeBinaryInfo(EGfxPipelineStage_Vertex, "vsA", false);
	SHBinaryInfo bb = makeBinaryInfo(EGfxPipelineStage_Vertex, "vsB", false);

	Test_assert(t, "a binary", SHFile_addBinary(&a, &ba, t->alloc, &t->err));
	Test_assert(t, "b binary", SHFile_addBinary(&b, &bb, t->alloc, &t->err));

	const U16 zero = 0;
	CharString semA = CharString_createRefCStrConst("POSITIONA");
	CharString semB = CharString_createRefCStrConst("POSITIONB");

	SHEntry ea = makeEntry("vsA", EGfxPipelineStage_Vertex, &zero, t);

	ea.uniqueInputSemantics = 1;
	ea.inputs[0] = ESBType_F32x4;
	ea.inputSemanticNames[0] = 1 << 4;
	Test_assert(t, "a semantics", ListCharString_createRefConst(&semA, 1, &ea.semanticNames, &t->err));
	Test_assert(t, "a entry", SHFile_addEntrypoint(&a, &ea, t->alloc, &t->err));

	SHEntry eb = makeEntry("vsB", EGfxPipelineStage_Vertex, &zero, t);

	eb.uniqueInputSemantics = 1;
	eb.inputs[0] = ESBType_F32x4;
	eb.inputSemanticNames[0] = 1 << 4;
	Test_assert(t, "b semantics", ListCharString_createRefConst(&semB, 1, &eb.semanticNames, &t->err));
	Test_assert(t, "b entry", SHFile_addEntrypoint(&b, &eb, t->alloc, &t->err));

	Test_assert(t, "combine", SHFile_combine(&a, &b, t->alloc, &out, &t->err));
	Test_assert(t, "2 entries", out.entries.length == 2);

	if(out.entries.length == 2) {

		const CharString outA = out.entries.ptr[0].semanticNames.ptr[0], outB = out.entries.ptr[1].semanticNames.ptr[0];

		Test_assert(t, "a's semantic kept", CharString_equalsCStringSensitive(&outA, "POSITIONA"));
		Test_assert(t, "b's semantic kept", CharString_equalsCStringSensitive(&outB, "POSITIONB"));
	}

	//Freeing in any order has to be safe; the guard allocator and sanitizers flag a double free.

	SHFile_free(&out, t->alloc);
	Test_assert(t, "a still intact", CharString_equalsCStringSensitive(&a.entries.ptr[0].semanticNames.ptr[0], "POSITIONA"));
	SHFile_free(&a, t->alloc);
	SHFile_free(&b, t->alloc);
}

//Every write of SHFile_write has to be checked, including the one that only lands when the cursor is flushed.

void Test_SHFileWriteReportsFailingStream(Test *t) {

	Test_setModule(t, "SHFile write: a failing stream write fails the writer");

	const RefPtrType msType = MemoryStream_makeType(t->alloc);

	SHFile sh = (SHFile) { 0 };

	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo bin = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);

	Test_assert(t, "binary", SHFile_addBinary(&sh, &bin, t->alloc, &t->err));

	const U16 zero = 0;
	SHEntry e = makeEntry("cs", EGfxPipelineStage_Compute, &zero, t);

	Test_assert(t, "entry", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

	//Count the writes of a clean run

	MemoryStreamRef *ms = NULL;
	U64 off = 0;

	Test_assert(t, "stream", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &msType, &ms, &t->err));
	FailingStream_arm(ms, 0);
	Test_assert(t, "clean write", SHFile_write((StreamRef*) ms, &off, &sh, t->alloc, &t->err));

	const U64 writes = FailingStream_writes();

	RefPtr_dec(&ms);

	Test_assert(t, "writes were counted", writes > 1);

	Bool allFailed = true, allReached = true, offsetKept = true;

	for(U64 i = 1; i <= writes; ++i) {

		off = 0;

		if(!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &msType, &ms, &t->err)) {
			allFailed = false;
			break;
		}

		FailingStream_arm(ms, i);

		if(SHFile_write((StreamRef*) ms, &off, &sh, t->alloc, NULL))
			allFailed = false;

		if(!FailingStream_failed())
			allReached = false;

		if(off)
			offsetKept = false;

		RefPtr_dec(&ms);
	}

	Test_assert(t, "every failing write was reached", allReached);
	Test_assert(t, "every failing write fails SHFile_write", allFailed);
	Test_assert(t, "every failed write leaves the offset", offsetKept);

	SHFile_free(&sh, t->alloc);
}

//A binary size from the file must not drive the allocation.

static U64 reviewMaxRequest;

static Bool reviewAlloc(void *ctx, U64 length, Buffer *output, Error *e_rr) {

	(void) ctx;

	if(length > reviewMaxRequest)
		reviewMaxRequest = length;

	if(length > ((U64) 1 << 28)) {

		if(e_rr)
			*e_rr = Error_outOfMemory(0, "Test allocator cap");

		return false;
	}

	return BasicAllocator_instance.alloc(BasicAllocator_instance.ptr, length, output, e_rr);
}

static void reviewFree(void *ctx, Buffer buf) {
	(void) ctx;
	BasicAllocator_instance.free(BasicAllocator_instance.ptr, buf);
}

void Test_SHFileBoundedChecksum(Test *t) {

	Test_setModule(t, "SHFile checksum: bounded allocation and identical CRC32C");

	SHFile sh = { 0 };
	Buffer binaryData = Buffer_createNull();
	const RefPtrType msType = MemoryStream_makeType(t->alloc);
	const Allocator measured = { .alloc = reviewAlloc, .free = reviewFree };

	Test_assert(t, "create", Test_SHFileCreate(t, &sh));
	Test_assert(t, "binary data", Buffer_createUninitializedBytes(192 * KIBI, t->alloc, &binaryData, &t->err));

	if (!binaryData.ptr)
		goto clean;

	for (U64 i = 0; i < Buffer_length(binaryData); ++i)
		binaryData.ptrNonConst[i] = (U8) (i * 17 + i / 251);

	for (U16 i = 0; i < 4; ++i) {

		C8 name[16];

		numberToString(name, "cs", i);

		SHBinaryInfo bin = makeBinaryInfo(EGfxPipelineStage_Compute, name, false);

		bin.binaries[EGfxBinaryType_SPIRV] = Buffer_createRefConst(binaryData.ptr, Buffer_length(binaryData));
		Test_assert(t, "binary", SHFile_addBinary(&sh, &bin, t->alloc, &t->err));

		SHEntry entry = makeEntry(name, EGfxPipelineStage_Compute, &i, t);

		Test_assert(t, "entry", SHFile_addEntrypoint(&sh, &entry, t->alloc, &t->err));
	}

	for (U8 subFile = 0; subFile < 2; ++subFile) {

		MemoryStreamRef *stream = NULL;
		SHFile readBack = { 0 };
		U64 start = subFile ? 16 : 0, offset = start, readOff = start;

		sh.flags = subFile ? ESHSettingsFlags_HideMagicNumber : ESHSettingsFlags_None;
		Test_assert(t, "stream", MemoryStream_create(2 * MIBI, EMemoryStreamFlags_WriteResize, &msType, &stream, &t->err));
		reviewMaxRequest = 0;

		Bool written = SHFile_write(stream, &offset, &sh, &measured, &t->err);

		Test_assert(t, "write", written);
		Test_assert(t, "write checksum avoids whole-file allocation", reviewMaxRequest <= 192 * KIBI);

		if (written) {

			MemoryStream *memory = RefPtr_data(stream, MemoryStream);

			memory->parent.size = offset;

			U64 headerStart = start + (subFile ? 0 : sizeof(U32));
			U64 hashStart = headerStart + offsetof(SHHeader, uniqueDefines);
			SHHeader header;

			Buffer_memcpy(Buffer_createRef(&header, sizeof(header)), Buffer_createRefConst(
				memory->data.ptr + headerStart, sizeof(header)
			));

			Test_assert(t, "checksum matches contiguous CRC", header.hash == Buffer_crc32c(Buffer_createRefConst(
				memory->data.ptr + hashStart, offset - hashStart
			)));

			reviewMaxRequest = 0;
			Test_assert(t, "read", SHFile_read(stream, &readOff, subFile != 0, &measured, &readBack, &t->err));
			Test_assert(t, "read checksum avoids whole-file allocation", reviewMaxRequest <= 192 * KIBI);
			Test_assert(t, "all binaries read", readBack.binaries.length == 4);
			Test_assert(t, "read offset", readOff == offset);
		}

		SHFile_free(&readBack, &measured);
		RefPtr_dec(&stream);
	}

clean:
	Buffer_free(&binaryData, t->alloc);
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileReadBinarySizeBoundedByStream(Test *t) {

	Test_setModule(t, "SHFile read: declared binary size is checked against the stream before allocating");

	const RefPtrType msType = MemoryStream_makeType(t->alloc);

	SHFile sh = (SHFile) { 0 };

	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo bin = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);

	Test_assert(t, "binary", SHFile_addBinary(&sh, &bin, t->alloc, &t->err));

	const U16 zero = 0;
	SHEntry e = makeEntry("cs", EGfxPipelineStage_Compute, &zero, t);

	Test_assert(t, "entry", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

	MemoryStreamRef *ms = NULL;
	U64 size = 0;

	Test_assert(t, "write", writeFile(&sh, &msType, &ms, &size, t, true));

	const U8 *data = RefPtr_data(ms, MemoryStream)->data.ptr;

	U64 spirv = 0;

	for(; spirv + 4 <= size; ++spirv)
		if(Buffer_eq(Buffer_createRefConst(data + spirv, 4), Buffer_createRefConst(kDummySPIRV, 4)))
			break;

	Test_assert(t, "found the binary", spirv + 4 <= size && spirv);

	if(spirv + 4 <= size && spirv) {

		//The size is a U8 right before the data; widen it to a U64 claiming 4 GiB.

		Buffer forged = Buffer_createNull();

		Test_assert(t, "forged", Buffer_createUninitializedBytes(size + 7, t->alloc, &forged, &t->err));

		U64 big = (U64) 1 << 32;

		U8 *dst = forged.ptrNonConst;

		Buffer_memcpy(Buffer_createRef(dst, spirv - 1), Buffer_createRefConst(data, spirv - 1));
		Buffer_memcpy(Buffer_createRef(dst + spirv - 1, sizeof(big)), Buffer_createRefConst(&big, sizeof(big)));

		Buffer_memcpy(
			Buffer_createRef(dst + spirv - 1 + sizeof(big), size - spirv), Buffer_createRefConst(data + spirv, size - spirv)
		);

		forged.ptrNonConst[4 + offsetof(SHHeader, sizeTypes)] |= 3;        //SPIRV size is a U64

		Buffer ref = Buffer_createRefConst(forged.ptr, Buffer_length(forged));
		MemoryStreamRef *forgedStream = NULL;

		Test_assert(t, "forged stream", MemoryStream_createFromBuffer(&ref, 0, &msType, &forgedStream, &t->err));

		const Allocator capped = (Allocator) { .alloc = reviewAlloc, .free = reviewFree };
		SHFile back = (SHFile) { 0 };
		U64 readOff = 0;

		reviewMaxRequest = 0;

		const Bool read = SHFile_read((StreamRef*) forgedStream, &readOff, false, &capped, &back, NULL);

		Test_assert(t, "forged file refused", !read);
		Test_assert(t, "no allocation of the declared size", reviewMaxRequest < ((U64) 1 << 20));

		if(read)
			SHFile_free(&back, &capped);

		RefPtr_dec(&forgedStream);
		Buffer_free(&forged, t->alloc);
	}

	RefPtr_dec(&ms);
	SHFile_free(&sh, t->alloc);
}

static void checkStringTableRead(Test *t, StreamRef *stream, U64 start, const SHFile *expected, Bool fullyLoaded) {

	SHFile readBack = (SHFile) { 0 };
	Error error = (Error) { 0 };
	const Bool read = SHFile_read(stream, &start, false, t->alloc, &readBack, &error);

	Test_assert(t, "reader follows oiDL loading limits", read == fullyLoaded);

	if (!fullyLoaded) {

		const CharString message = CharString_createRefCStrConst(error.errorStr);

		Test_assert(
			t, "reader reports the string table limit", error.genericError == EGenericError_InvalidParameter &&
			CharString_equalsCStringSensitive(&message, "SHFile_read()::strings exceed oiDL automatic loading limits")
		);

		Test_assert(t, "reader does not return a partial file", !readBack.binaries.ptr && !readBack.entries.ptr);
	}

	if (read && fullyLoaded) {

		Test_assert(t, "reader consumes the complete file", start == RefPtr_data(stream, OxStream)->size);

		Bool equal = readBack.binaries.length == 1 && readBack.binaries.ptr[0].identifier.defines.length ==
			expected->binaries.ptr[0].identifier.defines.length;

		for (U64 i = 0; equal && i < expected->binaries.ptr[0].identifier.defines.length; ++i)
			equal = CharString_equalsStringSensitive(
				&readBack.binaries.ptr[0].identifier.defines.ptr[i], &expected->binaries.ptr[0].identifier.defines.ptr[i]
			);

		Test_assert(t, "every define retains its complete contents", equal);
	}

	SHFile_free(&readBack, t->alloc);
}

static void checkStringTableLimits(Test *t, U32 valueCount, U64 valueLength, Bool fullyLoaded) {

	const RefPtrType msType = MemoryStream_makeType(t->alloc);
	const U64 stringsStart = (sizeof(U32) + sizeof(SHHeader) + 15) & ~15;
	SHFile sh = (SHFile) { 0 };
	DLFile strings = (DLFile) { 0 };
	MemoryStreamRef *fixture = NULL, *forged = NULL, *written = NULL;
	U64 fixtureSize = 0, stringsEnd = stringsStart;
	C8 names[34][16], values[34][16];
	CharString defines[68];
	SHBinaryInfo bin = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);

	if (!Test_assert(t, "create", Test_SHFileCreate(t, &sh)))
		goto clean;

	for (U32 i = 0; i < valueCount; ++i) {
		numberToString(names[i], "D", i);
		numberToString(values[i], "V", i);
		defines[i * 2] = CharString_createRefCStrConst(names[i]);
		defines[i * 2 + 1] = CharString_createRefCStrConst(values[i]);
	}

	if (!Test_assert(t, "defines", ListCharString_createRefConst(defines, valueCount * 2, &bin.identifier.defines, &t->err)))
		goto clean;

	if (!Test_assert(t, "binary", SHFile_addBinary(&sh, &bin, t->alloc, &t->err)))
		goto clean;

	const U16 zero = 0;
	SHEntry entry = makeEntry("cs", EGfxPipelineStage_Compute, &zero, t);

	if (!Test_assert(t, "entry", SHFile_addEntrypoint(&sh, &entry, t->alloc, &t->err)))
		goto clean;

	//Create a valid file with short values before testing lengths the oiSH writer must refuse.

	if (!Test_assert(t, "write short-value fixture", writeFile(&sh, &msType, &fixture, &fixtureSize, t, true)))
		goto clean;

	if (!Test_assert(t, "read fixture string table", DLFile_read(
		fixture, &stringsEnd, NULL, I32x4_zero(), true, false, t->alloc, NULL, &strings, &t->err
	)))
		goto clean;

	for (U32 i = 0; i < valueCount; ++i) {

		CharString *value = &sh.binaries.ptrNonConst[0].identifier.defines.ptrNonConst[i * 2 + 1];

		CharString_free(value, t->alloc);

		if (!Test_assert(t, "create full value", CharString_create('x', valueLength, t->alloc, value, &t->err)))
			goto clean;

		//Keep values distinct so the writer cannot deduplicate away the budget under test.

		value->ptrNonConst[0] = (C8) ('A' + i % 26);
		value->ptrNonConst[1] = (C8) ('A' + i / 26);

		CharString ref = CharString_createRefStrConst(*value);

		if (!Test_assert(t, "replace fixture value", DLFile_setEntryString(&strings, valueCount + i, &ref, t->alloc, &t->err)))
			goto clean;
	}

	Test_assert(t, "case matches oiDL loading policy", DLFile_canReadFullyLoaded(&strings) == fullyLoaded);

	//A rejected write must preserve existing contents and a nonzero caller offset, without issuing any writes.

	U8 sentinel[64];

	for (U64 i = 0; i < sizeof(sentinel); ++i)
		sentinel[i] = (U8) (i ^ 0xA5);

	if (!Test_assert(t, "output stream", MemoryStream_create(
		sizeof(sentinel), EMemoryStreamFlags_WriteResize, &msType, &written, &t->err
	)))
		goto clean;

	MemoryStream *output = RefPtr_data(written, MemoryStream);
	const U64 capacity = Buffer_length(output->data);
	U64 writeOffset = 16;
	Error writeError = (Error) { 0 };

	Buffer_memcpy(output->data, Buffer_createRefConst(sentinel, sizeof(sentinel)));
	FailingStream_arm(written, 0);

	const Bool write = SHFile_write(written, &writeOffset, &sh, t->alloc, &writeError);

	Test_assert(t, "writer follows oiDL loading limits", write == fullyLoaded);

	if (!fullyLoaded) {

		const CharString message = CharString_createRefCStrConst(writeError.errorStr);

		Test_assert(
			t, "writer reports the string table limit", writeError.genericError == EGenericError_InvalidParameter &&
			CharString_equalsCStringSensitive(&message, "SHFile_write()::strings exceed oiDL automatic loading limits")
		);

		Test_assert(t, "rejected write leaves offset unchanged", writeOffset == 16);
		Test_assert(t, "rejected write makes no writes", !FailingStream_writes());
		Test_assert(
			t, "rejected write leaves stream size and capacity", output->parent.size == sizeof(sentinel) &&
			Buffer_length(output->data) == capacity
		);

		Test_assert(t, "rejected write leaves output bytes unchanged", Buffer_eq(
			Buffer_createRefConst(output->data.ptr, sizeof(sentinel)), Buffer_createRefConst(sentinel, sizeof(sentinel))
		));
	}

	else if (write)
		checkStringTableRead(t, written, 16, &sh, true);

	//Replace only the fixture's oiDL table and its padding, keeping all indices and the remaining oiSH data valid.
	//This exercises the reader independently of the oiSH writer's refusal, including an updated checksum.

	U64 forgedEnd = stringsStart;

	if (!Test_assert(t, "measure replacement string table", DLFile_write(
		&strings, t->alloc, NULL, NULL, I32x4_zero(), &forgedEnd, &t->err
	)))
		goto clean;

	const U64 fixtureTail = (stringsEnd + 15) & ~15;
	const U64 forgedTail = (forgedEnd + 15) & ~15;
	const U64 forgedSize = forgedTail + fixtureSize - fixtureTail;

	if (!Test_assert(t, "replacement stream", MemoryStream_create(
		forgedSize, EMemoryStreamFlags_WriteResize, &msType, &forged, &t->err
	)))
		goto clean;

	const U8 *source = RefPtr_data(fixture, MemoryStream)->data.ptr;
	U8 *destination = RefPtr_data(forged, MemoryStream)->data.ptrNonConst;

	Buffer_memcpy(Buffer_createRef(destination, stringsStart), Buffer_createRefConst(source, stringsStart));
	forgedEnd = stringsStart;

	if (!Test_assert(t, "write replacement string table", DLFile_write(
		&strings, t->alloc, forged, NULL, I32x4_zero(), &forgedEnd, &t->err
	)))
		goto clean;

	Buffer_memcpy(
		Buffer_createRef(destination + forgedTail, fixtureSize - fixtureTail),
		Buffer_createRefConst(source + fixtureTail, fixtureSize - fixtureTail)
	);

	const U64 hashStart = sizeof(U32) + offsetof(SHHeader, uniqueDefines);
	const U32 hash = Buffer_crc32c(Buffer_createRefConst(destination + hashStart, forgedSize - hashStart));

	Buffer_memcpy(
		Buffer_createRef(destination + sizeof(U32) + offsetof(SHHeader, hash), sizeof(hash)),
		Buffer_createRefConst(&hash, sizeof(hash))
	);

	checkStringTableRead(t, forged, 0, &sh, fullyLoaded);

clean:
	DLFile_free(&strings, t->alloc);
	RefPtr_dec(&fixture);
	RefPtr_dec(&forged);
	RefPtr_dec(&written);
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileStringTableLoadingLimits(Test *t) {

	Test_setModule(t, "SHFile string table: individual automatic loading limit");
	checkStringTableLimits(t, 1, DLFile_medLen, true);
	checkStringTableLimits(t, 1, DLFile_medLen + 1, false);

	Test_setModule(t, "SHFile string table: small entries share oiDL's cache with names");
	checkStringTableLimits(t, 31, DLFile_smallLen, true);
	checkStringTableLimits(t, 32, DLFile_smallLen, false);

	Test_setModule(t, "SHFile string table: medium allocation budget");
	checkStringTableLimits(t, 32, DLFile_medLen, true);
	checkStringTableLimits(t, 33, DLFile_medLen, false);

	//oiDL tests the medium budget before the allocation, so its final loaded entry can cross the budget.

	Test_setModule(t, "SHFile string table: final medium entry can cross the allocation budget");
	checkStringTableLimits(t, 33, DLFile_medLen - 1, true);
	checkStringTableLimits(t, 34, DLFile_medLen - 1, false);
}

//Registers of one name but a different resource type are two resources, not one.

static Bool combineRegisters(
	Test *t,
	Bool (*addA)(Test*, SHBinaryInfo*),
	Bool (*addB)(Test*, SHBinaryInfo*),
	SHFile *out
) {

	SHFile a = (SHFile) { 0 }, b = (SHFile) { 0 };
	Bool ok = Test_SHFileCreate(t, &a) && Test_SHFileCreate(t, &b);

	SHBinaryInfo ba = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);
	SHBinaryInfo bb = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);

	ok = ok && addA(t, &ba) && addB(t, &bb);
	ok = ok && SHFile_addBinary(&a, &ba, t->alloc, &t->err) && SHFile_addBinary(&b, &bb, t->alloc, &t->err);

	const U16 zero = 0;
	SHEntry ea = makeEntry("cs", EGfxPipelineStage_Compute, &zero, t);
	SHEntry eb = makeEntry("cs", EGfxPipelineStage_Compute, &zero, t);

	ok = ok && SHFile_addEntrypoint(&a, &ea, t->alloc, &t->err) && SHFile_addEntrypoint(&b, &eb, t->alloc, &t->err);
	Test_assert(t, "register combine setup", ok);

	//The failures are expected by the callers, so the error stays out of the test's sticky one.

	const Bool combined = ok && SHFile_combine(&a, &b, t->alloc, out, NULL);

	SHFile_free(&a, t->alloc);
	SHFile_free(&b, t->alloc);
	return combined;
}

static Bool addTexSrv2D(Test *t, SHBinaryInfo *bin) {

	CharString n = CharString_createRefCStrConst("res");

	return ListSHRegisterRuntime_addTexture(
		&bin->registers, ESHTextureType_Texture2D, false, false, 1, EGfxTexturePrimitive_Float, &n, NULL,
		bindingFor(EGfxBinaryType_SPIRV, 0, 0), t->alloc, &t->err
	);
}

static Bool addTexSrv3D(Test *t, SHBinaryInfo *bin) {

	CharString n = CharString_createRefCStrConst("res");

	return ListSHRegisterRuntime_addTexture(
		&bin->registers, ESHTextureType_Texture3D, false, false, 1, EGfxTexturePrimitive_Float, &n, NULL,
		bindingFor(EGfxBinaryType_DXIL, 0, 0), t->alloc, &t->err
	);
}

static Bool addTexUav3D(Test *t, SHBinaryInfo *bin) {

	CharString n = CharString_createRefCStrConst("res");

	return ListSHRegisterRuntime_addRWTexture(
		&bin->registers, ESHTextureType_Texture3D, false, 2, EGfxTexturePrimitive_Float, ETextureFormatId_Undefined, &n, NULL,
		bindingFor(EGfxBinaryType_DXIL, 0, 0), t->alloc, &t->err
	);
}

static Bool addTexUav2D(Test *t, SHBinaryInfo *bin) {

	CharString n = CharString_createRefCStrConst("res");

	return ListSHRegisterRuntime_addRWTexture(
		&bin->registers, ESHTextureType_Texture2D, false, 2, EGfxTexturePrimitive_Float, ETextureFormatId_Undefined, &n, NULL,
		bindingFor(EGfxBinaryType_DXIL, 0, 0), t->alloc, &t->err
	);
}

static Bool addByteAddressRead(Test *t, SHBinaryInfo *bin) {

	CharString n = CharString_createRefCStrConst("res");

	return ListSHRegisterRuntime_addBuffer(
		&bin->registers, ESHBufferType_ByteAddressBuffer, false, 1, &n, NULL, NULL,
		bindingFor(EGfxBinaryType_SPIRV, 0, 0), t->alloc, &t->err
	);
}

static Bool addByteAddressWrite(Test *t, SHBinaryInfo *bin) {

	CharString n = CharString_createRefCStrConst("res");

	return ListSHRegisterRuntime_addBuffer(
		&bin->registers, ESHBufferType_ByteAddressBuffer, true, 2, &n, NULL, NULL,
		bindingFor(EGfxBinaryType_DXIL, 0, 0), t->alloc, &t->err
	);
}

static Bool addSampler(Test *t, SHBinaryInfo *bin) {

	CharString n = CharString_createRefCStrConst("res");

	return ListSHRegisterRuntime_addSampler(
		&bin->registers, 1, false, &n, NULL, bindingFor(EGfxBinaryType_SPIRV, 0, 0), t->alloc, &t->err
	);
}

static Bool addSamplerCmp(Test *t, SHBinaryInfo *bin) {

	CharString n = CharString_createRefCStrConst("res");

	return ListSHRegisterRuntime_addSampler(
		&bin->registers, 2, true, &n, NULL, bindingFor(EGfxBinaryType_DXIL, 0, 0), t->alloc, &t->err
	);
}

static Bool addConstants(Test *t, SHBinaryInfo *bin, ESHBufferType type, EGfxBinaryType binary, U8 used) {

	SBFile sb = (SBFile) { 0 };
	CharString var = CharString_createRefCStrConst("v");
	CharString n = CharString_createRefCStrConst("constants");

	Bool ok =
		SBFile_create(0, 16, t->alloc, &sb, &t->err) &&
		SBFile_addVariableAsType(&sb, &var, 0, U16_MAX, ESBType_F32, 0, NULL, t->alloc, &t->err);

	GfxBindings bindings = type == ESHBufferType_PushConstants ? GfxBindings_dummy() : bindingFor(binary, 0, 0);

	if(ok)
		ok = ListSHRegisterRuntime_addBuffer(&bin->registers, type, false, used, &n, NULL, &sb, bindings, t->alloc, &t->err);

	SBFile_free(&sb, t->alloc);
	return ok;
}

static Bool addCbv(Test *t, SHBinaryInfo *bin) {
	return addConstants(t, bin, ESHBufferType_ConstantBuffer, EGfxBinaryType_DXIL, 2);
}

static Bool addPush(Test *t, SHBinaryInfo *bin) {
	return addConstants(t, bin, ESHBufferType_PushConstants, EGfxBinaryType_SPIRV, 1);
}

void Test_SHFileCombineRegisterTypeMismatch(Test *t) {

	Test_setModule(t, "SHFile combine: same register name with a different resource type is rejected");

	SHFile out = (SHFile) { 0 };

	Test_assert(t, "Texture2D + Texture3D", !combineRegisters(t, addTexSrv2D, addTexSrv3D, &out));
	Test_assert(t, "SRV Texture2D + UAV RWTexture3D", !combineRegisters(t, addTexSrv2D, addTexUav3D, &out));
	Test_assert(t, "SRV Texture2D + UAV RWTexture2D", !combineRegisters(t, addTexSrv2D, addTexUav2D, &out));
	Test_assert(t, "UAV RWTexture2D + SRV Texture2D", !combineRegisters(t, addTexUav2D, addTexSrv2D, &out));
	Test_assert(t, "ByteAddressBuffer read + write", !combineRegisters(t, addByteAddressRead, addByteAddressWrite, &out));
	Test_assert(t, "Texture + Sampler", !combineRegisters(t, addTexSrv2D, addSampler, &out));
	Test_assert(t, "Sampler + Texture", !combineRegisters(t, addSampler, addTexSrv2D, &out));
	Test_assert(t, "Buffer + Texture", !combineRegisters(t, addByteAddressRead, addTexSrv2D, &out));

	//Same type still merges

	Test_assert(t, "Texture2D + Texture2D", combineRegisters(t, addTexSrv2D, addTexSrv2D, &out));
	SHFile_free(&out, t->alloc);
}

static void expectMergedType(
	Test *t, const C8 *section, Bool (*addA)(Test*, SHBinaryInfo*), Bool (*addB)(Test*, SHBinaryInfo*), U8 type
) {

	SHFile out = (SHFile) { 0 };

	Test_assert(t, section, combineRegisters(t, addA, addB, &out));

	if(out.binaries.length == 1 && out.binaries.ptr[0].registers.length == 1)
		Test_assert(t, section, out.binaries.ptr[0].registers.ptr[0].reg.registerType == type);

	else Test_assert(t, section, false);

	SHFile_free(&out, t->alloc);
}

void Test_SHFileCombineRegisterTypePromotion(Test *t) {

	Test_setModule(t, "SHFile combine: CBV/push constants and sampler promotions don't depend on order");

	expectMergedType(t, "CBV + push", addCbv, addPush, EGfxRegisterType_PushConstants);
	expectMergedType(t, "push + CBV", addPush, addCbv, EGfxRegisterType_PushConstants);
	expectMergedType(t, "Sampler + comparison", addSampler, addSamplerCmp, EGfxRegisterType_SamplerComparisonState);
	expectMergedType(t, "comparison + Sampler", addSamplerCmp, addSampler, EGfxRegisterType_SamplerComparisonState);
}

//A combined file must hold the same registers as its inputs, hash included.

void Test_SHFileCombineKeepsRegisterHash(Test *t) {

	Test_setModule(t, "SHFile combine: copied registers keep their hash");

	SHFile a = (SHFile) { 0 }, out = (SHFile) { 0 };

	Test_assert(t, "create", Test_SHFileCreate(t, &a));

	SHBinaryInfo bin = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);

	Test_assert(t, "register", addTexSrv2D(t, &bin));
	Test_assert(t, "binary", SHFile_addBinary(&a, &bin, t->alloc, &t->err));

	const U16 zero = 0;
	SHEntry e = makeEntry("cs", EGfxPipelineStage_Compute, &zero, t);

	Test_assert(t, "entry", SHFile_addEntrypoint(&a, &e, t->alloc, &t->err));

	Test_assert(t, "combine", SHFile_combine(&a, &a, t->alloc, &out, &t->err));

	if(out.binaries.length == 1 && out.binaries.ptr[0].registers.length == 1) {

		const SHRegisterRuntime *orig = &a.binaries.ptr[0].registers.ptr[0];
		const SHRegisterRuntime *copy = &out.binaries.ptr[0].registers.ptr[0];

		Test_assert(t, "original has a hash", orig->hash != 0);
		Test_assert(t, "same hash", orig->hash == copy->hash);
		Test_assert(t, "equals", SHRegisterRuntime_equals(orig, copy));
	}

	else Test_assert(t, "one register", false);

	//A register only one side has is copied, so it keeps its hash too

	SHFile b = (SHFile) { 0 }, out2 = (SHFile) { 0 };

	Test_assert(t, "create b", Test_SHFileCreate(t, &b));

	SHBinaryInfo binB = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);
	CharString n = CharString_createRefCStrConst("other");

	Test_assert(t, "register b", ListSHRegisterRuntime_addSampler(
		&binB.registers, 1, false, &n, NULL, bindingFor(EGfxBinaryType_DXIL, 0, 0), t->alloc, &t->err
	));

	Test_assert(t, "binary b", SHFile_addBinary(&b, &binB, t->alloc, &t->err));

	SHEntry eb = makeEntry("cs", EGfxPipelineStage_Compute, &zero, t);

	Test_assert(t, "entry b", SHFile_addEntrypoint(&b, &eb, t->alloc, &t->err));
	Test_assert(t, "combine a b", SHFile_combine(&a, &b, t->alloc, &out2, &t->err));

	if(out2.binaries.length == 1) {

		ListSHRegisterRuntime regs = out2.binaries.ptr[0].registers;
		Bool allHashed = regs.length == 2;

		for(U64 i = 0; i < regs.length; ++i)
			allHashed &= regs.ptr[i].hash != 0;

		Test_assert(t, "every combined register has a hash", allHashed);
	}

	SHFile_free(&a, t->alloc);
	SHFile_free(&b, t->alloc);
	SHFile_free(&out, t->alloc);
	SHFile_free(&out2, t->alloc);
}

//Gaps in vertex inputs and outputs are stored as empty slots.

static void SHFile_testRoundTripIOGaps(Test *t, Bool withSemantics) {

	Test_setModule(t, withSemantics ?
		"SHFile round-trip: sparse vertex slots with semantics" : "SHFile round-trip: sparse vertex slots without semantics"
	);

	const RefPtrType msType = MemoryStream_makeType(t->alloc);

	SHFile sh = (SHFile) { 0 }, rt = (SHFile) { 0 };

	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo bin = makeBinaryInfo(EGfxPipelineStage_Vertex, "vs", false);

	Test_assert(t, "binary", SHFile_addBinary(&sh, &bin, t->alloc, &t->err));

	const U16 zero = 0;
	SHEntry e = makeEntry("vs", EGfxPipelineStage_Vertex, &zero, t);

	e.inputs[0] = ESBType_F32x4;
	e.inputs[3] = ESBType_F32x2;
	e.outputs[1] = ESBType_F32x4;
	e.outputs[4] = ESBType_F32;

	CharString semantics[2] = {
		CharString_createRefCStrConst("TEXCOORD"), CharString_createRefCStrConst("COLOR")
	};

	if(withSemantics) {

		e.uniqueInputSemantics = 1;
		e.inputSemanticNames[3] = 1 << 4;
		e.outputSemanticNames[4] = 1 << 4;
		Test_assert(t, "semantics", ListCharString_createRefConst(semantics, 2, &e.semanticNames, &t->err));
	}

	Test_assert(t, "entry", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

	MemoryStreamRef *ms = NULL;
	U64 size = 0, readOff = 0;

	Test_assert(t, "write", writeFile(&sh, &msType, &ms, &size, t, true));
	Test_assert(t, "read", SHFile_read((StreamRef*) ms, &readOff, false, t->alloc, &rt, &t->err));

	if(rt.entries.length == 1) {

		const SHEntry *r = &rt.entries.ptr[0];

		Test_assert(t, "input 0", r->inputs[0] == ESBType_F32x4);
		Test_assert(t, "input 1 is the gap", r->inputs[1] == 0);
		Test_assert(t, "input 2 is another gap", r->inputs[2] == 0);
		Test_assert(t, "input 3 after the gaps", r->inputs[3] == ESBType_F32x2);
		Test_assert(t, "output 0 is the gap", r->outputs[0] == 0);
		Test_assert(t, "output 1", r->outputs[1] == ESBType_F32x4);
		Test_assert(t, "output 4 after the gaps", r->outputs[4] == ESBType_F32);
		Test_assert(t, "input semantic after gaps", r->inputSemanticNames[3] == (withSemantics ? 1 << 4 : 0));
		Test_assert(t, "output semantic after gaps", r->outputSemanticNames[4] == (withSemantics ? 1 << 4 : 0));
		Test_assert(t, "equal", SHEntry_equals(&sh.entries.ptr[0], r));
	}

	else Test_assert(t, "one entry", false);

	RefPtr_dec(&ms);
	SHFile_free(&sh, t->alloc);
	SHFile_free(&rt, t->alloc);
}

void Test_SHFileRoundTripIOGaps(Test *t) {
	SHFile_testRoundTripIOGaps(t, false);
	SHFile_testRoundTripIOGaps(t, true);
}

//An unbound array [0] is an array in the array table, since arrayDimOrId 0 means 'not an array'.

void Test_SHFileRoundTripUnboundArray(Test *t) {

	Test_setModule(t, "SHFile round-trip: unbound array [0] stays an array");

	const RefPtrType msType = MemoryStream_makeType(t->alloc);

	SHFile sh = (SHFile) { 0 }, rt = (SHFile) { 0 };

	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo bin = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);

	U32 dim = 0;
	ListU32 dims = (ListU32) { 0 };
	CharString n = CharString_createRefCStrConst("tex");

	Test_assert(t, "dims", ListU32_createRefConst(&dim, 1, &dims, &t->err));
	Test_assert(t, "register", ListSHRegisterRuntime_addTexture(
		&bin.registers, ESHTextureType_Texture2D, false, false, 1, EGfxTexturePrimitive_Float, &n, &dims,
		bindingFor(EGfxBinaryType_SPIRV, 0, 0), t->alloc, &t->err
	));

	Test_assert(t, "binary", SHFile_addBinary(&sh, &bin, t->alloc, &t->err));

	const U16 zero = 0;
	SHEntry e = makeEntry("cs", EGfxPipelineStage_Compute, &zero, t);

	Test_assert(t, "entry", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

	MemoryStreamRef *ms = NULL;
	U64 size = 0, readOff = 0;

	Test_assert(t, "write", writeFile(&sh, &msType, &ms, &size, t, true));
	Test_assert(t, "read", SHFile_read((StreamRef*) ms, &readOff, false, t->alloc, &rt, &t->err));

	if(rt.binaries.length == 1 && rt.binaries.ptr[0].registers.length == 1) {

		const SHRegisterRuntime *reg = &rt.binaries.ptr[0].registers.ptr[0];

		Test_assert(t, "still an array", reg->arrays.length == 1);
		Test_assert(t, "still unbound", reg->arrays.length == 1 && reg->arrays.ptr[0] == 0);
		Test_assert(t, "extension kept", !!(rt.binaries.ptr[0].identifier.extensions & ESHExtension_UnboundArraySize));
		Test_assert(t, "equal", SHRegisterRuntime_equals(reg, &sh.binaries.ptr[0].registers.ptr[0]));
	}

	else Test_assert(t, "one register", false);

	RefPtr_dec(&ms);
	SHFile_free(&sh, t->alloc);
	SHFile_free(&rt, t->alloc);
}

static void Test_SHFileArrayTableReuse(Test *t) {

	Test_setModule(t, "SHFile array table: indexed lookup preserves order and reuses dimensions across binaries");

	SHFile sh = { 0 }, readBack = { 0 };
	MemoryStreamRef *stream = NULL;
	const RefPtrType msType = MemoryStream_makeType(t->alloc);
	U64 size = 0, readOff = 0;

	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	for(U16 binaryId = 0; binaryId < 2; ++binaryId) {

		const C8 *entryName = binaryId ? "cs1" : "cs0";
		SHBinaryInfo bin = makeBinaryInfo(EGfxPipelineStage_Compute, entryName, false);

		for(U32 i = 0; i < 129; ++i) {

			U32 dimensions[3] = { 0, binaryId ? 129 - i : i + 1, 3 };
			ListU32 array = { 0 };
			C8 name[16];

			numberToString(name, "texture", i);

			CharString regName = CharString_createRefCStrConst(name);

			Test_assert(t, "array", ListU32_createRefConst(dimensions, 3, &array, &t->err));
			Test_assert(t, "register", ListSHRegisterRuntime_addTexture(
				&bin.registers, ESHTextureType_Texture2D, false, false, 1, EGfxTexturePrimitive_Float,
				&regName, &array, bindingFor(EGfxBinaryType_SPIRV, 0, i), t->alloc, &t->err
			));
		}

		Test_assert(t, "binary", SHFile_addBinary(&sh, &bin, t->alloc, &t->err));

		SHEntry entry = makeEntry(entryName, EGfxPipelineStage_Compute, &binaryId, t);

		Test_assert(t, "entry", SHFile_addEntrypoint(&sh, &entry, t->alloc, &t->err));
	}

	if(Test_assert(t, "write", writeFile(&sh, &msType, &stream, &size, t, true))) {

		SHHeader header;
		MemoryStream *memory = RefPtr_data(stream, MemoryStream);

		Buffer_memcpy(Buffer_createRef(&header, sizeof(header)), Buffer_createRefConst(memory->data.ptr + 4, sizeof(header)));
		Test_assert(t, "129 unique arrays", header.arrayDimCount == 129);
		Test_assert(t, "read", SHFile_read(stream, &readOff, false, t->alloc, &readBack, &t->err));
	}

	Test_assert(t, "two binaries", readBack.binaries.length == 2);

	for(U64 i = 0; i < readBack.binaries.length; ++i) {

		ListSHRegisterRuntime registers = readBack.binaries.ptr[i].registers;

		Test_assert(t, "all registers", registers.length == 129);

		for(U64 j = 0; j < registers.length; ++j)
			Test_assert(t, "array contents preserved", ListU32_eq(
				registers.ptr[j].arrays, sh.binaries.ptr[i].registers.ptr[j].arrays
			));
	}

	SHFile_free(&sh, t->alloc);
	SHFile_free(&readBack, t->alloc);
	RefPtr_dec(&stream);
}

//Array ids share a U16 with the 'is an id' bit, so only 0x8000 of them fit.

void Test_SHFileWriteTooManyArrays(Test *t) {

	Test_SHFileArrayTableReuse(t);

	Test_setModule(t, "SHFile write: array ids that don't fit 15 bits are rejected");

	const RefPtrType msType = MemoryStream_makeType(t->alloc);

	SHFile sh = (SHFile) { 0 };

	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	const U64 binaries = 66, perBinary = 500;
	U32 counter = 0;

	for(U64 i = 0; i < binaries; ++i) {

		SHBinaryInfo bin = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);

		C8 value[16] = { 0 };

		numberToString(value, "", (U32) i);

		CharString defs[2] = { CharString_createRefCStrConst("I"), CharString_createRefCStrConst(value) };

		Test_assert(t, "defines", ListCharString_createRefConst(defs, 2, &bin.identifier.defines, &t->err));

		for(U64 j = 0; j < perBinary; ++j) {

			C8 name[32] = { 0 };

			numberToString(name, "t", (U32) j);

			CharString n = CharString_createRefCStrConst(name);

			//[0, a, b] is unbounded, so none of these count towards the descriptor limits

			U32 dimsArr[3] = { 0, 1 + counter / 200, 1 + counter % 200 };
			ListU32 dims = (ListU32) { 0 };

			++counter;

			if (!ListU32_createRefConst(dimsArr, 3, &dims, &t->err) || !ListSHRegisterRuntime_addTexture(
				&bin.registers, ESHTextureType_Texture2D, false, false, 1, EGfxTexturePrimitive_Float, &n, &dims,
				bindingFor(EGfxBinaryType_SPIRV, 0, (U32) j), t->alloc, &t->err
			)) {
				Test_assert(t, "register", false);
				goto clean;
			}
		}

		if(!SHFile_addBinary(&sh, &bin, t->alloc, &t->err)) {

			Test_assert(t, "binary", false);
			SHBinaryInfo_free(&bin, t->alloc);
			goto clean;
		}
	}

	{

		SHEntry e = (SHEntry) { .name = CharString_createRefCStrConst("cs"), .stage = EGfxPipelineStage_Compute };

		e.groupX = e.groupY = e.groupZ = 1;

		U16 ids[66];

		for(U16 i = 0; i < binaries; ++i)
			ids[i] = i;

		Test_assert(t, "ids", ListU16_createRefConst(ids, binaries, &e.binaryIds, &t->err));
		Test_assert(t, "entry", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

		MemoryStreamRef *ms = NULL;
		U64 off = 0;

		Test_assert(t, "stream", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &msType, &ms, &t->err));
		Test_assert(t, "write refused", !SHFile_write((StreamRef*) ms, &off, &sh, t->alloc, NULL));
		RefPtr_dec(&ms);
	}

clean:
	SHFile_free(&sh, t->alloc);
}

//A flattened count that overflows a U64 can't slip below the bindless limits.

void Test_SHFileAddBinaryArrayCountOverflow(Test *t) {

	Test_setModule(t, "SHFile addBinary: register array counts saturate");

	SHFile sh = (SHFile) { 0 };

	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo bin = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);

	U32 dims[4] = { 65536, 65536, 65536, 65536 };
	ListU32 list = (ListU32) { 0 };
	CharString n = CharString_createRefCStrConst("overflow");

	Test_assert(t, "dims", ListU32_createRefConst(dims, 4, &list, &t->err));
	Test_assert(t, "register", ListSHRegisterRuntime_addSampler(
		&bin.registers, 1, false, &n, &list, bindingFor(EGfxBinaryType_SPIRV, 0, 0), t->alloc, &t->err
	));

	Test_assert(t, "65536^4 samplers refused", !SHFile_addBinary(&sh, &bin, t->alloc, NULL));

	SHBinaryInfo_free(&bin, t->alloc);
	SHFile_free(&sh, t->alloc);

	sh = (SHFile) { 0 };
	Test_assert(t, "create DXIL file", Test_SHFileCreate(t, &sh));
	bin = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);
	bin.binaries[EGfxBinaryType_DXIL] = bin.binaries[EGfxBinaryType_SPIRV];
	bin.binaries[EGfxBinaryType_SPIRV] = Buffer_createNull();

	for (U32 i = 0; i < 2; ++i) {

		n = CharString_createRefCStrConst(i ? "one" : "saturated");
		list = (ListU32) { 0 };
		Test_assert(t, "DXIL dims", ListU32_createRefConst(dims, 4, &list, &t->err));
		Test_assert(t, "DXIL sampler", ListSHRegisterRuntime_addSampler(
			&bin.registers, 1, false, &n, i ? NULL : &list,
			bindingFor(EGfxBinaryType_DXIL, i, 0), t->alloc, &t->err
		));
	}

	Test_assert(t, "saturated DXIL samplers plus one refused", !SHFile_addBinary(&sh, &bin, t->alloc, NULL));
	SHBinaryInfo_free(&bin, t->alloc);
	SHFile_free(&sh, t->alloc);
}

//Uniform names live in their own range of the string table, so a define of the same spelling is unrelated.

void Test_SHFileRoundTripUniformNamedLikeDefine(Test *t) {

	Test_setModule(t, "SHFile round-trip: uniform named like a define name or value");

	const RefPtrType msType = MemoryStream_makeType(t->alloc);

	SHFile sh = (SHFile) { 0 }, rt = (SHFile) { 0 };

	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo bin = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);

	CharString defs[2] = { CharString_createRefCStrConst("FOO"), CharString_createRefCStrConst("BAR") };
	const TypeIdShort f32 = ETypeId_toShortId((TypeId) ETypeId_F32);
	SHUniformRuntime uniforms[2] = {
		{ .name = CharString_createRefCStrConst("BAR"), .typeIdShort = f32, .dataOffset = 0 },
		{ .name = CharString_createRefCStrConst("FOO"), .typeIdShort = f32, .dataOffset = 4 }
	};

	F32 data[2] = { 1, 2 };

	Test_assert(t, "defines", ListCharString_createRefConst(defs, 2, &bin.identifier.defines, &t->err));
	Test_assert(t, "uniforms", ListSHUniformRuntime_createRefConst(uniforms, 2, &bin.identifier.uniforms, &t->err));
	Test_assert(t, "data", ListU8_createRefConst((const U8*) data, sizeof(data), &bin.identifier.uniformData, &t->err));
	Test_assert(t, "binary", SHFile_addBinary(&sh, &bin, t->alloc, &t->err));

	const U16 zero = 0;
	SHEntry e = makeEntry("cs", EGfxPipelineStage_Compute, &zero, t);

	Test_assert(t, "entry", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

	MemoryStreamRef *ms = NULL;
	U64 size = 0, readOff = 0;

	Test_assert(t, "write", writeFile(&sh, &msType, &ms, &size, t, true));
	Test_assert(t, "read", SHFile_read((StreamRef*) ms, &readOff, false, t->alloc, &rt, &t->err));

	if(rt.binaries.length == 1 && rt.binaries.ptr[0].identifier.uniforms.length == 2) {

		const ListSHUniformRuntime u = rt.binaries.ptr[0].identifier.uniforms;

		Test_assert(t, "uniform 0", CharString_equalsCStringSensitive(&u.ptr[0].name, "BAR"));
		Test_assert(t, "uniform 1", CharString_equalsCStringSensitive(&u.ptr[1].name, "FOO"));
		Test_assert(t, "define value", CharString_equalsCStringSensitive(&rt.binaries.ptr[0].identifier.defines.ptr[1], "BAR"));
	}

	else Test_assert(t, "round-tripped", false);

	RefPtr_dec(&ms);
	SHFile_free(&sh, t->alloc);
	SHFile_free(&rt, t->alloc);
}

//Entries and binaries have to agree on the entrypoint in both directions.

void Test_SHFileAddEntryRejectsOtherEntrypointsBinary(Test *t) {

	Test_setModule(t, "SHFile addEntrypoint: binary of another entrypoint is rejected");

	SHFile sh = (SHFile) { 0 };

	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo b0 = makeBinaryInfo(EGfxPipelineStage_Compute, "a", false);
	SHBinaryInfo b1 = makeBinaryInfo(EGfxPipelineStage_Compute, "b", false);

	Test_assert(t, "binary a", SHFile_addBinary(&sh, &b0, t->alloc, &t->err));
	Test_assert(t, "binary b", SHFile_addBinary(&sh, &b1, t->alloc, &t->err));

	const U16 ids[2] = { 0, 1 };

	SHEntry wrong = makeEntry("a", EGfxPipelineStage_Compute, &ids[1], t);

	Test_assert(t, "a -> binary of b refused", !SHFile_addEntrypoint(&sh, &wrong, t->alloc, NULL));

	SHEntry both = makeEntry("a", EGfxPipelineStage_Compute, &ids[0], t);
	U16 two[2] = { 0, 1 };

	both.binaryIds = (ListU16) { 0 };
	Test_assert(t, "mixed ids", ListU16_createRefConst(two, 2, &both.binaryIds, &t->err));
	Test_assert(t, "a -> binaries of a and b refused", !SHFile_addEntrypoint(&sh, &both, t->alloc, NULL));

	SHEntry right = makeEntry("a", EGfxPipelineStage_Compute, &ids[0], t);

	Test_assert(t, "a -> binary of a accepted", SHFile_addEntrypoint(&sh, &right, t->alloc, &t->err));

	SHFile_free(&sh, t->alloc);
}

//A failed read frees the entry it was building, binaryIds included (the leak check at the end of the suite catches it).

void Test_SHFileReadBadBinaryIdFreesEntry(Test *t) {

	Test_setModule(t, "SHFile read: failing on a binary id frees the entry");

	const RefPtrType msType = MemoryStream_makeType(t->alloc);

	SHFile sh = (SHFile) { 0 }, rt = (SHFile) { 0 };

	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo bin = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);

	Test_assert(t, "binary", SHFile_addBinary(&sh, &bin, t->alloc, &t->err));

	const U16 zero = 0;
	SHEntry e = makeEntry("cs", EGfxPipelineStage_Compute, &zero, t);

	Test_assert(t, "entry", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

	MemoryStreamRef *ms = NULL;
	U64 size = 0, readOff = 0;

	Test_assert(t, "write", writeFile(&sh, &msType, &ms, &size, t, true));

	MemoryStream *mem = RefPtr_data(ms, MemoryStream);

	mem->data.ptrNonConst[size - 2] = 0xFF;        //The entry's only binary id is the last U16 of the file
	mem->data.ptrNonConst[size - 1] = 0xFF;

	Test_assert(t, "corrupt binary id refused", !SHFile_read((StreamRef*) ms, &readOff, false, t->alloc, &rt, NULL));

	RefPtr_dec(&ms);
	SHFile_free(&sh, t->alloc);
}

//A define value is one of the define values, not any string up to the register names.

void Test_SHFileReadDefineValueBound(Test *t) {

	Test_setModule(t, "SHFile read: define value can't point at a uniform name");

	const RefPtrType msType = MemoryStream_makeType(t->alloc);

	SHFile sh = (SHFile) { 0 }, rt = (SHFile) { 0 };

	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo bin = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);

	CharString defs[6] = {
		CharString_createRefCStrConst("N0"), CharString_createRefCStrConst("V0"),
		CharString_createRefCStrConst("N1"), CharString_createRefCStrConst("V1"),
		CharString_createRefCStrConst("N2"), CharString_createRefCStrConst("V2")
	};

	SHUniformRuntime uniform = {
		.name = CharString_createRefCStrConst("U"), .typeIdShort = ETypeId_toShortId((TypeId) ETypeId_F32)
	};

	F32 data = 1;

	Test_assert(t, "defines", ListCharString_createRefConst(defs, 6, &bin.identifier.defines, &t->err));
	Test_assert(t, "uniforms", ListSHUniformRuntime_createRefConst(&uniform, 1, &bin.identifier.uniforms, &t->err));
	Test_assert(t, "data", ListU8_createRefConst((const U8*) &data, sizeof(data), &bin.identifier.uniformData, &t->err));
	Test_assert(t, "binary", SHFile_addBinary(&sh, &bin, t->alloc, &t->err));

	const U16 zero = 0;
	SHEntry e = makeEntry("cs", EGfxPipelineStage_Compute, &zero, t);

	Test_assert(t, "entry", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

	MemoryStreamRef *ms = NULL;
	U64 size = 0, readOff = 0;

	Test_assert(t, "write", writeFile(&sh, &msType, &ms, &size, t, true));
	Test_assert(t, "untouched file reads", SHFile_read((StreamRef*) ms, &readOff, false, t->alloc, &rt, &t->err));
	SHFile_free(&rt, t->alloc);

	//Names [0, 1, 2] then values [0, 1, 2] as U16s; move the last value to the first string after the values.

	static const U8 pattern[12] = { 0, 0, 1, 0, 2, 0, 0, 0, 1, 0, 2, 0 };

	MemoryStream *mem = RefPtr_data(ms, MemoryStream);
	U8 *data8 = mem->data.ptrNonConst;

	U64 at = 0;

	for(; at + sizeof(pattern) <= size; ++at)
		if(Buffer_eq(Buffer_createRefConst(data8 + at, sizeof(pattern)), Buffer_createRefConst(pattern, sizeof(pattern))))
			break;

	Test_assert(t, "found the define ids", at + sizeof(pattern) <= size);

	if(at + sizeof(pattern) <= size) {

		data8[at + 10] = 3;

		//The CRC covers everything behind the header's own hash, so it has to follow the edit.

		const U64 hashStart = 4 + offsetof(SHHeader, uniqueDefines);
		const U32 hash = Buffer_crc32c(Buffer_createRefConst(data8 + hashStart, size - hashStart));

		Buffer_memcpy(
			Buffer_createRef(data8 + 4 + offsetof(SHHeader, hash), sizeof(hash)), Buffer_createRefConst(&hash, sizeof(hash))
		);

		readOff = 0;

		const Bool read = SHFile_read((StreamRef*) ms, &readOff, false, t->alloc, &rt, NULL);

		Test_assert(t, "define value past the values refused", !read);
	}

	RefPtr_dec(&ms);
	SHFile_free(&sh, t->alloc);
}

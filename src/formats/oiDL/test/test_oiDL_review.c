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

//formats/oiDL/test/test_oiDL_review.c

#include "test_oiDL_shared.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_entry.h"
#include "formats/oiDL/dl_load.h"
#include "formats/oiDL/dl_headers.h"
#include "types/container/memory_stream.h"
#include "types/container/encryption_stream.h"
#include "types/container/test/failing_stream.h"

//Entry i of the file has len bytes of value fill, loading it first if it's still in a stream.

static Bool entryIs(Test *t, DLFile *f, U64 i, U8 fill, U64 len) {

	if (!DLFile_isFullyLoaded(f, i) && !DLFile_loadEntry(f, i, t->alloc, &t->err))
		return false;

	Buffer b = Buffer_createNull();

	if (!DLFile_loadedBufferAtConst(f, i, &b, &t->err) || Buffer_length(b) != len)
		return false;

	for (U64 j = 0; j < len; ++j)
		if (b.ptr[j] != fill)
			return false;

	return true;
}

static Bool addFilled(Test *t, DLFile *f, U64 len, U8 fill) {

	Buffer b = Buffer_createNull();

	if (len && !Buffer_createUninitializedBytes(len, t->alloc, &b, &t->err))
		return false;

	if (len)
		Buffer_setAllToU8(b, fill, NULL);

	Bool ok = DLFile_addEntry(f, &b, t->alloc, &t->err);

	Buffer_free(&b, t->alloc);
	return ok;
}

//The cache holds 1 MiB at most, so a small entry that starts below 1 MiB but doesn't end there can't go in it.

static void testCacheOverflow(Test *t, const RefPtrType *memType) {

	DLSettings s = (DLSettings) { 0 };
	DLFile f = { 0 }, back = { 0 };
	StreamRef *stream = NULL;

	enum { Count = 34, Len = 32000 };

	if (!DLFile_create(&s, 0, t->alloc, &f, &t->err))
		goto clean;

	for (U8 i = 0; i < Count; ++i)
		if (!Test_assert(t, "cache overflow: add", addFilled(t, &f, Len, (U8) (i + 1))))
			goto clean;

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, memType, &stream, &t->err))
		goto clean;

	U64 off = 0;

	if (!Test_assert(t, "cache overflow: write", DLFile_write(&f, t->alloc, stream, NULL, I32x4_zero(), &off, &t->err)))
		goto clean;

	U64 readOff = 0;

	if (!Test_assert(t, "cache overflow: read", DLFile_read(
		stream, &readOff, NULL, I32x4_zero(), false, false, t->alloc, NULL, &back, &t->err
	)))
		goto clean;

	Test_assert(t, "cache overflow: count", DLFile_entryCount(&back) == Count);

	Bool content = true;

	for (U8 i = 0; i < Count; ++i)
		content &= entryIs(t, &back, i, (U8) (i + 1), Len);

	Test_assert(t, "cache overflow: content", content);

clean:
	DLFile_free(&back, t->alloc);
	DLFile_free(&f, t->alloc);
	RefPtr_dec(&stream);
}

//Every entry takes a stride of its size and the extension, and the header ends after the last stride.

static void testExtendedData(Test *t, const RefPtrType *memType) {

	U8 data[96] = { 0 };
	DLFile back = { 0 };
	StreamRef *stream = NULL;

	const U32 magic = DLHeader_MAGIC;

	Buffer_memcpy(Buffer_createRef(data, 4), Buffer_createRefConst(&magic, 4));

	data[5] = EDLFlags_HasExtendedData;
	data[7] = 1 | (1 << 4);                    //Entry count and entry sizes are U16
	data[8] = 2;                               //Entry count
	data[10] = 1;                              //Extension magic
	data[14] = 5;                              //extendedHeader
	data[16] = 6;                              //perEntryExtendedData

	//Header ext at 18 (5 bytes), entries at 23 with a stride of 8: [size U16, 6 ext bytes].

	data[23] = 1;
	data[31] = 40;

	for (U64 i = 0; i < 6; ++i) {
		data[25 + i] = 0xEE;
		data[33 + i] = 0xEE;
	}

	//Data starts at the next 16-byte boundary after the last stride (39), so 48.

	data[48] = 0xA1;
	Buffer_setAllToU8(Buffer_createRef(data + 49, 40), 0xB2, NULL);

	if (!MemoryStream_createFromBufferRegion(
		Buffer_createRefConst(data, sizeof(data)), 0, 89, EMemoryStreamFlags_None, memType, &stream, &t->err
	))
		goto clean;

	U64 off = 0;

	if (!Test_assert(t, "extended: read", DLFile_read(
		stream, &off, NULL, I32x4_zero(), false, false, t->alloc, NULL, &back, &t->err
	)))
		goto clean;

	Test_assert(t, "extended: count", DLFile_entryCount(&back) == 2);
	Test_assert(t, "extended: entry 0", entryIs(t, &back, 0, 0xA1, 1));
	Test_assert(t, "extended: entry 1", entryIs(t, &back, 1, 0xB2, 40));
	Test_assert(t, "extended: end offset", off == 89);

clean:
	DLFile_free(&back, t->alloc);
	RefPtr_dec(&stream);
}

//Stream backed entries stay in the combined file, in order.

static void testCombineLazy(Test *t, const RefPtrType *memType) {

	DLSettings s = (DLSettings) { 0 };
	DLFile a = { 0 }, b = { 0 }, lazy = { 0 }, c = { 0 };
	StreamRef *stream = NULL;

	if (!DLFile_create(&s, 0, t->alloc, &a, &t->err) || !DLFile_create(&s, 0, t->alloc, &b, &t->err))
		goto clean;

	for (U8 i = 0; i < 3; ++i) {
		addFilled(t, &a, 10 + i, (U8) (1 + i));
		addFilled(t, &b, 20 + i, (U8) (50 + i));
	}

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, memType, &stream, &t->err))
		goto clean;

	U64 off = 0, readOff = 0;

	if (!DLFile_write(&b, t->alloc, stream, NULL, I32x4_zero(), &off, &t->err) || !DLFile_read(
		stream, &readOff, NULL, I32x4_zero(), false, true, t->alloc, NULL, &lazy, &t->err
	))
		goto clean;

	Test_assert(t, "combine lazy: combine", DLFile_combine(&a, &lazy, t->alloc, &c, &t->err));
	Test_assert(t, "combine lazy: count", DLFile_entryCount(&c) == 6);

	Test_assert(t, "combine lazy: a loaded", DLFile_isFullyLoaded(&c, 0) && DLFile_isFullyLoaded(&c, 2));
	Test_assert(t, "combine lazy: b in stream", !DLFile_isFullyLoaded(&c, 3) && !DLFile_isFullyLoaded(&c, 5));

	Test_assert(
		t, "combine lazy: sizes",
		DLFile_entrySize(&c, 2) == 12 && DLFile_entrySize(&c, 3) == 20 && DLFile_entrySize(&c, 5) == 22
	);

	Bool content = true;

	for (U8 i = 0; i < 3; ++i) {
		content &= entryIs(t, &c, i, (U8) (1 + i), 10 + i);
		content &= entryIs(t, &c, 3 + i, (U8) (50 + i), 20 + i);
	}

	Test_assert(t, "combine lazy: content", content);

clean:
	DLFile_free(&c, t->alloc);
	DLFile_free(&lazy, t->alloc);
	DLFile_free(&b, t->alloc);
	DLFile_free(&a, t->alloc);
	RefPtr_dec(&stream);
}

//An empty entry that's in a stream copies nothing (a zero length copy would mean the rest of the stream).

static void testZeroLengthStreams(Test *t, const RefPtrType *memType, Bool subFile) {

	DLSettings s = (DLSettings) { .flags = subFile ? EDLSettingsFlags_HideMagicNumber : EDLSettingsFlags_None };
	DLFile f = { 0 }, lazy = { 0 }, back = { 0 };
	StreamRef *st1 = NULL, *st2 = NULL;
	const U64 sizes[] = { 100, 0, 200, 50, 0 };

	if (!DLFile_create(&s, 0, t->alloc, &f, &t->err))
		goto clean;

	for (U8 i = 0; i < 5; ++i)
		addFilled(t, &f, sizes[i], (U8) (i + 1));

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, memType, &st1, &t->err))
		goto clean;

	U64 off = 0, readOff = 0;

	if (!Test_assert(t, "zero stream: write", DLFile_write(&f, t->alloc, st1, NULL, I32x4_zero(), &off, &t->err)))
		goto clean;

	//Something after the file, that a zero length copy would pick up.

	if (subFile) {

		U8 junk[5000];

		Buffer_setAllToU8(Buffer_createRef(junk, sizeof(junk)), 0xEE, NULL);

		Stream_write(
			RefPtr_data(st1, OxStream), off, sizeof(junk), Buffer_createRefConst(junk, sizeof(junk)), t->alloc, &t->err
		);
	}

	if (!Test_assert(t, "zero stream: read", DLFile_read(
		st1, &readOff, NULL, I32x4_zero(), subFile, true, t->alloc, NULL, &lazy, &t->err
	)))
		goto clean;

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, memType, &st2, &t->err))
		goto clean;

	U64 off2 = 0;

	Test_assert(t, "zero stream: subfile flag preserved", lazy.settings.flags == s.flags);

	if (!Test_assert(t, "zero stream: rewrite", DLFile_write(&lazy, t->alloc, st2, NULL, I32x4_zero(), &off2, &t->err)))
		goto clean;

	Test_assert(t, "zero stream: same size", off2 == off && RefPtr_data(st2, OxStream)->size == off);

	readOff = 0;

	if (!Test_assert(t, "zero stream: read rewritten", DLFile_read(
		st2, &readOff, NULL, I32x4_zero(), subFile, false, t->alloc, NULL, &back, &t->err
	)))
		goto clean;

	Bool content = DLFile_entryCount(&back) == 5;

	for (U8 i = 0; i < 5 && content; ++i)
		content &= entryIs(t, &back, i, (U8) (i + 1), sizes[i]);

	Test_assert(t, "zero stream: content", content);

	Test_assert(t, "zero stream: load empty entry", DLFile_loadEntry(&lazy, 1, t->alloc, &t->err));
	Test_assert(t, "zero stream: empty entry loaded", DLFile_isFullyLoaded(&lazy, 1) && DLFile_entrySize(&lazy, 1) == 0);

clean:
	DLFile_free(&back, t->alloc);
	DLFile_free(&lazy, t->alloc);
	DLFile_free(&f, t->alloc);
	RefPtr_dec(&st1);
	RefPtr_dec(&st2);
}

//Strings own their memory, whether they were read into a medium allocation or loaded from a stream.

static Bool stringIs(DLFile *f, U64 i, U64 len) {

	CharString str = CharString_createNull();

	if (!DLFile_loadedStringAtConst(f, i, &str, NULL) || CharString_length(str) != len)
		return false;

	for (U64 j = 0; j < len; ++j)
		if (str.ptr[j] != 'a')
			return false;

	return true;
}

static void testStrings(Test *t, const RefPtrType *memType) {

	DLSettings s = (DLSettings) { .dataType = EDLDataType_String };
	DLFile f = { 0 }, back = { 0 };
	StreamRef *stream = NULL;
	const U64 sizes[] = { 10, 40000, 200000 }; //Small (cache), medium (own allocation) and large (stream)

	if (!DLFile_create(&s, 0, t->alloc, &f, &t->err))
		goto clean;

	for (U8 i = 0; i < 3; ++i) {

		CharString str = CharString_createNull();

		if (
			!CharString_create('a', sizes[i], t->alloc, &str, &t->err) ||
			!DLFile_addEntryString(&f, &str, t->alloc, &t->err)
		) {
			CharString_free(&str, t->alloc);
			goto clean;
		}
	}

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, memType, &stream, &t->err))
		goto clean;

	U64 off = 0;

	if (!DLFile_write(&f, t->alloc, stream, NULL, I32x4_zero(), &off, &t->err))
		goto clean;

	U64 readOff = 0;

	if (!Test_assert(t, "strings: read", DLFile_read(
		stream, &readOff, NULL, I32x4_zero(), false, false, t->alloc, NULL, &back, &t->err
	)))
		goto clean;

	Test_assert(t, "strings: small and medium loaded", stringIs(&back, 0, sizes[0]) && stringIs(&back, 1, sizes[1]));

	if (!Test_assert(t, "strings: large is deferred", !DLFile_isFullyLoaded(&back, 2)))
		goto clean;

	const U64 largeOffset = back.entryStreams.ptr[2].dataOff;

	Test_assert(t, "strings: load large", DLFile_loadEntry(&back, 2, t->alloc, &t->err) && stringIs(&back, 2, sizes[2]));
	DLFile_free(&back, t->alloc);

	//Deferred strings must pass the same validation as strings loaded during DLFile_read.

	const U8 invalidBytes[] = { 0xFF, 0 };

	for (U8 i = 0; i < sizeof(invalidBytes); ++i) {

		if (!Stream_write(
			RefPtr_data(stream, OxStream), largeOffset + sizes[2] / 2, 1,
			Buffer_createRefConst(&invalidBytes[i], 1), t->alloc, &t->err
		))
			goto clean;

		readOff = 0;

		if (!Test_assert(t, "strings: read deferred invalid string", DLFile_read(
			stream, &readOff, NULL, I32x4_zero(), false, false, t->alloc, NULL, &back, &t->err
		)))
			goto clean;

		DLEntryStream deferred = back.entryStreams.ptr[2];

		Test_assert(t, "strings: reject invalid deferred string", !DLFile_loadEntry(&back, 2, t->alloc, NULL));
		Test_assert(t, "strings: failed load keeps entry deferred",
			!DLFile_isFullyLoaded(&back, 2) && !back.entryStrings.ptr[2].ptr &&
			back.entryStreams.ptr[2].stream == deferred.stream &&
			back.entryStreams.ptr[2].dataOff == deferred.dataOff &&
			back.entryStreams.ptr[2].len == sizes[2]
		);

		DLFile_free(&back, t->alloc);
	}

	const U8 validByte = 'a';

	if (!Stream_write(
		RefPtr_data(stream, OxStream), largeOffset + sizes[2] / 2, 1,
		Buffer_createRefConst(&validByte, 1), t->alloc, &t->err
	))
		goto clean;

	readOff = 0;

	if (!Test_assert(t, "strings: read in streams", DLFile_read(
		stream, &readOff, NULL, I32x4_zero(), false, true, t->alloc, NULL, &back, &t->err
	)))
		goto clean;

	Bool content = true;

	for (U8 i = 0; i < 3; ++i)
		content &= DLFile_loadEntry(&back, i, t->alloc, &t->err) && stringIs(&back, i, sizes[i]);

	Test_assert(t, "strings: loaded from streams", content);

clean:
	DLFile_free(&back, t->alloc);
	DLFile_free(&f, t->alloc);
	RefPtr_dec(&stream);
}

//A failed write leaves the caller's offset and a generated key alone.

static Bool keyIsZero(const DLFile *f) {

	for (U8 i = 0; i < 8; ++i)
		if (f->settings.encryptionKey[i])
			return false;

	return true;
}

static void testFailedEncryptedWrite(Test *t, const RefPtrType *memType, const RefPtrType *encType) {

	DLSettings s = (DLSettings) { .encryptionType = EXXEncryptionType_AES256GCM };
	DLFile f = { 0 };
	StreamRef *stream = NULL;
	const U64 start = 16;

	if (!DLFile_create(&s, 0, t->alloc, &f, &t->err))
		goto clean;

	for (U8 i = 0; i < 3; ++i)
		if (!addFilled(t, &f, 100, (U8) (i + 1)))
			goto clean;

	//Count the writes of a succeeding write first.

	U64 writes = 0;

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, memType, &stream, &t->err))
		goto clean;

	FailingStream_arm(stream, 0);

	U64 off = start;

	if (!Test_assert(t, "failed write: write", DLFile_write(&f, t->alloc, stream, encType, I32x4_zero(), &off, &t->err)))
		goto clean;

	writes = FailingStream_writes();
	Test_assert(t, "failed write: generated key reaches the settings", !keyIsZero(&f));
	Test_assert(t, "failed write: offset moves on success", off > start);

	RefPtr_dec(&stream);

	U32 failures = 0;

	for (U64 n = 1; n <= writes; ++n) {

		Buffer_unsetAllBits(Buffer_createRef(f.settings.encryptionKey, sizeof(f.settings.encryptionKey)), NULL);

		if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, memType, &stream, &t->err))
			goto clean;

		FailingStream_arm(stream, n);
		off = start;

		if (!DLFile_write(&f, t->alloc, stream, encType, I32x4_zero(), &off, NULL)) {

			++failures;
			Test_assert(t, "failed write: offset untouched", off == start);
			Test_assert(t, "failed write: key not published", keyIsZero(&f));
		}

		RefPtr_dec(&stream);
	}

	Test_assert(t, "failed write: failures were hit", failures > 0);

clean:
	DLFile_free(&f, t->alloc);
	RefPtr_dec(&stream);
}

//Frees that still hold a run of the plaintext marker, counted while a window is open.

typedef struct PlaintextWatch {
	const Allocator *inner;
	U8 marker;
	Bool watching;
	U64 leaks;
} PlaintextWatch;

static Bool PlaintextWatch_alloc(void *ptr, U64 length, Buffer *output, Error *e_rr) {
	const PlaintextWatch *watch = (const PlaintextWatch*) ptr;
	return watch->inner->alloc(watch->inner->ptr, length, output, e_rr);
}

static void PlaintextWatch_free(void *ptr, Buffer buf) {

	PlaintextWatch *watch = (PlaintextWatch*) ptr;

	U64 run = 0;

	for (U64 i = 0; watch->watching && i < Buffer_length(buf); ++i) {

		run = buf.ptr[i] == watch->marker ? run + 1 : 0;

		if (run == 32) {
			++watch->leaks;
			break;
		}
	}

	watch->inner->free(watch->inner->ptr, buf);
}

//A decrypted file's entries are the plaintext of an encrypted one: freeing it must not hand them back readable, in the
// cache, as an entry of its own, or as a string.

static void testFreeWipesDecrypted(Test *t, const RefPtrType *memType, const RefPtrType *encType, Bool strings) {

	const C8 *label = strings ? "free wipes (strings)" : "free wipes (data)";
	const U8 marker = 'Z';
	const U64 lengths[] = { 200, 40000, 100, 150000 };

	DLSettings s = (DLSettings) {
		.encryptionType = EXXEncryptionType_AES256GCM, .dataType = strings ? EDLDataType_String : EDLDataType_Data
	};

	DLFile f = { 0 }, back = { 0 };
	StreamRef *stream = NULL;

	PlaintextWatch watch = (PlaintextWatch) { .inner = t->alloc, .marker = marker };
	Allocator alloc = (Allocator) { .ptr = &watch, .alloc = PlaintextWatch_alloc, .free = PlaintextWatch_free };

	if (!Test_assert(t, label, DLFile_create(&s, 0, t->alloc, &f, &t->err)))
		goto clean;

	for (U64 i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {

		Bool ok = true;

		if (strings) {

			CharString str = CharString_createNull();
			ok = CharString_create(marker, lengths[i], t->alloc, &str, &t->err) &&
				DLFile_addEntryString(&f, &str, t->alloc, &t->err);

			CharString_free(&str, t->alloc);
		}

		else ok = addFilled(t, &f, lengths[i], marker);

		if (!Test_assert(t, label, ok))
			goto clean;
	}

	U64 off = 0;

	if (
		!Test_assert(t, label, MemoryStream_create(0, EMemoryStreamFlags_WriteResize, memType, &stream, &t->err)) ||
		!Test_assert(t, label, DLFile_write(&f, t->alloc, stream, encType, I32x4_zero(), &off, &t->err))
	)
		goto clean;

	off = 0;

	if (!Test_assert(t, label, DLFile_read(
		stream, &off, f.settings.encryptionKey, I32x4_zero(), false, false, &alloc, encType, &back, &t->err
	)))
		goto clean;

	for (U64 i = 0; i < DLFile_entryCount(&back); ++i)
		if (!DLFile_isFullyLoaded(&back, i) && !Test_assert(t, label, DLFile_loadEntry(&back, i, &alloc, &t->err)))
			goto clean;

	watch.watching = true;
	DLFile_free(&back, &alloc);
	watch.watching = false;

	Test_assert(t, label, watch.leaks == 0);

clean:
	DLFile_free(&back, &alloc);
	DLFile_free(&f, t->alloc);
	RefPtr_dec(&stream);
}

//Offset + length that wraps around is out of bounds.

static void testWrappingRange(Test *t, const RefPtrType *memType) {

	DLSettings s = (DLSettings) { 0 };
	DLFile f = { 0 };
	StreamRef *stream = NULL;

	if (!DLFile_create(&s, 0, t->alloc, &f, &t->err))
		goto clean;

	if (!MemoryStream_create(16, EMemoryStreamFlags_IsWritable, memType, &stream, &t->err))
		goto clean;

	Test_assert(t, "wrap: addEntryStream", !DLFile_addEntryStream(&f, &stream, U64_MAX, 2, t->alloc, NULL));
	Test_assert(t, "wrap: addEntryStream keeps the stream", stream != NULL && DLFile_entryCount(&f) == 0);
	Test_assert(t, "wrap: addEntryStream fits", DLFile_addEntryStream(&f, &stream, 14, 2, t->alloc, &t->err));

	if (!MemoryStream_create(16, EMemoryStreamFlags_IsWritable, memType, &stream, &t->err))
		goto clean;

	Test_assert(t, "wrap: setStream offset", !DLFile_setStream(&f, 0, &stream, U64_MAX, 2, t->alloc, NULL));
	Test_assert(t, "wrap: setStream length", !DLFile_setStream(&f, 0, &stream, 2, U64_MAX, t->alloc, NULL));
	Test_assert(t, "wrap: setStream fits", DLFile_setStream(&f, 0, &stream, 2, 14, t->alloc, &t->err));

clean:
	DLFile_free(&f, t->alloc);
	RefPtr_dec(&stream);
}

void Test_DLReview(Test *t) {

	Test_setModule(t, "DLFile_review");

	const RefPtrType memType = MemoryStream_makeType(t->alloc);
	const RefPtrType encType = EncryptionStream_makeType(t->alloc);

	testCacheOverflow(t, &memType);
	testExtendedData(t, &memType);
	testCombineLazy(t, &memType);
	testZeroLengthStreams(t, &memType, false);
	testZeroLengthStreams(t, &memType, true);
	testStrings(t, &memType);
	testFailedEncryptedWrite(t, &memType, &encType);
	testFreeWipesDecrypted(t, &memType, &encType, false);
	testFreeWipesDecrypted(t, &memType, &encType, true);
	testWrappingRange(t, &memType);
}

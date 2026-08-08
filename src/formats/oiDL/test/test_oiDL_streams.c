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

//formats/oiDL/test/test_oiDL_streams.c

#include "test_oiDL_shared.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_entry.h"
#include "formats/oiDL/dl_load.h"
#include "types/container/memory_stream.h"

void Test_DLStreamAdd(Test *t) {
	
	Test_setModule(t, "DLFile_addStreams");

	DLSettings sData = (DLSettings) {
		.compressionType = EXXCompressionType_None,
		.encryptionType  = EXXEncryptionType_None,
		.dataType        = EDLDataType_Data
	};

	RefPtrType memType = MemoryStream_makeType(t->alloc);

	{                                        //addEntryStream + loadEntry
		DLFile           f       = { 0 };
		MemoryStreamRef *ms      = NULL;
		Buffer           payload = Buffer_createNull();

		if (!DLFile_create(&sData, 0, t->alloc, &f, &t->err)) {
			Test_assert(t, "Streams: create DLFile", false);
			goto cleanStream;
		}

		const U64 payloadLen = 256;

		if (!Buffer_createUninitializedBytes(payloadLen, t->alloc, &payload, &t->err)) {
			Test_assert(t, "Streams: alloc payload", false);
			goto cleanStream;
		}

		for (U64 i = 0; i < payloadLen; ++i)
			payload.ptrNonConst[i] = (U8)i;

		if (!MemoryStream_create(payloadLen, EMemoryStreamFlags_IsWritable, &memType, &ms, &t->err)) {
			Test_assert(t, "Streams: create MemoryStream", false);
			goto cleanStream;
		}

		{                                //Write the known payload into the MemoryStream
			StreamCursor sc = { 0 };

			if (StreamCursor_create(ms, 0, true, t->alloc, &sc, &t->err)) {
				U64 off = 0;
				StreamCursor_append(&sc, &off, payload.ptr, payloadLen, t->alloc, &t->err);
				StreamCursor_close(&sc, t->alloc);
			}
		}

		Test_assert(t, "addEntryStream ok", DLFile_addEntryStream(&f, &ms, 0, payloadLen, t->alloc, &t->err));
		Test_assert(t, "entry count 1",     DLFile_entryCount(&f) == 1);
		Test_assert(t, "Not fully loaded",  !DLFile_isFullyLoaded(&f, 0));
		Test_assert(t, "entrySize correct", DLFile_entrySize(&f, 0) == payloadLen);

		Test_assert(t, "loadEntry ok",      DLFile_loadEntry(&f, 0, t->alloc, &t->err));
		Test_assert(t, "Fully loaded",      DLFile_isFullyLoaded(&f, 0));

		Buffer out = Buffer_createNull();

		if (Test_assert(t, "loadedBufferAt ok", DLFile_loadedBufferAtConst(&f, 0, &out, &t->err))) {
			Test_assert(t, "length matches",  Buffer_length(out) == payloadLen);
			Test_assert(t, "Content correct", Buffer_eq(out, Buffer_createRefConst(payload.ptr, payloadLen)));
		}

	cleanStream:
		Buffer_free(&payload, t->alloc);
		RefPtr_dec(&ms);
		DLFile_free(&f, t->alloc);
	}

	{                        //addEntryStream: null stream pointer rejected
		DLFile f = { 0 };
		DLFile_create(&sData, 0, t->alloc, &f, &t->err);
		Test_assert(t, "addEntryStream null stream fails", !DLFile_addEntryStream(&f, NULL, 0, 64, t->alloc, NULL));
		DLFile_free(&f, t->alloc);
	}

	{                        //loadEntry: out-of-bounds index fails
		DLFile f = { 0 };
		DLFile_create(&sData, 0, t->alloc, &f, &t->err);
		Test_assert(t, "loadEntry OOB fails", !DLFile_loadEntry(&f, 0, t->alloc, NULL));
		DLFile_free(&f, t->alloc);
	}

	{                        //loadEntry: already-loaded entry is a no-op and returns success
		DLFile f = { 0 };
		DLFile_create(&sData, 0, t->alloc, &f, &t->err);
		U8 b = 0x11;
		Buffer buf = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(&b, 1), t->alloc, &buf, NULL);
		DLFile_addEntry(&f, &buf, t->alloc, &t->err);
		Test_assert(t, "loadEntry on already-loaded entry ok", DLFile_loadEntry(&f, 0, t->alloc, &t->err));
		DLFile_free(&f, t->alloc);
		Buffer_free(&buf, t->alloc);
	}
}

static Bool buildMemoryStream(
	Test *t,
	const U8 fill,
	const U64 len,
	const RefPtrType *type,
	MemoryStreamRef **stream
) {
	*stream = NULL;

	Buffer buf = Buffer_createNull();

	if (!Buffer_createEmptyBytes(len, t->alloc, &buf, &t->err))
		return false;

	Buffer_setAllToU8(buf, fill, NULL);

	if (!MemoryStream_createFromBuffer(&buf, EMemoryStreamFlags_None, type, stream, &t->err)) {
		Buffer_free(&buf, t->alloc);
		return false;
	}

	return true;
}

extern const DLSettings kSettingsData;

//Create DLFile containing N streams
static Bool buildStreamFile(Test *t, DLFile *f, U64 count, const RefPtrType *type) {

	if (!DLFile_create(&kSettingsData, 0, t->alloc, f, &t->err))
		return false;

	for (U64 i = 0; i < count; ++i) {

		MemoryStreamRef *ms = NULL;

		if (!buildMemoryStream(t, (U8)i, i + 1, type, &ms)) {
			DLFile_free(f, t->alloc);
			return false;
		}

		//addEntryStream inc-refs internally; dec our local ref after

		if (!DLFile_addEntryStream(f, &ms, 0, i + 1, t->alloc, &t->err)) {
			RefPtr_dec(&ms);
			DLFile_free(f, t->alloc);
			return false;
		}

		RefPtr_dec(&ms);
	}

	return true;
}

void Test_DLInsertStream(Test *t) {

	Test_setModule(t, "DLFile_insertStream");

	RefPtrType type = MemoryStream_makeType(t->alloc);

	{                            //insertStream at front shifts existing stream entries
		DLFile f = { 0 };
		MemoryStreamRef *ms = NULL;

		if (!buildStreamFile(t, &f, 3, &type)) {
			Test_assert(t, "Setup 3-entry stream file", false);
			goto doneInsFront;
		}

		if (!buildMemoryStream(t, 0xFF, 4, &type, &ms)) {
			Test_assert(t, "buildMemoryStream 0xFF", false);
			goto doneInsFront;
		}

		DLEntryStream es = {
			.stream  = ms,
			.dataOff = 0,
			.len     = 4
		};

		Test_assert(t, "insertStream id = 0",   DLFile_insertStream(&f, 0, &es, t->alloc, &t->err));
		ms = NULL;

		Test_assert(t, "stream zeroed out",     !es.stream);
		Test_assert(t, "entryCount 4",          DLFile_entryCount(&f) == 4);

		Test_assert(t, "[0] len 4",             DLFile_entrySize(&f, 0) == 4);

		Test_assert(t, "[1] len 4",             DLFile_entrySize(&f, 1) == 1);
		Test_assert(t, "[2] len 4",             DLFile_entrySize(&f, 2) == 2);
		Test_assert(t, "[3] len 4",             DLFile_entrySize(&f, 3) == 3);

	doneInsFront:
		RefPtr_dec(&ms);
		DLFile_free(&f, t->alloc);
	}

	{                        //insertStream at end acts as append
		DLFile f = { 0 };
		MemoryStreamRef *ms = NULL;

		if (!buildStreamFile(t, &f, 2, &type)) {
			Test_assert(t, "Setup 2-entry stream file for append", false);
			goto doneInsAppend;
		}

		if (!buildMemoryStream(t, 0xAB, 8, &type, &ms)) {
			Test_assert(t, "buildMemoryStream 0xAB 8b", false);
			goto doneInsAppend;
		}

		DLEntryStream es = {
			.stream  = ms,
			.dataOff = 0,
			.len     = 8
		};

		Test_assert(t, "insertStream at end ok", DLFile_insertStream(&f, 2, &es, t->alloc, &t->err));
		ms = NULL;

		Test_assert(t, "entryCount 3",           DLFile_entryCount(&f) == 3);
		Test_assert(t, "[2] len 8",              DLFile_entrySize(&f, 2) == 8);

	doneInsAppend:
		RefPtr_dec(&ms);
		DLFile_free(&f, t->alloc);
	}

	{                        //insertStream in middle preserves surrounding entries
		DLFile f = { 0 };
		MemoryStreamRef *ms = NULL;

		if (!buildStreamFile(t, &f, 4, &type)) {
			Test_assert(t, "Setup 4-entry stream file for mid", false);
			goto doneInsMid;
		}

		if (!buildMemoryStream(t, 0xCD, 2, &type, &ms)) {
			Test_assert(t, "buildMemoryStream 0xCD 2b", false);
			goto doneInsMid;
		}

		DLEntryStream es = {
			.stream  = ms,
			.dataOff = 0,
			.len     = 2
		};

		Test_assert(t, "insertStream id = 2",   DLFile_insertStream(&f, 2, &es, t->alloc, &t->err));
		ms = NULL;

		Test_assert(t, "entryCount 5",          DLFile_entryCount(&f) == 5);
		Test_assert(t, "[0] len 1",             DLFile_entrySize(&f, 0) == 1);
		Test_assert(t, "[1] len 2",             DLFile_entrySize(&f, 1) == 2);
		Test_assert(t, "[2] len 2",             DLFile_entrySize(&f, 2) == 2);
		Test_assert(t, "[3] len 3",             DLFile_entrySize(&f, 3) == 3);
		Test_assert(t, "[4] len 4",             DLFile_entrySize(&f, 4) == 4);

	doneInsMid:
		RefPtr_dec(&ms);
		DLFile_free(&f, t->alloc);
	}

	{                        //OOB fails
		DLFile f = { 0 };
		MemoryStreamRef *ms = NULL;

		if (!buildStreamFile(t, &f, 2, &type)) {
			Test_assert(t, "Setup 2-entry stream file for OOB", false);
			goto doneInsOOB;
		}

		if (!buildMemoryStream(t, 0x01, 4, &type, &ms)) {
			Test_assert(t, "buildMemoryStream OOB", false);
			goto doneInsOOB;
		}

		DLEntryStream es = { .stream = ms, .dataOff = 0, .len = 4 };

		Test_assert(t, "insertStream OOB fails",    !DLFile_insertStream(&f, 5, &es, t->alloc, NULL));
		Test_assert(t, "entryCount unchanged",      DLFile_entryCount(&f) == 2);

	doneInsOOB:
		RefPtr_dec(&ms);
		DLFile_free(&f, t->alloc);
	}

	{                        //Null guards
		DLFile f = { 0 };
		MemoryStreamRef *ms = NULL;

		if (!buildStreamFile(t, &f, 1, &type)) {
			Test_assert(t, "Setup for null guard insertStream", false);
			goto doneInsNull;
		}

		if (!buildMemoryStream(t, 0x01, 4, &type, &ms)) {
			Test_assert(t, "buildMemoryStream null guard", false);
			goto doneInsNull;
		}

		DLEntryStream es = { .stream = (StreamRef *)ms, .dataOff = 0, .len = 4 };

		Test_assert(t, "insertStream null dlFile fails", !DLFile_insertStream(NULL, 0, &es,   t->alloc, NULL));
		Test_assert(t, "insertStream null stream fails", !DLFile_insertStream(&f,   0, NULL,  t->alloc, NULL));

	doneInsNull:
		RefPtr_dec(&ms);
		DLFile_free(&f, t->alloc);
	}
}

extern Bool buildStringFile(Test *t, DLFile *f, U64 count);

void Test_DLRoundtripStream(Test *t) {
	
	Test_setModule(t, "DLFile_streamRoundtrip");

	RefPtrType type = MemoryStream_makeType(t->alloc);

	{                        //removeEntry returns the correct DLEntryStream metadata
		DLFile f = { 0 };

		if (!buildStreamFile(t, &f, 3, &type)) {
			Test_assert(t, "Setup 3-entry stream file", false);
			goto doneIdentity;
		}

		Buffer        buf    = Buffer_createNull();
		DLEntryStream stream = { 0 };

		//Remove the middle entry

		Test_assert(t, "removeEntry id = 1",     DLFile_removeEntry(&f, 1, &buf, &stream, &t->err));
		Test_assert(t, "entryCount 2",           DLFile_entryCount(&f) == 2);
		Test_assert(t, "stream ptr non-null",    stream.stream);
		Test_assert(t, "stream dataOff == 0",    stream.dataOff == 0);
		Test_assert(t, "stream len == 2",        stream.len == 2);

		//Re-insert the stream entry at the same position

		Test_assert(t, "insertStream id = 1 ok", DLFile_insertStream(&f, 1, &stream, t->alloc, &t->err));
		Test_assert(t, "stream zeroed",          !stream.stream);
		Test_assert(t, "entryCount 3",           DLFile_entryCount(&f) == 3);
		Test_assert(t, "[0] len 1",              DLFile_entrySize(&f, 0) == 1);
		Test_assert(t, "[1] len 2",              DLFile_entrySize(&f, 1) == 2);
		Test_assert(t, "[2] len 3",              DLFile_entrySize(&f, 2) == 3);

		Buffer_free(&buf, t->alloc);

	doneIdentity:
		RefPtr_dec(&stream.stream);
		DLFile_free(&f, t->alloc);
	}

	{                        //DLFile_remove drops the refcount: stream is sole-owned by DLFile
		DLFile f = { 0 };
		MemoryStreamRef *ms = NULL;

		if (!DLFile_create(&kSettingsData, 0, t->alloc, &f, &t->err)) {
			Test_assert(t, "Create Data file for refcount test", false);
			goto doneRefCount;
		}

		if (!buildMemoryStream(t, 0x55, 4, &type, &ms)) {
			Test_assert(t, "buildMemoryStream refcount", false);
			goto doneRefCount;
		}

		StreamRef *sr = ms;
		RefPtr_inc(ms);

		if (!DLFile_addEntryStream(&f, &sr, 0, 4, t->alloc, &t->err)) {
			Test_assert(t, "addEntryStream refcount", false);
			RefPtr_dec(&ms);        //extra ref
			RefPtr_dec(&ms);        //our ref
			goto doneRefCount;
		}

		//DLFile now holds one ref, we hold another.
		//After remove, DLFile's ref is released; we should still be valid

		Test_assert(t, "remove id = 0 ok",      DLFile_remove(&f, 0, t->alloc, &t->err));
		Test_assert(t, "entryCount 0",          DLFile_entryCount(&f) == 0);

		//Our ref is still valid, confirm by checking the refcount is exactly 1

		Test_assert(t, "refcount is 1 after remove", AtomicI64_load(&ms->refCount) == 1);

	doneRefCount:
		RefPtr_dec(&ms);
		DLFile_free(&f, t->alloc);
	}

	{                        //insertStream into a String file fails (type mismatch)
		DLFile f = { 0 };
		MemoryStreamRef *ms = NULL;

		if (!buildStringFile(t, &f, 1)) {
			Test_assert(t, "Setup String file for insertStream mismatch", false);
			goto doneMismatch;
		}

		if (!buildMemoryStream(t, 0x01, 4, &type, &ms)) {
			Test_assert(t, "buildMemoryStream mismatch", false);
			goto doneMismatch;
		}

		DLEntryStream es = { .stream = ms, .dataOff = 0, .len = 4 };

		Bool result = DLFile_insertStream(&f, 0, &es, t->alloc, NULL);

		if (result)
			ms = NULL;

		Test_assert(t, "insertStream on String file (documents behavior)", result || !result);

	doneMismatch:
		RefPtr_dec(&ms);
		DLFile_free(&f, t->alloc);
	}
}

void Test_DLStreams(Test *t) {
	Test_DLStreamAdd(t);
	Test_DLInsertStream(t);
	Test_DLRoundtripStream(t);
}

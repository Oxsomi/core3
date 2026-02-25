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

#include "formats/oiDL/dl_file.h"
#include "types/container/memory_stream.h"
#include "shared.h"

void Test_DLStreams(Test *t) {

	Test_setModule(t, "DLFile_streams");

	DLSettings sData = (DLSettings) {
		.compressionType = EXXCompressionType_None,
		.encryptionType  = EXXEncryptionType_None,
		.dataType        = EDLDataType_Data
	};

	RefPtrType memType = MemoryStream_makeType(t->alloc);

	{										//addEntryStream + loadEntry
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

		{								//Write the known payload into the MemoryStream
			StreamCursor sc = { 0 };

			if (StreamCursor_create(ms, 0, true, t->alloc, &sc, &t->err)) {
				U64 off = 0;
				StreamCursor_append(&sc, &off, payload.ptr, payloadLen, t->alloc, &t->err);
				StreamCursor_close(&sc, t->alloc);
			}
		}

		Test_assert(t, "addEntryStream ok", DLFile_addEntryStream(&f, ms, 0, payloadLen, t->alloc, &t->err));
		Test_assert(t, "entry count 1",     DLFile_entryCount(&f) == 1);
		Test_assert(t, "Not fully loaded",  !DLFile_isFullyLoaded(&f, 0));
		Test_assert(t, "entrySize correct", DLFile_entrySize(&f, 0) == payloadLen);

		Test_assert(t, "loadEntry ok",      DLFile_loadEntry(&f, 0, t->alloc, &t->err));
		Test_assert(t, "Fully loaded",      DLFile_isFullyLoaded(&f, 0));

		Buffer out = Buffer_createNull();

		if (Test_assert(t, "loadedBufferAt ok", DLFile_loadedBufferAt(&f, 0, &out, &t->err))) {
			Test_assert(t, "length matches",  Buffer_length(out) == payloadLen);
			Test_assert(t, "Content correct", Buffer_eq(out, Buffer_createRefConst(payload.ptr, payloadLen)));
		}

	cleanStream:
		Buffer_free(&payload, t->alloc);
		RefPtr_dec(&ms);
		DLFile_free(&f, t->alloc);
	}

	{						//addEntryStream: null stream pointer rejected
		DLFile f = { 0 };
		DLFile_create(&sData, 0, t->alloc, &f, &t->err);
		Test_assert(t, "addEntryStream null stream fails", !DLFile_addEntryStream(&f, NULL, 0, 64, t->alloc, NULL));
		DLFile_free(&f, t->alloc);
	}

	{						//loadEntry: out-of-bounds index fails
		DLFile f = { 0 };
		DLFile_create(&sData, 0, t->alloc, &f, &t->err);
		Test_assert(t, "loadEntry OOB fails", !DLFile_loadEntry(&f, 0, t->alloc, NULL));
		DLFile_free(&f, t->alloc);
	}

	{						//loadEntry: already-loaded entry is a no-op and returns success
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

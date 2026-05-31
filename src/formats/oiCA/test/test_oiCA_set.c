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

//formats/oiCA/test/test_oiCA_set.c

#include "test_oiCA_shared.h"
#include "types/container/memory_stream.h"
#include "formats/oiCA/ca_props.h"
#include "formats/oiCA/ca_lookup.h"

CAHandle addFile(Test *t, CAFile *ca, CAHandle parent, const C8 *name, Ns time, Bool failIsSuccess);
CAHandle addFolder(Test *t, CAFile *ca, CAHandle parent, const C8 *name, Bool failIsSuccess);

static inline CAHandle CAFile_resolveCStr(CAFile *ca, const C8 *name) {
	return CAFile_resolve(ca, CharString_createRefCStrConst(name));
}

extern const CASettings kCASettings;
extern const CASettings kCASettingsDate;

void Test_CASetData(Test *t) {

	Test_setModule(t, "CAFile_setData");

	{
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;
		Buffer buf = Buffer_createNull();

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for setData", false);
			goto doneSetData;
		}

		CAHandle hf = addFile(t, &ca, root, "data.bin", 0, false);

		//Initially empty

		Test_assert(t, "initial size 0",         CAFile_fileSize(&ca, hf) == 0);

		//Set data

		U8 bytes[4] = { 0xDE, 0xAD, 0xBE, 0xEF };

		if (!Buffer_createCopy(Buffer_createRefConst(bytes, 4), t->alloc, &buf, &t->err)) {
			Test_assert(t, "Buffer_createCopy for setData", false);
			goto doneSetData;
		}

		Test_assert(t, "setData ok",             CAFile_setData(&ca, hf, t->alloc, &buf, &t->err));
		Test_assert(t, "buf zeroed out",         buf.ptr == NULL);
		Test_assert(t, "fileSize 4",             CAFile_fileSize(&ca, hf) == 4);

		//getData returns correct content

		Bool isValid = false;
		Buffer got   = CAFile_getDataConst(&ca, hf, &isValid);
		Test_assert(t, "getData valid",          isValid);
		Test_assert(t, "getData length 4",       Buffer_length(got) == 4);

		//Replace data with different size

		U8 bytes2[2] = { 0xAA, 0xBB };
		Buffer buf2   = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(bytes2, 2), t->alloc, &buf2, NULL);

		Test_assert(t, "setData replace ok",     CAFile_setData(&ca, hf, t->alloc, &buf2, &t->err));
		Test_assert(t, "fileSize now 2",         CAFile_fileSize(&ca, hf) == 2);

		//setData on a folder handle must fail

		CAHandle hDir = addFolder(t, &ca, root, "dir", false);
		Buffer buf3   = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(bytes, 1), t->alloc, &buf3, NULL);

		Test_assert(t, "setData on folder fails", !CAFile_setData(&ca, hDir, t->alloc, &buf3, NULL));
		Buffer_free(&buf3, t->alloc);

		//Null guards

		Buffer buf4 = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(bytes, 1), t->alloc, &buf4, NULL);
		Test_assert(t, "setData null caFile",    !CAFile_setData(NULL, hf, t->alloc, &buf4, NULL));
		Test_assert(t, "setData null buf",       !CAFile_setData(&ca, hf, t->alloc, NULL,  NULL));
		Buffer_free(&buf4, t->alloc);

	doneSetData:
		Buffer_free(&buf, t->alloc);
		CAFile_free(&ca, t->alloc);
	}
}

void Test_CASetTime(Test *t) {

	Test_setModule(t, "CAFile_setTime");

	//Basic round-trip with IncludeDate flag

	{
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettingsDate, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for setTime", false);
			goto doneSetTime;
		}

		Ns ts = (Ns)1234567 * MS;        //Must be expressible in the 48-bit ms field
		CAHandle hf = addFile(t, &ca, root, "timed.txt", ts, false);

		Test_assert(t, "initial time preserved", CAFile_fileTime(&ca, hf) == ts);

		//Update time

		Ns ts2 = (Ns)9999999 * MS;
		Test_assert(t, "setTime ok",             CAFile_setTime(&ca, hf, ts2, &t->err));
		Test_assert(t, "time updated",           CAFile_fileTime(&ca, hf) == ts2);

		//setTime on folder must fail

		CAHandle hDir = addFolder(t, &ca, root, "dir", false);
		Test_assert(t, "setTime on folder fails", !CAFile_setTime(&ca, hDir, ts2, NULL));

		//Null guard

		Test_assert(t, "setTime null caFile",    !CAFile_setTime(NULL, hf, ts2, NULL));

	doneSetTime:
		CAFile_free(&ca, t->alloc);
	}

	//setTime on a CAFile created WITHOUT a date flag, should be a no-op or fail
	//gracefully (never crash); the stored time must not become a garbage value.

	{
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca nodate for setTime", false);
			goto doneSetTimeNoDate;
		}

		CAHandle hf = addFile(t, &ca, root, "nodate.txt", 0, false);

		//Whether it succeeds or fails, it must not crash and fileTime must be
		//either 0 (unchanged) or the requested value, never an uninitialised
		//or out-of-range garbage number.

		Ns ts = (Ns)5000 * MS;
		Bool ok = CAFile_setTime(&ca, hf, ts, NULL);
		Ns got  = CAFile_fileTime(&ca, hf);

		if (ok)
			Test_assert(t, "setTime nodate: if ok, value stored", got == ts);

		else Test_assert(t, "setTime nodate: if fail, value unchanged", got == 0);

	doneSetTimeNoDate:
		CAFile_free(&ca, t->alloc);
	}
}

void Test_CASetStream(Test *t) {

	Test_setModule(t, "CAFile_setStream");

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	//OxStream: set then read back

	{
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for stream", false);
			goto doneStream;
		}

		CAHandle hf = addFile(t, &ca, root, "stream.bin", 0, false);

		//Before setting a stream the file is loaded (empty buffer)

		Test_assert(t, "isLoaded before stream", CAFile_isLoaded(&ca, hf));

		//Create a MemoryStream of 8 bytes

		StreamRef *sr = NULL;
		if (!MemoryStream_create(8, EMemoryStreamFlags_None, &type, &sr, &t->err)) {
			Test_assert(t, "MemoryStream_create", false);
			goto doneStream;
		}

		Test_assert(t, "setDataStream ok", CAFile_setDataStream(&ca, hf, t->alloc, &sr, 0, 8, &t->err));
		Test_assert(t, "sr moved to NULL", sr == NULL);
		RefPtr_dec(&sr);

		//After setting a stream, isLoaded returns false

		Test_assert(t, "isLoaded after stream", !CAFile_isLoaded(&ca, hf));

		//getDataStream returns the ref (increments refcount)

		U64 streamOff = U64_MAX;
		sr = CAFile_getDataStream(&ca, hf, &streamOff);
		Test_assert(t, "getDataStream non-null", sr != NULL);
		Test_assert(t, "getDataStream offset 0", streamOff == 0);
		RefPtr_dec(&sr);

		//setDataStream with offset + length subset

		U8 streamData2[16];
		for (U64 i = 0; i < 16; ++i) streamData2[i] = (U8)(i * 2);
		MemoryStream_create(16, EMemoryStreamFlags_None, &type, &sr, NULL);
		Test_assert(t, "setDataStream subset ok", CAFile_setDataStream(&ca, hf, t->alloc, &sr, 4, 8, &t->err));
		RefPtr_dec(&sr);

		streamOff = U64_MAX;
		sr = CAFile_getDataStream(&ca, hf, &streamOff);
		Test_assert(t, "subset stream non-null", sr != NULL);
		Test_assert(t, "subset stream offset 4", streamOff == 4);
		RefPtr_dec(&sr);

		//setDataStream: out-of-bounds offset+len must fail
		//4 + 8 > 8

		MemoryStream_create(8, EMemoryStreamFlags_None, &type, &sr, NULL);
		Test_assert(t, "stream OOB fails", !CAFile_setDataStream(&ca, hf, t->alloc, &sr, 4, 8, NULL));
		Test_assert(t, "sr not consumed", sr != NULL);        //OxStream must not be consumed on failure
		RefPtr_dec(&sr);

		//setDataStream: folder handle must fail

		CAHandle hDir = addFolder(t, &ca, root, "sdir", false);
		MemoryStream_create(8, EMemoryStreamFlags_None, &type, &sr, NULL);
		Test_assert(t, "stream on folder fails", !CAFile_setDataStream(&ca, hDir, t->alloc, &sr, 0, 8, NULL));
		RefPtr_dec(&sr);

		//Null guards

		MemoryStream_create(8, EMemoryStreamFlags_None, &type, &sr, NULL);
		Test_assert(t, "stream null caFile", !CAFile_setDataStream(NULL, hf, t->alloc, &sr, 0, 8, NULL));
		Test_assert(t, "stream null stream", !CAFile_setDataStream(&ca, hf, t->alloc, NULL,  0, 8, NULL));
		RefPtr_dec(&sr);

	doneStream:
		RefPtr_dec(&sr);
		CAFile_free(&ca, t->alloc);
	}

	//OxStream survives copy (independent refcount, not shared pointer)

	{
		CAFile src  = { 0 };
		CAFile copy = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &src, &t->err)) {
			Test_assert(t, "Create src for stream copy", false);
			goto doneStreamCopy;
		}

		CAHandle hf = addFile(t, &src, root, "streamed.bin", 0, false);

		StreamRef *sr = NULL;
		if (!MemoryStream_create(4, EMemoryStreamFlags_None, &type, &sr, &t->err)) {
			Test_assert(t, "MemoryStream_create for stream copy", false);
			goto doneStreamCopy;
		}

		Test_assert(t, "setDataStream for copy test ok", CAFile_setDataStream(&src, hf, t->alloc, &sr, 0, 4, &t->err));

		//Copy the CAFile, the copy must hold an independent reference to the stream

		Test_assert(t, "createCopy with stream ok", CAFile_createCopy(&src, t->alloc, &copy, &t->err));

		CAHandle hfCopy = CAFile_resolveCStr(&copy, "streamed.bin");
		Test_assert(t, "copy resolves streamed.bin", hfCopy != CAHandle_Invalid);

		//Both src and copy report isLoaded == false (stream-backed)

		Test_assert(t, "src isLoaded false",  !CAFile_isLoaded(&src,  hf));
		Test_assert(t, "copy isLoaded false", !CAFile_isLoaded(&copy, hfCopy));

		//getDataStream on copy returns a non-null ref with offset 0

		U64 copyOff = U64_MAX;
		StreamRef *copyStream = CAFile_getDataStream(&copy, hfCopy, &copyOff);
		Test_assert(t, "copy stream non-null", copyStream != NULL);
		Test_assert(t, "copy stream offset 0", copyOff == 0);

		//Replace the stream in copy with a plain buffer; src stream must be unaffected

		U8 newData[2] = { 0x11, 0x22 };
		Buffer newBuf = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(newData, 2), t->alloc, &newBuf, &t->err);
		CAFile_setData(&copy, hfCopy, t->alloc, &newBuf, NULL);

		//src still stream-backed, copy is now buffer-backed

		Test_assert(t, "src still stream-backed", !CAFile_isLoaded(&src, hf));
		Test_assert(t, "copy now buffer-backed",   CAFile_isLoaded(&copy, hfCopy));

		RefPtr_dec(&copyStream);    //Release the ref obtained above

	doneStreamCopy:
		RefPtr_dec(&sr);
		CAFile_free(&src,  t->alloc);
		CAFile_free(&copy, t->alloc);
	}
}

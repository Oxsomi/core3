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

#include "formats/oiDL/interface.h"
#include "types/container/memory_stream.h"
#include "types/container/encryption_stream.h"
#include "types/base/string_read_helper.h"
#include "shared.h"

//Write DLFile to stream and readback

static Bool DLFile_testRoundtrip(
	const DLFile *dlFile,
	Bool isSubFile,
	const U32 encryptionKey[8],
	const RefPtrType *encStreamType,
	const RefPtrType *memoryStreamType,
	Test *t,
	DLFile *out
) {
	MemoryStreamRef  *ms = NULL;
	Bool             ok  = false;

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, memoryStreamType, &ms, &t->err))
		goto clean;

	U64 off = 0;

	if (!DLFile_write(dlFile, t->alloc, ms, encStreamType, I32x4_zero(), &off, &t->err))
		goto clean;

	U64 readOff = 0;
	ok = DLFile_read(ms, readOff, encryptionKey, I32x4_zero(), isSubFile, t->alloc, encStreamType, out, &t->err);

clean:
	RefPtr_dec(&ms);
	return ok;
}


void Test_DLRoundtripPlain(Test *t) {

	Test_setModule(t, "DLFile_roundtripPlain");

	const RefPtrType memStreamType = MemoryStream_makeType(t->alloc);

	DLSettings sData = (DLSettings) {
		.compressionType = EXXCompressionType_None,
		.encryptionType  = EXXEncryptionType_None,
		.dataType        = EDLDataType_Data
	};

	I32x4 iv = I32x4_zero();

	DLSettings sStr = sData;
	sStr.dataType = EDLDataType_String;

	{									//Single buffer
		DLFile f = { 0 }, f2 = { 0 };
		const U8 bytes[] = { 0x11, 0x22, 0x33, 0x44, 0x55 };
		Buffer buf = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(bytes, sizeof(bytes)), t->alloc, &buf, NULL);

		if (!DLFile_create(&sData, 0, t->alloc, &f, &t->err) || !DLFile_addEntry(&f, &buf, t->alloc, &t->err))
			Test_assert(t, "Single buffer: setup", false);

		else if (Test_assert(
			t, "Single buffer: write+read", DLFile_testRoundtrip(&f, false, NULL, NULL, &memStreamType, t, &f2))
		) {

			Test_assert(t, "Single buffer: entryCount 1", DLFile_entryCount(&f2) == 1);
			Test_assert(t, "Single buffer: size correct", DLFile_entrySize(&f2, 0) == sizeof(bytes));

			Buffer out = Buffer_createNull();

			if (DLFile_isFullyLoaded(&f2, 0) && DLFile_loadedBufferAtConst(&f2, 0, &out, &t->err))
				Test_assert(t, "Single buffer: content correct", Buffer_eq(out, Buffer_createRefConst(bytes, sizeof(bytes))));
		}

		Buffer_free(&buf, t->alloc);
		DLFile_free(&f,   t->alloc);
		DLFile_free(&f2,  t->alloc);
	}

	{									//200 buffers of varying size (0 – 256 bytes)
		DLFile f = { 0 }, f2 = { 0 };

		if (!DLFile_create(&sData, 0, t->alloc, &f, &t->err)) {
			Test_assert(t, "200 buffers: create", false);
			goto skip200;
		}

		Bool addOk = true;

		for (U64 i = 0; i < 200 && addOk; ++i) {
			U64    sz  = (i * 3) % 257;
			Buffer buf = Buffer_createNull();

			if (sz) {
				Buffer_createUninitializedBytes(sz, t->alloc, &buf, &t->err);
				Buffer_setAllToU8(buf, (U8)i, NULL);
			}

			addOk = DLFile_addEntry(&f, &buf, t->alloc, &t->err);
			Buffer_free(&buf, t->alloc);
		}

		Test_assert(t, "200 buffers: add all", addOk);

		if (addOk && Test_assert(
			t, "200 buffers: roundtrip", DLFile_testRoundtrip(&f, false, NULL, NULL, &memStreamType, t, &f2)
		)) {
			Test_assert(t, "200 buffers: entryCount", DLFile_entryCount(&f2) == 200);

			Bool sizesOk = true;
			Bool contentOk = true;

			U8 mem[256];
			Buffer buf = Buffer_createRef(mem, sizeof(mem));

			for (U64 i = 0; i < 200 && sizesOk && contentOk; ++i) {

				Buffer_setAllToU8(buf, (U8)i, NULL);

				U64 siz = DLFile_entrySize(&f2, i);

				if (siz != (i * 3) % 257)
					sizesOk = false;

				contentOk = Buffer_eq(Buffer_createRefConst(mem, siz), f2.entryBuffers.ptr[i]);
			}

			Test_assert(t, "200 buffers: all sizes correct", sizesOk);
			Test_assert(t, "200 buffers: all data correct", contentOk);
		}

	skip200:
		DLFile_free(&f,  t->alloc);
		DLFile_free(&f2, t->alloc);
	}

	{						//20 strings
		DLFile f = { 0 }, f2 = { 0 };

		if (!DLFile_create(&sStr, 0, t->alloc, &f, &t->err)) {
			Test_assert(t, "Strings: create", false);
			goto skipStr;
		}

		C8  messages[20][48];
		Bool  addOk = true;

		for (U8 i = 0; i < 20 && addOk; ++i) {

			CharString str = CharString_createNull();
			addOk = CharString_format(t->alloc, &str, &t->err, "String entry %04d", i);

			Buffer_memcpy(Buffer_createRef(messages[i], sizeof(messages[0])), CharString_bufferConst(str));

			messages[i][CharString_length(str)] = '\0';
			
			if(addOk)
				addOk = DLFile_addEntryString(&f, &str, t->alloc, &t->err);

			CharString_free(&str, t->alloc);
		}

		Test_assert(t, "Strings: add all", addOk);

		if (addOk && Test_assert(
			t, "Strings: roundtrip", DLFile_testRoundtrip(&f, false, NULL, NULL, &memStreamType, t, &f2))
		) {
			Bool readOk = true;

			for (int i = 0; i < 20 && readOk; ++i) {
				CharString out      = CharString_createNull();
				CharString expected = CharString_createRefCStrConst(messages[i]);

				if (!DLFile_loadedStringAtConst(&f2, (U64)i, &out, &t->err)) {
					readOk = false; break;
				}

				if (!CharString_equalsStringSensitive(&out, &expected))
					readOk = false;
			}

			Test_assert(t, "Strings: all content correct", readOk);
		}

	skipStr:
		DLFile_free(&f,  t->alloc);
		DLFile_free(&f2, t->alloc);
	}

	{						//10 empty (zero-length) buffer entries
		DLFile f = { 0 }, f2 = { 0 };
		DLFile_create(&sData, 0, t->alloc, &f, &t->err);

		for (U8 i = 0; i < 10; ++i) {
			Buffer empty = Buffer_createNull();
			DLFile_addEntry(&f, &empty, t->alloc, &t->err);
		}

		if (Test_assert(t, "Empty entries: roundtrip", DLFile_testRoundtrip(&f, false, NULL, NULL, &memStreamType, t, &f2))) {

			Test_assert(t, "Empty entries: count", DLFile_entryCount(&f2) == 10);

			Bool zeroOk = true;

			for (U8 i = 0; i < 10 && zeroOk; ++i)
				if (DLFile_entrySize(&f2, (U64)i) != 0)
					zeroOk = false;

			Test_assert(t, "Empty entries: all sizes 0", zeroOk);
		}

		DLFile_free(&f,  t->alloc);
		DLFile_free(&f2, t->alloc);
	}

	{						//HideMagicNumber / isSubFile mode (like in oiCA)
		DLFile f = { 0 }, f2 = { 0 }, f3 = { 0 };
		DLSettings sHide = sData;
		sHide.flags |= EDLSettingsFlags_HideMagicNumber;
		MemoryStreamRef *ms = NULL;

		U8 b = 0x42;
		Buffer buf = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(&b, 1), t->alloc, &buf, NULL);
		DLFile_create(&sHide, 0, t->alloc, &f, &t->err);
		DLFile_addEntry(&f, &buf, t->alloc, &t->err);

		if (!MemoryStream_create(1 * MIBI, EMemoryStreamFlags_WriteResize, &memStreamType, &ms, &t->err)) {
			Test_assert(t, "SubFile: create stream", false);
			goto cleanSubFile;
		}

		U64 off = 0;

		if (!DLFile_write(&f, t->alloc, ms, NULL, iv, &off, &t->err)) {
			Test_assert(t, "SubFile: write", false);
			goto cleanSubFile;
		}

		U64 readOff = 0;
		Test_assert(t, "SubFile: read as subfile", DLFile_read(ms, readOff, NULL, iv, true, t->alloc, NULL, &f2, &t->err));
		Test_assert(t, "SubFile: entryCount 1", DLFile_entryCount(&f2) == 1);
		Test_assert(t, "SubFile: entrySize[0] 1", DLFile_entrySize(&f2, 0) == 1);

		readOff = 0;
		Test_assert(
			t, "SubFile: non-subfile read fails without magic",
			!DLFile_read(ms, readOff, NULL, iv, false, t->alloc, NULL, &f3, NULL)
		);

	cleanSubFile:
		Buffer_free(&buf, t->alloc);
		RefPtr_dec(&ms);
		DLFile_free(&f,  t->alloc);
		DLFile_free(&f2, t->alloc);
		DLFile_free(&f3, t->alloc);
	}

	{						//Reading null stream fails safely
		DLFile f = { 0 };
		Test_assert(t, "read null stream fails", !DLFile_read(NULL, 0, NULL, iv, false, t->alloc, NULL, &f, NULL));
	}

	{						//Reading into already-allocated dlFile fails
		DLFile f = { 0 }, g = { 0 };
		DLFile_create(&sData, 0, t->alloc, &f, &t->err);
		U8 b = 1;
		Buffer buf = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(&b, 1), t->alloc, &buf, NULL);
		DLFile_addEntry(&f, &buf, t->alloc, &t->err);

		MemoryStreamRef *ms = NULL;
		MemoryStream_create(1 * MIBI, EMemoryStreamFlags_WriteResize, &memStreamType, &ms, &t->err);
		U64 off = 0;
		DLFile_write(&f, t->alloc, ms, NULL, iv, &off, &t->err);
		DLFile_create(&sData, 0, t->alloc, &g, &t->err);
		U64 readOff = 0;
		Test_assert(
			t, "Read into allocated dlFile fails", !DLFile_read(ms, readOff, NULL, iv, false, t->alloc, NULL, &g, NULL)
		);

		Buffer_free(&buf, t->alloc);
		RefPtr_dec(&ms);
		DLFile_free(&f, t->alloc);
		DLFile_free(&g, t->alloc);
	}

	{						//Read: bad magic number fails
		DLFile f = { 0 };
		U8 garbage[32];
		Buffer_setAllToU8(Buffer_createRef(garbage, 32), 0xBB, NULL);
		MemoryStreamRef *ms = NULL;

		if (MemoryStream_create(32, EMemoryStreamFlags_IsWritable, &memStreamType, &ms, &t->err)) {
			StreamCursor sc = { 0 };

			if (StreamCursor_create(ms, 0, true, t->alloc, &sc, &t->err)) {
				U64 off = 0;
				StreamCursor_append(&sc, &off, garbage, 32, t->alloc, &t->err);
				StreamCursor_close(&sc, t->alloc);
			}

			U64 readOff = 0;
			Test_assert(t, "read bad magic fails", !DLFile_read(ms, readOff, NULL, iv, false, t->alloc, NULL, &f, NULL));
		}

		RefPtr_dec(&ms);
		DLFile_free(&f, t->alloc);
	}
}

void Test_DLRoundtripEncrypted(Test *t) {

	Test_setModule(t, "DLFile_roundtripEncrypted");

	const RefPtrType memStreamType = MemoryStream_makeType(t->alloc);
	const RefPtrType encStreamType = EncryptionStream_makeType(t->alloc);
	I32x4 iv = I32x4_zero();

	const U32 goodKey[8] = {
		0x01234567, 0x89ABCDEF, 0xFEDCBA98, 0x76543210,
		0x11223344, 0x55667788, 0x99AABBCC, 0xDDEEFF00
	};

	const U32 badKey[8] = { 0xCAFEBABE, 0xDEAD1234, 0, 0, 0, 0, 0, 0 };

	DLSettings s = (DLSettings) {
		.compressionType = EXXCompressionType_None,
		.encryptionType  = EXXEncryptionType_AES256GCM,
		.dataType        = EDLDataType_Data
	};

	Buffer_memcpy(
		Buffer_createRef(s.encryptionKey, sizeof(goodKey)),
		Buffer_createRefConst(goodKey, sizeof(goodKey))
	);

	{									//5x 32 byte buffers
		DLFile f = { 0 }, f2 = { 0 };

		if (!DLFile_create(&s, 0, t->alloc, &f, &t->err)) {
			Test_assert(t, "Encrypted: create", false);
			goto skipEnc;
		}

		Bool addOk = true;

		for (U8 i = 0; i < 5 && addOk; ++i) {
			Buffer buf = Buffer_createNull();
			Buffer_createUninitializedBytes(32, t->alloc, &buf, &t->err);
			Buffer_setAllToU8(buf, (U8)(0xA0 + i), NULL);
			addOk = DLFile_addEntry(&f, &buf, t->alloc, &t->err);
			Buffer_free(&buf, t->alloc);
		}

		Test_assert(t, "Encrypted: add 5 entries", addOk);

		if (!addOk)
			goto skipEnc;

		MemoryStreamRef *ms = NULL;

		if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &memStreamType, &ms, &t->err)) {
			Test_assert(t, "Encrypted: create stream", false);
			goto skipEnc;
		}

		U64 off = 0;

		if (!Test_assert(t, "Encrypted: write", DLFile_write(&f, t->alloc, ms, &encStreamType, iv, &off, &t->err)))
			goto cleanEnc;

		{
			U64 readOff = 0;

			if (!Test_assert(
				t, "Encrypted: read back", DLFile_read(ms, readOff, goodKey, iv, false, t->alloc, &encStreamType, &f2, &t->err)
			))
				goto cleanEnc;
		}

		Test_assert(t, "Encrypted: entryCount", DLFile_entryCount(&f2) == 5);

		Bool contentOk = true;

		for (U8 i = 0; i < 5 && contentOk; ++i) {

			if (!DLFile_isFullyLoaded(&f2, (U64)i))
				DLFile_loadEntry(&f2, (U64)i, t->alloc, &t->err);

			Buffer out = Buffer_createNull();

			if (!DLFile_loadedBufferAtConst(&f2, (U64)i, &out, &t->err)) {
				contentOk = false;
				break;
			}

			if (Buffer_length(out) != 32) {
				contentOk = false;
				break;
			}

			for (U64 j = 0; j < 32 && contentOk; ++j)
				if (((const U8*)out.ptr)[j] != (U8)(0xA0 + i))
					contentOk = false;
		}

		Test_assert(t, "Encrypted: all content correct", contentOk);

	cleanEnc:
		RefPtr_dec(&ms);

	skipEnc:
		DLFile_free(&f,  t->alloc);
		DLFile_free(&f2, t->alloc);
	}

	{									//Wrong key must fail
		DLFile f = { 0 }, f2 = { 0 };
		DLFile_create(&s, 0, t->alloc, &f, &t->err);
		U8 b = 0x55;
		Buffer buf = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(&b, 1), t->alloc, &buf, NULL);
		DLFile_addEntry(&f, &buf, t->alloc, &t->err);

		MemoryStreamRef *ms = NULL;
		MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &memStreamType, &ms, &t->err);
		U64 off = 0;
		DLFile_write(&f, t->alloc, ms, &encStreamType, iv, &off, &t->err);
		U64 readOff = 0;
		Test_assert(
			t, "Wrong key: read fails", !DLFile_read(ms, readOff, badKey, iv, false, t->alloc, &encStreamType, &f2, NULL)
		);

		Buffer_free(&buf, t->alloc);
		RefPtr_dec(&ms);
		DLFile_free(&f,  t->alloc);
		DLFile_free(&f2, t->alloc);
	}

	{									//Encrypted file, null key supplied, must fail
		DLFile f = { 0 }, f2 = { 0 };
		DLFile_create(&s, 0, t->alloc, &f, &t->err);
		U8 b = 0x77;
		Buffer buf = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(&b, 1), t->alloc, &buf, NULL);
		DLFile_addEntry(&f, &buf, t->alloc, &t->err);

		MemoryStreamRef *ms = NULL;
		MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &memStreamType, &ms, &t->err);
		U64 off = 0;
		DLFile_write(&f, t->alloc, ms, &encStreamType, iv, &off, &t->err);
		U64 readOff = 0;
		Test_assert(
			t, "Encrypted + null key: read fails",
			!DLFile_read(ms, readOff, NULL, iv, false, t->alloc, &encStreamType, &f2, NULL)
		);

		Buffer_free(&buf, t->alloc);
		RefPtr_dec(&ms);
		DLFile_free(&f,  t->alloc);
		DLFile_free(&f2, t->alloc);
	}

	{								//Unencrypted file, key supplied, must fail
		DLSettings sPlain = s;
		sPlain.encryptionType = EXXEncryptionType_None;
		Buffer_setAllToU8(
			Buffer_createRef(sPlain.encryptionKey, sizeof(sPlain.encryptionKey)),
			0,
			NULL
		);

		DLFile f = { 0 }, f2 = { 0 };
		DLFile_create(&sPlain, 0, t->alloc, &f, &t->err);
		U8 b = 0x99;
		Buffer buf = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(&b, 1), t->alloc, &buf, NULL);
		DLFile_addEntry(&f, &buf, t->alloc, &t->err);

		MemoryStreamRef *ms = NULL;
		MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &memStreamType, &ms, &t->err);
		U64 off = 0;
		DLFile_write(&f, t->alloc, ms, &encStreamType, iv, &off, &t->err);
		U64 readOff = 0;
		Test_assert(
			t, "Unencrypted + key supplied: read fails",
			!DLFile_read(ms, readOff, goodKey, iv, false, t->alloc, &encStreamType, &f2, NULL)
		);

		Buffer_free(&buf, t->alloc);
		RefPtr_dec(&ms);
		DLFile_free(&f,  t->alloc);
		DLFile_free(&f2, t->alloc);
	}
}

void Test_DLStress(Test *t) {

	Test_setModule(t, "DLFile_stress");

	DLSettings sData = (DLSettings) {
		.compressionType = EXXCompressionType_None,
		.encryptionType  = EXXEncryptionType_None,
		.dataType        = EDLDataType_Data
	};

	const RefPtrType memStreamType = MemoryStream_makeType(t->alloc);

	{									//10k entries of 1 byte (all in cache)
		DLFile f = { 0 }, f2 = { 0 };

		if (!DLFile_create(&sData, 10000, t->alloc, &f, &t->err)) {
			Test_assert(t, "Stress 10k: create", false);
			goto skip10k;
		}

		Bool addOk = true;

		for (int i = 0; i < 10000 && addOk; ++i) {
			U8 b = (U8)(i & 0xFF);
			Buffer buf = Buffer_createNull();
			addOk =
				Buffer_createCopy(Buffer_createRefConst(&b, 1), t->alloc, &buf, &t->err) &&
				DLFile_addEntry(&f, &buf, t->alloc, &t->err);
			Buffer_free(&buf, t->alloc);
		}

		Test_assert(t, "Stress 10k: add all", addOk);

		if (addOk && Test_assert(
			t, "Stress 10k: roundtrip", DLFile_testRoundtrip(&f, false, NULL, NULL, &memStreamType, t, &f2)
		)) {
			Test_assert(t, "Stress 10k: count", DLFile_entryCount(&f2) == 10000);
			Bool sizeOk = true;

			for (U64 i = 0; i < 10000 && sizeOk; ++i)
				if (DLFile_entrySize(&f2, i) != 1)
					sizeOk = false;

			Test_assert(t, "Stress 10k: all sizes 1", sizeOk);
		}

	skip10k:
		DLFile_free(&f,  t->alloc);
		DLFile_free(&f2, t->alloc);
	}

	{									//Single 200 KiB entry, above DLFile_medLen (128 KiB), stored as stream
		DLFile f = { 0 }, f2 = { 0 };
		Buffer original = Buffer_createNull();

		if (!DLFile_create(&sData, 0, t->alloc, &f, &t->err)) {
			Test_assert(t, "Stress large: create", false);
			goto skipLarge;
		}

		const U64 sz = 200 * KIBI;
		Buffer buf = Buffer_createNull();

		if (!Buffer_createUninitializedBytes(sz, t->alloc, &buf, &t->err)) {
			Test_assert(t, "Stress large: alloc", false);
			goto skipLarge;
		}

		for (U64 i = 0; i < sz; ++i)
			buf.ptrNonConst[i] = (U8)(i % 251);

		Buffer_createCopy(buf, t->alloc, &original, NULL);

		Test_assert(t, "Stress large: add entry", DLFile_addEntry(&f, &buf, t->alloc, &t->err));

		if (Test_assert(t, "Stress large: roundtrip", DLFile_testRoundtrip(&f, false, NULL, NULL, &memStreamType, t, &f2))) {

			Test_assert(t, "Stress large: size", DLFile_entrySize(&f2, 0) == sz);
			Test_assert(t, "Stress large: is file stream", !DLFile_isFullyLoaded(&f2, 0));

			if (!DLFile_isFullyLoaded(&f2, 0))
				DLFile_loadEntry(&f2, 0, t->alloc, &t->err);

			Buffer out = Buffer_createNull();

			if (
				DLFile_isFullyLoaded(&f2, 0) &&
				Test_assert(t, "Stress large: loadedBufferAt", DLFile_loadedBufferAtConst(&f2, 0, &out, &t->err))
			)
				Test_assert(t, "Stress large: content correct", Buffer_eq(out, original));
		}

	skipLarge:
		Buffer_free(&buf,      t->alloc);
		Buffer_free(&original, t->alloc);
		DLFile_free(&f,  t->alloc);
		DLFile_free(&f2, t->alloc);
	}

	//300 entries cycling through 0 B / 100 B / 33 KiB / 135 KiB
	//Exercises small-alloc, medium-alloc and stream tiers together
	{
		DLFile f = { 0 }, f2 = { 0 };
		const U64 sizes[4] = { 0, 100, 33 * KIBI, 135 * KIBI };

		if (!DLFile_create(&sData, 64 * KIBI, t->alloc, &f, &t->err)) {
			Test_assert(t, "Stress mixed: create", false);
			goto skipMixed;
		}

		Bool addOk = true;

		for (U16 i = 0; i < 300 && addOk; ++i) {

			U64    sz  = sizes[i % 4];
			Buffer buf = Buffer_createNull();

			if (sz) {
				Buffer_createUninitializedBytes(sz, t->alloc, &buf, &t->err);
				Buffer_setAllToU8(buf, (U8)i, NULL);
			}

			addOk = DLFile_addEntry(&f, &buf, t->alloc, &t->err);
			Buffer_free(&buf, t->alloc);
		}

		Test_assert(t, "Stress mixed: add all", addOk);

		if (addOk && Test_assert(
			t, "Stress mixed: roundtrip", DLFile_testRoundtrip(&f, false, NULL, NULL, &memStreamType, t, &f2)
		)) {

			Test_assert(t, "Stress mixed: count", DLFile_entryCount(&f2) == 300);
			Bool sizeOk = true;

			for (U16 i = 0; i < 300 && sizeOk; ++i)
				if (DLFile_entrySize(&f2, (U64)i) != sizes[i % 4])
					sizeOk = false;

			Test_assert(t, "Stress mixed: all sizes correct", sizeOk);
		}

	skipMixed:
		DLFile_free(&f,  t->alloc);
		DLFile_free(&f2, t->alloc);
	}
}

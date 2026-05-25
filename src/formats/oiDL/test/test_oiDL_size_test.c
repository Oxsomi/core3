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

//formats/oiDL/test/test_oiDL_size_test.c

#include "test_oiDL_shared.h"
#include "formats/oiDL/dl_entry.h"
#include "formats/oiDL/dl_file.h"
#include "types/container/ref_ptr.h"
#include "types/container/memory_stream.h"
#include "types/container/encryption_stream.h"

static Bool DLFile_testSizeConsistency(
	const DLFile *dlFile,
	const RefPtrType *encStreamType,
	const RefPtrType *memoryStreamType,
	Test *t
) {
	MemoryStreamRef *ms  = NULL;
	Bool             ok  = false;

	U64 sizeOnly = 0;

	if (!DLFile_write(dlFile, t->alloc, NULL, encStreamType, I32x4_zero(), &sizeOnly, &t->err))
		goto clean;

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, memoryStreamType, &ms, &t->err))
		goto clean;

	U64 streamSize = 0;

	if (!DLFile_write(dlFile, t->alloc, ms, encStreamType, I32x4_zero(), &streamSize, &t->err))
		goto clean;

	ok = sizeOnly == streamSize;

clean:
	RefPtr_dec(&ms);
	return ok;
}

void Test_DLWriteSizeConsistency(Test *t) {

	Test_setModule(t, "DLFile_writeSizeConsistency");

	const RefPtrType memStreamType = MemoryStream_makeType(t->alloc);

	DLSettings sData = (DLSettings) {
		.compressionType = EXXCompressionType_None,
		.encryptionType  = EXXEncryptionType_None,
		.dataType        = EDLDataType_Data
	};

	DLSettings sStr = sData;
	sStr.dataType = EDLDataType_String;

	{									//Single buffer entry
		DLFile f = { 0 };
		const U8 bytes[] = { 0x11, 0x22, 0x33 };
		Buffer buf = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(bytes, sizeof(bytes)), t->alloc, &buf, NULL);

		if (!DLFile_create(&sData, 0, t->alloc, &f, &t->err) || !DLFile_addEntry(&f, &buf, t->alloc, &t->err))
			Test_assert(t, "SizeConsistency buffer: setup", false);

		else Test_assert(t, "SizeConsistency buffer", DLFile_testSizeConsistency(&f, NULL, &memStreamType, t));

		Buffer_free(&buf, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{									//10 string entries
		DLFile f = { 0 };

		if (!DLFile_create(&sStr, 0, t->alloc, &f, &t->err)) {
			Test_assert(t, "SizeConsistency strings: setup", false);
			goto skipStr;
		}

		Bool addOk = true;

		for (U8 i = 0; i < 10 && addOk; ++i) {

			CharString str = CharString_createNull();
			addOk = CharString_format(t->alloc, &str, &t->err, "Entry %03d", i);

			if (addOk)
				addOk = DLFile_addEntryString(&f, &str, t->alloc, &t->err);

			CharString_free(&str, t->alloc);
		}

		if (addOk)
			Test_assert(t, "SizeConsistency strings", DLFile_testSizeConsistency(&f, NULL, &memStreamType, t));

		else Test_assert(t, "SizeConsistency strings: add", false);

	skipStr:
		DLFile_free(&f, t->alloc);
	}

	{									//Empty DLFile
		DLFile f = { 0 };
		DLFile_create(&sData, 0, t->alloc, &f, &t->err);
		Test_assert(t, "SizeConsistency empty", DLFile_testSizeConsistency(&f, NULL, &memStreamType, t));
		DLFile_free(&f, t->alloc);
	}

	{									//HideMagicNumber variant
		DLSettings sHide = sData;
		sHide.flags |= EDLSettingsFlags_HideMagicNumber;
		DLFile f = { 0 };
		const U8 b = 0x42;
		Buffer buf = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(&b, 1), t->alloc, &buf, NULL);
		DLFile_create(&sHide, 0, t->alloc, &f, &t->err);
		DLFile_addEntry(&f, &buf, t->alloc, &t->err);
		Test_assert(t, "SizeConsistency hideMagic", DLFile_testSizeConsistency(&f, NULL, &memStreamType, t));
		Buffer_free(&buf, t->alloc);
		DLFile_free(&f, t->alloc);
	}
}

void Test_DLWriteSizeConsistencyEncrypted(Test *t) {

	Test_setModule(t, "DLFile_writeSizeConsistencyEncrypted");

	const RefPtrType memStreamType = MemoryStream_makeType(t->alloc);
	const RefPtrType encStreamType = EncryptionStream_makeType(t->alloc);

	const U32 key[8] = {
		0x01234567, 0x89ABCDEF, 0xFEDCBA98, 0x76543210,
		0x11223344, 0x55667788, 0x99AABBCC, 0xDDEEFF00
	};

	DLSettings sData = (DLSettings) {
		.compressionType = EXXCompressionType_None,
		.encryptionType  = EXXEncryptionType_None,
		.dataType        = EDLDataType_Data
	};

	DLSettings sStr = sData;
	sStr.dataType = EDLDataType_String;

	DLSettings sEnc = sData;
	sEnc.encryptionType = EXXEncryptionType_AES256GCM;
	Buffer_memcpy(Buffer_createRef(sEnc.encryptionKey, sizeof(key)), Buffer_createRefConst(key, sizeof(key)));

	DLSettings sEncHide = sEnc;
	sEncHide.flags |= EDLSettingsFlags_HideMagicNumber;

	{									//Single buffer entry
		DLFile f = { 0 };
		const U8 bytes[] = { 0x11, 0x22, 0x33 };
		Buffer buf = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(bytes, sizeof(bytes)), t->alloc, &buf, NULL);

		if (!DLFile_create(&sData, 0, t->alloc, &f, &t->err) || !DLFile_addEntry(&f, &buf, t->alloc, &t->err))
			Test_assert(t, "SizeConsistency buffer: setup", false);

		else Test_assert(t, "SizeConsistency buffer", DLFile_testSizeConsistency(&f, NULL, &memStreamType, t));

		Buffer_free(&buf, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{									//10 string entries
		DLFile f = { 0 };

		if (!DLFile_create(&sStr, 0, t->alloc, &f, &t->err)) {
			Test_assert(t, "SizeConsistency strings: setup", false);
			goto skipStr;
		}

		Bool addOk = true;

		for (U8 i = 0; i < 10 && addOk; ++i) {
			CharString str = CharString_createNull();
			addOk = CharString_format(t->alloc, &str, &t->err, "Entry %03d", i);
			if (addOk)
				addOk = DLFile_addEntryString(&f, &str, t->alloc, &t->err);
			CharString_free(&str, t->alloc);
		}

		if (addOk)
			Test_assert(t, "SizeConsistency strings", DLFile_testSizeConsistency(&f, NULL, &memStreamType, t));

		else Test_assert(t, "SizeConsistency strings: add", false);

	skipStr:
		DLFile_free(&f, t->alloc);
	}

	{									//Empty DLFile
		DLFile f = { 0 };
		DLFile_create(&sData, 0, t->alloc, &f, &t->err);
		Test_assert(t, "SizeConsistency empty", DLFile_testSizeConsistency(&f, NULL, &memStreamType, t));
		DLFile_free(&f, t->alloc);
	}

	{									//HideMagicNumber variant
		DLSettings sHide = sData;
		sHide.flags |= EDLSettingsFlags_HideMagicNumber;
		DLFile f = { 0 };
		const U8 b = 0x42;
		Buffer buf = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(&b, 1), t->alloc, &buf, NULL);
		DLFile_create(&sHide, 0, t->alloc, &f, &t->err);
		DLFile_addEntry(&f, &buf, t->alloc, &t->err);
		Test_assert(t, "SizeConsistency hideMagic", DLFile_testSizeConsistency(&f, NULL, &memStreamType, t));
		Buffer_free(&buf, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{									//Encrypted, with magic number
		DLFile f = { 0 };

		if (!DLFile_create(&sEnc, 0, t->alloc, &f, &t->err)) {
			Test_assert(t, "SizeConsistency encrypted: setup", false);
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

		if (addOk)
			Test_assert(t, "SizeConsistency encrypted", DLFile_testSizeConsistency(&f, &encStreamType, &memStreamType, t));

		else Test_assert(t, "SizeConsistency encrypted: add", false);

	skipEnc:
		DLFile_free(&f, t->alloc);
	}

	{									//Encrypted, HideMagicNumber (IV managed by parent)
		DLFile f = { 0 };

		if (!DLFile_create(&sEncHide, 0, t->alloc, &f, &t->err)) {
			Test_assert(t, "SizeConsistency enc+hideMagic: setup", false);
			goto skipEncHide;
		}

		Bool addOk = true;

		for (U8 i = 0; i < 5 && addOk; ++i) {
			Buffer buf = Buffer_createNull();
			Buffer_createUninitializedBytes(32, t->alloc, &buf, &t->err);
			Buffer_setAllToU8(buf, (U8)(0xB0 + i), NULL);
			addOk = DLFile_addEntry(&f, &buf, t->alloc, &t->err);
			Buffer_free(&buf, t->alloc);
		}

		if (addOk)
			Test_assert(t, "SizeConsistency enc+hideMagic", DLFile_testSizeConsistency(&f, &encStreamType, &memStreamType, t));

		else Test_assert(t, "SizeConsistency enc+hideMagic: add", false);

	skipEncHide:
		DLFile_free(&f, t->alloc);
	}
}

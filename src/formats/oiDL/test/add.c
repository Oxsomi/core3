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
#include "shared.h"

void Test_DLAddEntries(Test *t) {

	Test_setModule(t, "DLFile_addEntries");

	DLSettings sData = (DLSettings) {
		.compressionType = EXXCompressionType_None,
		.encryptionType = EXXEncryptionType_None,
		.dataType = EDLDataType_Data
	};

	DLSettings sStr = sData;
	sStr.dataType = EDLDataType_String;

	//Buffer entry basics

	{
		DLFile f = { 0 };
		const U8 bytes[] = { 0x01, 0x02, 0x03, 0x04 };
		Buffer buf = Buffer_createNull();

		if (!DLFile_create(&sData, 0, t->alloc, &f, &t->err)) {
			Test_assert(t, "Create Data file", false);
			goto cleanBuf;
		}

		if (!Buffer_createCopy(Buffer_createRefConst(bytes, sizeof(bytes)), t->alloc, &buf, &t->err)) {
			Test_assert(t, "Buffer_createCopy", false);
			goto cleanBuf;
		}

		Test_assert(t, "addEntry ok", DLFile_addEntry(&f, &buf, t->alloc, &t->err));
		Test_assert(t, "Ownership transferred", buf.ptr == NULL);
		Test_assert(t, "entryCount 1", DLFile_entryCount(&f) == 1);
		Test_assert(t, "entrySize correct", DLFile_entrySize(&f, 0) == sizeof(bytes));

		//Empty (zero-length) buffer must be accepted

		Buffer empty = Buffer_createNull();
		Test_assert(t, "addEntry empty ok", DLFile_addEntry(&f, &empty, t->alloc, &t->err));
		Test_assert(t, "entryCount 2", DLFile_entryCount(&f) == 2);
		Test_assert(t, "entrySize 0", DLFile_entrySize(&f, 1) == 0);

		//Type mismatch: addEntry on String file

		{
			DLFile fStr = { 0 };
			Buffer bMismatch = Buffer_createNull();
			Buffer_createCopy(Buffer_createRefConst(bytes, 1), t->alloc, &bMismatch, NULL);
			DLFile_create(&sStr, 0, t->alloc, &fStr, NULL);
			Test_assert(t, "addEntry on String file fails", !DLFile_addEntry(&fStr, &bMismatch, t->alloc, NULL));
			Buffer_free(&bMismatch, t->alloc);
			DLFile_free(&fStr, t->alloc);
		}

		//Null guards

		Test_assert(t, "addEntry null dlFile", !DLFile_addEntry(NULL, &empty, t->alloc, NULL));
		Test_assert(t, "addEntry null entry", !DLFile_addEntry(&f, NULL, t->alloc, NULL));

	cleanBuf:
		Buffer_free(&buf, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	//String entry basics

	{
		DLFile f = { 0 };
		CharString str = CharString_createNull();

		if (!DLFile_create(&sStr, 0, t->alloc, &f, &t->err)) {
			Test_assert(t, "Create String file", false);
			goto cleanStr;
		}

		if (!CharString_createCopy(CharString_createRefCStrConst("Hello, World!"), t->alloc, &str, &t->err)) {
			Test_assert(t, "CharString_createCopy", false);
			goto cleanStr;
		}

		Test_assert(t, "addEntryString ok", DLFile_addEntryString(&f, &str, t->alloc, &t->err));
		Test_assert(t, "String ownership cleared", str.ptr == NULL);
		Test_assert(t, "entryCount 1", DLFile_entryCount(&f) == 1);

		//Unicode (UTF-8), Chinese: Guang ji

		{
			CharString uni = CharString_createNull();
			CharString_createCopy(
				CharString_createRefSizedConst("\xE5\x85\x89\xE8\xBF\xB9", 6, true),
				t->alloc, &uni, NULL
			);
			Test_assert(t, "addEntryString unicode ok", DLFile_addEntryString(&f, &uni, t->alloc, &t->err));
			Test_assert(t, "entryCount 2", DLFile_entryCount(&f) == 2);
			CharString_free(&uni, t->alloc);
		}

		//Invalid UTF-8 must be rejected

		{
			CharString bad = CharString_createRefSizedConst("\xFF\xFE", 2, false);
			Test_assert(t, "addEntryString bad UTF-8 fails", !DLFile_addEntryString(&f, &bad, t->alloc, NULL));
		}

		//Type mismatch: addEntryString on Data file

		{
			DLFile fData = { 0 };
			CharString mismatch = CharString_createNull();
			DLFile_create(&sData, 0, t->alloc, &fData, NULL);
			CharString_createCopy(CharString_createRefCStrConst("x"), t->alloc, &mismatch, NULL);
			Test_assert(t, "addEntryString on Data file fails", !DLFile_addEntryString(&fData, &mismatch, t->alloc, NULL));
			CharString_free(&mismatch, t->alloc);
			DLFile_free(&fData, t->alloc);
		}

		//Null guards

		{
			CharString nullStr = CharString_createNull();
			Test_assert(t, "addEntryString null dlFile", !DLFile_addEntryString(NULL, &nullStr, t->alloc, NULL));
			Test_assert(t, "addEntryString null entry", !DLFile_addEntryString(&f, NULL, t->alloc, NULL));
		}

	cleanStr:
		CharString_free(&str, t->alloc);
		DLFile_free(&f, t->alloc);
	}
}


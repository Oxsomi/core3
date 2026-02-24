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
#include "types/container/list_basic_types.h"
#include "types/base/string_read_helper.h"
#include "shared.h"

void Test_DLCreateFree(Test *t) {

	Test_setModule(t, "DLFile_createFree");

	DLSettings s = (DLSettings) {
		.compressionType = EXXCompressionType_None,
		.encryptionType  = EXXEncryptionType_None,
		.dataType        = EDLDataType_Data,
		.flags           = EDLSettingsFlags_None
	};

	//Must succeed

	{						//Data
		DLFile f = { 0 };
		Test_assert(t, "Create Data",    DLFile_create(&s, 0, t->alloc, &f, &t->err));
		Test_assert(t, "IsAllocated",    DLFile_isAllocated(&f));
		Test_assert(t, "EntryCount 0",   DLFile_entryCount(&f) == 0);
		DLFile_free(&f, t->alloc);
		Test_assert(t, "Free clears",    !DLFile_isAllocated(&f));
	}

	{						//String
		DLFile f = { 0 };
		s.dataType = EDLDataType_String;
		Test_assert(t, "Create String+cache", DLFile_create(&s, 65536, t->alloc, &f, &t->err));
		Test_assert(t, "dataType String",     f.settings.dataType == EDLDataType_String);
		DLFile_free(&f, t->alloc);
		s.dataType = EDLDataType_Data;
	}

	//Must fail

	{						//Double create
		DLFile f = { 0 };
		Test_assert(t, "Create first",        DLFile_create(&s, 0, t->alloc, &f, NULL));
		Test_assert(t, "Double-create fails", !DLFile_create(&s, 0, t->alloc, &f, NULL));
		DLFile_free(&f, t->alloc);
	}

	Test_assert(t, "Null dlFile", !DLFile_create(&s, 0, t->alloc, NULL, NULL));

	{						//Invalid compression type
		DLSettings bad = s;
		bad.compressionType = (XXCompressionType)0xFF;
		DLFile f = { 0 };
		Test_assert(t, "Bad compressionType", !DLFile_create(&bad, 0, t->alloc, &f, NULL));
	}

	{						//Invalid encryptionType 
		DLSettings bad = s;
		bad.encryptionType = (XXEncryptionType)0xFF;
		DLFile f = { 0 };
		Test_assert(t, "Bad encryptionType", !DLFile_create(&bad, 0, t->alloc, &f, NULL));
	}

	{						//Invalid data type
		DLSettings bad = s;
		bad.dataType = (DLDataType)0xFF;
		DLFile f = { 0 };
		Test_assert(t, "Bad dataType", !DLFile_create(&bad, 0, t->alloc, &f, NULL));
	}

	{						//Invalid flags
		DLSettings bad = s;
		bad.flags = EDLSettingsFlags_Invalid;
		DLFile f = { 0 };
		Test_assert(t, "Bad flags", !DLFile_create(&bad, 0, t->alloc, &f, NULL));
	}

	DLFile_free(NULL, t->alloc);		//Hopefully doesn't crash
	Test_assert(t, "Free NULL safe", true);

	{						//Empty DLFile free
		DLFile f = { 0 };
		DLFile_free(&f, t->alloc);
		Test_assert(t, "Free unallocated safe", true);
	}

	//Null-pointer helpers is safe
	Test_assert(t, "entryCount(NULL) == 0",        DLFile_entryCount(NULL) == 0);
	Test_assert(t, "isAllocated(NULL) == false",   !DLFile_isAllocated(NULL));
	Test_assert(t, "isFullyLoaded(NULL) == false", !DLFile_isFullyLoaded(NULL, 0));
	Test_assert(t, "entrySize(NULL) == 0",         DLFile_entrySize(NULL, 0) == 0);
}

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

void Test_DLLoadedAt(Test *t) {

	Test_setModule(t, "DLFile_loadedAt");

	DLSettings sData = (DLSettings) {
		.compressionType = EXXCompressionType_None,
		.encryptionType  = EXXEncryptionType_None,
		.dataType        = EDLDataType_Data
	};

	DLSettings sStr = sData;
	sStr.dataType = EDLDataType_String;

	//loadedBufferAt

	{
		DLFile f = { 0 };
		const U8 bytes[] = { 0xAA, 0xBB, 0xCC };
		Buffer in = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(bytes, sizeof(bytes)), t->alloc, &in, NULL);

		if (
			!DLFile_create(&sData, 0, t->alloc, &f, &t->err) ||
			!DLFile_addEntry(&f, &in, t->alloc, &t->err)
		) {
			Test_assert(t, "Setup loadedBufferAt", false);
			goto cleanBuf;
		}

		Buffer out = Buffer_createNull();
		Test_assert(t, "loadedBufferAt ok", DLFile_loadedBufferAt(&f, 0, &out, &t->err));
		Test_assert(t, "length matches",    Buffer_length(out) == sizeof(bytes));
		Test_assert(t, "Content correct",   Buffer_eq(out, Buffer_createRefConst(bytes, sizeof(bytes))));

		//NULL output is valid (existence check only)
		Test_assert(t, "loadedBufferAt null out ok", DLFile_loadedBufferAt(&f, 0, NULL, NULL));

		//OOB
		Test_assert(t, "loadedBufferAt OOB fails",   !DLFile_loadedBufferAt(&f, 99, &out, NULL));

		//Wrong type

		{
			DLFile fStr = { 0 };
			CharString s = CharString_createNull();
			CharString_createCopy(CharString_createRefCStrConst("hello"), t->alloc, &s, NULL);
			DLFile_create(&sStr, 0, t->alloc, &fStr, NULL);
			DLFile_addEntryString(&fStr, &s, t->alloc, NULL);
			Buffer dummy = Buffer_createNull();
			Test_assert(t, "loadedBufferAt wrong type fails", !DLFile_loadedBufferAt(&fStr, 0, &dummy, NULL));
			DLFile_free(&fStr, t->alloc);
		}

	cleanBuf:
		Buffer_free(&in, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	//loadedStringAt

	{
		DLFile f = { 0 };
		const char *msg = "OxC3 says hi!";
		CharString in = CharString_createNull();
		CharString_createCopy(CharString_createRefCStrConst(msg), t->alloc, &in, NULL);

		if (
			!DLFile_create(&sStr, 0, t->alloc, &f, &t->err) ||
			!DLFile_addEntryString(&f, &in, t->alloc, &t->err)
		) {
			Test_assert(t, "Setup loadedStringAt", false);
			goto cleanStr;
		}

		CharString out = CharString_createNull();
		CharString expected = CharString_createRefCStrConst(msg);
		Test_assert(t, "loadedStringAt ok",  DLFile_loadedStringAt(&f, 0, &out, &t->err));
		Test_assert(t, "length matches",     CharString_length(out) == CharString_length(expected));
		Test_assert(t, "Content correct",    CharString_equalsStringSensitive(&out, &expected));

		//NULL output is valid
		Test_assert(t, "loadedStringAt null out ok", DLFile_loadedStringAt(&f, 0, NULL, NULL));

		//OOB
		Test_assert(t, "loadedStringAt OOB fails",   !DLFile_loadedStringAt(&f, 99, &out, NULL));

		//Wrong type
		{
			DLFile fData = { 0 };
			U8 b = 1;
			Buffer buf = Buffer_createNull();
			Buffer_createCopy(Buffer_createRefConst(&b, 1), t->alloc, &buf, NULL);
			DLFile_create(&sData, 0, t->alloc, &fData, NULL);
			DLFile_addEntry(&fData, &buf, t->alloc, NULL);
			CharString dummy = CharString_createNull();
			Test_assert(t, "loadedStringAt wrong type fails", !DLFile_loadedStringAt(&fData, 0, &dummy, NULL));
			DLFile_free(&fData, t->alloc);
		}

	cleanStr:
		CharString_free(&in, t->alloc);
		DLFile_free(&f, t->alloc);
	}
}

void Test_DLCreateFromList(Test *t) {

	Test_setModule(t, "DLFile_createFromList");

	DLSettings sData = (DLSettings) {
		.compressionType = EXXCompressionType_None,
		.encryptionType  = EXXEncryptionType_None,
		.dataType        = EDLDataType_Data
	};

	DLSettings sStr = sData;
	sStr.dataType = EDLDataType_String;

	{								//createBufferList: valid
		ListBuffer list = { 0 };
		Bool setupOk = true;

		U8 data[5][8];

		for (U8 i = 0; i < 5 && setupOk; ++i) {
			Buffer_setAllToU8(Buffer_createRef(data[i], 8), (U8)i, NULL);
			Buffer buf = Buffer_createNull();
			setupOk =
				Buffer_createCopy(Buffer_createRefConst(data[i], 8), t->alloc, &buf, &t->err) &&
				ListBuffer_pushBack(&list, buf, t->alloc, &t->err);
		}

		if (!setupOk) {
			Test_assert(t, "createBufferList: setup", false);
			ListBuffer_freeUnderlying(&list, t->alloc);
		} else {
			DLFile f = { 0 };
			Test_assert(t, "createBufferList ok",  DLFile_createBufferList(&sData, &list, t->alloc, &f, &t->err));
			Test_assert(t, "list cleared (moved)", list.ptr == NULL);
			Test_assert(t, "entryCount 5",         DLFile_entryCount(&f) == 5);
			for (U64 i = 0; i < 5; ++i)
				Test_assert(t, "entry size 8", DLFile_entrySize(&f, i) == 8);
			DLFile_free(&f, t->alloc);
		}
	}

	{								//createBufferList: empty list
		ListBuffer empty = { 0 };
		DLFile f = { 0 };
		Test_assert(t, "createBufferList empty", DLFile_createBufferList(&sData, &empty, t->alloc, &f, &t->err));
		DLFile_free(&f, t->alloc);
	}

	{									//createStringList: wrong dataType in settings rejected
		ListBuffer list = { 0 };
		U8 b = 1;
		Buffer buf = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(&b, 1), t->alloc, &buf, NULL);
		ListBuffer_pushBack(&list, buf, t->alloc, NULL);
		DLFile f = { 0 };
		Test_assert(t, "createBufferList wrong type fails", !DLFile_createBufferList(&sStr, &list, t->alloc, &f, NULL));
		ListBuffer_freeUnderlying(&list, t->alloc);
	}

	{									//createStringList: valid
		static const char *words[] = { "alpha", "beta", "gamma", "delta", "epsilon" };
		ListCharString list = { 0 };
		Bool setupOk = true;

		for (U8 i = 0; i < 5 && setupOk; ++i) {

			CharString str = CharString_createNull();
			setupOk = CharString_createCopy(CharString_createRefCStrConst(words[i]), t->alloc, &str, &t->err);

			if (setupOk && !ListCharString_pushBack(&list, str, t->alloc, &t->err)) {
				CharString_free(&str, t->alloc);
				setupOk = false;
			}
		}

		if (!setupOk) {
			Test_assert(t, "createStringList: setup", false);
			ListCharString_freeUnderlying(&list, t->alloc);
		} else {
			DLFile f = { 0 };
			Test_assert(t, "createStringList ok",  DLFile_createStringList(&sStr, &list, t->alloc, &f, &t->err));
			Test_assert(t, "list cleared (moved)", list.ptr == NULL);
			Test_assert(t, "entryCount 5",         DLFile_entryCount(&f) == 5);
			DLFile_free(&f, t->alloc);
		}
	}

	{									//createStringList: empty list
		ListCharString empty = { 0 };
		DLFile f = { 0 };
		Test_assert(t, "createStringList empty", DLFile_createStringList(&sStr, &empty, t->alloc, &f, &t->err));
		DLFile_free(&f, t->alloc);
	}

	{									//createStringList: wrong dataType in settings rejected
		ListCharString list = { 0 };
		CharString str = CharString_createNull();
		CharString_createCopy(CharString_createRefCStrConst("x"), t->alloc, &str, NULL);
		ListCharString_pushBack(&list, str, t->alloc, NULL);
		DLFile f = { 0 };
		Test_assert(t, "createStringList wrong type fails", !DLFile_createStringList(&sData, &list, t->alloc, &f, NULL));
		ListCharString_freeUnderlying(&list, t->alloc);
	}
}

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

//formats/oiDL/test/test_oiDL_basic.c

#include "test_oiDL_shared.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_load.h"
#include "formats/oiDL/dl_list.h"
#include "formats/oiDL/dl_entry.h"
#include "types/container/list_basic_types.h"
#include "types/base/string_read_helper.h"

void Test_DLCreateFree(Test *t) {

	Test_setModule(t, "DLFile_createFree");

	DLSettings s = (DLSettings) {
		.compressionType = EXXCompressionType_None,
		.encryptionType  = EXXEncryptionType_None,
		.dataType        = EDLDataType_Data,
		.flags           = EDLSettingsFlags_None
	};

	//Must succeed

	{                        //Data
		DLFile f = { 0 };
		Test_assert(t, "Create Data",    DLFile_create(&s, 0, t->alloc, &f, &t->err));
		Test_assert(t, "IsAllocated",    DLFile_isAllocated(&f));
		Test_assert(t, "EntryCount 0",   DLFile_entryCount(&f) == 0);
		DLFile_free(&f, t->alloc);
		Test_assert(t, "Free clears",    !DLFile_isAllocated(&f));
	}

	{                        //String
		DLFile f = { 0 };
		s.dataType = EDLDataType_String;
		Test_assert(t, "Create String+cache", DLFile_create(&s, 65536, t->alloc, &f, &t->err));
		Test_assert(t, "dataType String",     f.settings.dataType == EDLDataType_String);
		DLFile_free(&f, t->alloc);
		s.dataType = EDLDataType_Data;
	}

	//Must fail

	{                        //Double create
		DLFile f = { 0 };
		Test_assert(t, "Create first",        DLFile_create(&s, 0, t->alloc, &f, NULL));
		Test_assert(t, "Double-create fails", !DLFile_create(&s, 0, t->alloc, &f, NULL));
		DLFile_free(&f, t->alloc);
	}

	Test_assert(t, "Null dlFile", !DLFile_create(&s, 0, t->alloc, NULL, NULL));

	{                        //Invalid compression type
		DLSettings bad = s;
		bad.compressionType = (XXCompressionType)0xFF;
		DLFile f = { 0 };
		Test_assert(t, "Bad compressionType", !DLFile_create(&bad, 0, t->alloc, &f, NULL));
	}

	{                        //Invalid encryptionType 
		DLSettings bad = s;
		bad.encryptionType = (XXEncryptionType)0xFF;
		DLFile f = { 0 };
		Test_assert(t, "Bad encryptionType", !DLFile_create(&bad, 0, t->alloc, &f, NULL));
	}

	{                        //Invalid data type
		DLSettings bad = s;
		bad.dataType = (DLDataType)0xFF;
		DLFile f = { 0 };
		Test_assert(t, "Bad dataType", !DLFile_create(&bad, 0, t->alloc, &f, NULL));
	}

	{                        //Invalid flags
		DLSettings bad = s;
		bad.flags = EDLSettingsFlags_Invalid;
		DLFile f = { 0 };
		Test_assert(t, "Bad flags", !DLFile_create(&bad, 0, t->alloc, &f, NULL));
	}

	DLFile_free(NULL, t->alloc);        //Hopefully doesn't crash
	Test_assert(t, "Free NULL safe", true);

	{                        //Empty DLFile free
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
		Test_assert(t, "loadedBufferAt ok", DLFile_loadedBufferAtConst(&f, 0, &out, &t->err));
		Test_assert(t, "length matches",    Buffer_length(out) == sizeof(bytes));
		Test_assert(t, "Content correct",   Buffer_eq(out, Buffer_createRefConst(bytes, sizeof(bytes))));

		//NULL output is valid (existence check only)
		Test_assert(t, "loadedBufferAt null out ok", DLFile_loadedBufferAtConst(&f, 0, NULL, NULL));

		//OOB
		Test_assert(t, "loadedBufferAt OOB fails",   !DLFile_loadedBufferAtConst(&f, 99, &out, NULL));

		//Wrong type

		{
			DLFile fStr = { 0 };
			CharString s = CharString_createNull();
			CharString_createCopy(CharString_createRefCStrConst("hello"), t->alloc, &s, NULL);
			DLFile_create(&sStr, 0, t->alloc, &fStr, NULL);
			DLFile_addEntryString(&fStr, &s, t->alloc, NULL);
			Buffer dummy = Buffer_createNull();
			Test_assert(t, "loadedBufferAt wrong type fails", !DLFile_loadedBufferAtConst(&fStr, 0, &dummy, NULL));
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
		Test_assert(t, "loadedStringAt ok",  DLFile_loadedStringAtConst(&f, 0, &out, &t->err));
		Test_assert(t, "length matches",     CharString_length(out) == CharString_length(expected));
		Test_assert(t, "Content correct",    CharString_equalsStringSensitive(&out, &expected));

		//NULL output is valid
		Test_assert(t, "loadedStringAt null out ok", DLFile_loadedStringAtConst(&f, 0, NULL, NULL));

		//OOB
		Test_assert(t, "loadedStringAt OOB fails",   !DLFile_loadedStringAtConst(&f, 99, &out, NULL));

		//Wrong type
		{
			DLFile fData = { 0 };
			U8 b = 1;
			Buffer buf = Buffer_createNull();
			Buffer_createCopy(Buffer_createRefConst(&b, 1), t->alloc, &buf, NULL);
			DLFile_create(&sData, 0, t->alloc, &fData, NULL);
			DLFile_addEntry(&fData, &buf, t->alloc, NULL);
			CharString dummy = CharString_createNull();
			Test_assert(t, "loadedStringAt wrong type fails", !DLFile_loadedStringAtConst(&fData, 0, &dummy, NULL));
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

	{                                //createBufferList: valid
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

	{                                //createBufferList: empty list
		ListBuffer empty = { 0 };
		DLFile f = { 0 };
		Test_assert(t, "createBufferList empty", DLFile_createBufferList(&sData, &empty, t->alloc, &f, &t->err));
		DLFile_free(&f, t->alloc);
	}

	{                                    //createStringList: wrong dataType in settings rejected
		ListBuffer list = { 0 };
		U8 b = 1;
		Buffer buf = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(&b, 1), t->alloc, &buf, NULL);
		ListBuffer_pushBack(&list, buf, t->alloc, NULL);
		DLFile f = { 0 };
		Test_assert(t, "createBufferList wrong type fails", !DLFile_createBufferList(&sStr, &list, t->alloc, &f, NULL));
		ListBuffer_freeUnderlying(&list, t->alloc);
	}

	{                                    //createStringList: valid
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

	{                                    //createStringList: empty list
		ListCharString empty = { 0 };
		DLFile f = { 0 };
		Test_assert(t, "createStringList empty", DLFile_createStringList(&sStr, &empty, t->alloc, &f, &t->err));
		DLFile_free(&f, t->alloc);
	}

	{                                    //createStringList: wrong dataType in settings rejected
		ListCharString list = { 0 };
		CharString str = CharString_createNull();
		CharString_createCopy(CharString_createRefCStrConst("x"), t->alloc, &str, NULL);
		ListCharString_pushBack(&list, str, t->alloc, NULL);
		DLFile f = { 0 };
		Test_assert(t, "createStringList wrong type fails", !DLFile_createStringList(&sData, &list, t->alloc, &f, NULL));
		ListCharString_freeUnderlying(&list, t->alloc);
	}
}

void Test_DLFindLoadedString(Test *t) {

	Test_setModule(t, "DLFile_findLoadedString");

	DLSettings sStr = (DLSettings) {
		.compressionType = EXXCompressionType_None,
		.encryptionType  = EXXEncryptionType_None,
		.dataType        = EDLDataType_String
	};

	DLSettings sData = sStr;
	sData.dataType = EDLDataType_Data;

	DLFile f = { 0 };

	if (!DLFile_create(&sStr, 0, t->alloc, &f, &t->err)) {
		Test_assert(t, "DLFile_create", false);
		goto clean;
	}

	//Apple is duplicated at 0 and 3
	const char *words[] = { "apple", "banana", "cherry", "apple", "date" };

	for (U8 i = 0; i < 5; ++i) {
		CharString s = CharString_createNull();
		CharString_createCopy(CharString_createRefCStrConst(words[i]), t->alloc, &s, NULL);
		DLFile_addEntryString(&f, &s, t->alloc, &t->err);
	}

	{        //Find first cherry (2)
		CharString needle = CharString_createRefCStrConst("cherry");
		Test_assert(t, "find 'cherry' at 2", DLFile_findLoadedString(&f, 0, 5, &needle) == 2);
	}

	{        //Find second apple (3)
		CharString needle = CharString_createRefCStrConst("apple");
		Test_assert(t, "find 'apple' from 1 at 3", DLFile_findLoadedString(&f, 1, 5, &needle) == 3);
	}

	{        //No match for third apple
		CharString needle = CharString_createRefCStrConst("apple");
		Test_assert(t, "find 'apple' in [1,3) not found",
			DLFile_findLoadedString(&f, 1, 3, &needle) == U64_MAX);
	}

	{        //No match
		CharString needle = CharString_createRefCStrConst("elderberry");
		Test_assert(t, "find absent word == U64_MAX",
			DLFile_findLoadedString(&f, 0, 5, &needle) == U64_MAX);
	}

	{        //Wrong data type
		DLFile fData = { 0 };
		DLFile_create(&sData, 0, t->alloc, &fData, &t->err);
		U8 b = 1;
		Buffer buf = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(&b, 1), t->alloc, &buf, NULL);
		DLFile_addEntry(&fData, &buf, t->alloc, &t->err);
		CharString needle = CharString_createRefCStrConst("x");
		Test_assert(t, "find on Data file == U64_MAX", DLFile_findLoadedString(&fData, 0, 1, &needle) == U64_MAX);
		DLFile_free(&fData, t->alloc);
	}

	{        //Null guard
		CharString needle = CharString_createRefCStrConst("x");
		Test_assert(t, "find null file == U64_MAX", DLFile_findLoadedString(NULL, 0, 5, &needle) == U64_MAX);
		Test_assert(t, "find null needle == U64_MAX", DLFile_findLoadedString(&f, 0, 5, NULL) == U64_MAX);
	}

clean:
	DLFile_free(&f, t->alloc);
}

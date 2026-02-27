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
#include "types/container/list_basic_types.h"
#include "types/base/string_read_helper.h"
#include "shared.h"

void Test_DLCombine(Test *t) {

	Test_setModule(t, "DLFile_combine");

	DLSettings sData = (DLSettings) {
		.compressionType = EXXCompressionType_None,
		.encryptionType  = EXXEncryptionType_None,
		.dataType        = EDLDataType_Data
	};

	DLSettings sStr = sData;
	sStr.dataType = EDLDataType_String;

	{			//Buffer combine: 3 + 5 = 8 entries
		DLFile a = { 0 }, b = { 0 }, c = { 0 };
		DLFile_create(&sData, 0, t->alloc, &a, &t->err);
		DLFile_create(&sData, 0, t->alloc, &b, &t->err);

		for (U8 i = 0; i < 3; ++i) {
			Buffer buf = Buffer_createNull();
			Buffer_createUninitializedBytes(4, t->alloc, &buf, &t->err);
			Buffer_setAllToU8(buf, i, NULL);
			DLFile_addEntry(&a, &buf, t->alloc, &t->err);
			Buffer_free(&buf, t->alloc);
		}

		for (U8 i = 10; i < 15; ++i) {
			Buffer buf = Buffer_createNull();
			Buffer_createUninitializedBytes(8, t->alloc, &buf, &t->err);
			Buffer_setAllToU8(buf, i, NULL);
			DLFile_addEntry(&b, &buf, t->alloc, &t->err);
			Buffer_free(&buf, t->alloc);
		}

		Test_assert(t, "combine buffers ok", DLFile_combine(&a, &b, t->alloc, &c, &t->err));
		Test_assert(t, "combined count 8",   DLFile_entryCount(&c) == 8);
		Test_assert(t, "entry[0] size 4",    DLFile_entrySize(&c, 0) == 4);
		Test_assert(t, "entry[3] size 8",    DLFile_entrySize(&c, 3) == 8);
		Test_assert(t, "a survived",         DLFile_entryCount(&a) == 3);
		Test_assert(t, "b survived",         DLFile_entryCount(&b) == 5);

		Bool entry = true;

		for (U8 i = 0; i < 3 && entry; ++i) {
			Buffer out = Buffer_createNull();
			if (DLFile_loadedBufferAtConst(&c, i, &out, &t->err)) {
				U32 v = 0;
				entry = Buffer_consumeU32(&out, &v, NULL) && v == C8x4(i, i, i, i);
			}
		}

		Test_assert(t, "entry[0-2] content correct", entry);

		for (U8 i = 0; i < 5 && entry; ++i) {
			Buffer out = Buffer_createNull();
			if (DLFile_loadedBufferAtConst(&c, i + 3, &out, &t->err)) {
				U64 v = 0;
				U8 j = i + 10;
				entry = Buffer_consumeU64(&out, &v, NULL) && v == C8x8(j, j, j, j, j, j, j, j);
			}
		}

		Test_assert(t, "entry[3-7] content correct", entry);

		DLFile_free(&a, t->alloc);
		DLFile_free(&b, t->alloc);
		DLFile_free(&c, t->alloc);
	}

	{			//String combine: 2 + 3 = 5
		DLFile a = { 0 }, b = { 0 }, c = { 0 };
		const C8 *wa[] = { "hello", "world" };
		const C8 *wb[] = { "foo", "bar", "baz" };

		DLFile_create(&sStr, 0, t->alloc, &a, &t->err);
		DLFile_create(&sStr, 0, t->alloc, &b, &t->err);

		for (U8 i = 0; i < 2; ++i) {
			CharString s = CharString_createNull();
			CharString_createCopy(CharString_createRefCStrConst(wa[i]), t->alloc, &s, NULL);
			DLFile_addEntryString(&a, &s, t->alloc, &t->err);
		}

		for (U8 i = 0; i < 3; ++i) {
			CharString s = CharString_createNull();
			CharString_createCopy(CharString_createRefCStrConst(wb[i]), t->alloc, &s, NULL);
			DLFile_addEntryString(&b, &s, t->alloc, &t->err);
		}

		Test_assert(t, "combine strings ok", DLFile_combine(&a, &b, t->alloc, &c, &t->err));
		Test_assert(t, "combined count 5",   DLFile_entryCount(&c) == 5);

		const C8 *expected[] = { "hello", "world", "foo", "bar", "baz" };
		Bool contentOk = true;

		for (U8 i = 0; i < 5 && contentOk; ++i) {
			CharString out = CharString_createNull();
			CharString exp = CharString_createRefCStrConst(expected[i]);
			if (DLFile_loadedStringAtConst(&c, i, &out, &t->err))
				contentOk = CharString_equalsStringSensitive(&out, &exp);
		}

		Test_assert(t, "string content correct", contentOk);

		DLFile_free(&a, t->alloc);
		DLFile_free(&b, t->alloc);
		DLFile_free(&c, t->alloc);
	}

	{			//Incompatible settings (Data vs String) must fail
		DLFile a = { 0 }, b = { 0 }, c = { 0 };
		DLFile_create(&sData, 0, t->alloc, &a, &t->err);
		DLFile_create(&sStr,  0, t->alloc, &b, &t->err);
		U8 byte = 1;
		Buffer buf = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(&byte, 1), t->alloc, &buf, NULL);
		DLFile_addEntry(&a, &buf, t->alloc, &t->err);
		CharString str = CharString_createNull();
		CharString_createCopy(CharString_createRefCStrConst("x"), t->alloc, &str, NULL);
		DLFile_addEntryString(&b, &str, t->alloc, &t->err);

		Test_assert(t, "combine incompatible fails", !DLFile_combine(&a, &b, t->alloc, &c, NULL));
		Test_assert(t, "combine fail: c not allocated", !DLFile_isAllocated(&c));

		Buffer_free(&buf, t->alloc);
		CharString_free(&str, t->alloc);
		DLFile_free(&a, t->alloc);
		DLFile_free(&b, t->alloc);
	}

	{			//Null input guards
		DLFile a = { 0 }, c = { 0 };
		DLFile_create(&sData, 0, t->alloc, &a, &t->err);
		Test_assert(t, "combine null a fails", !DLFile_combine(NULL, &a, t->alloc, &c, NULL));
		Test_assert(t, "combine null b fails", !DLFile_combine(&a, NULL, t->alloc, &c, NULL));
		DLFile_free(&a, t->alloc);
	}
}

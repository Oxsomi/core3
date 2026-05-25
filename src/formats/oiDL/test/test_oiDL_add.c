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

//formats/oiDL/test/test_oiDL_add.c

#include "test_oiDL_shared.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_entry.h"

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

Bool buildDataFile(Test *t, DLFile *f, U64 count);
Bool buildStringFile(Test *t, DLFile *f, U64 count);

extern const DLSettings kSettingsData;
extern const DLSettings kSettingsStr;

void Test_DLInsertEntry(Test *t) {

	Test_setModule(t, "DLFile_insertEntry");

	/* Helper: build a Buffer containing a single byte value. */
	#define MAKE_BUF(val, bufVar)                                                           \
		do {                                                                                \
			U8 _v = (U8)(val);                                                              \
			(bufVar) = Buffer_createNull();                                                 \
			if (!Buffer_createCopy(Buffer_createRefConst(&_v, 1), t->alloc, &(bufVar), &t->err)) { \
				Test_assert(t, "Buffer_createCopy for MAKE_BUF", false);                    \
				goto done##val;                                                             \
			}                                                                               \
		} while(0)

	{						//Insert at front (id = 0) shifts existing entries right
		DLFile f = { 0 };
		Buffer buf = Buffer_createNull();

		if (!buildDataFile(t, &f, 3)) {
			Test_assert(t, "Setup 3-entry for insertEntry front", false);
			goto doneInsertFront;
		}

		U8 val[3] = { 0xFF };

		if (!Buffer_createCopy(Buffer_createRefConst(&val, 3), t->alloc, &buf, &t->err)) {
			Test_assert(t, "Buffer_createCopy", false);
			goto doneInsertFront;
		}

		Test_assert(t, "insertEntry id=0 ok",   DLFile_insertEntry(&f, 0, &buf, t->alloc, &t->err));
		Test_assert(t, "buf zeroed out",        buf.ptr == NULL);
		Test_assert(t, "entryCount 4",          DLFile_entryCount(&f) == 4);
		Test_assert(t, "[0] == 3",              DLFile_entrySize(&f, 0) == 3);
		Test_assert(t, "[1] == 0",              DLFile_entrySize(&f, 1) == 0);
		Test_assert(t, "[2] == 1",              DLFile_entrySize(&f, 2) == 1);
		Test_assert(t, "[3] == 2",              DLFile_entrySize(&f, 3) == 2);

	doneInsertFront:
		Buffer_free(&buf, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{						//Insert in middle
		DLFile f = { 0 };
		Buffer buf = Buffer_createNull();

		if (!buildDataFile(t, &f, 3)) {
			Test_assert(t, "Setup 3-entry for insertEntry mid", false);
			goto doneInsertMid;
		}

		U8 val[3] = { 0xAB };

		if (!Buffer_createCopy(Buffer_createRefConst(&val, 3), t->alloc, &buf, &t->err)) {
			Test_assert(t, "Buffer_createCopy mid", false);
			goto doneInsertMid;
		}

		Test_assert(t, "insertEntry id=2 ok",   DLFile_insertEntry(&f, 2, &buf, t->alloc, &t->err));
		Test_assert(t, "entryCount 4",          DLFile_entryCount(&f) == 4);
		Test_assert(t, "[0] == 0",              DLFile_entrySize(&f, 0) == 0);
		Test_assert(t, "[1] == 1",              DLFile_entrySize(&f, 1) == 1);
		Test_assert(t, "[2] == 3",              DLFile_entrySize(&f, 2) == 3);
		Test_assert(t, "[3] == 2",              DLFile_entrySize(&f, 3) == 2);

	doneInsertMid:
		Buffer_free(&buf, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{						//Insert at end (id == length) acts as append
		DLFile f = { 0 };
		Buffer buf = Buffer_createNull();

		if (!buildDataFile(t, &f, 2)) {
			Test_assert(t, "Setup 2-entry for insertEntry append", false);
			goto doneInsertAppend;
		}

		U8 val[3] = { 0xCD };

		if (!Buffer_createCopy(Buffer_createRefConst(&val, 3), t->alloc, &buf, &t->err)) {
			Test_assert(t, "Buffer_createCopy append", false);
			goto doneInsertAppend;
		}

		Test_assert(t, "insertEntry at end ok", DLFile_insertEntry(&f, 2, &buf, t->alloc, &t->err));
		Test_assert(t, "entryCount 3",          DLFile_entryCount(&f) == 3);
		Test_assert(t, "[2] == 3",              DLFile_entrySize(&f, 2) == 3);

	doneInsertAppend:
		Buffer_free(&buf, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{						//Insert into empty file at id = 0
		DLFile f = { 0 };
		Buffer buf = Buffer_createNull();

		if (!DLFile_create(&kSettingsData, 0, t->alloc, &f, &t->err)) {
			Test_assert(t, "Create empty Data file", false);
			goto doneInsertEmpty;
		}

		U8 val = 0x42;

		if (!Buffer_createCopy(Buffer_createRefConst(&val, 1), t->alloc, &buf, &t->err)) {
			Test_assert(t, "Buffer_createCopy empty insert", false);
			goto doneInsertEmpty;
		}

		Test_assert(t, "insertEntry empty file ok", DLFile_insertEntry(&f, 0, &buf, t->alloc, &t->err));
		Test_assert(t, "entryCount 1",              DLFile_entryCount(&f) == 1);
		Test_assert(t, "[0] == 1",                  DLFile_entrySize(&f, 0) == 1);

	doneInsertEmpty:
		Buffer_free(&buf, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{						//OOB (id > length) fails
		DLFile f = { 0 };
		Buffer buf = Buffer_createNull();

		if (!buildDataFile(t, &f, 2)) {
			Test_assert(t, "Setup for OOB insertEntry", false);
			goto doneInsertOOB;
		}

		U8 val = 0x99;

		if (!Buffer_createCopy(Buffer_createRefConst(&val, 1), t->alloc, &buf, &t->err)) {
			Test_assert(t, "Buffer_createCopy OOB", false);
			goto doneInsertOOB;
		}

		Test_assert(t, "insertEntry OOB fails",     !DLFile_insertEntry(&f, 5, &buf, t->alloc, NULL));
		Test_assert(t, "entryCount unchanged",      DLFile_entryCount(&f) == 2);

	doneInsertOOB:
		Buffer_free(&buf, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{						//Type mismatch: String file
		DLFile f = { 0 };
		Buffer buf = Buffer_createNull();

		if (!buildStringFile(t, &f, 1)) {
			Test_assert(t, "Setup String file for insertEntry type mismatch", false);
			goto doneMismatch;
		}

		U8 val = 0x01;

		if (!Buffer_createCopy(Buffer_createRefConst(&val, 1), t->alloc, &buf, &t->err)) {
			Test_assert(t, "Buffer_createCopy mismatch", false);
			goto doneMismatch;
		}

		Test_assert(t, "insertEntry on String file fails", !DLFile_insertEntry(&f, 0, &buf, t->alloc, NULL));

	doneMismatch:
		Buffer_free(&buf, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{						//Null guards
		DLFile f = { 0 };
		Buffer buf = Buffer_createNull();

		if (!buildDataFile(t, &f, 1)) {
			Test_assert(t, "Setup for null guard insertEntry", false);
			goto doneNullGuard;
		}

		U8 val = 0x01;
		Buffer_createCopy(Buffer_createRefConst(&val, 1), t->alloc, &buf, NULL);

		Test_assert(t, "insertEntry null dlFile fails", !DLFile_insertEntry(NULL, 0, &buf,  t->alloc, NULL));
		Test_assert(t, "insertEntry null buf fails",    !DLFile_insertEntry(&f,   0, NULL,  t->alloc, NULL));

	doneNullGuard:
		Buffer_free(&buf, t->alloc);
		DLFile_free(&f, t->alloc);
	}
}
void Test_DLInsertEntryString(Test *t) {

	Test_setModule(t, "DLFile_insertEntryString");

	{						//Insert at front shifts existing entries
		DLFile f = { 0 };
		CharString s = CharString_createNull();

		if (!buildStringFile(t, &f, 3)) {
			Test_assert(t, "Setup 3-entry String file", false);
			goto doneInsStrFront;
		}

		if (!CharString_createCopy(CharString_createRefCStrConst("NEW"), t->alloc, &s, &t->err)) {
			Test_assert(t, "CharString_createCopy NEW", false);
			goto doneInsStrFront;
		}

		Test_assert(t, "insertEntryString id=0 ok", DLFile_insertEntryString(&f, 0, &s, t->alloc, &t->err));
		Test_assert(t, "str zeroed out",            s.ptr == NULL);
		Test_assert(t, "entryCount 4",              DLFile_entryCount(&f) == 4);
		Test_assert(t, "[0] == 3",                  DLFile_entrySize(&f, 0) == 3);
		Test_assert(t, "[1] == 0",                  DLFile_entrySize(&f, 1) == 0);
		Test_assert(t, "[2] == 1",                  DLFile_entrySize(&f, 2) == 1);
		Test_assert(t, "[3] == 2",                  DLFile_entrySize(&f, 3) == 2);

	doneInsStrFront:
		CharString_free(&s, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{						//Insert in middle
		DLFile f = { 0 };
		CharString s = CharString_createNull();

		if (!buildStringFile(t, &f, 3)) {
			Test_assert(t, "Setup 3-entry String for mid insert", false);
			goto doneInsStrMid;
		}

		if (!CharString_createCopy(CharString_createRefCStrConst("MID"), t->alloc, &s, &t->err)) {
			Test_assert(t, "CharString_createCopy MID", false);
			goto doneInsStrMid;
		}

		Test_assert(t, "insertEntryString id=1 ok", DLFile_insertEntryString(&f, 1, &s, t->alloc, &t->err));
		Test_assert(t, "entryCount 4",              DLFile_entryCount(&f) == 4);
		Test_assert(t, "[0] == 0",                  DLFile_entrySize(&f, 0) == 0);
		Test_assert(t, "[1] == 3",                  DLFile_entrySize(&f, 1) == 3);
		Test_assert(t, "[2] == 1",                  DLFile_entrySize(&f, 2) == 1);
		Test_assert(t, "[3] == 2",                  DLFile_entrySize(&f, 3) == 2);

	doneInsStrMid:
		CharString_free(&s, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{						//Insert at end acts as append
		DLFile f = { 0 };
		CharString s = CharString_createNull();

		if (!buildStringFile(t, &f, 2)) {
			Test_assert(t, "Setup 2-entry String for append", false);
			goto doneInsStrAppend;
		}

		if (!CharString_createCopy(CharString_createRefCStrConst("END"), t->alloc, &s, &t->err)) {
			Test_assert(t, "CharString_createCopy END", false);
			goto doneInsStrAppend;
		}

		Test_assert(t, "insertEntryString at end ok", DLFile_insertEntryString(&f, 2, &s, t->alloc, &t->err));
		Test_assert(t, "entryCount 3",                DLFile_entryCount(&f) == 3);
		Test_assert(t, "[2] == 3",                    DLFile_entrySize(&f, 2) == 3);

	doneInsStrAppend:
		CharString_free(&s, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{						//OOB fails
		DLFile f = { 0 };
		CharString s = CharString_createNull();

		if (!buildStringFile(t, &f, 2)) {
			Test_assert(t, "Setup for OOB insertEntryString", false);
			goto doneInsStrOOB;
		}

		CharString_createCopy(CharString_createRefCStrConst("X"), t->alloc, &s, NULL);

		Test_assert(t, "insertEntryString OOB fails",   !DLFile_insertEntryString(&f, 5, &s, t->alloc, NULL));
		Test_assert(t, "entryCount unchanged",          DLFile_entryCount(&f) == 2);

	doneInsStrOOB:
		CharString_free(&s, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{						//Type mismatch: Data file
		DLFile f = { 0 };
		CharString s = CharString_createNull();

		if (!buildDataFile(t, &f, 1)) {
			Test_assert(t, "Setup Data file for insertEntryString type mismatch", false);
			goto doneMismatch;
		}

		CharString_createCopy(CharString_createRefCStrConst("X"), t->alloc, &s, NULL);
		Test_assert(t, "insertEntryString on Data file fails", !DLFile_insertEntryString(&f, 0, &s, t->alloc, NULL));

	doneMismatch:
		CharString_free(&s, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{						//Null guards
		DLFile f = { 0 };
		CharString s = CharString_createNull();

		if (!buildStringFile(t, &f, 1)) {
			Test_assert(t, "Setup for null guard insertEntryString", false);
			goto doneNullGuard;
		}

		CharString_createCopy(CharString_createRefCStrConst("G"), t->alloc, &s, NULL);

		Test_assert(t, "insertEntryString null dlFile", !DLFile_insertEntryString(NULL, 0, &s,   t->alloc, NULL));
		Test_assert(t, "insertEntryString null str",    !DLFile_insertEntryString(&f,   0, NULL, t->alloc, NULL));

	doneNullGuard:
		CharString_free(&s, t->alloc);
		DLFile_free(&f, t->alloc);
	}
}

void Test_DLRemoveInsertRoundtrip(Test *t) {

	Test_setModule(t, "DLFile_removeInsertRoundtrip");

	{						//Data: remove middle then re-insert at same position
		DLFile f = { 0 };

		if (!buildDataFile(t, &f, 4)) {
			Test_assert(t, "Setup 4-entry for roundtrip", false);
			goto doneRT;
		}

		Buffer        moved  = Buffer_createNull();
		DLEntryStream stream = { 0 };

		Test_assert(t, "removeEntry id = 2", DLFile_removeEntry(&f, 2, &moved, &stream, &t->err));
		Test_assert(t, "entryCount 3",       DLFile_entryCount(&f) == 3);
		Test_assert(t, "Moved buf len 2",    Buffer_length(moved) == 2);

		//Re-insert the removed buffer at its original position

		Test_assert(t, "insertEntry id = 2", DLFile_insertEntry(&f, 2, &moved, t->alloc, &t->err));
		Test_assert(t, "entryCount 4",       DLFile_entryCount(&f) == 4);

		Test_assert(t, "[0] == 0",           DLFile_entrySize(&f, 0) == 0);
		Test_assert(t, "[1] == 1",           DLFile_entrySize(&f, 1) == 1);
		Test_assert(t, "[2] == 2",           DLFile_entrySize(&f, 2) == 2);
		Test_assert(t, "[3] == 3",           DLFile_entrySize(&f, 3) == 3);

	doneRT:
		Buffer_free(&moved, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{						//String: remove first then re-insert at front
		DLFile f = { 0 };

		if (!buildStringFile(t, &f, 3)) {
			Test_assert(t, "Setup 3-string for roundtrip", false);
			goto doneRTStr;
		}

		CharString    moved  = CharString_createNull();
		DLEntryStream stream = { 0 };

		Test_assert(t, "removeEntryString id = 0", DLFile_removeEntryString(&f, 0, &moved, &stream, &t->err));
		Test_assert(t, "entryCount 2",             DLFile_entryCount(&f) == 2);

		Test_assert(t, "moved == 0",               CharString_isEmpty(moved));

		Test_assert(t, "insertEntryString id = 0", DLFile_insertEntryString(&f, 0, &moved, t->alloc, &t->err));
		Test_assert(t, "entryCount 3",             DLFile_entryCount(&f) == 3);
		Test_assert(t, "[0] == 0",                 DLFile_entrySize(&f, 0) == 0);
		Test_assert(t, "[1] == 1",                 DLFile_entrySize(&f, 1) == 1);
		Test_assert(t, "[2] == 2",                 DLFile_entrySize(&f, 2) == 2);

	doneRTStr:
		CharString_free(&moved, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{						//Multiple removes followed by multiple inserts
		DLFile f = { 0 };

		if (!buildDataFile(t, &f, 5)) {
			Test_assert(t, "Setup 5-entry for multi remove/insert", false);
			goto doneMulti;
		}

		//Remove id=0 and id=0 (which is old id=1 after first removal)

		Test_assert(t, "remove id = 0 (1st)", DLFile_remove(&f, 0, t->alloc, &t->err));
		Test_assert(t, "remove id = 0 (2nd)", DLFile_remove(&f, 0, t->alloc, &t->err));
		Test_assert(t, "entryCount 3",        DLFile_entryCount(&f) == 3);
		Test_assert(t, "[0] == 2",            DLFile_entrySize(&f, 0) == 0x02);
		Test_assert(t, "[1] == 3",            DLFile_entrySize(&f, 1) == 0x03);
		Test_assert(t, "[2] == 4",            DLFile_entrySize(&f, 2) == 0x04);

		//Insert new entries at the front

		U8 va[2] = { 0xAA }, vb = 0xBB;
		Buffer ba = Buffer_createNull(), bb = Buffer_createNull();

		if (
			Buffer_createCopy(Buffer_createRefConst(&va, 2), t->alloc, &ba, &t->err) &&
			Buffer_createCopy(Buffer_createRefConst(&vb, 1), t->alloc, &bb, &t->err)
		) {
			Test_assert(t, "insert 0xAA at 0",  DLFile_insertEntry(&f, 0, &ba, t->alloc, &t->err));
			Test_assert(t, "insert 0xBB at 1",  DLFile_insertEntry(&f, 1, &bb, t->alloc, &t->err));
			Test_assert(t, "entryCount 5",      DLFile_entryCount(&f) == 5);
			Test_assert(t, "[0] == 2",          DLFile_entrySize(&f, 0) == 2);
			Test_assert(t, "[1] == 1",          DLFile_entrySize(&f, 1) == 1);
			Test_assert(t, "[2] == 2",          DLFile_entrySize(&f, 2) == 2);
			Test_assert(t, "[3] == 3",          DLFile_entrySize(&f, 3) == 3);
			Test_assert(t, "[4] == 4",          DLFile_entrySize(&f, 4) == 4);
		}

		Buffer_free(&ba, t->alloc);
		Buffer_free(&bb, t->alloc);

	doneMulti:
		DLFile_free(&f, t->alloc);
	}
}

void Test_DLInsertEntries(Test *t) {
	Test_DLInsertEntry(t);
	Test_DLInsertEntryString(t);
	Test_DLRemoveInsertRoundtrip(t);
}

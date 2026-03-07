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

#include "test_oiDL_shared.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_entry.h"

const DLSettings kSettingsData = {
	.compressionType = EXXCompressionType_None,
	.encryptionType  = EXXEncryptionType_None,
	.dataType        = EDLDataType_Data,
	.flags           = EDLSettingsFlags_None
};

const DLSettings kSettingsStr = {
	.compressionType = EXXCompressionType_None,
	.encryptionType  = EXXEncryptionType_None,
	.dataType        = EDLDataType_String,
	.flags           = EDLSettingsFlags_None
};

//oiDL containing Buffers in incrementing length (also referenced by add.c)
Bool buildDataFile(Test *t, DLFile *f, U64 count) {

	if (!DLFile_create(&kSettingsData, 0, t->alloc, f, &t->err))
		return false;

	for (U64 i = 0; i < count; ++i) {

		Buffer buf = Buffer_createNull();
		if (i && !Buffer_createEmptyBytes(i, t->alloc, &buf, &t->err)) {
			DLFile_free(f, t->alloc);
			return false;
		}

		if (!DLFile_addEntry(f, &buf, t->alloc, &t->err)) {
			Buffer_free(&buf, t->alloc);
			DLFile_free(f, t->alloc);
			return false;
		}
	}

	return true;
}

//oiDL containing Strings in incrementing length (also referenced by add.c)
Bool buildStringFile(Test *t, DLFile *f, U64 count) {

	if (!DLFile_create(&kSettingsStr, 0, t->alloc, f, &t->err))
		return false;

	for (U64 i = 0; i < count; ++i) {

		CharString s = CharString_createNull();

		if (i && !CharString_create('A', i, t->alloc, &s, &t->err)) {
			DLFile_free(f, t->alloc);
			return false;
		}

		if (!DLFile_addEntryString(f, &s, t->alloc, &t->err)) {
			CharString_free(&s, t->alloc);
			DLFile_free(f, t->alloc);
			return false;
		}
	}

	return true;
}

void Test_DLRemove(Test *t) {

	Test_setModule(t, "DLFile_remove");

	{						//Remove first, validate shift
		DLFile f = { 0 };

		if (!buildDataFile(t, &f, 3)) {
			Test_assert(t, "Setup 3-entry Data file", false);
			goto doneRemoveFirst;
		}

		Test_assert(t, "remove id = 0 ok",    DLFile_remove(&f, 0, t->alloc, &t->err));
		Test_assert(t, "entryCount 2",        DLFile_entryCount(&f) == 2);

		//original [0] = 0x00 is gone; [1] = 0x01 is now at [0], [2] = 0x02 at [1]

		Test_assert(t, "shifted [0] == 0x01", DLFile_entrySize(&f, 0) == 1);
		Test_assert(t, "shifted [1] == 0x02", DLFile_entrySize(&f, 1) == 2);

	doneRemoveFirst:
		DLFile_free(&f, t->alloc);
	}

	{						//Remove last
		DLFile f = { 0 };

		if (!buildDataFile(t, &f, 3)) {
			Test_assert(t, "Setup 3-entry Data file (last)", false);
			goto doneRemoveLast;
		}

		Test_assert(t, "remove id = 2 ok",    DLFile_remove(&f, 2, t->alloc, &t->err));
		Test_assert(t, "entryCount 2",        DLFile_entryCount(&f) == 2);
		Test_assert(t, "[0] still 0x00",      DLFile_entrySize(&f, 0) == 0);
		Test_assert(t, "[1] still 0x01",      DLFile_entrySize(&f, 1) == 1);

	doneRemoveLast:
		DLFile_free(&f, t->alloc);
	}

	{						//Remove 1st of 4
		DLFile f = { 0 };

		if (!buildDataFile(t, &f, 4)) {
			Test_assert(t, "Setup 4-entry Data file (mid)", false);
			goto doneRemoveMid;
		}

		Test_assert(t, "remove id = 1 ok",    DLFile_remove(&f, 1, t->alloc, &t->err));
		Test_assert(t, "entryCount 3",        DLFile_entryCount(&f) == 3);
		Test_assert(t, "[0] == 0",            DLFile_entrySize(&f, 0) == 0);
		Test_assert(t, "[1] == 2",            DLFile_entrySize(&f, 1) == 2);
		Test_assert(t, "[2] == 3",            DLFile_entrySize(&f, 2) == 3);

	doneRemoveMid:
		DLFile_free(&f, t->alloc);
	}

	{						//Remove the only one remaining
		DLFile f = { 0 };

		if (!buildDataFile(t, &f, 1)) {
			Test_assert(t, "Setup 1-entry Data file", false);
			goto doneRemoveSole;
		}

		Test_assert(t, "remove sole entry ok",  DLFile_remove(&f, 0, t->alloc, &t->err));
		Test_assert(t, "entryCount 0",          DLFile_entryCount(&f) == 0);

	doneRemoveSole:
		DLFile_free(&f, t->alloc);
	}

	{						//Remove string from string file
		DLFile f = { 0 };

		if (!buildStringFile(t, &f, 3)) {
			Test_assert(t, "Setup 3-entry String file", false);
			goto doneRemoveStr;
		}

		Test_assert(t, "remove str id = 1 ok",  DLFile_remove(&f, 1, t->alloc, &t->err));
		Test_assert(t, "entryCount 2",          DLFile_entryCount(&f) == 2);
		Test_assert(t, "[0] still str0",        DLFile_entrySize(&f, 0) == 0);
		Test_assert(t, "[1] now str2",          DLFile_entrySize(&f, 1) == 2);

	doneRemoveStr:
		DLFile_free(&f, t->alloc);
	}

	{						//Out of bounds
		DLFile f = { 0 };

		if (!buildDataFile(t, &f, 2)) {
			Test_assert(t, "Setup for OOB remove", false);
			goto doneRemoveOOB;
		}

		Test_assert(t, "remove OOB fails",      !DLFile_remove(&f, 2, t->alloc, NULL));
		Test_assert(t, "entryCount unchanged",  DLFile_entryCount(&f) == 2);

	doneRemoveOOB:
		DLFile_free(&f, t->alloc);
	}

	//Null guard
	Test_assert(t, "remove null dlFile fails",  !DLFile_remove(NULL, 0, t->alloc, NULL));
}

void Test_DLRemoveEntry(Test *t) {

	Test_setModule(t, "DLFile_removeEntry");

	{						//Remove middle and return buffer
		DLFile f = { 0 };

		if (!buildDataFile(t, &f, 3)) {
			Test_assert(t, "Setup 3-entry Data file", false);
			goto doneBasic;
		}

		Buffer        out    = Buffer_createNull();
		DLEntryStream stream = { 0 };

		Test_assert(t, "removeEntry id = 1 ok", DLFile_removeEntry(&f, 1, &out, &stream, &t->err));
		Test_assert(t, "entryCount 2",          DLFile_entryCount(&f) == 2);
		Test_assert(t, "returned buf length 1", Buffer_length(out) == 1);

		Test_assert(t, "[0] == 0",              DLFile_entrySize(&f, 0) == 0);
		Test_assert(t, "[1] == 2",              DLFile_entrySize(&f, 1) == 2);

		Buffer_free(&out, t->alloc);

	doneBasic:
		DLFile_free(&f, t->alloc);
	}

	{						//Remove last
		DLFile f = { 0 };

		if (!buildDataFile(t, &f, 2)) {
			Test_assert(t, "Setup 2-entry Data file", false);
			goto doneFirst;
		}

		Buffer        out    = Buffer_createNull();
		DLEntryStream stream = { 0 };

		Test_assert(t, "removeEntry id = 0 ok", DLFile_removeEntry(&f, 0, &out, &stream, &t->err));
		Test_assert(t, "entryCount 1",          DLFile_entryCount(&f) == 1);
		Test_assert(t, "Returned buf length 1", Buffer_length(out) == 0);
		Test_assert(t, "[0] now 1",				DLFile_entrySize(&f, 0) == 1);

		Buffer_free(&out, t->alloc);

	doneFirst:
		DLFile_free(&f, t->alloc);
	}

	{					//Out of bounds
		DLFile f = { 0 };

		if (!buildDataFile(t, &f, 1)) {
			Test_assert(t, "Setup for OOB removeEntry", false);
			goto doneOOB;
		}

		Buffer        out    = Buffer_createNull();
		DLEntryStream stream = { 0 };

		Test_assert(t, "removeEntry OOB fails", !DLFile_removeEntry(&f, 1, &out, &stream, NULL));
		Test_assert(t, "entryCount unchanged",  DLFile_entryCount(&f) == 1);

	doneOOB:
		DLFile_free(&f, t->alloc);
	}

	{					//Type mismatch
		DLFile f = { 0 };

		if (!buildStringFile(t, &f, 1)) {
			Test_assert(t, "Setup String file for type mismatch", false);
			goto doneMismatch;
		}

		Buffer        out    = Buffer_createNull();
		DLEntryStream stream = { 0 };

		Test_assert(t, "removeEntry on String file fails", !DLFile_removeEntry(&f, 0, &out, &stream, NULL));
		Test_assert(t, "entryCount unchanged", DLFile_entryCount(&f) == 1);

	doneMismatch:
		DLFile_free(&f, t->alloc);
	}

	{					//Null guards
		DLFile f = { 0 };

		if (!buildDataFile(t, &f, 1)) {
			Test_assert(t, "Setup for null guard removeEntry", false);
			goto doneNull;
		}

		Buffer        out    = Buffer_createNull();
		DLEntryStream stream = { 0 };

		Test_assert(t, "removeEntry null dlFile",  !DLFile_removeEntry(NULL, 0, &out, &stream, NULL));
		Test_assert(t, "removeEntry null buf",     !DLFile_removeEntry(&f,   0, NULL, &stream, NULL));
		Test_assert(t, "removeEntry null stream",  !DLFile_removeEntry(&f,   0, &out, NULL,    NULL));

	doneNull:
		DLFile_free(&f, t->alloc);
	}
}

void Test_DLRemoveEntryString(Test *t) {

	Test_setModule(t, "DLFile_removeEntryString");

	{						//Move last
		DLFile f = { 0 };

		if (!buildStringFile(t, &f, 3)) {
			Test_assert(t, "Setup 3-entry String file", false);
			goto doneBasic;
		}

		CharString    out    = CharString_createNull();
		DLEntryStream stream = { 0 };

		Test_assert(t, "removeEntryString id = 2 ok", DLFile_removeEntryString(&f, 2, &out, &stream, &t->err));
		Test_assert(t, "entryCount 2",                DLFile_entryCount(&f) == 2);

		Test_assert(t, "returned str",                CharString_length(out) == 2);
		Test_assert(t, "[0] still str0",              DLFile_entrySize(&f, 0) == 0);
		Test_assert(t, "[1] still str1",              DLFile_entrySize(&f, 1) == 1);

		CharString_free(&out, t->alloc);

	doneBasic:
		DLFile_free(&f, t->alloc);
	}

	{						//OOB
		DLFile f = { 0 };

		if (!buildStringFile(t, &f, 2)) {
			Test_assert(t, "Setup for OOB removeEntryString", false);
			goto doneOOB;
		}

		CharString    out    = CharString_createNull();
		DLEntryStream stream = { 0 };

		Test_assert(t, "removeEntryString OOB fails",   !DLFile_removeEntryString(&f, 2, &out, &stream, NULL));
		Test_assert(t, "entryCount unchanged",          DLFile_entryCount(&f) == 2);

	doneOOB:
		DLFile_free(&f, t->alloc);
	}

	{						//Type mismatch (data)
		DLFile f = { 0 };

		if (!buildDataFile(t, &f, 1)) {
			Test_assert(t, "Setup Data file for type mismatch", false);
			goto doneMismatch;
		}

		CharString    out    = CharString_createNull();
		DLEntryStream stream = { 0 };

		Test_assert(t, "removeEntryString on Data file fails",
			!DLFile_removeEntryString(&f, 0, &out, &stream, NULL));

	doneMismatch:
		DLFile_free(&f, t->alloc);
	}

	{						//Null guards
		DLFile f = { 0 };

		if (!buildStringFile(t, &f, 1)) {
			Test_assert(t, "Setup for null guard removeEntryString", false);
			goto doneNull;
		}

		CharString    out    = CharString_createNull();
		DLEntryStream stream = { 0 };

		Test_assert(t, "removeEntryString null dlFile", !DLFile_removeEntryString(NULL, 0, &out, &stream, NULL));
		Test_assert(t, "removeEntryString null str",    !DLFile_removeEntryString(&f,   0, NULL, &stream, NULL));
		Test_assert(t, "removeEntryString null stream", !DLFile_removeEntryString(&f,   0, &out, NULL,    NULL));

	doneNull:
		DLFile_free(&f, t->alloc);
	}
}

void Test_DLRemoveEntries(Test *t) {
	Test_DLRemove(t);
	Test_DLRemoveEntry(t);
	Test_DLRemoveEntryString(t);
}


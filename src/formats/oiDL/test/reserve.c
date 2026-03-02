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
#include "formats/oiDL/dl_entry.h"
#include "shared.h"

Bool buildDataFile(Test *t, DLFile *f, U64 count);

extern const DLSettings kSettingsData;
extern const DLSettings kSettingsStr;

void Test_DLReserve(Test *t) {

	Test_setModule(t, "DLFile_reserve");

	{						//Reserve on a fresh Data file: addEntry calls should not reallocate
		DLFile f = { 0 };

		if (!DLFile_create(&kSettingsData, 0, t->alloc, &f, &t->err)) {
			Test_assert(t, "Create Data file for reserve", false);
			goto doneReserveData;
		}

		Test_assert(t, "reserve 8 ok", DLFile_reserve(&f, 8, t->alloc, &t->err));

		//Fill all 8 reserved slots — none must fail due to allocation

		for (U64 i = 0; i < 8; ++i) {
			U8 val = (U8)i;
			Buffer buf = Buffer_createNull();

			if (!Buffer_createCopy(Buffer_createRefConst(&val, 1), t->alloc, &buf, &t->err)) {
				Test_assert(t, "Buffer_createCopy in reserve loop", false);
				break;
			}

			Test_assert(t, "addEntry within reserve ok", DLFile_addEntry(&f, &buf, t->alloc, &t->err));
		}

		Test_assert(t, "entryCount 8 after reserve fill", DLFile_entryCount(&f) == 8);

	doneReserveData:
		DLFile_free(&f, t->alloc);
	}

	{						//Reserve on a fresh String file
		DLFile f = { 0 };

		if (!DLFile_create(&kSettingsStr, 0, t->alloc, &f, &t->err)) {
			Test_assert(t, "Create String file for reserve", false);
			goto doneReserveStr;
		}

		Test_assert(t, "reserve 4 String ok", DLFile_reserve(&f, 4, t->alloc, &t->err));

		for (U64 i = 0; i < 4; ++i) {
			CharString s = CharString_createNull();

			if (!CharString_createCopy(CharString_createRefCStrConst("hi"), t->alloc, &s, &t->err)) {
				Test_assert(t, "CharString_createCopy in reserve loop", false);
				break;
			}

			Test_assert(t, "addEntryString within reserve ok", DLFile_addEntryString(&f, &s, t->alloc, &t->err));
		}

		Test_assert(t, "entryCount 4 after reserve fill", DLFile_entryCount(&f) == 4);

	doneReserveStr:
		DLFile_free(&f, t->alloc);
	}

	{						//Reserve(0) on an empty file is a no-op and must succeed
		DLFile f = { 0 };

		if (!DLFile_create(&kSettingsData, 0, t->alloc, &f, &t->err)) {
			Test_assert(t, "Create Data file for reserve(0)", false);
			goto doneReserveZero;
		}

		Test_assert(t, "reserve 0 ok",       DLFile_reserve(&f, 0, t->alloc, &t->err));
		Test_assert(t, "entryCount still 0", DLFile_entryCount(&f) == 0);

	doneReserveZero:
		DLFile_free(&f, t->alloc);
	}

	{						//Reserve on an already-populated file must not lose existing entries
		DLFile f = { 0 };

		if (!buildDataFile(t, &f, 3)) {
			Test_assert(t, "Setup 3-entry for reserve-after-add", false);
			goto doneReserveAfterAdd;
		}

		Test_assert(t, "reserve 16 ok",           DLFile_reserve(&f, 16, t->alloc, &t->err));
		Test_assert(t, "existing entries intact", DLFile_entryCount(&f) == 3);
		Test_assert(t, "[0] size intact",         DLFile_entrySize(&f, 0) == 0);
		Test_assert(t, "[1] size intact",         DLFile_entrySize(&f, 1) == 1);
		Test_assert(t, "[2] size intact",         DLFile_entrySize(&f, 2) == 2);

	doneReserveAfterAdd:
		DLFile_free(&f, t->alloc);
	}

	{						//Reserve smaller than current count must not truncate
		DLFile f = { 0 };

		if (!buildDataFile(t, &f, 5)) {
			Test_assert(t, "Setup 5-entry for reserve-smaller", false);
			goto doneReserveSmaller;
		}

		Test_assert(t, "reserve 2 ok (no truncate)",   DLFile_reserve(&f, 2, t->alloc, &t->err));
		Test_assert(t, "entryCount still 5",           DLFile_entryCount(&f) == 5);

	doneReserveSmaller:
		DLFile_free(&f, t->alloc);
	}

	//Null guard

	Test_assert(t, "reserve null dlFile fails", !DLFile_reserve(NULL, 4, t->alloc, NULL));
}

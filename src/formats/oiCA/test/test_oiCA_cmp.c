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

//formats/oiCA/test/test_oiCA_cmp.c

#include "test_oiCA_shared.h"
#include "types/container/memory_stream.h"
#include "formats/oiCA/ca_compare.h"
#include "formats/oiCA/ca_props.h"
#include "formats/oiCA/ca_edit.h"

void Test_CACompare(Test *t) {

	Test_setModule(t, "CAFile_compare");

	CAFile ca = (CAFile) { 0 };
	CAFile cb = (CAFile) { 0 };
	CAFile cc = (CAFile) { 0 };

	ECompareResult result = ECompareResult_Eq;

	CASettings settings = (CASettings) { 0 };

	if (!CAFile_create(&settings, 4, 0, t->alloc, &ca, &t->err)) {
		Test_assert(t, "Create ca", false);
		goto clean;
	}

	if (!CAFile_create(&settings, 4, 0, t->alloc, &cb, &t->err)) {
		Test_assert(t, "Create cb", false);
		goto clean;
	}

	//Add matching files to both

	CharString nameA = CharString_createRefCStrConst("file.bin");
	CharString nameB = CharString_createRefCStrConst("file.bin");

	CharString nameCopyA = CharString_createNull();
	CharString nameCopyB = CharString_createNull();

	if (!CharString_createCopy(nameA, t->alloc, &nameCopyA, &t->err)) {
		Test_assert(t, "Copy nameA", false);
		goto clean;
	}

	if (!CharString_createCopy(nameB, t->alloc, &nameCopyB, &t->err)) {
		Test_assert(t, "Copy nameB", false);
		CharString_free(&nameCopyA, t->alloc);
		goto clean;
	}

	CAHandle hA = CAFile_addFile(&ca, CAHandle_Root, &nameCopyA, 0, t->alloc, &t->err);
	CAHandle hB = CAFile_addFile(&cb, CAHandle_Root, &nameCopyB, 0, t->alloc, &t->err);
	CharString_free(&nameCopyA, t->alloc);
	CharString_free(&nameCopyB, t->alloc);

	Test_assert(t, "Add file to ca", hA != CAHandle_Invalid);
	Test_assert(t, "Add file to cb", hB != CAHandle_Invalid);

	if (hA == CAHandle_Invalid || hB == CAHandle_Invalid)
		goto clean;

	//Write identical data

	U8 src[64];

	for (U8 i = 0; i < 64; ++i)
		src[i] = i;

	Buffer bufA = Buffer_createNull();
	Buffer bufB = Buffer_createNull();

	if (!Buffer_createCopy(Buffer_createRefConst(src, 64), t->alloc, &bufA, &t->err)) {
		Test_assert(t, "Create bufA", false);
		goto clean;
	}

	if (!Buffer_createCopy(Buffer_createRefConst(src, 64), t->alloc, &bufB, &t->err)) {
		Test_assert(t, "Create bufB", false);
		Buffer_free(&bufA, t->alloc);
		goto clean;
	}

	if (!CAFile_setData(&ca, hA, t->alloc, &bufA, &t->err)) {
		Test_assert(t, "SetData ca", false);
		goto clean;
	}

	if (!CAFile_setData(&cb, hB, t->alloc, &bufB, &t->err)) {
		Test_assert(t, "SetData cb", false);
		goto clean;
	}

	//Equal buffers

	if (CAFile_dataEqual(&ca, hA, &cb, hB, t->alloc, &result, &t->err))
		Test_assert(t, "Identical data: Eq", result == ECompareResult_Eq);

	else Test_assert(t, "Identical data: no error", false);

	//Modify one byte in cb -> A < B

	Bool bValid = false;
	Buffer bData = CAFile_getData(&cb, hB, &bValid);

	if (bValid) {

		bData.ptrNonConst[32] = 255;

		if (CAFile_dataEqual(&ca, hA, &cb, hB, t->alloc, &result, &t->err))
			Test_assert(t, "A[32]=32 < B[32]=255: Lt", result == ECompareResult_Lt);

		else Test_assert(t, "Modified byte: no error", false);

		if (CAFile_dataEqual(&cb, hB, &ca, hA, t->alloc, &result, &t->err))
			Test_assert(t, "B[32]=255 > A[32]=32: Gt", result == ECompareResult_Gt);

		else Test_assert(t, "Modified byte symmetric: no error", false);

		//Restore

		bData.ptrNonConst[32] = 32;
	}

	//Different sizes: add a smaller file to a new archive

	if (!CAFile_create(&settings, 1, 0, t->alloc, &cc, &t->err)) {
		Test_assert(t, "Create cc", false);
		goto clean;
	}

	CharString nameCopyC = CharString_createNull();
	CharString nameC = CharString_createRefCStrConst("file.bin");

	if (!CharString_createCopy(nameC, t->alloc, &nameCopyC, &t->err)) {
		Test_assert(t, "Copy nameC", false);
		CAFile_free(&cc, t->alloc);
		goto clean;
	}

	CAHandle hC = CAFile_addFile(&cc, CAHandle_Root, &nameCopyC, 0, t->alloc, &t->err);
	Test_assert(t, "Add smaller file to cc", hC != CAHandle_Invalid);

	if (hC != CAHandle_Invalid) {

		Buffer bufC = Buffer_createNull();

		if (
			Buffer_createCopy(Buffer_createRefConst(src, 32), t->alloc, &bufC, &t->err) &&
			CAFile_setData(&cc, hC, t->alloc, &bufC, &t->err)
		) {
			if (CAFile_dataEqual(&ca, hA, &cc, hC, t->alloc, &result, &t->err))
				Test_assert(t, "Larger A(64) > smaller C(32): Gt", result == ECompareResult_Gt);

			else Test_assert(t, "Different sizes: no error", false);

			if (CAFile_dataEqual(&cc, hC, &ca, hA, t->alloc, &result, &t->err))
				Test_assert(t, "Smaller C(32) < larger A(64): Lt", result == ECompareResult_Lt);

			else Test_assert(t, "Different sizes symmetric: no error", false);
		}
	}

	//Error cases: folder handle rejected

	CharString folderName = CharString_createRefCStrConst("dir");
	CharString folderCopy = CharString_createNull();

	if (CharString_createCopy(folderName, t->alloc, &folderCopy, &t->err)) {

		CAHandle hFolder = CAFile_addFolder(&ca, CAHandle_Root, &folderCopy, t->alloc, &t->err);
		Test_assert(t, "Add folder", hFolder != CAHandle_Invalid);

		if (hFolder != CAHandle_Invalid)
			Test_assert(t, "Folder handle rejected", !CAFile_dataEqual(&ca, hFolder, &cb, hB, t->alloc, &result, NULL));
	}

clean:
	CAFile_free(&cc, t->alloc);
	CAFile_free(&ca, t->alloc);
	CAFile_free(&cb, t->alloc);
}

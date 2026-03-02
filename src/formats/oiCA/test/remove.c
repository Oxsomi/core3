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

#include "shared.h"
#include "types/base/string_read_helper.h"
#include "formats/oiCA/ca_edit.h"
#include "formats/oiCA/ca_props.h"
#include "formats/oiCA/ca_lookup.h"

extern const CASettings kCASettings;

CAHandle addFile(Test *t, CAFile *ca, CAHandle parent, const C8 *name, Ns time, Bool failIsSuccess);
CAHandle addFolder(Test *t, CAFile *ca, CAHandle parent, const C8 *name, Bool failIsSuccess);

void Test_CARemoveFile(Test *t) {

	Test_setModule(t, "CAFile_removeFile");

	{							//Remove middle file, remaining files must shift and stay resolvable
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for removeFile", false);
			goto doneRemoveMid;
		}

		addFile(t, &ca, root, "a.txt", 0, false);
		CAHandle hb = addFile(t, &ca, root, "b.txt", 0, false);
		addFile(t, &ca, root, "c.txt", 0, false);

		Test_assert(t, "remove b.txt ok",        CAFile_removeFile(&ca, hb, t->alloc, &t->err));
		Test_assert(t, "files.length 2",         ca.files.length == 2);
		Test_assert(t, "fileCount 2",            CAFile_fileCount(&ca, root, false) == 2);

		//a and c must still resolve correctly

		CharString pa = CharString_createRefCStrConst("a.txt");
		CharString pc = CharString_createRefCStrConst("c.txt");
		Test_assert(t, "a.txt still found",      CAFile_resolve(&ca, pa) != CAHandle_Invalid);
		Test_assert(t, "c.txt still found",      CAFile_resolve(&ca, pc) != CAHandle_Invalid);

		//b must be gone

		CharString pb = CharString_createRefCStrConst("b.txt");
		Test_assert(t, "b.txt gone",             CAFile_resolve(&ca, pb) == CAHandle_Invalid);

	doneRemoveMid:
		CAFile_free(&ca, t->alloc);
	}

	{							//Remove first file, second becomes index 0
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for removeFront", false);
			goto doneRemoveFront;
		}

		CAHandle ha = addFile(t, &ca, root, "a.txt", 0, false);
		addFile(t, &ca, root, "b.txt", 0, false);

		Test_assert(t, "remove a.txt ok",        CAFile_removeFile(&ca, ha, t->alloc, &t->err));

		CharString first = CAFile_getName(&ca, CAFile_fileAt(&ca, root, 0));
		Test_assert(t, "first is now b.txt",     CharString_equalsCStringSensitive(&first, "b.txt"));

	doneRemoveFront:
		CAFile_free(&ca, t->alloc);
	}

	{							//Remove last file
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for removeLast", false);
			goto doneRemoveLast;
		}

		addFile(t, &ca, root, "a.txt", 0, false);
		CAHandle hb = addFile(t, &ca, root, "b.txt", 0, false);

		Test_assert(t, "remove b.txt ok",        CAFile_removeFile(&ca, hb, t->alloc, &t->err));
		Test_assert(t, "files.length 1",         ca.files.length == 1);

		CharString only = CAFile_getName(&ca, CAFile_fileAt(&ca, root, 0));
		Test_assert(t, "remaining is a.txt",     CharString_equalsCStringSensitive(&only, "a.txt"));

	doneRemoveLast:
		CAFile_free(&ca, t->alloc);
	}

	{							//Remove only file, folder becomes empty, fileOffset resets to 0
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for removeOnly", false);
			goto doneRemoveOnly;
		}

		CAHandle h = addFile(t, &ca, root, "only.txt", 0, false);

		Test_assert(t, "remove only ok",         CAFile_removeFile(&ca, h, t->alloc, &t->err));
		Test_assert(t, "files.length 0",         ca.files.length == 0);
		Test_assert(t, "fileCount root 0",       CAFile_fileCount(&ca, root, false) == 0);
		Test_assert(t, "root fileOffset reset",  ca.folders.ptr[0].fileOffset == 0);

	doneRemoveOnly:
		CAFile_free(&ca, t->alloc);
	}

	{							//Remove file, fileOffsets in sibling folders must be adjusted
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for fileOffset fixup", false);
			goto doneOffsetFixup;
		}

		CAHandle hA = addFolder(t, &ca, root, "A", false);
		CAHandle hB = addFolder(t, &ca, root, "B", false);

		//Two files in A, one in B

		CAHandle ha0 = addFile(t, &ca, hA, "a0.txt", 0, false);
		addFile(t, &ca, hA, "a1.txt", 0, false);
		addFile(t, &ca, hB, "b0.txt", 0, false);

		//Remove the first file in A; B's fileOffset must decrease by 1

		U64 bOffsetBefore = ca.folders.ptr[CAHandle_getId(hB)].fileOffset;

		Test_assert(t, "remove a0 ok",           CAFile_removeFile(&ca, ha0, t->alloc, &t->err));
		Test_assert(t, "files.length 2",         ca.files.length == 2);

		U64 bOffsetAfter = ca.folders.ptr[CAHandle_getId(hB)].fileOffset;
		Test_assert(t, "B fileOffset decremented", bOffsetAfter == bOffsetBefore - 1);

		//B's file must still resolve

		CharString pb0 = CharString_createRefCStrConst("B/b0.txt");
		Test_assert(t, "B/b0.txt still found",   CAFile_resolve(&ca, pb0) != CAHandle_Invalid);

	doneOffsetFixup:
		CAFile_free(&ca, t->alloc);
	}

	{							//Removing a folder handle via removeFile must fail
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for removeFile folder guard", false);
			goto doneRemoveFolderGuard;
		}

		CAHandle hd = addFolder(t, &ca, root, "dir", false);

		Test_assert(t, "removeFile on folder fails", !CAFile_removeFile(&ca, hd, t->alloc, NULL));

	doneRemoveFolderGuard:
		CAFile_free(&ca, t->alloc);
	}
}

void Test_CARemoveFolder(Test *t) {

	Test_setModule(t, "CAFile_removeFolder");

	{							//Remove empty leaf folder
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for removeFolder leaf", false);
			goto doneLeaf;
		}

		CAHandle hd = addFolder(t, &ca, root, "empty", false);

		Test_assert(t, "remove empty ok",        CAFile_removeFolder(&ca, hd, t->alloc, &t->err));
		Test_assert(t, "folders.length 1",       ca.folders.length == 1);
		Test_assert(t, "dirCount root 0",        CAFile_dirCount(&ca, root, false) == 0);

	doneLeaf:
		CAFile_free(&ca, t->alloc);
	}

	{							//Remove folder that has only subdirs (no files at any level)
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for removeFolder dirs-only", false);
			goto doneDirsOnly;
		}

		CAHandle hOuter = addFolder(t, &ca, root,   "outer", false);
		CAHandle hInner = addFolder(t, &ca, hOuter, "inner", false);
		addFolder(t, &ca, hInner, "leaf", false);

		Test_assert(t, "folders 4 before",       ca.folders.length == 4);
		Test_assert(t, "remove outer ok",        CAFile_removeFolder(&ca, hOuter, t->alloc, &t->err));
		Test_assert(t, "folders 1 after",        ca.folders.length == 1);
		Test_assert(t, "files 0 after",          ca.files.length   == 0);
		Test_assert(t, "dirCount root 0",        CAFile_dirCount(&ca, root, false) == 0);

	doneDirsOnly:
		CAFile_free(&ca, t->alloc);
	}


	{							//Remove non-empty folder recursively, all contents gone
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for removeFolder recursive", false);
			goto doneRecursive;
		}

		CAHandle hSrc = addFolder(t, &ca, root, "src", false);
		CAHandle hSub = addFolder(t, &ca, hSrc, "sub", false);
		addFile(t, &ca, hSrc, "main.c",    0, false);
		addFile(t, &ca, hSub, "helper.c",  0, false);

		Test_assert(t, "remove src recursive ok", CAFile_removeFolder(&ca, hSrc, t->alloc, &t->err));
		Test_assert(t, "files.length 0",          ca.files.length == 0);
		Test_assert(t, "folders.length 1",        ca.folders.length == 1);

	doneRecursive:
		CAFile_free(&ca, t->alloc);
	}

	{							//Removing root must fail
		CAFile ca   = { 0 };

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for removeFolder root guard", false);
			goto doneRootGuard;
		}

		Test_assert(t, "removeFolder root fails", !CAFile_removeFolder(&ca, CAHandle_Root, t->alloc, NULL));

	doneRootGuard:
		CAFile_free(&ca, t->alloc);
	}

	{							//CAFile_remove on non-empty folder (without recursion) must fail
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for remove non-empty guard", false);
			goto doneNonEmpty;
		}

		CAHandle hd = addFolder(t, &ca, root, "full", false);
		addFile(t, &ca, hd, "x.txt", 0, false);

		Test_assert(t, "remove non-empty folder fails", !CAFile_remove(&ca, hd, t->alloc, NULL));

	doneNonEmpty:
		CAFile_free(&ca, t->alloc);
	}

	{							//Remove folder, sibling folder dirOffset must be adjusted
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for dirOffset fixup", false);
			goto doneDirOffset;
		}

		CAHandle hA = addFolder(t, &ca, root, "A", false);
		addFolder(t, &ca, root, "B", false);
		CAHandle hC = addFolder(t, &ca, root, "C", false);

		U64 cIdBefore = CAHandle_getId(hC);

		//Remove A; B and C must shift down and still be resolvable

		Test_assert(t, "remove A ok",            CAFile_removeFolder(&ca, hA, t->alloc, &t->err));
		Test_assert(t, "folders.length 3",       ca.folders.length == 3);		//root + B + C

		CharString pathB = CharString_createRefCStrConst("B");
		CharString pathC = CharString_createRefCStrConst("C");
		Test_assert(t, "B still found",          CAFile_resolve(&ca, pathB) != CAHandle_Invalid);
		Test_assert(t, "C still found",          CAFile_resolve(&ca, pathC) != CAHandle_Invalid);

		//C's id should have shifted

		CAHandle hCNew = CAFile_resolve(&ca, pathC);
		Test_assert(t, "C id shifted by -1",     CAHandle_getId(hCNew) == cIdBefore - 1);

	doneDirOffset:
		CAFile_free(&ca, t->alloc);
	}

	{							//Remove a folder that contains files; verify recursive fileObjectCount after removal
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for post-remove counts", false);
			goto donePostRemoveCounts;
		}

		CAHandle hA = addFolder(t, &ca, root, "A", false);
		CAHandle hB = addFolder(t, &ca, root, "B", false);

		addFile(t, &ca, hA, "a1.txt", 0, false);
		addFile(t, &ca, hA, "a2.txt", 0, false);
		addFile(t, &ca, hB, "b1.txt", 0, false);
		addFile(t, &ca, root, "r.txt", 0, false);

		//Before: root recursive = 3 dirs (A,B) + ... wait, A and B are direct children,
		//recursive fileObjectCount = 2 dirs + 4 files = 6.
		Test_assert(t, "before: root objCount r 6", CAFile_fileObjectCount(&ca, root, true) == 6);

		//Remove A (contains 2 files); all handles stale after this, re-resolve B.
		Test_assert(t, "remove A ok",            CAFile_removeFolder(&ca, hA, t->alloc, &t->err));

		//After: root recursive = 1 dir (B) + 2 files (b1, r) = 3 objects.
		Test_assert(t, "after: root objCount r 3", CAFile_fileObjectCount(&ca, root, true) == 3);

		CharString pathB = CharString_createRefCStrConst("B");
		CAHandle hBNew = CAFile_resolve(&ca, pathB);
		Test_assert(t, "B still found",          hBNew != CAHandle_Invalid);
		Test_assert(t, "B fileCount 1",          CAFile_fileCount(&ca, hBNew, false) == 1);

		CharString pathR = CharString_createRefCStrConst("r.txt");
		Test_assert(t, "r.txt still found",      CAFile_resolve(&ca, pathR) != CAHandle_Invalid);

	donePostRemoveCounts:
		CAFile_free(&ca, t->alloc);
	}
}

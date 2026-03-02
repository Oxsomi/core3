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
#include "formats/oiCA/ca_lookup.h"

extern const CASettings kCASettings;

CAHandle addFile(Test *t, CAFile *ca, CAHandle parent, const C8 *name, Ns time, Bool failIsSuccess);
CAHandle addFolder(Test *t, CAFile *ca, CAHandle parent, const C8 *name, Bool failIsSuccess);

static inline CAHandle CAFile_resolveCStr(CAFile *ca, const C8 *name) {
	return CAFile_resolve(ca, CharString_createRefCStrConst(name));
}

void Test_CARename(Test *t) {

	Test_setModule(t, "CAFile_rename");

	{
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for rename", false);
			goto doneRename;
		}

		CAHandle hf  = addFile(t,   &ca, root, "old.txt", 0, false);
		CAHandle hd  = addFolder(t, &ca, root, "oldDir", false);

		//Rename file

		CharString newName = CharString_createNull();
		CharString_createCopy(CharString_createRefCStrConst("new.txt"), t->alloc, &newName, NULL);
		Test_assert(t, "rename file ok",         CAFile_rename(&ca, hf, t->alloc, &newName, &t->err));
		Test_assert(t, "newName zeroed out",     newName.ptr == NULL);

		CharString nameAfter = CAFile_getName(&ca, hf);
		Test_assert(t, "name is now new.txt",    CharString_equalsCStringSensitive(&nameAfter, "new.txt"));

		//Old name no longer resolvable, new name is

		Test_assert(t, "old.txt gone",           CAFile_resolveCStr(&ca, "old.txt") == CAHandle_Invalid);
		Test_assert(t, "new.txt found",          CAFile_resolveCStr(&ca, "new.txt") == hf);

		//Rename folder

		CharString newDir = CharString_createNull();
		CharString_createCopy(CharString_createRefCStrConst("newDir"), t->alloc, &newDir, NULL);
		Test_assert(t, "rename folder ok",       CAFile_rename(&ca, hd, t->alloc, &newDir, &t->err));

		Test_assert(t, "newDir found",           CAFile_resolveCStr(&ca, "newDir") != CAHandle_Invalid);

		//Rename to existing name must fail

		CharString dup = CharString_createNull();
		CharString_createCopy(CharString_createRefCStrConst("new.txt"), t->alloc, &dup, NULL);
		addFile(t, &ca, root, "other.txt", 0, false);

		CharString other = CharString_createNull();
		CharString_createCopy(CharString_createRefCStrConst("new.txt"), t->alloc, &other, NULL);
		CAHandle hOther = CAFile_resolveCStr(&ca, "other.txt");
		Test_assert(t, "rename to existing fails", !CAFile_rename(&ca, hOther, t->alloc, &other, NULL));
		CharString_free(&other, t->alloc);
		CharString_free(&dup, t->alloc);

		//Rename root must fail

		CharString rootName = CharString_createNull();
		CharString_createCopy(CharString_createRefCStrConst("root"), t->alloc, &rootName, NULL);
		Test_assert(t, "rename root fails",      !CAFile_rename(&ca, CAHandle_Root, t->alloc, &rootName, NULL));
		CharString_free(&rootName, t->alloc);

	doneRename:
		CharString_free(&newName, t->alloc);
		CAFile_free(&ca, t->alloc);
	}
}

void Test_CAMove(Test *t) {

	Test_setModule(t, "CAFile_move");

	{							//Move file from one folder to another
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for move file", false);
			goto doneMoveFile;
		}

		CAHandle hSrc = addFolder(t, &ca, root, "src", false);
		CAHandle hDst = addFolder(t, &ca, root, "dst", false);
		CAHandle hf   = addFile(t, &ca, hSrc, "move.txt", 0, false);

		Test_assert(t, "move file ok",           CAFile_move(&ca, hf, hDst, t->alloc, &t->err));

		//src no longer has the file, re-resolve; hf is now stale

		Test_assert(t, "src/move.txt gone",      CAFile_resolveCStr(&ca, "src/move.txt") == CAHandle_Invalid);

		//dst now has it

		Test_assert(t, "dst/move.txt found",     CAFile_resolveCStr(&ca, "dst/move.txt") != CAHandle_Invalid);

		//Counts updated, re-resolve hSrc/hDst since a file move can shift file
		//indices but does NOT shift folder indices, so hSrc and hDst remain valid.

		Test_assert(t, "src fileCount 0",        CAFile_fileCount(&ca, hSrc, false) == 0);
		Test_assert(t, "dst fileCount 1",        CAFile_fileCount(&ca, hDst, false) == 1);

	doneMoveFile:
		CAFile_free(&ca, t->alloc);
	}

	{							//Move subfolder into another folder
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for move folder", false);
			goto doneMoveFolder;
		}

		CAHandle hA = addFolder(t, &ca, root, "A", false);
		CAHandle hB = addFolder(t, &ca, root, "B", false);
		CAHandle hC = addFolder(t, &ca, hA,   "C", false);

		Test_assert(t, "move folder ok", CAFile_move(&ca, hC, hB, t->alloc, &t->err));

		//hA, hB, hC are all stale after moving a folder, re-resolve everything

		Test_assert(t, "A/C gone",       CAFile_resolveCStr(&ca, "A/C") == CAHandle_Invalid);
		Test_assert(t, "B/C found",      CAFile_resolveCStr(&ca, "B/C") != CAHandle_Invalid);

		CAHandle hANew = CAFile_resolveCStr(&ca, "A");
		CAHandle hBNew = CAFile_resolveCStr(&ca, "B");
		Test_assert(t, "A dirCount 0",           CAFile_dirCount(&ca, hANew, false) == 0);
		Test_assert(t, "B dirCount 1",           CAFile_dirCount(&ca, hBNew, false) == 1);

	doneMoveFolder:
		CAFile_free(&ca, t->alloc);
	}

	{							//Move non-empty folder (with files) into a non-empty destination
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for move non-empty folder", false);
			goto doneMoveNonEmpty;
		}

		CAHandle hSrc = addFolder(t, &ca, root, "src", false);
		CAHandle hDst = addFolder(t, &ca, root, "dst", false);
		CAHandle hSub = addFolder(t, &ca, hSrc, "sub", false);

		addFile(t, &ca, hSrc, "s1.txt", 0, false);
		addFile(t, &ca, hSub, "s2.txt", 0, false);
		addFile(t, &ca, hDst, "d1.txt", 0, false);

		//Move hSrc (contains sub/, s1.txt) into dst.
		//After this all handles derived from folder indices may be stale.

		Test_assert(t, "move non-empty folder ok", CAFile_move(&ca, hSrc, hDst, t->alloc, &t->err));

		//Re-resolve everything by path

		Test_assert(t, "dst/src found",            CAFile_resolveCStr(&ca, "dst/src") != CAHandle_Invalid);
		Test_assert(t, "dst/src/s1.txt found",     CAFile_resolveCStr(&ca, "dst/src/s1.txt") != CAHandle_Invalid);
		Test_assert(t, "dst/src/sub/s2.txt found", CAFile_resolveCStr(&ca, "dst/src/sub/s2.txt") != CAHandle_Invalid);
		Test_assert(t, "dst/d1.txt found",         CAFile_resolveCStr(&ca, "dst/d1.txt") != CAHandle_Invalid);
		Test_assert(t, "src gone from root",       CAFile_resolveCStr(&ca, "src") == CAHandle_Invalid);

		CAHandle hDstNew = CAFile_resolveCStr(&ca, "dst");
		Test_assert(t, "dst has 1 dir",            CAFile_dirCount(&ca, hDstNew, false) == 1);
		Test_assert(t, "dst recursive files 3",    CAFile_fileCount(&ca, hDstNew, true) == 3);

	doneMoveNonEmpty:
		CAFile_free(&ca, t->alloc);
	}

	{							//Move to same parent is legal (no-op structurally) and must not crash
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for move same parent", false);
			goto doneMoveSame;
		}

		CAHandle hf = addFile(t, &ca, root, "same.txt", 0, false);

		Test_assert(t, "move to same parent ok", CAFile_move(&ca, hf, root, t->alloc, &t->err));
		Test_assert(t, "still in root",          CAFile_fileCount(&ca, root, false) == 1);

	doneMoveSame:
		CAFile_free(&ca, t->alloc);
	}

	{							//Name collision in destination must fail
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for move collision", false);
			goto doneMoveCollide;
		}

		CAHandle hSrc = addFolder(t, &ca, root, "src", false);
		CAHandle hDst = addFolder(t, &ca, root, "dst", false);
		addFile(t, &ca, hSrc, "clash.txt", 0, false);
		addFile(t, &ca, hDst, "clash.txt", 0, false);

		CAHandle hf = CAFile_resolveCStr(&ca, "src/clash.txt");
		Test_assert(t, "move collision fails",   !CAFile_move(&ca, hf, hDst, t->alloc, NULL));

	doneMoveCollide:
		CAFile_free(&ca, t->alloc);
	}

	{							//Moving root must fail
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for move root guard", false);
			goto doneMoveRoot;
		}

		CAHandle hD = addFolder(t, &ca, root, "d", false);
		Test_assert(t, "move root fails",        !CAFile_move(&ca, root, hD, t->alloc, NULL));

	doneMoveRoot:
		CAFile_free(&ca, t->alloc);
	}

	{							//Moving a folder into itself must fail
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for move self guard", false);
			goto doneMoveSelf;
		}

		CAHandle hD = addFolder(t, &ca, root, "d", false);
		Test_assert(t, "move into self fails",   !CAFile_move(&ca, hD, hD, t->alloc, NULL));

	doneMoveSelf:
		CAFile_free(&ca, t->alloc);
	}

	{							//Moving a folder into one of its own descendants must fail
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for move descendant guard", false);
			goto doneMoveDescendant;
		}

		CAHandle hParent = addFolder(t, &ca, root,    "parent", false);
		CAHandle hChild  = addFolder(t, &ca, hParent, "child", false);

		//Moving parent into child (a descendant) must fail

		Test_assert(t, "move into descendant fails", !CAFile_move(&ca, hParent, hChild, t->alloc, NULL));

		//Verify tree is unmodified after the failed move

		Test_assert(t, "parent still at root", CAFile_resolveCStr(&ca, "parent") != CAHandle_Invalid);
		Test_assert(t, "child still in parent", CAFile_resolveCStr(&ca, "parent/child") != CAHandle_Invalid);

	doneMoveDescendant:
		CAFile_free(&ca, t->alloc);
	}
}

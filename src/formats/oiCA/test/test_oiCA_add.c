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

//formats/oiCA/test/test_oiCA_add.c

#include "test_oiCA_shared.h"
#include "types/container/file_base.h"
#include "types/base/string_read_helper.h"
#include "formats/oiCA/ca_edit.h"
#include "formats/oiCA/ca_lookup.h"

extern const CASettings kCASettings;

CAHandle addFile(Test *t, CAFile *ca, CAHandle parent, const C8 *name, Ns time, Bool failIsSuccess) {

	CharString s = CharString_createNull();

	if (!CharString_createCopy(CharString_createRefCStrConst(name), t->alloc, &s, &t->err)) {
		Test_assert(t, "CharString_createCopy addFile", false);
		return CAHandle_Invalid;
	}

	CAHandle h = CAFile_addFile(ca, parent, &s, time, t->alloc, failIsSuccess ? NULL : &t->err);
	CharString_free(&s, t->alloc);
	return h;
}

CAHandle addFolder(Test *t, CAFile *ca, CAHandle parent, const C8 *name, Bool failIsSuccess) {

	CharString s = CharString_createNull();

	if (!CharString_createCopy(CharString_createRefCStrConst(name), t->alloc, &s, &t->err)) {
		Test_assert(t, "CharString_createCopy addFolder", false);
		return CAHandle_Invalid;
	}

	CAHandle h = CAFile_addFolder(ca, parent, &s, t->alloc, failIsSuccess ? NULL : &t->err);
	CharString_free(&s, t->alloc);
	return h;
}

void Test_CAAddFile(Test *t) {
	
	Test_setModule(t, "CAFile_addFile");

	{                        //Single file added to root
		CAFile ca = { 0 };
		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for addFile", false);
			goto doneAddSingle;
		}

		CAHandle root = CAHandle_Root;
		CAHandle h    = addFile(t, &ca, root, "readme.txt", 0, false);

		Test_assert(t, "addFile ok",             h != CAHandle_Invalid);
		Test_assert(t, "isFile",                 CAHandle_isFile(h));
		Test_assert(t, "files.length 1",         ca.files.length == 1);
		Test_assert(t, "fileCount root 1",       CAFile_fileCount(&ca, root, false) == 1);
		Test_assert(t, "dirCount root 0",        CAFile_dirCount(&ca, root, false) == 0);

		CharString name = CAFile_getName(&ca, h);
		Test_assert(t, "getName readme.txt",     CharString_equalsCStringSensitive(&name, "readme.txt"));

	doneAddSingle:
		CAFile_free(&ca, t->alloc);
	}

	{                            //Multiple files maintain insertion order
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for addFile order", false);
			goto doneAddOrder;
		}

		CAHandle ha = addFile(t, &ca, root, "a.txt", 0, false);
		CAHandle hb = addFile(t, &ca, root, "b.txt", 0, false);
		CAHandle hc = addFile(t, &ca, root, "c.txt", 0, false);

		Test_assert(t, "three files",            ca.files.length == 3);

		//fileAt must return them in insertion order

		Test_assert(t, "fileAt 0 == ha",         CAFile_fileAt(&ca, root, 0) == ha);
		Test_assert(t, "fileAt 1 == hb",         CAFile_fileAt(&ca, root, 1) == hb);
		Test_assert(t, "fileAt 2 == hc",         CAFile_fileAt(&ca, root, 2) == hc);

		//Names must survive the order

		CharString na = CAFile_getName(&ca, ha);
		CharString nb = CAFile_getName(&ca, hb);
		CharString nc = CAFile_getName(&ca, hc);

		Test_assert(t, "name[0] == a.txt",       CharString_equalsCStringSensitive(&na, "a.txt"));
		Test_assert(t, "name[1] == b.txt",       CharString_equalsCStringSensitive(&nb, "b.txt"));
		Test_assert(t, "name[2] == c.txt",       CharString_equalsCStringSensitive(&nc, "c.txt"));

	doneAddOrder:
		CAFile_free(&ca, t->alloc);
	}

	{                            //Duplicate name in same folder must fail
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for dup name", false);
			goto doneDupName;
		}

		addFile(t, &ca, root, "dup.txt", 0, false);
		CAHandle hDup = addFile(t, &ca, root, "dup.txt", 0, true);

		Test_assert(t, "duplicate name fails",   hDup == CAHandle_Invalid);
		Test_assert(t, "still 1 file",           ca.files.length == 1);

	doneDupName:
		CAFile_free(&ca, t->alloc);
	}

	{                            //Names differing only in case are distinct (resolution is case-sensitive)
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for case distinct", false);
			goto doneCaseDistinct;
		}

		CAHandle hAssets = addFolder(t, &ca, root, "Assets", false);
		CAHandle hAssets2 = addFile(t, &ca, root, "assets", 0, false);

		//Both must succeed, "Assets" (folder) and "assets" (file) are different names

		Test_assert(t, "Assets folder added",    hAssets  != CAHandle_Invalid);
		Test_assert(t, "assets file added",      hAssets2 != CAHandle_Invalid);
		Test_assert(t, "folders 2",              ca.folders.length == 2);
		Test_assert(t, "files 1",                ca.files.length   == 1);

		//Exact-case lookup finds each independently

		CharString upper = CharString_createRefCStrConst("Assets");
		CharString lower = CharString_createRefCStrConst("assets");
		Test_assert(t, "Assets resolves as folder", CAFile_resolve(&ca, upper) == hAssets);
		Test_assert(t, "assets resolves as file",   CAFile_resolve(&ca, lower) == hAssets2);

	doneCaseDistinct:
		CAFile_free(&ca, t->alloc);
	}

	{                            //Name containing path separator must fail
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for slash name", false);
			goto doneSlashName;
		}

		CAHandle hSlash = addFile(t, &ca, root, "a/b", 0, true);
		Test_assert(t, "/ in name fails",    hSlash == CAHandle_Invalid);

		CAHandle hDotDot = addFile(t, &ca, root, "../escape", 0, true);
		Test_assert(t, ".. in name fails",   hDotDot == CAHandle_Invalid);

	doneSlashName:
		CAFile_free(&ca, t->alloc);
	}

	{                        //Null caFile / null name must fail
		CAFile ca = { 0 };
		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for null guards", false);
			goto doneNullGuards;
		}

		CharString s = CharString_createNull();
		CharString_createCopy(CharString_createRefCStrConst("f.txt"), t->alloc, &s, NULL);

		CAHandle hNull = CAFile_addFile(&ca, CAHandle_Root, NULL, 0, t->alloc, NULL);
		Test_assert(t, "null name fails",        hNull == CAHandle_Invalid);

		CAHandle hNullCA = CAFile_addFile(NULL, CAHandle_Root, &s, 0, t->alloc, NULL);
		Test_assert(t, "null caFile fails",      hNullCA == CAHandle_Invalid);

		CharString_free(&s, t->alloc);

	doneNullGuards:
		CAFile_free(&ca, t->alloc);
	}

	{                            //Parent must be a folder handle; passing a file handle must fail
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for bad parent", false);
			goto doneBadParent;
		}

		CAHandle hf = addFile(t, &ca, root, "parent.txt", 0, false);

		//Try to add a child to a file handle (must fail)

		CAHandle hChild = addFile(t, &ca, hf, "child.txt", 0, true);
		Test_assert(t, "file as parent fails",   hChild == CAHandle_Invalid);

	doneBadParent:
		CAFile_free(&ca, t->alloc);
	}
}

void Test_CAAddFolder(Test *t) {

	Test_setModule(t, "CAFile_addFolder");

	{                            //Single subfolder added to root
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for addFolder", false);
			goto doneFolderSingle;
		}

		CAHandle hd = addFolder(t, &ca, root, "src", false);

		Test_assert(t, "addFolder ok",           hd != CAHandle_Invalid);
		Test_assert(t, "isFolder",               CAHandle_isFolder(hd));
		Test_assert(t, "folders.length 2",       ca.folders.length == 2);    //root + src
		Test_assert(t, "dirCount root 1",        CAFile_dirCount(&ca, root, false) == 1);
		Test_assert(t, "fileCount root 0",       CAFile_fileCount(&ca, root, false) == 0);

		CharString name = CAFile_getName(&ca, hd);
		Test_assert(t, "getName src",            CharString_equalsCStringSensitive(&name, "src"));

	doneFolderSingle:
		CAFile_free(&ca, t->alloc);
	}

	{                            //Nested folders: root -> a -> b -> c
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for nested folders", false);
			goto doneNested;
		}

		CAHandle ha = addFolder(t, &ca, root, "a", false);
		CAHandle hb = addFolder(t, &ca, ha,   "b", false);
		CAHandle hc = addFolder(t, &ca, hb,   "c", false);

		Test_assert(t, "folders.length 4",       ca.folders.length == 4);
		Test_assert(t, "root dirCount 1",        CAFile_dirCount(&ca, root, false) == 1);
		Test_assert(t, "a dirCount 1",           CAFile_dirCount(&ca, ha,   false) == 1);
		Test_assert(t, "b dirCount 1",           CAFile_dirCount(&ca, hb,   false) == 1);
		Test_assert(t, "c dirCount 0",           CAFile_dirCount(&ca, hc,   false) == 0);

		//Recursive count from root must sum all nested dirs (3: a, b, c)

		Test_assert(t, "root recursive dirCount 3", CAFile_dirCount(&ca, root, true) == 3);

		//dirAt must return the right handles

		Test_assert(t, "dirAt root 0 == ha",     CAFile_dirAt(&ca, root, 0) == ha);
		Test_assert(t, "dirAt ha 0 == hb",       CAFile_dirAt(&ca, ha,   0) == hb);
		Test_assert(t, "dirAt hb 0 == hc",       CAFile_dirAt(&ca, hb,   0) == hc);

	doneNested:
		CAFile_free(&ca, t->alloc);
	}

	{                            //Multiple sibling folders maintain insertion order
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for sibling folders", false);
			goto doneSiblings;
		}

		CAHandle hx = addFolder(t, &ca, root, "x", false);
		CAHandle hy = addFolder(t, &ca, root, "y", false);
		CAHandle hz = addFolder(t, &ca, root, "z", false);

		Test_assert(t, "dirCount 3",             CAFile_dirCount(&ca, root, false) == 3);
		Test_assert(t, "dirAt 0 == hx",          CAFile_dirAt(&ca, root, 0) == hx);
		Test_assert(t, "dirAt 1 == hy",          CAFile_dirAt(&ca, root, 1) == hy);
		Test_assert(t, "dirAt 2 == hz",          CAFile_dirAt(&ca, root, 2) == hz);

	doneSiblings:
		CAFile_free(&ca, t->alloc);
	}

	{                            //Duplicate folder name must fail
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for dup folder", false);
			goto doneDupFolder;
		}

		addFolder(t, &ca, root, "lib", false);
		CAHandle hDup = addFolder(t, &ca, root, "lib", true);

		Test_assert(t, "dup folder fails",       hDup == CAHandle_Invalid);
		Test_assert(t, "still 2 folders",        ca.folders.length == 2);

	doneDupFolder:
		CAFile_free(&ca, t->alloc);
	}

	{                            //Name with path separator must fail for folders too
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for slash folder name", false);
			goto doneSlashFolder;
		}

		CAHandle hSlash = addFolder(t, &ca, root, "a/b", true);
		Test_assert(t, "slash in folder name fails", hSlash == CAHandle_Invalid);

	doneSlashFolder:
		CAFile_free(&ca, t->alloc);
	}
}

void Test_CACounts(Test *t) {

	Test_setModule(t, "CAFile_counts");

	//Build a small tree and verify recursive vs non-recursive counts

	{
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for counts", false);
			goto doneCounts;
		}

		CAHandle hA = addFolder(t, &ca, root, "A",  false);
		CAHandle hB = addFolder(t, &ca, root, "B",  false);
		CAHandle hAA = addFolder(t, &ca, hA,  "AA", false);

		addFile(t, &ca, root, "root.txt", 0,false);
		addFile(t, &ca, hA,   "a.txt",    0,false);
		addFile(t, &ca, hA,   "a2.txt",   0,false);
		addFile(t, &ca, hAA,  "aa.txt",   0,false);
		addFile(t, &ca, hB,   "b.txt",    0,false);

		//Non-recursive

		Test_assert(t, "root dirCount nr 2",     CAFile_dirCount(&ca, root, false) == 2);
		Test_assert(t, "root fileCount nr 1",    CAFile_fileCount(&ca, root, false) == 1);
		Test_assert(t, "A dirCount nr 1",        CAFile_dirCount(&ca, hA, false) == 1);
		Test_assert(t, "A fileCount nr 2",       CAFile_fileCount(&ca, hA, false) == 2);
		Test_assert(t, "AA fileCount nr 1",      CAFile_fileCount(&ca, hAA, false) == 1);
		Test_assert(t, "B fileCount nr 1",       CAFile_fileCount(&ca, hB, false) == 1);

		//Recursive

		Test_assert(t, "root dirCount r 3",      CAFile_dirCount(&ca, root, true) == 3);
		Test_assert(t, "root fileCount r 5",     CAFile_fileCount(&ca, root, true) == 5);
		Test_assert(t, "A fileCount r 3",        CAFile_fileCount(&ca, hA, true) == 3);

		//fileObjectCount

		Test_assert(t, "root objCount nr 3",     CAFile_fileObjectCount(&ca, root, false) == 3);
		Test_assert(t, "root objCount r 8",      CAFile_fileObjectCount(&ca, root, true) == 8);

	doneCounts:
		CAFile_free(&ca, t->alloc);
	}
}

extern const CASettings kCASettingsDate;

void Test_CAGetInfo(Test *t) {

	Test_setModule(t, "CAFile_getInfo");

	{
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettingsDate, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for getInfo", false);
			goto doneGetInfo;
		}

		Ns ts = (Ns)5000 * MS;
		CAHandle hDir  = addFolder(t, &ca, root, "subdir", false);
		CAHandle hFile = addFile(t,   &ca, hDir, "info.txt", ts, false);

		U8 bytes[3] = { 1, 2, 3 };
		Buffer buf   = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(bytes, 3), t->alloc, &buf, NULL);
		CAFile_setData(&ca, hFile, t->alloc, &buf, NULL);

		//File info

		FileInfo fi = { 0 };
		Test_assert(t, "getInfo file ok",        CAFile_getInfo(&ca, hFile, &fi, t->alloc, &t->err));
		Test_assert(t, "type is file",           fi.type == EFileType_File);
		Test_assert(t, "fileSize 3",             fi.fileSize == 3);
		Test_assert(t, "timestamp matches",      fi.timestamp == ts);
		Test_assert(t, "path == subdir/info.txt", CharString_equalsCStringSensitive(&fi.path, "subdir/info.txt"));
		FileInfo_free(&fi, t->alloc);

		//Folder info

		FileInfo di = { 0 };
		Test_assert(t, "getInfo folder ok",      CAFile_getInfo(&ca, hDir, &di, t->alloc, &t->err));
		Test_assert(t, "type is folder",         di.type == EFileType_Folder);
		Test_assert(t, "folder path == subdir",  CharString_equalsCStringSensitive(&di.path, "subdir"));
		FileInfo_free(&di, t->alloc);

		//Root info

		FileInfo ri = { 0 };
		Test_assert(t, "getInfo root ok",        CAFile_getInfo(&ca, root, &ri, t->alloc, &t->err));
		Test_assert(t, "root type is folder",    ri.type == EFileType_Folder);
		Test_assert(t, "root path is empty",     CharString_isEmpty(ri.path));
		FileInfo_free(&ri, t->alloc);

		//Invalid handle

		FileInfo inv = { 0 };
		Test_assert(t, "invalid handle fails",   !CAFile_getInfo(&ca, CAHandle_Invalid, &inv, t->alloc, NULL));

	doneGetInfo:
		CAFile_free(&ca, t->alloc);
	}
}

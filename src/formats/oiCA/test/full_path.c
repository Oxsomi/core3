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
#include "formats/oiCA/ca_lookup.h"

extern const CASettings kCASettings;

CAHandle addFile(Test *t, CAFile *ca, CAHandle parent, const C8 *name, Ns time, Bool failIsSuccess);
CAHandle addFolder(Test *t, CAFile *ca, CAHandle parent, const C8 *name, Bool failIsSuccess);
void assertResolve(Test *t, const CAFile *ca, const C8 *path, CAHandle expected, const C8 *label);

static inline CAHandle CAFile_resolveCStr(CAFile *ca, const C8 *name) {
	return CAFile_resolve(ca, CharString_createRefCStrConst(name));
}

void Test_CAResolve(Test *t) {

	Test_setModule(t, "CAFile_resolve");

	//src/
	//  main.c
	//include/
	//  util/
	//    helper.h
	//readme.txt

	{
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for resolve", false);
			goto doneResolve;
		}

		CAHandle hSrc     = addFolder(t, &ca, root,  "src", false);
		CAHandle hInc     = addFolder(t, &ca, root,  "include", false);
		CAHandle hUtil    = addFolder(t, &ca, hInc,  "util", false);
		CAHandle hMain    = addFile(t,   &ca, hSrc,  "main.c",   0, false);
		CAHandle hHelper  = addFile(t,   &ca, hUtil, "helper.h", 0, false);
		CAHandle hReadme  = addFile(t,   &ca, root,  "readme.txt", 0, false);

		//resolveFile

		assertResolve(t, &ca, "src/main.c",              hMain,   "resolve src/main.c");
		assertResolve(t, &ca, "include/util/helper.h",   hHelper, "resolve include/util/helper.h");
		assertResolve(t, &ca, "readme.txt",              hReadme, "resolve readme.txt");

		//resolveFolder

		assertResolve(t, &ca, "src",             hSrc,  "resolve folder src");
		assertResolve(t, &ca, "include",         hInc,  "resolve folder include");
		assertResolve(t, &ca, "include/util",    hUtil, "resolve folder include/util");

		//Non-existent paths return CAHandle_Invalid

		CharString miss = CharString_createRefCStrConst("src/missing.c");
		Test_assert(t, "missing file invalid",   CAFile_resolve(&ca, miss) == CAHandle_Invalid);

		CharString missDir = CharString_createRefCStrConst("nope");
		Test_assert(t, "missing dir invalid",    CAFile_resolve(&ca, missDir) == CAHandle_Invalid);

		//Resolution is case-sensitive, wrong case must NOT find anything

		CharString srcUp = CharString_createRefCStrConst("SRC/MAIN.C");
		Test_assert(t, "wrong-case path fails",  CAFile_resolve(&ca, srcUp) == CAHandle_Invalid);

		CharString srcFolderUp = CharString_createRefCStrConst("SRC");
		Test_assert(t, "wrong-case folder fails", CAFile_resolve(&ca, srcFolderUp) == CAHandle_Invalid);

		CharString mixedUtil = CharString_createRefCStrConst("INCLUDE/UTIL");
		Test_assert(t, "wrong-case multi-segment fails", CAFile_resolve(&ca, mixedUtil) == CAHandle_Invalid);

	doneResolve:
		CAFile_free(&ca, t->alloc);
	}
}

void Test_CAGetFullName(Test *t) {

	Test_setModule(t, "CAFile_getFullName");

	{
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for getFullName", false);
			goto doneFullName;
		}

		CAHandle hSrc  = addFolder(t, &ca, root, "src", false);
		CAHandle hFile = addFile(t,   &ca, hSrc, "main.c", 0, false);

		//Root must return empty string

		CharString rootName = CharString_createNull();
		Test_assert(t, "getFullName root ok",    CAFile_getFullName(&ca, root, t->alloc, &rootName, &t->err));
		Test_assert(t, "root name empty",        CharString_isEmpty(rootName));
		CharString_free(&rootName, t->alloc);

		//Folder path

		CharString srcPath = CharString_createNull();
		Test_assert(t, "getFullName src ok",     CAFile_getFullName(&ca, hSrc, t->alloc, &srcPath, &t->err));
		Test_assert(t, "src path == src",        CharString_equalsCStringSensitive(&srcPath, "src"));
		CharString_free(&srcPath, t->alloc);

		//File path

		CharString filePath = CharString_createNull();
		Test_assert(t, "getFullName file ok",    CAFile_getFullName(&ca, hFile, t->alloc, &filePath, &t->err));
		Test_assert(t, "file path == src/main.c", CharString_equalsCStringSensitive(&filePath, "src/main.c"));
		CharString_free(&filePath, t->alloc);

		//Non-empty result must fail (memleak guard)

		CharString dirty = CharString_createRefCStrConst("dirty");
		Test_assert(t, "dirty result fails",     !CAFile_getFullName(&ca, hFile, t->alloc, &dirty, NULL));

		//Invalid handle must fail

		CharString inv = CharString_createNull();
		Test_assert(t, "invalid handle fails",   !CAFile_getFullName(&ca, CAHandle_Invalid, t->alloc, &inv, NULL));
		CharString_free(&inv, t->alloc);

	doneFullName:
		CAFile_free(&ca, t->alloc);
	}
}

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

//formats/oiCA/test/test_oiCA_foreach.c

#include "test_oiCA_shared.h"
#include "types/base/string_read_helper.h"
#include "formats/oiCA/ca_lookup.h"

extern const CASettings kCASettings;

CAHandle addFile(Test *t, CAFile *ca, CAHandle parent, const C8 *name, Ns time, Bool failIsSuccess);
CAHandle addFolder(Test *t, CAFile *ca, CAHandle parent, const C8 *name, Bool failIsSuccess);

typedef struct ForeachCollector {
	CharString paths[64];
	U64        count;
	U64        stopAfter;    //Stop after this many callbacks (0 = never)
} ForeachCollector;

static Bool foreachCb(const FileInfo *info, void *obj, const Allocator *alloc, Error *e_rr) {

	(void)e_rr;

	ForeachCollector *c = (ForeachCollector *)obj;

	if (c->count < 64 && !CharString_createCopy(info->path, alloc, &c->paths[c->count], e_rr))
		return false;

	++c->count;

	if (c->stopAfter && c->count >= c->stopAfter)
		return false;

	return true;
}

static Bool pathInCollector(const ForeachCollector *c, const C8 *path) {

	CharString p = CharString_createRefCStrConst(path);

	for (U64 i = 0; i < c->count && i < 64; ++i)
		if (CharString_equalsStringSensitive(&c->paths[i], &p))
			return true;

	return false;
}

void Test_CAForeach(Test *t) {

	Test_setModule(t, "CAFile_foreach");

	{
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		ForeachCollector r = { 0 };

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for foreach", false);
			goto doneForeach;
		}

		//a/
		//  b/
		//    deep.txt
		//  a_file.txt
		//root_file.txt

		CAHandle hA    = addFolder(t, &ca, root, "a", false);
		CAHandle hB    = addFolder(t, &ca, hA,   "b", false);
		addFile(t, &ca, hB,   "deep.txt",      0, false);
		addFile(t, &ca, hA,   "a_file.txt",    0, false);
		addFile(t, &ca, root, "root_file.txt", 0, false);

		//Non-recursive from root: only immediate children (a/ and root_file.txt)

		ForeachCollector empty = { 0 };

		Test_assert(t, "foreach non-recurse ok", CAFile_foreach(&ca, root, foreachCb, &r, false, t->alloc, &t->err));
		Test_assert(t, "nr count == 2",    r.count == 2);
		Test_assert(t, "nr has a/",        pathInCollector(&r, "a"));
		Test_assert(t, "nr has root_file", pathInCollector(&r, "root_file.txt"));
		Test_assert(t, "nr no deep.txt",   !pathInCollector(&r, "a/b/deep.txt"));

		//Recursive from root: all 5 entries (2 dirs + 3 files)

		for (U64 i = 0; i < r.count; ++i)
			CharString_free(&r.paths[i], t->alloc);

		r = empty;

		Test_assert(t, "foreach recurse ok", CAFile_foreach(&ca, root, foreachCb, &r, true, t->alloc, &t->err));
		Test_assert(t, "r count == 5",       r.count == 5);
		Test_assert(t, "r has a/",           pathInCollector(&r, "a"));
		Test_assert(t, "r has a/b",          pathInCollector(&r, "a/b"));
		Test_assert(t, "r has a/b/deep.txt", pathInCollector(&r, "a/b/deep.txt"));
		Test_assert(t, "r has a/a_file.txt", pathInCollector(&r, "a/a_file.txt"));
		Test_assert(t, "r has root_file",    pathInCollector(&r, "root_file.txt"));

		//Early abort: stop after 1 item

		for (U64 i = 0; i < r.count; ++i)
			CharString_free(&r.paths[i], t->alloc);

		r = empty;
		r.stopAfter = 1;

		CAFile_foreach(&ca, root, foreachCb, &r, true, t->alloc, NULL);
		Test_assert(t, "early abort count 1", r.count == 1);

		//Recursive from a subfolder

		for (U64 i = 0; i < r.count; ++i)
			CharString_free(&r.paths[i], t->alloc);

		r = empty;

		Test_assert(t, "foreach from hA ok", CAFile_foreach(&ca, hA, foreachCb, &r, true, t->alloc, &t->err));
		Test_assert(t, "sub count == 3", r.count == 3);
		Test_assert(t, "sub has a/b",          pathInCollector(&r, "a/b"));
		Test_assert(t, "sub has a/b/deep.txt", pathInCollector(&r, "a/b/deep.txt"));
		Test_assert(t, "sub has a/a_file.txt", pathInCollector(&r, "a/a_file.txt"));
		Test_assert(t, "sub no root_file",     !pathInCollector(&r, "root_file.txt"));

		//Degenerate case: single-file tree with no subdirs

		CAFile ca2 = { 0 };
		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca2, &t->err)) {
			Test_assert(t, "Create ca2 for degenerate foreach", false);
			goto doneForeach;
		}

		addFile(t, &ca2, CAHandle_Root, "lone.txt", 0, false);

		for (U64 i = 0; i < r.count; ++i)
			CharString_free(&r.paths[i], t->alloc);

		r = empty;

		Test_assert(t, "lone foreach ok",       CAFile_foreach(&ca2, CAHandle_Root, foreachCb, &r, true, t->alloc, &t->err));
		Test_assert(t, "lone count == 1",       r.count == 1);
		Test_assert(t, "lone has lone.txt",     pathInCollector(&r, "lone.txt"));
		CAFile_free(&ca2, t->alloc);

		//Null guards

		Test_assert(t, "foreach null caFile",
			!CAFile_foreach(NULL, root, foreachCb, NULL, false, t->alloc, NULL));
		Test_assert(t, "foreach null cb",
			!CAFile_foreach(&ca, root, NULL, NULL, false, t->alloc, NULL));

	doneForeach:

		for (U64 i = 0; i < r.count; ++i)
			CharString_free(&r.paths[i], t->alloc);

		CAFile_free(&ca, t->alloc);
	}
}

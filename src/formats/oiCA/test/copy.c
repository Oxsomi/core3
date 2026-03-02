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
#include "formats/oiCA/ca_file.h"
#include "formats/oiCA/ca_lookup.h"

extern const CASettings kCASettings;

CAHandle addFile(Test *t, CAFile *ca, CAHandle parent, const C8 *name, Ns time, Bool failIsSuccess);
CAHandle addFolder(Test *t, CAFile *ca, CAHandle parent, const C8 *name, Bool failIsSuccess);

void Test_CACreateCopy(Test *t) {

	Test_setModule(t, "CAFile_createCopy");

	{
		CAFile src  = { 0 };
		CAFile copy = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &src, &t->err)) {
			Test_assert(t, "Create src for createCopy", false);
			goto doneCACopy;
		}

		CAHandle hDir  = addFolder(t, &src, root, "docs", false);
		addFile(t,   &src, hDir, "readme.md", 0, false);

		Test_assert(t, "createCopy ok",          CAFile_createCopy(&src, t->alloc, &copy, &t->err));

		//Structural equality

		Test_assert(t, "copy folders.length",    copy.folders.length == src.folders.length);
		Test_assert(t, "copy files.length",      copy.files.length   == src.files.length);

		//Resolve works in copy

		CharString path = CharString_createRefCStrConst("docs/readme.md");
		Test_assert(t, "resolve in copy ok",     CAFile_resolve(&copy, path) != CAHandle_Invalid);

		//Mutating copy must not affect src

		CAHandle hNew = addFile(t, &copy, root, "extra.txt", 0, false);
		Test_assert(t, "add to copy ok",         hNew != CAHandle_Invalid);
		Test_assert(t, "src files unchanged",    src.files.length == 1);

		//Null guards

		Test_assert(t, "null caFile fails",      !CAFile_createCopy(NULL, t->alloc, &copy, NULL));
		Test_assert(t, "null result fails",      !CAFile_createCopy(&src, t->alloc, NULL,  NULL));

	doneCACopy:
		CAFile_free(&src,  t->alloc);
		CAFile_free(&copy, t->alloc);
	}
}

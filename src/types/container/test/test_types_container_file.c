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

//types/container/test/test_types_container_file.c

//File_resolve and File_makeRelative.
//The path sandbox lives here: resolve is what stops a path from escaping the app/working directory, so
// the rejection cases matter as much as the happy ones.
//CharString_isValidFileName / isValidFilePath are covered separately, in test_types_base_string_read.c.

#include "test_types_container_shared.h"
#include "types/container/file_base.h"
#include "types/container/string.h"
#include "types/base/string_read.h"
#include "types/base/string_read_helper.h"
#include "types/base/error.h"
#include "types/base/platform_types.h"

//Absolute directory everything is resolved against.
//Windows wants a drive, unix wants a leading slash.

#if _PLATFORM_TYPE == PLATFORM_WINDOWS
	#define BASE "C:/base"
#else
	#define BASE "/base"
#endif

static CharString Test_ref(const C8 *str) { return CharString_createRefCStrConst(str); }

//Resolve `loc` and check it lands on `expected`, with the expected virtual-ness

static void Test_resolves(Test *t, const C8 *loc, const C8 *expected, Bool expectVirtual) {

	CharString locStr = Test_ref(loc);
	CharString base = Test_ref(BASE);
	CharString expectStr = Test_ref(expected);
	CharString res = CharString_createNull();

	Bool isVirtual = false;
	Error err = Error_none();

	const Bool ok = File_resolve(&locStr, &isVirtual, 0, &base, t->alloc, &res, &err);

	Test_assert(t, loc[0] ? loc : "(empty)",
		ok && isVirtual == expectVirtual && CharString_equalsStringSensitive(&res, &expectStr)
	);

	CharString_free(&res, t->alloc);
}

//Resolve `loc` and check it's refused.
//Errors are the security boundary, so they get their own helper.

static void Test_resolveFails(Test *t, const C8 *loc, const C8 *why) {

	CharString locStr = Test_ref(loc);
	CharString base = Test_ref(BASE);
	CharString res = CharString_createNull();

	Bool isVirtual = false;
	Error err = Error_none();

	const Bool ok = File_resolve(&locStr, &isVirtual, 0, &base, t->alloc, &res, &err);

	Test_assert(t, why, !ok && !res.ptr);

	CharString_free(&res, t->alloc);
}

void Test_filePaths(Test *t) {

	Test_setModule(t, "File_resolve");

	//Relative paths get the absolute directory prepended

	Test_resolves(t, "a",            BASE "/a",     false);
	Test_resolves(t, "a/b",          BASE "/a/b",   false);
	Test_resolves(t, "a/b/c.txt",    BASE "/a/b/c.txt", false);

	//"." is a no-op, ".." pops the previous segment, empty segments collapse

	Test_resolves(t, "a/./b",        BASE "/a/b",   false);
	Test_resolves(t, "./a",          BASE "/a",     false);
	Test_resolves(t, "a/b/../c",     BASE "/a/c",   false);
	Test_resolves(t, "a/b/../../c",  BASE "/c",     false);
	Test_resolves(t, "a/b/..",       BASE "/a",     false);

	//A trailing slash is trimmed rather than leaving an empty last segment

	Test_resolves(t, "a/",           BASE "/a",     false);
	Test_resolves(t, "a/b/",         BASE "/a/b",   false);

	//Backslashes are normalised, so windows-style input works everywhere

	Test_resolves(t, "a\\b",         BASE "/a/b",   false);
	Test_resolves(t, "a\\b\\c",      BASE "/a/b/c", false);

	//Collapsing to nothing means "the directory itself"

	Test_resolves(t, ".",            BASE,          false);
	Test_resolves(t, "./",           BASE,          false);

	//An absolute path is allowed as long as it stays inside the base

	Test_resolves(t, BASE,           BASE,          false);
	Test_resolves(t, BASE "/a",      BASE "/a",     false);
	Test_resolves(t, BASE "/a/../b", BASE "/b",     false);

	// -- Rejections ---------------------------------------------------------------------------------

	Test_setModule(t, "File_resolve(reject)");

	//Escaping the base is the whole point of the sandbox

	Test_resolveFails(t, "../outside",      "..-escapes");
	Test_resolveFails(t, "..",              "bare ..");
	Test_resolveFails(t, "a/../../outside", "..-escapes via subfolder");
	Test_resolveFails(t, "a/../..",         "..-escapes exactly");

	//Empty is not a path

	Test_resolveFails(t, "",                "(empty)");

	//An empty segment is not a valid path component, so a doubled slash is rejected rather than collapsed

	Test_resolveFails(t, "a//b",            "empty segment");

	//Illegal characters and reserved MS-DOS device names, which are invalid on every platform here

	Test_resolveFails(t, "a<b",             "illegal <");
	Test_resolveFails(t, "a>b",             "illegal >");
	Test_resolveFails(t, "a|b",             "illegal |");
	Test_resolveFails(t, "a\"b",            "illegal quote");
	Test_resolveFails(t, "CON",             "device CON");
	Test_resolveFails(t, "a/NUL",           "device NUL in subfolder");
	Test_resolveFails(t, "a/PRN.txt",       "device PRN with extension");
	Test_resolveFails(t, "COM1",            "device COM1");
	Test_resolveFails(t, "LPT9.txt",        "device LPT9 with extension");
	Test_resolveFails(t, "a ",              "trailing space");
	Test_resolveFails(t, " a",              "leading space");

	//PRNtscreen.pdf is fine, only the exact device name (or device + extension) is reserved

	Test_resolves(t, "PRNtscreen.pdf", BASE "/PRNtscreen.pdf", false);
	Test_resolves(t, "NULlpointer.txt", BASE "/NULlpointer.txt", false);

	//UNC paths would let a file read reach a network host, so they're refused outright

	Test_resolveFails(t, "\\\\server\\share", "UNC path");

	//Platform specific spellings

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS

		Test_resolveFails(t, "C:folder",    "drive-relative path");
		Test_resolveFails(t, "/absolute",   "unix absolute on windows");
		Test_resolveFails(t, "D:/other",    "absolute outside base");
		Test_resolveFails(t, "C:/other",    "same drive outside base");
		Test_resolveFails(t, "0:/folder",   "non-alpha drive");

	#else

		Test_resolveFails(t, "C:/folder",   "windows path on unix");
		Test_resolveFails(t, "C:folder",    "drive-relative on unix");
		Test_resolveFails(t, "/outside",    "absolute outside base");

	#endif

	//A resolved path longer than MAX_OXC_PATH can't be stack allocated by callers, so it's refused

	{
		C8 longPath[MAX_OXC_PATH + 64];
		const U64 count = sizeof(longPath) - 1;

		for(U64 i = 0; i < count; ++i)
			longPath[i] = (i % 8) == 7 ? '/' : 'a';

		longPath[count] = '\0';
		Test_resolveFails(t, longPath, "over MAX_OXC_PATH");
	}

	// -- Virtual paths ------------------------------------------------------------------------------

	Test_setModule(t, "File_resolve(virtual)");

	//The bare virtual root has no section component and resolves to the empty string

	Test_resolves(t, "//",             "",              true);
	Test_resolves(t, "//.",            "",              true);
	Test_resolves(t, "//./",           "",              true);

	//Otherwise the leading "//" is stripped and the rest resolves as usual, without the base prepended

	Test_resolves(t, "//section",      "section",       true);
	Test_resolves(t, "//section/file", "section/file",  true);
	Test_resolves(t, "//section/./f",  "section/f",     true);
	Test_resolves(t, "//a/b/../c",     "a/c",           true);
	Test_resolves(t, "//section/",     "section",       true);

	//A virtual path can't climb out of the virtual root either

	Test_resolveFails(t, "//..",       "virtual ..-escape");
	Test_resolveFails(t, "//a/../..",  "virtual ..-escape via subfolder");

	// -- File_makeRelative --------------------------------------------------------------------------

	Test_setModule(t, "File_makeRelative");

	{
		const struct {
			const C8 *base, *sub, *expected;
		} cases[] = {

			//Same folder: just the file name

			{ "a/b.txt",     "a/c.txt",     "c.txt"          },
			{ "b.txt",       "c.txt",       "c.txt"          },

			//Sideways: back out of the base's folder, then down

			{ "a/b.txt",     "d/c.txt",     "../d/c.txt"     },
			{ "a/b/c.txt",   "x.txt",       "../../x.txt"    },
			{ "a/b/c.txt",   "a/d.txt",     "../d.txt"       },

			//Downwards into a subfolder of the base's folder

			{ "a/b.txt",     "a/sub/c.txt", "sub/c.txt"      },

			//Virtual on both sides behaves the same

			{ "//s/a/b.txt", "//s/a/c.txt", "c.txt"          },
			{ "//s/a/b.txt", "//s/d/c.txt", "../d/c.txt"     }
		};

		for(U64 i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {

			CharString result = CharString_createNull();
			Error err = Error_none();

			const Bool ok = File_makeRelative(
				Test_ref(BASE), Test_ref(cases[i].base), Test_ref(cases[i].sub),
				0, t->alloc, &result, &err
			);

			CharString expected = Test_ref(cases[i].expected);
			Test_assert(t, cases[i].expected, ok && CharString_equalsStringSensitive(&result, &expected));

			CharString_free(&result, t->alloc);
		}
	}

	//Mixing a virtual path with a real one has no meaningful answer

	{
		CharString result = CharString_createNull();

		const Bool ok = File_makeRelative(
			Test_ref(BASE), Test_ref("//s/a.txt"), Test_ref("b.txt"), 0, t->alloc, &result, NULL
		);

		Test_assert(t, "virtual/real mismatch", !ok);
		CharString_free(&result, t->alloc);
	}

	//A non-empty result is a caller bug (it would leak), so it's rejected rather than overwritten

	{
		CharString result = CharString_createNull();
		Error err = Error_none();

		Test_assert(t, "result must start empty", File_makeRelative(
			Test_ref(BASE), Test_ref("a/b.txt"), Test_ref("a/c.txt"), 0, t->alloc, &result, &err
		));

		const Bool reused = File_makeRelative(
			Test_ref(BASE), Test_ref("a/b.txt"), Test_ref("a/c.txt"), 0, t->alloc, &result, NULL
		);

		Test_assert(t, "non-empty result rejected", !reused);
		CharString_free(&result, t->alloc);
	}

	Test_setModule(t, NULL);
}

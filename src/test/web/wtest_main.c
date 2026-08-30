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

//test/web/wtest_main.c
//Runs every unit test suite in one wasm64 module under node.
//Same shape as test/android/atest_main.c, but node gives a real exec: stdout just works and the process exit code
// reports pass/fail, so there is no logcat pump and no sentinel-grepping runner.
//Each suite is a named entry rather than a main(), see OXC3_TEST_MAIN/OXC3_TEST_ENTRY in types/test/test.h.

#include "types/base/types.h"
#include "types/base/string_base.h"
#include "types/base/string_read_helper.h"

#include <stdio.h>
#include <unistd.h>

typedef int (*WTestFn)(void *state);

#define WTEST_SUITE(name) int OxC3_test_##name(void *state)

//Suites bundled into this module. Differences from android:
// - shader_compiler IS present: the whole point of the wasm64 port is DXC running in wasm.
// - graphics_interface is absent: no GPU under node (the Vulkan loader can't be dlopened).
// - the functional suites are absent: they need a human at a window/speaker; web is headless here.

WTEST_SUITE(types_base);
WTEST_SUITE(types_math);
WTEST_SUITE(types_container);
WTEST_SUITE(formats_bmp);
WTEST_SUITE(formats_dds);
WTEST_SUITE(formats_oiBC);
WTEST_SUITE(formats_oiCA);
WTEST_SUITE(formats_oiDL);
WTEST_SUITE(formats_oiSB);
WTEST_SUITE(formats_oiSH);
WTEST_SUITE(formats_wav);
WTEST_SUITE(audio_interface);
WTEST_SUITE(platforms_interface);
WTEST_SUITE(shader_compiler);

//Suites open their files relative to the working directory (OxC3 restricts file access to it).
//Both of the suites that read data want the same place: the build output holding packages/.
//platforms_interface reads testdata out of it, and shader_compiler is bundled (_OXC3_TEST_BUNDLED),
// so TEST_SHADER_ROOT is the virtual path //<target>/shaderdata rather than a real directory,
// and webplatform.c only finds sections by scanning packages/ relative to cwd.
//Each suite creates its own Platform (see types/test/test.h), so a chdir between suites is picked up cleanly.

typedef enum EWTestDir {
	EWTestDir_Default, //Wherever the runner started
	EWTestDir_Packages //-packages <dir>
} EWTestDir;

typedef struct WTestSuite {
	const C8 *name;
	WTestFn fn;
	EWTestDir dir;
} WTestSuite;

static const WTestSuite WTest_suites[] = {

	{ "types_base",          OxC3_test_types_base,          EWTestDir_Default      },
	{ "types_math",          OxC3_test_types_math,          EWTestDir_Default      },
	{ "types_container",     OxC3_test_types_container,     EWTestDir_Default      },

	{ "formats_bmp",         OxC3_test_formats_bmp,         EWTestDir_Default      },
	{ "formats_dds",         OxC3_test_formats_dds,         EWTestDir_Default      },
	{ "formats_oiBC",        OxC3_test_formats_oiBC,        EWTestDir_Default      },
	{ "formats_oiCA",        OxC3_test_formats_oiCA,        EWTestDir_Default      },
	{ "formats_oiDL",        OxC3_test_formats_oiDL,        EWTestDir_Default      },
	{ "formats_oiSB",        OxC3_test_formats_oiSB,        EWTestDir_Default      },
	{ "formats_oiSH",        OxC3_test_formats_oiSH,        EWTestDir_Default      },
	{ "formats_wav",         OxC3_test_formats_wav,         EWTestDir_Default      },

	{ "audio_interface",     OxC3_test_audio_interface,     EWTestDir_Default      },
	{ "platforms_interface", OxC3_test_platforms_interface, EWTestDir_Packages     },

	//Last: it's the slowest (a full DXC compile corpus), so every cheap failure surfaces first

	{ "shader_compiler",     OxC3_test_shader_compiler,     EWTestDir_Packages     }
};

int main(int argc, const char *argv[]) {

	//`node OxC3_wtest.js [suite] [-packages <dir>]`
	//No suite argument runs them all. -packages gives suites their expected working directory.

	const C8 *only = NULL;
	const C8 *dirs[2] = { NULL, NULL };

	for(int i = 1; i < argc; ++i) {

		const CharString arg = CharString_createRefCStrConst(argv[i]);

		if(CharString_equalsCStringSensitive(&arg, "-packages") && i + 1 < argc)
			dirs[EWTestDir_Packages] = argv[++i];

		else only = argv[i];
	}

	const U64 count = sizeof(WTest_suites) / sizeof(WTest_suites[0]);

	U64 ran = 0, failed = 0, skipped = 0;

	printf("OXC3_TEST_BEGIN suites=%llu\n", (unsigned long long) count);

	for(U64 i = 0; i < count; ++i) {

		const WTestSuite *suite = &WTest_suites[i];

		if(only) {

			const CharString onlyStr = CharString_createRefCStrConst(only);

			if(!CharString_equalsCStringSensitive(&onlyStr, suite->name)) {
				++skipped;
				continue;
			}
		}

		if(dirs[suite->dir] && chdir(dirs[suite->dir])) {
			++failed; ++ran;
			printf("FAIL %s (couldn't chdir to %s)\n", suite->name, dirs[suite->dir]);
			continue;
		}

		printf("RUN  %s\n", suite->name);
		fflush(stdout);

		const int res = suite->fn(NULL);
		++ran;

		if(res) {
			++failed;
			printf("FAIL %s (%i)\n", suite->name, res);
		}

		else printf("PASS %s\n", suite->name);

		fflush(stdout);
	}

	printf(
		"OXC3_TEST_END ran=%llu failed=%llu skipped=%llu result=%s\n",
		(unsigned long long) ran, (unsigned long long) failed, (unsigned long long) skipped,
		failed ? "FAILED" : "PASSED"
	);

	//A run that matched no suite is a failure, not a pass: an unknown -suite name (or a bad filter)
	// would otherwise exit 0 having tested nothing, which is exactly how a green CI hides a mistake.

	if(!ran) {
		printf("OXC3_TEST_END no suites ran (bad suite name?) result=FAILED\n");
		return 1;
	}

	return failed ? 1 : 0;
}

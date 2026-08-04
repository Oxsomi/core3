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

//test/android/atest_main.c
//Runs every unit test suite in one APK, since android has no exec to run them separately with.
//Each suite is a named entry rather than a main(), see OXC3_TEST_MAIN/OXC3_TEST_ENTRY in types/test/test.h.

#include "platforms/platform.h"
#include "types/base/types.h"

#include <android/log.h>
#include <android_native_app_glue.h>
#include <sys/system_properties.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define ATEST_TAG "OxC3"

//Suites bundled into this .so. Interactive ones need a human at the device, so they're off unless asked
//for; everything else runs unattended. shader_compiler is absent on purpose, DXC isn't built for android.

typedef int (*ATestFn)(void *state);

#define ATEST_SUITE(name) int OxC3_test_##name(void *state)

ATEST_SUITE(types_base);
ATEST_SUITE(types_math);
ATEST_SUITE(types_container);
ATEST_SUITE(formats_bmp);
ATEST_SUITE(formats_dds);
ATEST_SUITE(formats_oiBC);
ATEST_SUITE(formats_oiCA);
ATEST_SUITE(formats_oiDL);
ATEST_SUITE(formats_oiSB);
ATEST_SUITE(formats_oiSH);
ATEST_SUITE(formats_wav);
ATEST_SUITE(audio_interface);
ATEST_SUITE(platforms_interface);
ATEST_SUITE(graphics_interface);
ATEST_SUITE(audio_functional);
ATEST_SUITE(platforms_functional);

typedef struct ATestSuite {
	const C8 *name;
	ATestFn fn;
	Bool interactive;
} ATestSuite;

static const ATestSuite ATest_suites[] = {

	{ "types_base",           OxC3_test_types_base,           false },
	{ "types_math",           OxC3_test_types_math,           false },
	{ "types_container",      OxC3_test_types_container,      false },

	{ "formats_bmp",          OxC3_test_formats_bmp,          false },
	{ "formats_dds",          OxC3_test_formats_dds,          false },
	{ "formats_oiBC",         OxC3_test_formats_oiBC,         false },
	{ "formats_oiCA",         OxC3_test_formats_oiCA,         false },
	{ "formats_oiDL",         OxC3_test_formats_oiDL,         false },
	{ "formats_oiSB",         OxC3_test_formats_oiSB,         false },
	{ "formats_oiSH",         OxC3_test_formats_oiSH,         false },
	{ "formats_wav",          OxC3_test_formats_wav,          false },

	{ "audio_interface",      OxC3_test_audio_interface,      false },
	{ "platforms_interface",  OxC3_test_platforms_interface,  false },
	{ "graphics_interface",   OxC3_test_graphics_interface,   false },

	{ "audio_functional",     OxC3_test_audio_functional,     true  },
	{ "platforms_functional", OxC3_test_platforms_functional, true  }

};

//Test_print and friends use printf (they sit below OxC3_types_container, so they have no access to Log).
//stdout goes nowhere on android, so pump it into logcat; that makes every existing test print visible
//through `adb logcat -s OxC3` without touching the suites themselves.

static void *ATest_logPump(void *arg) {

	const int readFd = (int)(U64) arg;
	C8 line[512];
	U64 len = 0;

	for(;;) {

		C8 c;
		const ssize_t got = read(readFd, &c, 1);

		if(got <= 0)
			break;

		if(c == '\n' || len + 1 == sizeof(line)) {
			line[len] = '\0';
			__android_log_write(ANDROID_LOG_INFO, ATEST_TAG, line);
			len = 0;
			continue;
		}

		if(c != '\r')
			line[len++] = c;
	}

	return NULL;
}

static Bool ATest_redirectStdio() {

	int fds[2];

	if(pipe(fds))
		return false;

	//Line buffering so a suite's output shows up as it runs rather than only at exit

	setvbuf(stdout, NULL, _IOLBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	if(dup2(fds[1], STDOUT_FILENO) < 0 || dup2(fds[1], STDERR_FILENO) < 0)
		return false;

	pthread_t thread;

	if(pthread_create(&thread, NULL, ATest_logPump, (void*)(U64) fds[0]))
		return false;

	pthread_detach(thread);
	return true;
}

//`adb shell setprop debug.oxc3.interactive 1` opts into the suites that need someone pressing keys.
//A system property rather than an intent extra so it needs no JNI and no change to OxC3Activity.

static Bool ATest_wantsInteractive() {

	C8 value[PROP_VALUE_MAX] = { 0 };

	if(__system_property_get("debug.oxc3.interactive", value) <= 0)
		return false;

	return value[0] == '1' || value[0] == 't' || value[0] == 'y';
}

Platform_defineEntrypoint() {

	//Without this android_native_app_glue can strip the glue and the activity never attaches

	const Bool interactive = ATest_wantsInteractive();
	const U64 count = sizeof(ATest_suites) / sizeof(ATest_suites[0]);

	U64 ran = 0, failed = 0, skipped = 0;

	__android_log_print(
		ANDROID_LOG_INFO, ATEST_TAG, "OXC3_TEST_BEGIN suites=%llu interactive=%s",
		(unsigned long long) count, interactive ? "yes" : "no"
	);

	for(U64 i = 0; i < count; ++i) {

		const ATestSuite *suite = &ATest_suites[i];

		if(suite->interactive && !interactive) {
			++skipped;
			__android_log_print(ANDROID_LOG_INFO, ATEST_TAG, "SKIP %s (interactive)", suite->name);
			continue;
		}

		__android_log_print(ANDROID_LOG_INFO, ATEST_TAG, "RUN  %s", suite->name);

		const int res = suite->fn(state);
		++ran;

		if(res) {
			++failed;
			__android_log_print(ANDROID_LOG_ERROR, ATEST_TAG, "FAIL %s (%i)", suite->name, res);
		}

		else __android_log_print(ANDROID_LOG_INFO, ATEST_TAG, "PASS %s", suite->name);
	}

	//Sentinel: `am start` gives no exit code back, so the runner greps logcat for this line.

	__android_log_print(
		ANDROID_LOG_INFO, ATEST_TAG, "OXC3_TEST_END ran=%llu failed=%llu skipped=%llu result=%s",
		(unsigned long long) ran, (unsigned long long) failed, (unsigned long long) skipped,
		failed ? "FAILED" : "PASSED"
	);

	//The activity would otherwise sit there; exiting makes `adb shell am start` -> logcat a finite run

	ANativeActivity_finish(((struct android_app*) state)->activity);
}

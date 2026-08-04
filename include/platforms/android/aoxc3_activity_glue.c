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

//platforms/android/aoxc3_activity_glue.c

#include "platforms/platform.h"
#include "platforms/window.h"

#include <android_native_app_glue.h>

void AWindow_queueTypeChar(const U16 *utf16, U64 len);

//Soft keyboard text from OxC3Activity's TextWatcher. This runs on the android UI thread, so it only
//queues; AWindow_flushTypeChar drains it from WindowManager_updateExt on the app thread, where every
//other input callback already runs.

JNIEXPORT void JNICALL
Java_net_osomi_nativeactivity_OxC3Activity_onTypeChar(JNIEnv *env, jobject thiz, jstring input) {

	(void) thiz;

	if(!input)
		return;

	const jsize len = (*env)->GetStringLength(env, input);

	if(!len)
		return;

	//UTF-16 rather than GetStringUTFChars: the latter returns modified UTF-8, which encodes anything
	//outside the BMP (emoji) as CESU-8 surrogate pairs.

	const jchar *utf16 = (*env)->GetStringChars(env, input, NULL);

	if(!utf16)
		return;

	AWindow_queueTypeChar((const U16*) utf16, (U64) len);
	(*env)->ReleaseStringChars(env, input, utf16);
}

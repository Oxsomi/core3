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

//platforms/android/aplatform.c

#include "platforms/platform.h"
#include "platforms/keyboard.h"
#include "platforms/input_device.h"
#include "platforms/logx.h"
#include "types/container/string.h"
#include "types/container/string_unicode.h"
#include "types/base/string_read_helper.h"
#include "types/base/string_mut.h"
#include "types/base/atomic.h"
#include "types/base/error.h"

#include <android/asset_manager.h>
#include <android/keycodes.h>
#include <android_native_app_glue.h>
#include <jni.h>

Bool Platform_initUnixExt(Error *e_rr) {

	Bool s_uccess = true;
	ANativeActivity *activity = ((struct android_app*)Platform_instance->data)->activity;
	AAssetManager *assetManager = activity->assetManager;
	AAssetDir *dir = NULL;
	AAssetDir *subDir = NULL;
	AAsset *asset = NULL;
	const C8 *nameSection = NULL;
	const C8 *nameSubSection = NULL;
	CharString tmpStr = CharString_createNull();
	CharString tmpStr1 = CharString_createNull();
	CharString tmpStr2 = CharString_createNull();

	//We will put app dir = internal data path and working dir = external data path
	//Because unlike CLI we don't have a working directory and our apk is virtual, so only internal is read/write.

	CharString internal = CharString_createRefCStrConst(activity->internalDataPath);
	CharString external = CharString_createRefCStrConst(activity->externalDataPath);

	gotoIfError3(clean, CharString_createCopy(internal, Platform_instance->alloc, &Platform_instance->appDirectory, e_rr));
	gotoIfError3(clean, CharString_createCopy(external, Platform_instance->alloc, &Platform_instance->workDirectory, e_rr));

	if(!CharString_endsWithSensitive(Platform_instance->appDirectory, '/', 0))
		gotoIfError3(clean, CharString_append(&Platform_instance->appDirectory, '/', Platform_instance->alloc, e_rr));

	if(!CharString_endsWithSensitive(Platform_instance->workDirectory, '/', 0))
		gotoIfError3(clean, CharString_append(&Platform_instance->workDirectory, '/', Platform_instance->alloc, e_rr));

	//Foreach file, load archive entry
	//We applied a hack here; the NDK has a longstanding issue where getNextFileName will exclude directories.
	//https://issuetracker.google.com/issues/37002833?pli=1
	//There are ways around it with the jni, but I prefer not to use the JNI whenever possible.
	// For other methods, see marcel303's approach in https://github.com/android/ndk-samples/issues/603.
	//Instead, our packager will create files called "section_{section}" and we can scan those and opendir on the section.
	//Alternatively, we could name files "{section}_{subSection}" and go through the flattened files.
	// That however would break when the name contains an underscore, so this is more robust.
	
	dir = AAssetManager_openDir(assetManager, "packages");

	if(!dir)
		Log_warnLnx("OxC3 packages folder not found (missing virtual files), OK if not built through OxC3 completely");

	else while ((nameSection = AAssetDir_getNextFileName(dir)) != NULL) {

		CharString sectionName = CharString_createRefCStrConst(nameSection);
		CharString section = CharString_createRefCStrConst("section_");

		if(!CharString_startsWithStringSensitive(&sectionName, &section, 0))
			continue;

		//Now we can open the section directly and find the files through there

		sectionName.ptr += sizeof("section");        //sizeof includes null terminator so no need for section_
		sectionName.lenAndNullTerminated -= sizeof("section");

		gotoIfError3(clean, CharString_createCopy(sectionName, Platform_instance->alloc, &tmpStr, e_rr));

		CharString packages = CharString_createRefCStrConst("packages/");
		gotoIfError3(clean, CharString_insertString(&tmpStr, &packages, 0, Platform_instance->alloc, e_rr));

		subDir = AAssetManager_openDir(assetManager, tmpStr.ptr);

		if(!subDir)
			retError(clean, Error_invalidState(0, "Platform_initUnixExt() failed, couldn't opendir"));

		//Go through subsection

		while ((nameSubSection = AAssetDir_getNextFileName(subDir)) != NULL) {

			CharString subSectStr = CharString_createRefCStrConst(nameSubSection);

			gotoIfError3(clean, CharString_createCopy(tmpStr, Platform_instance->alloc, &tmpStr1, e_rr));
			gotoIfError3(clean, CharString_append(&tmpStr1, '/', Platform_instance->alloc, e_rr));
			gotoIfError3(clean, CharString_appendString(&tmpStr1, &subSectStr, Platform_instance->alloc, e_rr));
			
			gotoIfError3(clean, CharString_createCopy(sectionName, Platform_instance->alloc, &tmpStr2, e_rr));
			gotoIfError3(clean, CharString_append(&tmpStr2, '/', Platform_instance->alloc, e_rr));
			gotoIfError3(clean, CharString_appendString(&tmpStr2, &subSectStr, Platform_instance->alloc, e_rr));

			CharString oiCA = CharString_createRefCStrConst(".oiCA");

			if(CharString_endsWithStringInsensitive(&tmpStr2, &oiCA, 0))
				gotoIfError3(clean, CharString_popEndCount(&tmpStr2, sizeof("oiCA"), e_rr));    //sizeof("oiCA") == 5

			asset = AAssetManager_open(assetManager, tmpStr1.ptr, AASSET_MODE_STREAMING);

			if(!asset)
				retError(clean, Error_invalidState(0, "Platform_initUnixExt() failed, couldn't open asset"));

			//afile.c reconstructs "packages/<path>.oiCA" from this, so path stays extension-less and matches
			//how the ELF/MACH section name is stored on the other unices (see lplatform.c / oplatform.c).

			VirtualSection virtualSection = (VirtualSection) { .path = tmpStr2 };
			virtualSection.lenExt = (U64) AAsset_getLength64(asset);

			gotoIfError3(clean, ListVirtualSection_pushBack(
				&Platform_instance->virtualSections, virtualSection, Platform_instance->alloc, e_rr
			));

			tmpStr2 = CharString_createNull();
			AAsset_close(asset);
			asset = NULL;
			CharString_free(&tmpStr1, Platform_instance->alloc);
		}

		CharString_free(&tmpStr, Platform_instance->alloc);
		AAssetDir_close(subDir);
		subDir = NULL;
	}

clean:

	if(!s_uccess) {
		Log_errorLnx("Couldn't initialize app, encountered an issue");
		if(e_rr) Error_print(Platform_instance->alloc, e_rr, ELogLevel_Error, ELogOptions_Default);
	}
	
	if(asset) AAsset_close(asset);
	if(subDir) AAssetDir_close(subDir);
	if(dir) AAssetDir_close(dir);
	CharString_free(&tmpStr, Platform_instance->alloc);
	CharString_free(&tmpStr1, Platform_instance->alloc);
	CharString_free(&tmpStr2, Platform_instance->alloc);
	return s_uccess;
}

void Platform_cleanupUnixExt() { }

//Defined in awindow.c, next to the AKEYCODE -> EKey switch it mirrors

extern const I32 EKey_toAndroidKeyCode[EKey_Count];
extern AtomicI64 AWindow_lastKeyboardDeviceId;

Bool Keyboard_remap(const Keyboard *keyboard, EKey key, const Allocator *alloc, CharString *result, Error *e_rr) {

	Bool s_uccess = true;
	Bool attached = false;
	struct android_app *app = NULL;
	JavaVM *vm = NULL;
	JNIEnv *env = NULL;
	jclass cls = NULL;
	jmethodID methodId = NULL;
	jstring label = NULL;
	const jchar *labelPtr = NULL;
	I32 keyCode = AKEYCODE_UNKNOWN;
	I32 deviceId = -1;        //KeyCharacterMap.VIRTUAL_KEYBOARD, overwritten below

	if(!keyboard)
		retError(clean, Error_nullPointer(0, "Keyboard_remap()::keyboard is required"));

	if((U32) key >= EKey_Count)
		retError(clean, Error_outOfBounds(1, (U64) key, EKey_Count, "Keyboard_remap()::key out of range"));

	if(!result)
		retError(clean, Error_nullPointer(3, "Keyboard_remap()::result is required"));

	if(result->ptr)
		retError(clean, Error_invalidParameter(3, 0, "Keyboard_remap()::result is non empty, indicating possible memleak"));

	//The NDK has no key -> label API (AKeyEvent_* stops at the raw keycode), so the localized label has to
	//come from the framework's KeyCharacterMap through OxC3Activity.getKeyLabel.
	//Note EKey is positional (scan code) on desktop but android hands us already layout-translated keycodes,
	//so the label is right for the layout even though the EKey a physical key produces differs from desktop.

	keyCode = EKey_toAndroidKeyCode[key];

	if(keyCode == AKEYCODE_UNKNOWN)
		retError(clean, Error_notFound(0, 0, "Keyboard_remap() key doesn't exist on android"));

	app = (struct android_app*) Platform_instance->data;
	vm = app->activity->vm;
	env = app->activity->env;

	if((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_6) != JNI_OK) {
		(*vm)->AttachCurrentThread(vm, &env, NULL);
		attached = true;
	}

	cls = (*env)->GetObjectClass(env, app->activity->clazz);

	if(!cls)
		retError(clean, Error_invalidState(0, "Couldn't find OxC3Activity"));

	methodId = (*env)->GetMethodID(env, cls, "getKeyLabel", "(II)Ljava/lang/String;");

	if(!methodId)
		retError(clean, Error_invalidState(0, "Couldn't find OxC3Activity.getKeyLabel"));

	deviceId = (I32) AtomicI64_load(&AWindow_lastKeyboardDeviceId);

	label = (jstring) (*env)->CallObjectMethod(env, app->activity->clazz, methodId, keyCode, deviceId);

	if((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionClear(env);
		retError(clean, Error_invalidState(0, "OxC3Activity.getKeyLabel threw"));
	}

	if(!label || !(*env)->GetStringLength(env, label))
		retError(clean, Error_notFound(0, 0, "Keyboard_remap() couldn't be translated"));

	//UTF-16 rather than GetStringUTFChars: the latter hands back modified UTF-8, which encodes anything
	//outside the BMP as a surrogate pair (CESU-8) that our UTF-8 reader would reject.

	labelPtr = (*env)->GetStringChars(env, label, NULL);

	if(!labelPtr)
		retError(clean, Error_outOfMemory(0, "Keyboard_remap() couldn't read the label"));

	//Owned copy, matching windows/linux; the caller frees it

	gotoIfError3(clean, CharString_createFromUTF16(
		(const U16*) labelPtr, (U64) (*env)->GetStringLength(env, label), alloc, result, e_rr
	));

clean:

	if(labelPtr)
		(*env)->ReleaseStringChars(env, label, labelPtr);

	if(label)
		(*env)->DeleteLocalRef(env, label);

	if(cls)
		(*env)->DeleteLocalRef(env, cls);

	if(attached)
		(*vm)->DetachCurrentThread(vm);

	return s_uccess;
}

Bool Platform_setKeyboardVisible(Bool isVisible) {

	struct android_app *app = (struct android_app*)Platform_instance->data;
	JavaVM *vm = app->activity->vm;
	JNIEnv *env = app->activity->env;

	Bool attached = false;
	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;

	if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) {
		(*vm)->AttachCurrentThread(vm, &env, NULL);
		attached = true;
	}

	jclass cls = (*env)->GetObjectClass(env, app->activity->clazz);

	if(!cls)
		retError(clean, Error_invalidState(0, "Couldn't find OxC3Activity"));

	jmethodID methodId = (*env)->GetMethodID(env, cls, "toggleKeyboard", "(Z)V");

	if (!methodId)
		retError(clean, Error_invalidState(0, "Couldn't find OxC3Activity.toggleKeyboard"));

	(*env)->CallVoidMethod(env, app->activity->clazz, methodId, isVisible);

clean:

	if(cls)
		(*env)->DeleteLocalRef(env, cls);

	if(!s_uccess)
		Error_print(Platform_instance->alloc, e_rr, ELogLevel_Error, ELogOptions_Default);

	if(attached)
		(*vm)->DetachCurrentThread(vm);

	return true;
}

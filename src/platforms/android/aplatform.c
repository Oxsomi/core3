/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "platforms/ext/listx_impl.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/errorx.h"
#include "platforms/platform.h"
#include "platforms/keyboard.h"
#include "platforms/input_device.h"
#include "platforms/log.h"
#include "types/base/error.h"

#include <android/asset_manager.h>
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

	gotoIfError2(clean, CharString_createCopyx(internal, &Platform_instance->appDirectory))
	gotoIfError2(clean, CharString_createCopyx(external, &Platform_instance->workDirectory))

	if(!CharString_endsWithSensitive(Platform_instance->appDirectory, '/', 0))
		gotoIfError2(clean, CharString_appendx(&Platform_instance->appDirectory, '/'))

	if(!CharString_endsWithSensitive(Platform_instance->workDirectory, '/', 0))
		gotoIfError2(clean, CharString_appendx(&Platform_instance->workDirectory, '/'))

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

		if(!CharString_startsWithStringSensitive(sectionName, CharString_createRefCStrConst("section_"), 0))
			continue;

		//Now we can open the section directly and find the files through there

		sectionName.ptr += sizeof("section");		//sizeof includes null terminator so no need for section_
		sectionName.lenAndNullTerminated -= sizeof("section");

		gotoIfError2(clean, CharString_createCopyx(sectionName, &tmpStr))
		gotoIfError2(clean, CharString_insertStringx(&tmpStr, CharString_createRefCStrConst("packages/"), 0))

		subDir = AAssetManager_openDir(assetManager, tmpStr.ptr);

		if(!subDir)
			retError(clean, Error_invalidState(0, "Platform_initUnixExt() failed, couldn't opendir"))

		//Go through subsection

		while ((nameSubSection = AAssetDir_getNextFileName(subDir)) != NULL) {

			gotoIfError2(clean, CharString_createCopyx(tmpStr, &tmpStr1))
			gotoIfError2(clean, CharString_appendx(&tmpStr1, '/'))
			gotoIfError2(clean, CharString_appendStringx(&tmpStr1, CharString_createRefCStrConst(nameSubSection)))
			
			gotoIfError2(clean, CharString_createCopyx(sectionName, &tmpStr2))
			gotoIfError2(clean, CharString_appendx(&tmpStr2, '/'))
			gotoIfError2(clean, CharString_appendStringx(&tmpStr2, CharString_createRefCStrConst(nameSubSection)))

			if(CharString_endsWithStringInsensitive(tmpStr2, CharString_createRefCStrConst(".oiCA"), 0))
				gotoIfError2(clean, CharString_popEndCount(&tmpStr2, sizeof("oiCA")))	//sizeof("oiCA") == 5

			asset = AAssetManager_open(assetManager, tmpStr1.ptr, AASSET_MODE_STREAMING);

			if(!asset)
				retError(clean, Error_invalidState(0, "Platform_initUnixExt() failed, couldn't open asset"))

			VirtualSection section = (VirtualSection) { .path = tmpStr2 };
			section.lenExt = AAsset_getLength64(asset);

			gotoIfError2(clean, ListVirtualSection_pushBackx(&Platform_instance->virtualSections, section))
			tmpStr2 = CharString_createNull();
			AAsset_close(asset);
			asset = NULL;
			CharString_freex(&tmpStr1);
		}

		CharString_freex(&tmpStr);
		AAssetDir_close(subDir);
		subDir = NULL;
	}

clean:

	if(!s_uccess) {
		Log_errorLnx("Couldn't initialize app, encountered issue");
		if(e_rr) Error_printLnx(*e_rr);
	}
	
	if(asset) AAsset_close(asset);
	if(subDir) AAssetDir_close(subDir);
	if(dir) AAssetDir_close(dir);
	CharString_freex(&tmpStr);
	CharString_freex(&tmpStr1);
	CharString_freex(&tmpStr2);
	return s_uccess;
}

void Platform_cleanupUnixExt() { }

CharString Keyboard_remap(const Keyboard *keyboard, EKey key) {

	if(!keyboard || key >= keyboard->buttons)
		return CharString_createNull();

	//There's no better way to remap keys on Android that is safe for different localizations,
	//Because nobody is crazy enough to use keyboards on Android.
	//+ sizeof(EKey) == substr("EKey_".size())

	CharString oldStr = CharString_createRefCStrConst(InputDevice_getButton(*keyboard, key)->name + sizeof("EKey"));

	CharString str = CharString_createNull();
	CharString_createCopyx(oldStr, &str);
	return str;
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
		retError(clean, Error_invalidState(0, "Couldn't find OxC3Activity"))

	jmethodID methodId = (*env)->GetMethodID(env, cls, "toggleKeyboard", "(Z)V");

	if (!methodId)
		retError(clean, Error_invalidState(0, "Couldn't find OxC3Activity.toggleKeyboard"))

	(*env)->CallVoidMethod(env, app->activity->clazz, methodId, isVisible);

clean:

	if(!s_uccess)
		Error_printLnx(err);

	if(attached)
		(*vm)->DetachCurrentThread(vm);

	return true;
}

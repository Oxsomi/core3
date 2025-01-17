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
#include "platforms/platform.h"
#include "platforms/keyboard.h"
#include "platforms/log.h"
#include "types/base/error.h"

#include <android/asset_manager.h>
#include <android_native_app_glue.h>

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

	Log_debugLnx("Hello world!");

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
		sectionName.lenAndNullTerminated = (sectionName.lenAndNullTerminated << 1 >> 1) - sizeof("section");

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

			asset = AAssetManager_open(assetManager, tmpStr1.ptr, AASSET_MODE_STREAMING);

			if(!asset)
				retError(clean, Error_invalidState(0, "Platform_initUnixExt() failed, couldn't open asset"))

			VirtualSection section = (VirtualSection) { .path = tmpStr1 };
			section.lenExt = AAsset_getLength64(asset);

			gotoIfError2(clean, ListVirtualSection_pushBackx(&Platform_instance->virtualSections, section))
			tmpStr1 = CharString_createNull();
			AAsset_close(asset);
			asset = NULL;
		}

		CharString_freex(&tmpStr);
		AAssetDir_close(subDir);
		subDir = NULL;
	}

clean:
	if(asset) AAsset_close(asset);
	if(subDir) AAssetDir_close(subDir);
	if(dir) AAssetDir_close(dir);
	CharString_freex(&tmpStr);
	CharString_freex(&tmpStr1);
	return s_uccess;
}

void Platform_cleanupUnixExt() { }

CharString Keyboard_remap(EKey key) {
	(void) key;
	return CharString_createNull();			//TODO: 
}

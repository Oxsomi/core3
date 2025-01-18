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

#include "types/base/error.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "formats/oiCA/ca_file.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/formatx.h"
#include "platforms/file.h"
#include "types/base/lock.h"

#include <android/asset_manager.h>
#include <android_native_app_glue.h>

Bool File_loadVirtualInternal1(FileLoadVirtual *userData, CharString loc, Bool allowLoad, Error *e_rr) {

	AAssetManager *assetManager = ((struct android_app*)Platform_instance->data)->activity->assetManager;

	CharString isChild = CharString_createNull();
	CharString tmp = CharString_createNull();
	Buffer buf = Buffer_createNull();
	Bool s_uccess = true;
	AAsset *asset = NULL;
	ELockAcquire acq = ELockAcquire_Invalid;

	gotoIfError2(clean, CharString_createCopyx(loc, &isChild))

	if(CharString_length(isChild))
		gotoIfError2(clean, CharString_appendx(&isChild, '/'))		//Don't append to root

	acq = SpinLock_lock(&Platform_instance->virtualSectionsLock, U64_MAX);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "File_loadVirtualInternal1() couldn't lock virtualSectionsLock"))

	Bool foundAny = false;

	for (U64 i = 0; i < Platform_instance->virtualSections.length; ++i) {

		VirtualSection *section = Platform_instance->virtualSections.ptrNonConst + i;

		if(
			!CharString_equalsStringInsensitive(loc, section->path) &&
			!CharString_startsWithStringInsensitive(section->path, isChild, 0)
		)
			continue;

		//Load

		if (userData->doLoad) {

			if (!section->loaded) {

				if (!allowLoad)
					retError(clean, Error_notFound(0, 0, "File_loadVirtualInternal1() was queried but none was found"));

				gotoIfError2(clean, CharString_formatx(
					&tmp, "packages/%.*s.oiCA", (int) CharString_length(section->path), section->path.ptr
				))

				asset = AAssetManager_open(assetManager, tmp.ptr, AASSET_MODE_BUFFER);

				if(!asset)
					retError(clean, Error_notFound(0, 0, "File_loadVirtualInternal1() was not found by asset manager"));

				CAFile file = (CAFile) { 0 };

				gotoIfError2(clean, Buffer_createUninitializedBytesx(section->lenExt, &buf))

				int r = AAsset_read(asset, buf.ptrNonConst, section->lenExt);

				if(r < 0 || (U32)r != section->lenExt)
					retError(clean, Error_invalidState(0, "File_loadVirtualInternal1() asset wasn't readable"));

				gotoIfError3(clean, CAFile_readx(buf, userData->encryptionKey, &file, e_rr))

				AAsset_close(asset);
				asset = NULL;

				section->loadedData = file.archive;
				section->loaded = true;
				foundAny = true;

				Buffer_freex(&buf);
				CharString_freex(&tmp);
			}
		}

		//Otherwise we want to use error to determine if it's present or not

		else if(!section->loaded)
			retError(clean, Error_notFound(1, 1, "File_loadVirtualInternal1()::loc not found (1)"))
	}

	if(!foundAny)
		retError(clean, Error_notFound(2, 1, "File_loadVirtualInternal1()::loc not found (2)"))

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&Platform_instance->virtualSectionsLock);

	if(asset)
		AAsset_close(asset);

	Buffer_freex(&buf);
	CharString_freex(&isChild);
	CharString_freex(&tmp);
	return s_uccess;
}

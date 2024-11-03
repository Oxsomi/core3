/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "tools/cli.h"
#include "platforms/platform.h"
#include "platforms/log.h"
#include "platforms/ext/errorx.h"

#include "types/container/ref_ptr.h"

/*
#include "audio/interface.h"
#include "audio/device.h"
#include "audio/stream.h"
#include "audio/source.h"
*/

Platform_defineEntrypoint() {

	int status = 0;
	Error err = Platform_create(argc, argv, Platform_getData(), NULL, true);

	if(err.genericError) {
		Error_printLnx(err);
		return -2;
	}

	/*
	AudioInterfaceRef *ref = NULL;
	AudioInterface_createx(&ref, &err);

	AudioDeviceInfo info = (AudioDeviceInfo) { 0 };
	AudioInterface_getPreferredDevicex(AudioInterfaceRef_ptr(ref), EAudioDeviceFlags_MainOutput, &info, &err);

	AudioDeviceRef *dev = NULL;
	AudioDeviceRef_createx(ref, &info, false, &dev, &err);

	AudioStreamRef *stream = NULL;
	AudioDeviceRef_createFileStreamx(dev, CharString_createRefCStrConst("music.wav"), false, 0, 1, &stream, &err);

	AudioSourceRef *source = NULL;
	AudioDeviceRef_createSourcex(dev, stream, (AudioModifier) { 0 }, &source, &err);

	AudioStreamRef_playx(stream, &err);
	AudioDeviceRef_waitx(dev, true, &err);

	AudioSourceRef_dec(&source);
	AudioStreamRef_dec(&stream);
	AudioDeviceRef_dec(&dev);
	AudioInterfaceRef_dec(&ref);*/

	CLI_init();

	if (!CLI_execute(Platform_instance->args)) {
		status = -1;
		goto clean;
	}

clean:
	CLI_shutdown();
	Platform_cleanup();
	return status;
}

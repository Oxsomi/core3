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

#include "platforms/ext/listx_impl.h"
#include "tools/cli.h"
#include "audio/interface.h"
#include "audio/device.h"
#include "types/container/ref_ptr.h"

Bool CLI_audioDevices(ParsedArgs args) {

	(void) args;

	Bool s_uccess = true;
	AudioInterfaceRef *ref = NULL;
	ListAudioDeviceInfo infos = (ListAudioDeviceInfo) { 0 };
	Error err = Error_none();

	gotoIfError3(clean, AudioInterface_createx(&ref, &err))
	gotoIfError3(clean, AudioInterface_getDeviceInfosx(AudioInterfaceRef_ptr(ref), &infos, &err))

	for(U64 i = 0; i < infos.length; ++i)
		AudioDeviceInfo_printx(infos.ptr[i]);

clean:

	ListAudioDeviceInfo_freex(&infos);

	if(ref)
		AudioInterfaceRef_dec(&ref);

	return s_uccess;
}

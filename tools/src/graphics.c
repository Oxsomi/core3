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
#include "cli.h"
#include "types/error.h"
#include "platforms/ext/errorx.h"
#include "platforms/log.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/device_info.h"

Bool CLI_graphicsDevices(ParsedArgs args) {

	Error err = Error_none();
	GraphicsInstanceRef *instanceRef = NULL;
	ListGraphicsDeviceInfo infos = (ListGraphicsDeviceInfo) { 0 };

	gotoIfError(clean, GraphicsInstance_create(
		(GraphicsApplicationInfo) {
			.name = CharString_createRefCStrConst("OxC3 CLI"),
			.version = GraphicsApplicationInfo_Version(0, 2, 0)
		},
		EGraphicsInstanceFlags_None,
		&instanceRef
	))

	gotoIfError(clean, GraphicsInstance_getDeviceInfos(GraphicsInstanceRef_ptr(instanceRef), &infos))

	//If entry or length is there, we will print full info

	if (args.parameters & (EOperationHasParameter_Count | EOperationHasParameter_Entry)) {

		U64 count = 0;
		U64 offset = 0;

		if (args.parameters & EOperationHasParameter_Count) {

			CharString arg = args.args.ptr[offset++];
			
			if(!CharString_parseU64(arg, &count))
				gotoIfError(clean, Error_invalidParameter(0, 0, "CLI_graphicsDevices() expected count as U64"))
		}

		U64 entry = 0;

		if (args.parameters & EOperationHasParameter_Entry) {

			CharString arg = args.args.ptr[offset++];
			
			if(!CharString_parseU64(arg, &entry))
				gotoIfError(clean, Error_invalidParameter(0, 0, "CLI_graphicsDevices() expected entry as U64"))

			if (!(args.parameters & EOperationHasParameter_Count))
				count = 1;
		}

		if(!count && entry < infos.length)
			count = infos.length - entry;

		Log_debugLnx("Graphics device matching ranges [ %"PRIu64", %"PRIu64" >", entry, entry + count);

		for(U64 i = entry; i < infos.length && i < entry + count; ++i)
			GraphicsDeviceInfo_print(GraphicsInstanceRef_ptr(instanceRef)->api, &infos.ptr[i], true);
	}

	//Otherwise, we will simply list the basic information of the devices

	else {

		Log_debugLnx("%"PRIu64" graphics devices:", infos.length);

		for(U64 i = 0; i < infos.length; ++i)
			GraphicsDeviceInfo_print(GraphicsInstanceRef_ptr(instanceRef)->api, &infos.ptr[i], false);
	}

clean:
	ListGraphicsDeviceInfo_freex(&infos);
	GraphicsInstanceRef_dec(&instanceRef);
	Error_printx(err, ELogLevel_Error, ELogOptions_Default);
	return !err.genericError;
}

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
#include "tools/cli.h"
#include "types/base/error.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/stringx.h"
#include "platforms/log.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/device_info.h"

#ifdef CLI_GRAPHICS

	Bool CLI_graphicsDevices(ParsedArgs args) {

		Bool s_uccess = true;
		Error err = Error_none(), *e_rr = &err;
		GraphicsInstanceRef *instanceRef = NULL;
		ListGraphicsDeviceInfo infos = (ListGraphicsDeviceInfo) { 0 };
		ListCharString strings = (ListCharString) { 0 };

		gotoIfError3(clean, GraphicsInterface_create(e_rr))

		U64 queried = CLI_parseGraphicsAPIs(args);

		if(queried == U64_MAX)
			retError(clean, Error_invalidState(0, "CLI_parseGraphicsAPIs() failed"))

		Bool wasExplicit = queried != U32_MAX;

		for(U64 j = 0; j < EGraphicsApi_Count; ++j) {

			EGraphicsApi api = (EGraphicsApi) j;

			if(!((queried >> j) & 1))
				continue;

			if (!GraphicsInterface_supportsApi(api)) {

				if(wasExplicit)
					Log_warnLnx("CLI_graphicsDevices() -graphics-api specifically requested API, but wasn't found");

				continue;
			}

			gotoIfError2(clean, GraphicsInstance_create(
				(GraphicsApplicationInfo) {
					.name = CharString_createRefCStrConst("OxC3 CLI"),
					.version = OXC3_MAKE_VERSION(OXC3_MAJOR, OXC3_MINOR, OXC3_PATCH)
				},
				api,
				EGraphicsInstanceFlags_None,
				&instanceRef
			))

			gotoIfError2(clean, GraphicsInstance_getDeviceInfos(GraphicsInstanceRef_ptr(instanceRef), &infos))

			//If entry or length is there, we will print full info

			if (args.parameters & (EOperationHasParameter_CountArg | EOperationHasParameter_Entry)) {

				U64 count = 0;

				if (args.parameters & EOperationHasParameter_CountArg) {

					CharString arg = CharString_createNull();
					gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_CountShift, &arg))

					if(!CharString_parseU64(arg, &count))
						gotoIfError2(clean, Error_invalidParameter(0, 0, "CLI_graphicsDevices() expected count as U64"))
				}

				U64 entry = 0;

				if (args.parameters & EOperationHasParameter_Entry) {

					CharString arg = CharString_createNull();
					gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_EntryShift, &arg))

					if(!CharString_parseU64(arg, &entry))
						gotoIfError2(clean, Error_invalidParameter(0, 0, "CLI_graphicsDevices() expected entry as U64"))

					if (!(args.parameters & EOperationHasParameter_CountArg))
						count = 1;
				}

				if(!count && entry < infos.length)
					count = infos.length - entry;

				Log_debugLnx("Graphics device matching ranges [%"PRIu64", %"PRIu64">", entry, entry + count);

				for(U64 i = entry; i < infos.length && i < entry + count; ++i)
					GraphicsDeviceInfo_print(GraphicsInstanceRef_ptr(instanceRef)->api, &infos.ptr[i], true);
			}

			//Otherwise, we will simply list the basic information of the devices

			else {

				Log_debugLnx("%s: %"PRIu64" graphics devices:", EGraphicsApi_name[api], infos.length);

				for(U64 i = 0; i < infos.length; ++i)
					GraphicsDeviceInfo_print(GraphicsInstanceRef_ptr(instanceRef)->api, &infos.ptr[i], args.flags & EOperation_Verbose);
			}

			ListGraphicsDeviceInfo_freex(&infos);
			GraphicsInstanceRef_dec(&instanceRef);
		}

	clean:
		ListCharString_freex(&strings);
		ListGraphicsDeviceInfo_freex(&infos);
		GraphicsInstanceRef_dec(&instanceRef);
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);
		return s_uccess;
	}

#else
	Bool CLI_graphicsDevices(ParsedArgs args) { (void)args; return false; }
#endif

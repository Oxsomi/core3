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

//tools/oxc3_cli/graphics.c

#include "tools/oxc3_cli/cli.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "types/base/error.h"
#include "types/base/string_read.h"
#include "types/base/constants.h"

#ifdef CLI_GRAPHICS

	#include "graphics/generic/instance.h"
	#include "graphics/generic/device.h"
	#include "graphics/generic/device_info.h"

	Bool CLI_graphicsCreate(const ParsedArgs *args) {

		if(!args) return false;

		Bool s_uccess = true;
		Error err = Error_none(), *e_rr = &err;
		const Allocator *alloc = Platform_instance->alloc;
		RefPtrType instanceType = (RefPtrType) { 0 };        //Must outlive the instance
		GraphicsInstanceRef *instanceRef = NULL;
		GraphicsDeviceRef *deviceRef = NULL;
		ListGraphicsDeviceInfo infos = (ListGraphicsDeviceInfo) { 0 };

		//A missing graphics stack (headless CI, no driver/ICD) means there's simply nothing to create; not fatal.

		if(!GraphicsInterface_create(&err)) {
			Log_debugLnx("No graphics interface available (headless / no driver); nothing to create");
			err = Error_none();
			goto clean;
		}

		U64 queried = CLI_parseGraphicsAPIs(args);

		if(queried == U64_MAX)
			retError(clean, Error_invalidState(0, "CLI_graphicsCreate() CLI_parseGraphicsAPIs failed"));
		
		Bool wasExplicit = queried != U32_MAX;

		for(U64 j = 0; j < EGraphicsApi_Count; ++j) {

			EGraphicsApi api = (EGraphicsApi) j;

			if(!((queried >> j) & 1))
				continue;

			if (!GraphicsInterface_supportsApi(api)) {

				if(wasExplicit)
					Log_warnLnx("CLI_graphicsCreate() -graphics-api specifically requested API, but wasn't found");

				continue;
			}
			
			instanceType = GraphicsInstance_makeType(api, alloc);

			const GraphicsApplicationInfo applicationInfo = {
				.name = CharString_createRefCStrConst("OxC3 CLI"),
				.version = OXC3_MAKE_VERSION(OXC3_MAJOR, OXC3_MINOR, OXC3_PATCH)
			};

			//A driverless API on this machine (e.g. no Vulkan ICD in headless CI) shouldn't abort; skip it.

			if(!GraphicsInstance_create(
				&applicationInfo,
				api,
				EGraphicsInstanceFlags_None,
				alloc,
				&instanceType,
				&instanceRef,
				&err
			)) {
				Log_debugLnx("Couldn't create a %s instance (headless / no driver); skipping", EGraphicsApi_name[api]);
				err = Error_none();
				continue;
			}

			//Enumeration can fail per-API on headless CI; like instance creation, that shouldn't abort the whole run.

			if(!GraphicsInstance_getDeviceInfos(GraphicsInstanceRef_ptr(instanceRef), &infos, &err)) {
				Log_debugLnx("Couldn't enumerate %s devices (driver error / headless); skipping", EGraphicsApi_name[api]);
				err = Error_none();
				ListGraphicsDeviceInfo_free(&infos, alloc);
				RefPtr_dec(&instanceRef);
				continue;
			}
			
			U64 entry = 0;

			if (args->parameters & EOperationHasParameter_Entry) {

				CharString arg = CharString_createNull();
				gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_EntryShift, &arg, e_rr));

				if(!CharString_parseU64(arg, &entry))
					retError(clean, Error_invalidParameter(0, 0, "CLI_graphicsCreate() expected entry as U64"));

				if(entry >= infos.length)
					retError(clean, Error_invalidParameter(0, 0, "CLI_graphicsCreate() device entry not found"));
			}
			
			U64 count = 0;

			if (args->parameters & EOperationHasParameter_CountArg) {

				CharString arg = CharString_createNull();
				gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_CountShift, &arg, e_rr));

				if(!CharString_parseU64(arg, &entry))
					retError(clean, Error_invalidParameter(0, 0, "CLI_graphicsCreate() expected count as U64"));

				if(entry + count > infos.length)
					retError(clean, Error_invalidParameter(0, 0, "CLI_graphicsCreate() device index not found"));
			}

			if(!count)
				count = infos.length - entry;

			for (U64 i = entry; i < entry + count; ++i) {

				gotoIfError3(clean, GraphicsDeviceRef_create(
					instanceRef, &infos.ptr[i], EGraphicsDeviceFlags_None, EGraphicsBufferingMode_Default, &deviceRef, e_rr
				));
				
				RefPtr_dec(&deviceRef);
				Log_debugLnx("Create device success %"PRIu64" (%s)", i, EGraphicsApi_name[api]);
			}

			ListGraphicsDeviceInfo_free(&infos, alloc);
			RefPtr_dec(&instanceRef);
		}

	clean:
		ListGraphicsDeviceInfo_free(&infos, alloc);
		RefPtr_dec(&instanceRef);
		RefPtr_dec(&deviceRef);
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
		return s_uccess;
	}

	//Live memory usage requires actually creating the device (there's no cross-API way to query it otherwise).
	//Non-fatal: if the device can't be created we just note it and continue listing.

	static void CLI_printMemoryBudget(
		GraphicsInstanceRef *instanceRef, const GraphicsDeviceInfo *info, const Allocator *alloc
	) {

		(void) alloc;

		GraphicsDeviceRef *deviceRef = NULL;
		Error err = Error_none();

		//Querying live memory requires a real device. If creation fails (e.g. a build without embedded graphics
		//shaders, or a driver issue) we just note it rather than dumping the full error/stacktrace per device.

		if(!GraphicsDeviceRef_create(
			instanceRef, info, EGraphicsDeviceFlags_None, EGraphicsBufferingMode_Default, &deviceRef, &err
		)) {
			Log_debugLnx("\tMemory in use: (unavailable, couldn't create a device to query)");
			return;
		}

		const U64 deviceLocal = GraphicsDeviceRef_getMemoryBudget(deviceRef, true);
		const U64 shared = GraphicsDeviceRef_getMemoryBudget(deviceRef, false);

		Log_debugLnx(
			"\tMemory in use: %"PRIu64" bytes device-local, %"PRIu64" bytes shared", deviceLocal, shared
		);

		RefPtr_dec(&deviceRef);
	}

	Bool CLI_graphicsDevices(const ParsedArgs *args) {

		if(!args) return false;

		Bool s_uccess = true;
		Error err = Error_none(), *e_rr = &err;
		const Allocator *alloc = Platform_instance->alloc;
		RefPtrType instanceType = (RefPtrType) { 0 };        //Must outlive the instance
		GraphicsInstanceRef *instanceRef = NULL;
		ListGraphicsDeviceInfo infos = (ListGraphicsDeviceInfo) { 0 };
		ListCharString strings = (ListCharString) { 0 };

		//Listing devices is informational, so a missing graphics stack (headless CI, no driver/ICD) isn't fatal.

		if(!GraphicsInterface_create(&err)) {
			Log_debugLnx("No graphics interface available (headless / no driver); no graphics devices to show");
			err = Error_none();
			goto clean;
		}

		U64 queried = CLI_parseGraphicsAPIs(args);

		if(queried == U64_MAX)
			retError(clean, Error_invalidState(0, "CLI_graphicsDevices(): CLI_parseGraphicsAPIs failed"));

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

			instanceType = GraphicsInstance_makeType(api, alloc);

			const GraphicsApplicationInfo applicationInfo = {
				.name = CharString_createRefCStrConst("OxC3 CLI"),
				.version = OXC3_MAKE_VERSION(OXC3_MAJOR, OXC3_MINOR, OXC3_PATCH)
			};

			//A driverless API on this machine (e.g. no Vulkan ICD in headless CI) shouldn't abort the listing.

			if(!GraphicsInstance_create(
				&applicationInfo,
				api,
				EGraphicsInstanceFlags_None,
				alloc,
				&instanceType,
				&instanceRef,
				&err
			)) {
				Log_debugLnx("Couldn't create a %s instance (headless / no driver); skipping", EGraphicsApi_name[api]);
				err = Error_none();
				continue;
			}

			//Enumeration can fail per-API (e.g. a driver that creates an instance but errors listing adapters on
			// headless CI); like instance creation above, that shouldn't abort the whole informational listing.

			if(!GraphicsInstance_getDeviceInfos(GraphicsInstanceRef_ptr(instanceRef), &infos, &err)) {
				Log_debugLnx("Couldn't enumerate %s devices (driver error / headless); skipping", EGraphicsApi_name[api]);
				err = Error_none();
				ListGraphicsDeviceInfo_free(&infos, alloc);
				RefPtr_dec(&instanceRef);
				continue;
			}

			//If entry or length is there, we will print full info

			if (args->parameters & (EOperationHasParameter_CountArg | EOperationHasParameter_Entry)) {

				U64 count = 0;

				if (args->parameters & EOperationHasParameter_CountArg) {

					CharString arg = CharString_createNull();
					gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_CountShift, &arg, e_rr));

					if(!CharString_parseU64(arg, &count))
						retError(clean, Error_invalidParameter(0, 0, "CLI_graphicsDevices() expected count as U64"));
				}

				U64 entry = 0;

				if (args->parameters & EOperationHasParameter_Entry) {

					CharString arg = CharString_createNull();
					gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_EntryShift, &arg, e_rr));

					if(!CharString_parseU64(arg, &entry))
						retError(clean, Error_invalidParameter(0, 0, "CLI_graphicsDevices() expected entry as U64"));

					if (!(args->parameters & EOperationHasParameter_CountArg))
						count = 1;
				}

				if(!count && entry < infos.length)
					count = infos.length - entry;

				Log_debugLnx("Graphics device matching ranges [%"PRIu64", %"PRIu64">", entry, entry + count);

				for(U64 i = entry; i < infos.length && i < entry + count; ++i) {
					GraphicsDeviceInfo_print(GraphicsInstanceRef_ptr(instanceRef)->api, &infos.ptr[i], true);
					CLI_printMemoryBudget(instanceRef, &infos.ptr[i], alloc);
				}
			}

			//Otherwise, we will simply list the basic information of the devices

			else {

				Log_debugLnx("%s: %"PRIu64" graphics devices:", EGraphicsApi_name[api], infos.length);

				const Bool verbose = args->flags & EOperationFlags_Verbose;

				for(U64 i = 0; i < infos.length; ++i) {
					GraphicsDeviceInfo_print(GraphicsInstanceRef_ptr(instanceRef)->api, &infos.ptr[i], verbose);
					if(verbose)
						CLI_printMemoryBudget(instanceRef, &infos.ptr[i], alloc);
				}
			}

			ListGraphicsDeviceInfo_free(&infos, alloc);
			RefPtr_dec(&instanceRef);
		}

	clean:
		ListCharString_freeUnderlying(&strings, alloc);
		ListGraphicsDeviceInfo_free(&infos, alloc);
		RefPtr_dec(&instanceRef);
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
		return s_uccess;
	}

#else
	//No graphics in this build: listing devices is informational, so report it and succeed rather than fail
	//(e.g. so "devices all" still dumps CPU + audio).
	Bool CLI_graphicsDevices(const ParsedArgs *args) {
		if(!args) return false;
		Log_debugLnx("No graphics support in this build; no graphics devices to show");
		return true;
	}
#endif

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
#include "graphics/generic/interface.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/device.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/ref_ptrx.h"
#include "types/base/error.h"

TListImpl(GraphicsDeviceInfo);

const C8 *EGraphicsApi_name[EGraphicsApi_Count] = {
	"Vulkan",
	"D3D12"
};

Error GraphicsInstanceRef_dec(GraphicsInstanceRef **inst) {
	return !RefPtr_dec(inst) ?
		Error_invalidOperation(0, "GraphicsInstanceRef_dec()::inst is required") : Error_none();
}

Error GraphicsInstanceRef_inc(GraphicsInstanceRef *inst) {
	return !RefPtr_inc(inst) ?
		Error_invalidOperation(0, "GraphicsInstanceRef_inc()::inst is required") : Error_none();
}

Error GraphicsInstance_getPreferredDevice(
	const GraphicsInstance *inst,
	GraphicsDeviceCapabilities requiredCapabilities,
	U64 vendorMask,
	U64 deviceTypeMask,
	GraphicsDeviceInfo *deviceInfo
) {

	if(!deviceInfo)
		return Error_nullPointer(4, "GraphicsInstance_getPreferredDevice()::deviceInfo is required");

	if(deviceInfo->name[0])
		return Error_invalidParameter(
			4, 0, "GraphicsInstance_getPreferredDevice()::*deviceInfo must be empty"
		);

	ListGraphicsDeviceInfo tmp = (ListGraphicsDeviceInfo) { 0 };
	Error err = GraphicsInstance_getDeviceInfosExt(inst, &tmp);

	if(err.genericError)
		return err;

	U64 preferredDedicated = 0;
	U64 preferredNonDedicated = 0;
	U64 preferredIntegrated = 0;
	Bool hasDedicated = false;
	Bool hasIntegrated = false;
	Bool hasAny = false;

	for (U64 i = 0; i < tmp.length; ++i) {

		const GraphicsDeviceInfo info = tmp.ptr[i];

		//Check if vendor and device type are supported

		if(!((vendorMask >> info.vendor) & 1) || !((deviceTypeMask >> info.type) & 1))
			continue;

		//Check capabilities

		if((info.capabilities.dataTypes & requiredCapabilities.dataTypes) != requiredCapabilities.dataTypes)
			continue;

		if((info.capabilities.features & requiredCapabilities.features) != requiredCapabilities.features)
			continue;

		if((info.capabilities.features2 & requiredCapabilities.features2) != requiredCapabilities.features2)
			continue;

		if((info.capabilities.featuresExt & requiredCapabilities.featuresExt) != requiredCapabilities.featuresExt)
			continue;

		if(
			info.capabilities.sharedMemory < requiredCapabilities.sharedMemory ||
			info.capabilities.dedicatedMemory < requiredCapabilities.dedicatedMemory ||
			info.capabilities.maxBufferSize < requiredCapabilities.maxBufferSize ||
			info.capabilities.maxAllocationSize < requiredCapabilities.maxAllocationSize
		)
			continue;

		if(info.type == EGraphicsDeviceType_Dedicated) {
			preferredDedicated = i;
			hasDedicated = hasAny = true;
			break;
		}

		else {

			if (info.type == EGraphicsDeviceType_Integrated) {
				preferredIntegrated = i;
				hasIntegrated = true;
			}

			else preferredNonDedicated = i;
		}

		hasAny = true;
	}

	if(!hasAny)
		gotoIfError(clean, Error_notFound(0, 0, "GraphicsInstance_getPreferredDevice() no supported queried devices"))

	const U64 picked = hasDedicated ? preferredDedicated : (hasIntegrated ? preferredIntegrated : preferredNonDedicated);
	*deviceInfo = tmp.ptr[picked];

clean:
	ListGraphicsDeviceInfo_freex(&tmp);
	return err;
}

Bool GraphicsInterface_create(Error *e_rr) {
	return GraphicsInterface_init(e_rr);
}

Bool GraphicsInterface_supportsApi(EGraphicsApi api) {
	return GraphicsInterface_supports(api);
}

Error GraphicsInstance_create(
	GraphicsApplicationInfo info,
	EGraphicsApi api,
	EGraphicsInstanceFlags flags,
	GraphicsInstanceRef **instanceRef
) {

	Error err = Error_none();
	Bool initRefPtr = false;

	if (api >= EGraphicsApi_Count) {
		#if _PLATFORM_TYPE == PLATFORM_WINDOWS
			api = GraphicsInterface_supports(EGraphicsApi_Direct3D12) ? EGraphicsApi_Direct3D12 : EGraphicsApi_Vulkan;
		#else
			api = EGraphicsApi_Vulkan;
		#endif
	}

	gotoIfError(clean, RefPtr_createx(
		(U32)(sizeof(GraphicsInstance) + GraphicsInterface_getObjectSizes(api)->instance),
		(ObjectFreeFunc) GraphicsInstance_freeExt,
		(ETypeId) EGraphicsTypeId_GraphicsInstance,
		instanceRef
	))

	initRefPtr = true;

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(*instanceRef);

	*instance = (GraphicsInstance) { .application = info, .api = api, .flags = flags };

	#ifndef NDEBUG
		if(!(flags & EGraphicsInstanceFlags_DisableDebug))
			instance->flags |= EGraphicsInstanceFlags_IsDebug;
	#endif

	gotoIfError(clean, GraphicsInstance_createExt(info, instanceRef));

clean:

	if(err.genericError && initRefPtr)
		GraphicsInstanceRef_dec(instanceRef);

	return err;
}

Error GraphicsInstance_getDeviceInfos(const GraphicsInstance *inst, ListGraphicsDeviceInfo *infos) {
	return GraphicsInstance_getDeviceInfosExt(inst, infos);
}

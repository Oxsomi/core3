/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "graphics/generic/instance.h"
#include "graphics/generic/device.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/ref_ptrx.h"
#include "types/error.h"

TListImpl(GraphicsDeviceInfo);

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
	Bool verbose,
	GraphicsDeviceInfo *deviceInfo
) {

	if(!deviceInfo)
		return Error_nullPointer(4, "GraphicsInstance_getPreferredDevice()::deviceInfo is required");

	if(deviceInfo->driverInfo[0])
		return Error_invalidParameter(
			4, 0, "GraphicsInstance_getPreferredDevice()::*deviceInfo must be empty"
		);

	ListGraphicsDeviceInfo tmp = (ListGraphicsDeviceInfo) { 0 };
	Error err = GraphicsInstance_getDeviceInfos(inst, verbose, &tmp);

	if(err.genericError)
		return err;

	U64 preferredDedicated = 0;
	U64 preferredNonDedicated = 0;
	Bool hasDedicated = false;
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
			info.capabilities.dedicatedMemory < requiredCapabilities.dedicatedMemory
		)
			continue;

		if(info.type == EGraphicsDeviceType_Dedicated) {
			preferredDedicated = i;
			hasDedicated = hasAny = true;
			break;
		}

		else preferredNonDedicated = i;

		hasAny = true;
	}

	if(!hasAny)
		gotoIfError(clean, Error_notFound(0, 0, "GraphicsInstance_getPreferredDevice() no supported queried devices"))

	const U64 picked = hasDedicated ? preferredDedicated : preferredNonDedicated;
	*deviceInfo = tmp.ptr[picked];

clean:
	ListGraphicsDeviceInfo_freex(&tmp);
	return err;
}

impl Error GraphicsInstance_createExt(GraphicsApplicationInfo info, Bool isVerbose, GraphicsInstanceRef **instanceRef);
impl Bool GraphicsInstance_free(GraphicsInstance *inst, Allocator alloc);
impl extern const U64 GraphicsInstanceExt_size;

Error GraphicsInstance_create(GraphicsApplicationInfo info, Bool isVerbose, GraphicsInstanceRef **instanceRef) {

	Error err = RefPtr_createx(
		(U32)(sizeof(GraphicsInstance) + GraphicsInstanceExt_size),
		(ObjectFreeFunc) GraphicsInstance_free,
		(ETypeId) EGraphicsTypeId_GraphicsInstance,
		instanceRef
	);

	if(err.genericError)
		return err;

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(*instanceRef);

	*instance = (GraphicsInstance) { .application = info };
	gotoIfError(clean, GraphicsInstance_createExt(info, isVerbose, instanceRef));

clean:

	if(err.genericError)
		GraphicsInstanceRef_dec(instanceRef);

	return err;
}

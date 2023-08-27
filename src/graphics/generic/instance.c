/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
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

#include "graphics/generic/instance.h"
#include "graphics/generic/device_info.h"
#include "platforms/ext/listx.h"
#include "types/error.h"

U64 GraphicsInstance_vendorMaskAll = 0xFFFFFFFFFFFFFFFF;
U64 GraphicsInstance_deviceTypeAll = 0xFFFFFFFFFFFFFFFF;

Error GraphicsInstance_getPreferredGpu(
	const GraphicsInstance *inst,
	GraphicsDeviceCapabilities requiredCapabilities,
	U64 vendorMask,
	U64 deviceTypeMask,
	void **deviceExt
) {

	if(!deviceExt)
		return Error_nullPointer(4);

	List tmp = (List) { 0 };
	Error err = GraphicsInstance_getDeviceInfos(inst, &tmp);

	if(err.genericError)
		return err;

	void *preferredDedicated = NULL;
	void *preferredNonDedicated = NULL;
	Bool hasDedicated = false;
	Bool hasAny = false;

	for (U64 i = 0; i < tmp.length; ++i) {

		GraphicsDeviceInfo info = ((GraphicsDeviceInfo*)tmp.ptr)[i];

		//Check if vendor and device type are supported

		if(!((vendorMask >> info.vendor) & 1) || !((deviceTypeMask >> info.type) & 1))
			continue;

		//Check capabilities

		if(info.capabilities.bindingTier < requiredCapabilities.bindingTier)
			continue;

		if((info.capabilities.dataTypes & requiredCapabilities.dataTypes) != requiredCapabilities.dataTypes)
			continue;

		if((info.capabilities.features & requiredCapabilities.features) != requiredCapabilities.features)
			continue;

		//Remember

		if(info.type == EGraphicsDeviceType_Dedicated) {
			preferredDedicated = info.ext;
			hasDedicated = true;
		}

		else preferredNonDedicated = info.ext;

		hasAny = true;
	}

	if(!hasAny)
		_gotoIfError(clean, Error_notFound(0, 0));

	if (hasDedicated)
		*deviceExt = preferredDedicated;

	else *deviceExt = preferredNonDedicated;

clean:
	List_freex(&tmp);
	return err;
}

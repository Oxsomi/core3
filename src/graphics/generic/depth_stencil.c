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

#include "graphics/generic/depth_stencil.h"
#include "graphics/generic/device.h"
#include "types/error.h"
#include "types/string.h"

Error DepthStencilRef_dec(DepthStencilRef **depthStencil) {
	return !RefPtr_dec(depthStencil) ? Error_invalidOperation(0, "DepthStencilRef_dec()::depthStencil is invalid") : 
	Error_none();
}

Error DepthStencilRef_inc(DepthStencilRef *depthStencil) {
	return !RefPtr_inc(depthStencil) ? Error_invalidOperation(0, "DepthStencilRef_dec()::depthStencil is invalid") : 
	Error_none();
}

impl Error GraphicsDeviceRef_createDepthStencilExt(
	GraphicsDeviceRef *deviceRef, 
	I32x2 size, 
	EDepthStencilFormat format, 
	Bool allowShaderRead,
	CharString name,
	DepthStencil *depthStencil
);

impl Bool GraphicsDevice_freeDepthStencilExt(DepthStencil *data, Allocator alloc);

impl extern const U64 DepthStencilExt_size;

Bool GraphicsDevice_freeDepthStencil(DepthStencil *depthStencil, Allocator alloc) {
	GraphicsDevice_freeDepthStencilExt(depthStencil, alloc);
	GraphicsDeviceRef_dec(&depthStencil->device);
	return true;
}

Error GraphicsDeviceRef_createDepthStencil(
	GraphicsDeviceRef *deviceRef, 
	I32x2 size, 
	EDepthStencilFormat format, 
	Bool allowShaderRead,
	CharString name,
	DepthStencilRef **depthStencilRef
) {

	if(!deviceRef || deviceRef->typeId != EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_createDepthStencil()::deviceRef is required");


	if(format <= EDepthStencilFormat_None || format >= EDepthStencilFormat_Count)
		return Error_invalidParameter(2, 0, "GraphicsDeviceRef_createDepthStencil()::format is required");

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	if(!GraphicsDeviceInfo_supportsDepthStencilFormat(&device->info, format))
		return Error_unsupportedOperation(0, "GraphicsDeviceRef_createDepthStencil()::format is unsupported");

	Error err = RefPtr_createx(
		(U32)(sizeof(DepthStencil) + DepthStencilExt_size), 
		(ObjectFreeFunc) GraphicsDevice_freeDepthStencil, 
		EGraphicsTypeId_DepthStencil, 
		depthStencilRef
	);

	if(err.genericError)
		return err;

	DepthStencil *depthStencil = DepthStencilRef_ptr(*depthStencilRef);
	_gotoIfError(clean, GraphicsDeviceRef_createDepthStencilExt(
		deviceRef, size, format, allowShaderRead, name, depthStencil
	));

clean:

	if(err.genericError)
		DepthStencilRef_dec(depthStencilRef);

	return err;
}

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

#include "platforms/ext/listx.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/ref_ptrx.h"
#include "graphics/generic/acceleration_structure.h"
#include "graphics/generic/device_buffer.h"
#include "types/buffer.h"

Error RTAS_validateDeviceBuffer(DeviceData *bufPtr) {

	if(!bufPtr || !bufPtr->buffer || bufPtr->buffer->typeId != (ETypeId) EGraphicsTypeId_DeviceBuffer)
		return Error_nullPointer(0, "BLAS_validateDeviceBuffer()::bufPtr/bufPtr->buffer is required");

	DeviceBuffer *buf = DeviceBufferRef_ptr(bufPtr->buffer);
	U64 totalLen = buf->resource.size;

	if(bufPtr->offset >= totalLen)
		return Error_nullPointer(0, "BLAS_validateDeviceBuffer()::bufPtr->offset out of bounds");

	if(!bufPtr->len)
		bufPtr->len = totalLen - bufPtr->offset;

	if(bufPtr->offset >> 48 || bufPtr->len >> 48)
		return Error_unsupportedOperation(1, "BLAS_validateDeviceBuffer()::buf.len or offset out of bounds");

	U64 len = bufPtr->offset + bufPtr->len;

	if(len > totalLen)
		return Error_invalidParameter(1, 0, "BLAS_validateDeviceBuffer()::buf is out of bounds");

	if(!(buf->usage & EDeviceBufferUsage_ASReadExt))
		return Error_invalidParameter(1, 0, "BLAS_validateDeviceBuffer()::buf usage ASReadExt is required for BLAS create");

	return Error_none();
}

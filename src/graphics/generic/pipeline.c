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
#include "graphics/generic/pipeline.h"
#include "graphics/generic/device.h"
#include "graphics/generic/texture.h"
#include "graphics/generic/device_buffer.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/errorx.h"
#include "platforms/log.h"
#include "formats/oiSH/sh_file.h"

const C8 *EPipelineStage_names[] = {
	"vertex",
	"pixel",
	"compute",
	"geometry",
	"hull",
	"domain",
	"raygeneration",
	"callable",
	"miss",
	"closesthit",
	"anyhit",
	"intersection"
};

TListImpl(PipelineStage);

void *Pipeline_infoOffset(Pipeline *pipeline) {

	if(!pipeline || !pipeline->device)
		return NULL;

	return (U8*)(pipeline + 1) + GraphicsDeviceRef_getObjectSizes(pipeline->device)->pipeline;
}

Error PipelineRef_dec(PipelineRef **pipeline) {
	return !RefPtr_dec(pipeline) ?
		Error_invalidOperation(0, "PipelineRef_dec():: pipeline invalid") : Error_none();
}

Error PipelineRef_inc(PipelineRef *pipeline) {
	return !RefPtr_inc(pipeline) ?
		Error_invalidOperation(0, "PipelineRef_inc()::pipeline invalid") : Error_none();
}

Bool Pipeline_free(Pipeline *pipeline, Allocator alloc) {

	Pipeline_freeExt(pipeline, alloc);

	//Log_debugLnx("Destroy: %p", pipeline);

	if (pipeline->type == EPipelineType_RaytracingExt) {
		PipelineRaytracingInfo *info = Pipeline_info(pipeline, PipelineRaytracingInfo);
		ListPipelineRaytracingGroup_freex(&info->groups);
		DeviceBufferRef_dec(&info->shaderBindingTable);
	}

	ListPipelineStage_freex(&pipeline->stages);

	if(!(pipeline->flags & EPipelineFlags_InternalWeakDeviceRef))
		GraphicsDeviceRef_dec(&pipeline->device);

	return true;
}

U32 GraphicsDeviceRef_getFirstShaderEntry(
	GraphicsDeviceRef *deviceRef,
	SHFile shaderBinary,
	CharString entrypointName,
	ListCharString uniforms,
	ESHExtension disallow,
	ESHExtension require
) {

	if(!deviceRef || deviceRef->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		return U32_MAX;

	for (U64 i = 0; i < shaderBinary.entries.length; ++i) {

		SHEntry entry = shaderBinary.entries.ptr[i];

		if(!CharString_equalsStringSensitive(entry.name, entrypointName))
			continue;

		Error err = Error_none();

		for (U64 j = 0; j < entry.binaryIds.length; ++j) {

			U16 binj = entry.binaryIds.ptr[j];
			SHBinaryInfo binInfo = shaderBinary.binaries.ptr[binj];

			ListCharString uniforms2 = binInfo.identifier.uniforms;

			//Find all uniforms

			if (uniforms2.length != uniforms.length)
				continue;

			Bool missing = false;

			for (U64 k = 0; k < uniforms.length / 2; ++k) {

				Bool contains = false;

				for (U64 l = 0; l < uniforms2.length / 2; ++l) {

					if (
						!CharString_equalsStringSensitive(uniforms.ptr[l << 1], uniforms2.ptr[l << 1]) ||
						!CharString_equalsStringSensitive(uniforms.ptr[(l << 1) | 1], uniforms2.ptr[(l << 1) | 1])
					)
						continue;

					contains = true;
					break;
				}

				if (!contains) {
					missing = true;
					break;
				}
			}

			if(missing)
				continue;

			//Ensure it's compatible

			if((binInfo.identifier.extensions & disallow) || (binInfo.identifier.extensions & require) != require)
				continue;
				
			if(!GraphicsDeviceRef_checkShaderFeatures(deviceRef, binInfo, entry, &err))
				continue;

			return (U16)i | ((U16)j << 16);
		}

		Error_printLnx(err);
		return U32_MAX;
	}

	return U32_MAX;
}

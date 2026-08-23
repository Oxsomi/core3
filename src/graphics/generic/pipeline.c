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

//graphics/generic/pipeline.c

#include "types/container/list_impl.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/device.h"
#include "graphics/generic/interface.h"
#include "platforms/logx.h"
#include "formats/oiSH/sh_file.h"
#include "types/base/string_read.h"

const C8 *EPipelineStage_names[] = {
	"vertex", "pixel", "compute", "geometry", "hull", "domain", "raygeneration", "callable", "miss", "closesthit",
	"anyhit", "intersection"
};

TListImpl(PipelineStage);
TListImpl(PipelineStatistic);
TListImpl(PipelineExecutable);

void *Pipeline_infoOffset(Pipeline *pipeline) {

	if(!pipeline || !pipeline->device)
		return NULL;

	//The info struct sits behind the backend's pipeline ext, so the ext's stride is what to skip.

	return (U8*)(pipeline + 1) + GraphicsObjectSize_stride(GraphicsDeviceRef_getObjectSizes(pipeline->device)->pipeline);
}

void Pipeline_free(void *pipelineGeneric, const Allocator *alloc) {

	Pipeline *pipeline = (Pipeline*) pipelineGeneric;

	Pipeline_freeExt(pipeline, alloc);

	if (pipeline->type == EPipelineType_RaytracingExt) {
		PipelineRaytracingInfo *info = Pipeline_info(pipeline, PipelineRaytracingInfo);
		ListPipelineRaytracingGroup_free(&info->groups, alloc);
		RefPtr_dec(&info->shaderBindingTable);
	}

	ListPipelineStage_free(&pipeline->stages, alloc);

	if(!(pipeline->flags & EPipelineFlags_InternalWeakDeviceRef)) {
		RefPtr_dec(&pipeline->layout);
		RefPtr_dec(&pipeline->device);
	}
}

void PipelineExecutable_free(PipelineExecutable *exec, const Allocator *alloc) {

	if(!exec)
		return;

	CharString_free(&exec->name, alloc);
	CharString_free(&exec->description, alloc);
	CharString_free(&exec->disassembly, alloc);

	for(U64 i = 0; i < exec->statistics.length; ++i) {
		CharString_free(&exec->statistics.ptrNonConst[i].name, alloc);
		CharString_free(&exec->statistics.ptrNonConst[i].description, alloc);
	}

	ListPipelineStatistic_free(&exec->statistics, alloc);
	*exec = (PipelineExecutable) { 0 };
}

void ListPipelineExecutable_freeUnderlying(ListPipelineExecutable *list, const Allocator *alloc) {

	if(!list)
		return;

	for(U64 i = 0; i < list->length; ++i)
		PipelineExecutable_free(&list->ptrNonConst[i], alloc);

	ListPipelineExecutable_free(list, alloc);
}

Bool GraphicsDeviceRef_getPipelineExecutables(
	PipelineRef *pipeline,
	const Allocator *alloc,
	ListPipelineExecutable *result,
	Error *e_rr
) {
	Bool s_uccess = true;

	if(!pipeline || !result)
		retError(clean, Error_nullPointer(
			!pipeline ? 0 : 2, "GraphicsDeviceRef_getPipelineExecutables()::pipeline and result are required"
		));

	if(result->ptr)
		retError(clean, Error_invalidParameter(
			2, 0, "GraphicsDeviceRef_getPipelineExecutables()::result isn't empty, may indicate memleak"
		));

	Pipeline *pipelinePtr = PipelineRef_ptr(pipeline);

	if(!(GraphicsDeviceRef_ptr(pipelinePtr->device)->info.capabilities.features2 & EGraphicsFeatures2_PipelineExecutableInfo))
		retError(clean, Error_unsupportedOperation(
			0, "GraphicsDeviceRef_getPipelineExecutables() device lacks EGraphicsFeatures2_PipelineExecutableInfo"
		));

	if(!(pipelinePtr->flags & EPipelineFlags_CaptureISA))
		retError(clean, Error_invalidState(
			0, "GraphicsDeviceRef_getPipelineExecutables() pipeline wasn't created with EPipelineFlags_CaptureISA"
		));

	gotoIfError3(clean, Pipeline_getExecutablesExt(pipelinePtr, alloc, result, e_rr));

clean:
	return s_uccess;
}

U32 GraphicsDeviceRef_getFirstShaderEntry(
	GraphicsDeviceRef *deviceRef,
	const SHFile *shaderBinary,
	const CharString *entrypointName,
	const ListCharString *defines,
	const ListCharString *uniforms,
	ESHExtension disallow,
	ESHExtension require
) {

	if(!deviceRef || !shaderBinary || deviceRef->refPtrType->typeId != (TypeId)EGraphicsTypeId_GraphicsDevice)
		return U32_MAX;

	for (U64 i = 0; i < shaderBinary->entries.length; ++i) {

		SHEntry entry = shaderBinary->entries.ptr[i];

		if(!CharString_equalsString(&entry.name, entrypointName, EStringCase_Sensitive))
			continue;

		for (U64 j = 0; j < entry.binaryIds.length; ++j) {

			U16 binj = entry.binaryIds.ptr[j];
			SHBinaryInfo binInfo = shaderBinary->binaries.ptr[binj];

			ListCharString defines2 = binInfo.identifier.defines;

			//Find all defines

			if (defines2.length != (!defines ? 0 : defines->length))
				continue;

			Bool missing = false;

			for (U64 k = 0; k < (defines ? defines->length / 2 : 0); ++k) {

				Bool contains = false;

				for (U64 l = 0; l < defines->length / 2; ++l) {

					if (
						!CharString_equalsString(&(defines->ptr[l << 1]), &(defines2.ptr[l << 1]), EStringCase_Sensitive) ||
						!CharString_equalsString(
							&(defines->ptr[(l << 1) | 1]),
							&(defines2.ptr[(l << 1) | 1]),
							EStringCase_Sensitive
						)
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

			//Match uniforms the same way as defines: the binary's uniform set must equal the requested set.
			//A uniform carries a typed value, so compare the requested value against the stored value stringified.

			ListSHUniformRuntime uniforms2 = binInfo.identifier.uniforms;

			if (uniforms2.length != (!uniforms ? 0 : uniforms->length / 2))
				continue;

			Bool uniformMissing = false;

			for (U64 k = 0; k < (uniforms ? uniforms->length / 2 : 0); ++k) {

				const CharString reqName  = uniforms->ptr[k << 1];
				const CharString reqValue = uniforms->ptr[(k << 1) | 1];

				Bool contains = false;

				for (U64 l = 0; l < uniforms2.length; ++l) {

					const SHUniformRuntime u = uniforms2.ptr[l];

					if (!CharString_equalsString(&u.name, &reqName, EStringCase_Sensitive))
						continue;

					const TypeId typeId = ETypeId_arr[u.typeIdShort];

					SHValue value = (SHValue) { 0 };
					Buffer_memcpy(
						Buffer_createRef(&value, sizeof(value)),
						Buffer_createRefConst(binInfo.identifier.uniformData.ptr + u.dataOffset, ETypeId_getBytes(typeId))
					);

					CharString valStr = CharString_createNull();

					if (SHValue_stringify(&value, typeId, GraphicsDeviceRef_getAlloc(deviceRef), &valStr, NULL))
						contains = CharString_equalsString(&valStr, &reqValue, EStringCase_Sensitive);

					CharString_free(&valStr, GraphicsDeviceRef_getAlloc(deviceRef));
					break;
				}

				if (!contains) {
					uniformMissing = true;
					break;
				}
			}

			if (uniformMissing)
				continue;

			//Ensure it's compatible

			if((binInfo.identifier.extensions & disallow) || (binInfo.identifier.extensions & require) != require)
				continue;

			Error err = Error_none();

			//A rejected candidate is the normal outcome of a search, not a failure:
			// an oiSH that carries an SER variant beside a baseline one always has the SER binary rejected on a device
			// without reordering.
			//Error_print logs the message at the caller's level but the stack trace unconditionally at Error,
			// so a lookup that goes on to succeed still reads as a crash; the reason goes out at Debug instead,
			// and the real failure, U32_MAX out of the whole loop, stays the caller's to report.

			if(!GraphicsDeviceRef_checkShaderFeatures(deviceRef, &binInfo, &entry, &err)) {

				Log_debugLn(
					GraphicsDeviceRef_getAlloc(deviceRef),
					"GraphicsDeviceRef_getFirstShaderEntry(): skipped entry %"PRIu64" binary %"PRIu64", "
					"the device is missing a feature it needs",
					i, j
				);

				continue;
			}

			return (U16)i | ((U16)j << 16);
		}

		return U32_MAX;
	}

	return U32_MAX;
}

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

//graphics/generic/pipeline_serialize.h

#pragma once
#include "graphics/generic/pipeline.h"
#include "formats/oiSP/sp_file.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Interop between a stored pipeline (oiSP) and a live one.
//oiSP holds no graphics types on purpose, so it stays loadable without a device; this is the layer that owns
// PipelineGraphicsInfo, so the lowering in both directions lives here.

//Lowers a stored pipeline into the create info a backend takes.
//Returns false when the pipeline isn't of that kind, so a caller can branch on SPPipelineBase.type instead.

//Compute has no state to lower, only the one stage it's allowed to bind, so its lowering yields that stage instead of
// a create info. It has no fromComputeInfo to pair with, because a compute pipeline records nothing beyond the shader
// it was derived from, which deriving already made exact.

Bool SPFile_toComputeStage(
	const SPFile *spFile,
	U32 pipelineId,
	const ListPipelineStage *stages,            //This pipeline's stages, already resolved against the oiSH files
	PipelineStage *stage,
	Error *e_rr
);

Bool SPFile_toGraphicsInfo(const SPFile *spFile, U32 pipelineId, PipelineGraphicsInfo *info, Error *e_rr);
Bool SPFile_toRaytracingInfo(const SPFile *spFile, U32 pipelineId, PipelineRaytracingInfo *info, Error *e_rr);

//Records a create info into an existing pipeline of the file, so what a renderer actually built can be stored.
//The pipeline has to have been derived already (that's where its stages and provenance come from); every field the
// info carries is written as Supplied, since it came from the caller rather than from a guess.

Bool SPFile_fromGraphicsInfo(SPFile *spFile, U32 pipelineId, const PipelineGraphicsInfo *info, Error *e_rr);
Bool SPFile_fromRaytracingInfo(SPFile *spFile, U32 pipelineId, const PipelineRaytracingInfo *info, Error *e_rr);

//Dumps a live pipeline into a file, deriving it from the shaders it was built from and then recording the state the
// pipeline was actually created with.
//shaderNames is optional and parallel to `files`: the name each oiSH is stored under, so the result can be resolved
// again after a load.

Bool Pipeline_toSPFile(
	PipelineRef *pipeline,
	const ListSHFile *files,
	const ListCharString *shaderNames,          //Optional, parallel to files
	CharString name,                            //Optional pipeline name
	const Allocator *alloc,
	SPFile *spFile,                             //Appended to; create it first
	U32 *pipelineId,
	Error *e_rr
);

//Creates a live pipeline from a stored one.
//The stages are resolved by name against `files`: a stage's shader file name has to match one of `shaderNames` and
// its entrypoint has to exist in that oiSH, so a pipeline stored on one run can be rebuilt on the next.
//A stage whose source hash no longer matches the oiSH it resolves to is reported rather than silently accepted.

Bool GraphicsDeviceRef_createPipelineFromSPFile(
	GraphicsDeviceRef *deviceRef,
	const SPFile *spFile,
	U32 pipelineId,
	const ListSHFile *files,
	const ListCharString *shaderNames,          //Parallel to files, used to resolve a stage's shader by name
	PipelineLayoutRef *layout,                  //NULL takes the device's default layout
	const Allocator *alloc,
	PipelineRef **pipelineRef,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif

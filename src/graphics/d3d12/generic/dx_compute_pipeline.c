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

//graphics/d3d12/generic/dx_compute_pipeline.c

#include "graphics/generic/pipeline.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/device.h"
#include "graphics/d3d12/dx_device.h"
#include "formats/oiSH/sh_file.h"
#include "types/container/string_unicode.h"
#include "types/container/list_basic_types.h"
#include "types/base/buffer_base.h"
#include "types/base/error.h"

Bool DX_WRAP_FUNC(GraphicsDevice_createPipelineCompute)(
	GraphicsDevice *device,
	const CharString *name,
	Pipeline *pipeline,
	const SHBinaryInfo *binary,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDevice_getAlloc(device);

	const DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);
	ListU16 tmp = (ListU16) { 0 };
	Buffer dxil = binary->binaries[ESHBinaryType_DXIL];

	//TODO: Push constants

	D3D12_COMPUTE_PIPELINE_STATE_DESC compute = (D3D12_COMPUTE_PIPELINE_STATE_DESC) {
		.pRootSignature = PipelineLayout_ext(PipelineLayoutRef_ptr(pipeline->layout), Dx)->rootSig,
		.CS = (D3D12_SHADER_BYTECODE) {
			.pShaderBytecode = dxil.ptr,
			.BytecodeLength = Buffer_length(dxil)
		}
	};

	ID3D12PipelineState **pipelinei = &Pipeline_ext(pipeline, Dx)->pso;

	gotoIfError3(clean, dxCheck(deviceExt->device->lpVtbl->CreateComputePipelineState(
		deviceExt->device,
		&compute,
		&IID_ID3D12PipelineState,
		(void**) pipelinei
	), e_rr));

	if((device->flags & EGraphicsDeviceFlags_IsDebug) && name && CharString_length(*name)) {
		gotoIfError3(clean, CharString_toUTF16(*name, alloc, &tmp, e_rr));
		gotoIfError3(clean, dxCheck((*pipelinei)->lpVtbl->SetName(*pipelinei, (const wchar_t*) tmp.ptr), e_rr));
	}

clean:
	ListU16_free(&tmp, alloc);
	return s_uccess;
}

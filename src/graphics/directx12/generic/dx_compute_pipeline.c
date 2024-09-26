/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "graphics/generic/pipeline.h"
#include "graphics/generic/device.h"
#include "graphics/generic/texture.h"
#include "graphics/directx12/dx_device.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "types/string.h"
#include "types/error.h"
#include "formats/oiSH.h"

Bool GraphicsDevice_createPipelineComputeExt(
	GraphicsDevice *device,
	CharString name,
	Pipeline *pipeline,
	SHBinaryInfo binary,
	Error *e_rr
) {

	const DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);
	Bool s_uccess = true;
	ListU16 tmp = (ListU16) { 0 };
	Buffer dxil = binary.binaries[ESHBinaryType_DXIL];

	//TODO: Push constants

	D3D12_COMPUTE_PIPELINE_STATE_DESC compute = (D3D12_COMPUTE_PIPELINE_STATE_DESC) {
		.pRootSignature = deviceExt->defaultLayout,
		.CS = (D3D12_SHADER_BYTECODE) {
			.pShaderBytecode = dxil.ptr,
			.BytecodeLength = Buffer_length(dxil)
		}
	};

	ID3D12PipelineState **pipelinei = &Pipeline_ext(pipeline, Dx)->pso;

	gotoIfError2(clean, dxCheck(deviceExt->device->lpVtbl->CreateComputePipelineState(
		deviceExt->device,
		&compute,
		&IID_ID3D12PipelineState,
		(void**) pipelinei
	)))

	if((device->flags & EGraphicsDeviceFlags_IsDebug) && CharString_length(name)) {
		gotoIfError2(clean, CharString_toUTF16x(name, &tmp))
		gotoIfError2(clean, dxCheck((*pipelinei)->lpVtbl->SetName(*pipelinei, (const wchar_t*) tmp.ptr)))
	}

clean:
	ListU16_freex(&tmp);
	return s_uccess;
}

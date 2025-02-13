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
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/d3d12/dx_device.h"
#include "platforms/ext/stringx.h"
#include "types/container/string.h"

Bool DX_WRAP_FUNC(DescriptorHeap_free)(DescriptorHeap *heap) {

	const DxDescriptorHeap *heapExt = DescriptorHeap_ext(heap, Dx);

	if(heapExt->samplerHeap.heap)
		heapExt->samplerHeap.heap->lpVtbl->Release(heapExt->samplerHeap.heap);

	if(heapExt->resourcesHeap.heap)
		heapExt->resourcesHeap.heap->lpVtbl->Release(heapExt->resourcesHeap.heap);

	return true;
}

Error DX_WRAP_FUNC(GraphicsDeviceRef_createDescriptorHeap)(GraphicsDeviceRef *dev, DescriptorHeap *heap, CharString name) {

	Error err = Error_none();

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	DxDescriptorHeap *heapExt = DescriptorHeap_ext(heap, Dx);

	const DescriptorHeapInfo info = heap->info;
	CharString tmpName = CharString_createNull();

	U32 srvCbvUav =
		(U32) info.maxAccelerationStructures + info.maxTextures + info.maxConstantBuffers +
		info.maxTexturesRW + info.maxBuffersRW;

	if (srvCbvUav) {

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = (D3D12_DESCRIPTOR_HEAP_DESC) {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			.NumDescriptors = srvCbvUav,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
		};

		if((device->flags & EGraphicsDeviceFlags_IsDebug) && CharString_length(name))
			gotoIfError(clean, CharString_formatx(&tmpName, "%.*s resources heap", (int) CharString_length(name), name.ptr))

		gotoIfError(clean, DxGraphicsDevice_createDescriptorHeapSingle(
			deviceExt, heapDesc, &tmpName, &heapExt->resourcesHeap, true
		))
	}

	if (info.maxSamplers) {

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = (D3D12_DESCRIPTOR_HEAP_DESC) {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
			.NumDescriptors = info.maxSamplers,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
		};

		if((device->flags & EGraphicsDeviceFlags_IsDebug) && CharString_length(name))
			gotoIfError(clean, CharString_formatx(&tmpName, "%.*s sampler heap", (int) CharString_length(name), name.ptr))

		gotoIfError(clean, DxGraphicsDevice_createDescriptorHeapSingle(
			deviceExt, heapDesc, &tmpName, &heapExt->samplerHeap, true
		))
	}

clean:
	CharString_freex(&tmpName);
	return err;
}

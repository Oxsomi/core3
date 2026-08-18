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

//graphics/d3d12/generic/dx_opacity_micromap.c

#include "graphics/generic/opacity_micromap.h"
#include "graphics/generic/device.h"
#include "graphics/d3d12/direct3d12.h"
#include "graphics/d3d12/dx_device.h"
#include "graphics/d3d12/dx_interface.h"
#include "types/container/list_impl.h"
#include "types/container/string.h"
#include "types/base/error.h"

TListNamedImpl(ListDxOMMHistogram);

Bool DX_WRAP_FUNC(OpacityMicromap_init)(OpacityMicromap *micromap, Error *e_rr) {

	Bool s_uccess = true;

	const Allocator *alloc = GraphicsDeviceRef_getAlloc(micromap->base.device);
	DxOpacityMicromap *micromapExt = OpacityMicromap_ext(micromap, Dx);

	//On D3D12 an OMM array IS an acceleration structure: same build call, same prebuild info call, so the
	// histogram is prepared here in the shape the build will hand over.

	gotoIfError3(clean, ListDxOMMHistogram_resize(
		&micromapExt->histogram, micromap->usages.length, alloc, e_rr
	));

	for (U64 i = 0; i < micromap->usages.length; ++i) {

		const OpacityMicromapUsage usage = micromap->usages.ptr[i];

		D3D12_RAYTRACING_OPACITY_MICROMAP_FORMAT format = D3D12_RAYTRACING_OPACITY_MICROMAP_FORMAT_OC1_2_STATE;

		switch ((EOpacityMicromapFormat) usage.format) {

			case EOpacityMicromapFormat_Opacity2State:
				format = D3D12_RAYTRACING_OPACITY_MICROMAP_FORMAT_OC1_2_STATE;
				break;

			case EOpacityMicromapFormat_Opacity4State:
				format = D3D12_RAYTRACING_OPACITY_MICROMAP_FORMAT_OC1_4_STATE;
				break;

			default:
				retError(clean, Error_unsupportedOperation(0, "D3D12OpacityMicromap_init() unsupported format"));
		}

		micromapExt->histogram.ptrNonConst[i] = (D3D12_RAYTRACING_OPACITY_MICROMAP_HISTOGRAM_ENTRY) {
			.Count = usage.count,
			.SubdivisionLevel = usage.subdivisionLevel,
			.Format = format
		};
	}

	micromapExt->array = (D3D12_RAYTRACING_OPACITY_MICROMAP_ARRAY_DESC) {
		.NumOmmHistogramEntries = (U32) micromapExt->histogram.length,
		.pOmmHistogram = micromapExt->histogram.ptr
	};

clean:
	return s_uccess;
}

void DX_WRAP_FUNC(OpacityMicromap_free)(OpacityMicromap *micromap) {

	const Allocator *alloc = GraphicsDeviceRef_getAlloc(micromap->base.device);
	DxOpacityMicromap *micromapExt = OpacityMicromap_ext(micromap, Dx);

	ListDxOMMHistogram_free(&micromapExt->histogram, alloc);
}

Bool DX_WRAP_FUNC(OpacityMicromapRef_flush)(
	void *commandBufferExt, GraphicsDeviceRef *deviceRef, OpacityMicromapRef *pending, Error *e_rr
) {

	Bool s_uccess = true;

	(void) commandBufferExt; (void) deviceRef; (void) pending;

	retError(clean, Error_unsupportedOperation(
		0, "D3D12OpacityMicromapRef_flush() building an opacity micromap isn't implemented yet"
	));

clean:
	return s_uccess;
}

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
#include "graphics/generic/texture.h"
#include "graphics/directx12/dx_device.h"
#include "platforms/ext/bufferx.h"

Bool DX_WRAP_FUNC(Pipeline_free)(Pipeline *pipeline, Allocator allocator) {

	(void)allocator;

	if(!pipeline)
		return true;

	const DxPipeline *dxPipeline = Pipeline_ext(pipeline, Dx);

	if(!dxPipeline->pso)
		return true;

	if(pipeline->type == EPipelineType_RaytracingExt) {

		if(dxPipeline->stateObjectProps)
			dxPipeline->stateObjectProps->lpVtbl->Release(dxPipeline->stateObjectProps);

		dxPipeline->stateObject->lpVtbl->Release(dxPipeline->stateObject);
	}

	else dxPipeline->pso->lpVtbl->Release(dxPipeline->pso);

	return true;
}

/* OxC3/RT Core(Oxsomi core 3/RT Core), a general framework for raytracing applications.
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

#include "resource_bindings.hlsli"

[shader("compute")]
[numthreads(256, 1, 1)]
void main(U32 i : SV_DispatchThreadID) {

	if(i >= 2)			//As a test, we always draw 2 objects from the same buffer
		return;

	//View projection matrices

	if(i == 0) {

		F32x3 camPos = F32x3(0, 0, 2);

		F32 aspect = 1;

		if(_swapchainCount) {

			U32 readSwapchain = getReadSwapchain(0);

			if(readSwapchain != U32_MAX) {

				U32x3 dims;
				texture2DUniform(readSwapchain).GetDimensions(0, dims.x, dims.y, dims.z);

				aspect = (float)dims.x / dims.y;
			}
		}

		U32 orientation = getAppData1u(EResourceBinding_Orientation);

		F32x4x4 v = F32x4x4_lookAt(camPos, 0.xxx, F32x3(0, 1, 0));
		v = mul(v, F32x4x4_rotateZ(orientation * F32_degToRad));

		F32x4x4 p = F32x4x4_perspective(120 * F32_degToRad, aspect, 0.1, 100);

		ViewProjMatrices vp;
		vp.view = v;
		vp.proj = p;
		vp.viewProj = mul(v, p);

		vp.viewInv = inverseSlow(vp.view);
		vp.projInv = inverseSlow(vp.proj);
		vp.viewProjInv = inverseSlow(vp.viewProj);

		U32 viewProjMatRW = getAppData1u(EResourceBinding_ViewProjMatricesRW);
		setAtUniform<ViewProjMatrices>(viewProjMatRW, 0, vp);
	}

	//Indirect dispatch

	if (i == 0) {
		U32 resourceId = getAppData1u(EResourceBinding_IndirectDispatchRW);
		setAtUniform(resourceId, 0, I32x3(1, 1, 1));
	}

	//Indirect draws

	{
		U32 resourceId = getAppData1u(EResourceBinding_IndirectDrawRW);

		IndirectDrawIndexed indirectDraw = (IndirectDrawIndexed) 0;
		indirectDraw.instanceCount = 1;

		switch (i) {

			case 0:
				indirectDraw.indexCount = 3;
				indirectDraw.indexOffset = 6;
				break;

			default:
				indirectDraw.indexCount = 6;
				indirectDraw.indexOffset = 9;
				break;
		}

		setAtUniform(resourceId, i * sizeof(IndirectDrawIndexed), indirectDraw);
	}
}

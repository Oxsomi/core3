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

#include "@resources.hlsli"
#include "ray_basics.hlsli"

//Generating camera rays using a vInv and vpInv

struct Camera {

	F32x4x4 v, p, vp;
	F32x4x4 vInv, pInv, vpInv;

	RayDesc getRay(U32x2 id, U32x2 dims) {

		//Generate primaries

		F32x2 uv = (F32x2(id) + 0.5) / F32x2(dims);
		uv.y = 1 - uv.y;

		F32x3 eye = mul(F32x4(0.xxx, 1), vInv).xyz;

		F32x3 rayDest = mul(F32x4(uv * 2 - 1, 1, 1), vpInv).xyz;
		F32x3 rayDir = normalize(rayDest - eye);

		return createRay(eye, 0, rayDir, 1e6);		//1e6 limit is to please NV drivers
	}
};

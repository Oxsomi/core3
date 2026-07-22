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

[[oxc::extension("I64")]]
[[oxc::extension("F64")]]
[shader("compute")]
[numthreads(256, 1, 1)]
void main(U32 i : SV_DispatchThreadID) {

	U32 objectTransformGlobal = getAppData1u(0);
	U32 objectTransformLocal = getAppData1u(1);

	U32 bytes = bufferBytesUniform(objectTransformGlobal);
	U32 loops = bytes > 0 ? 16 : (bytes > 1 ? 64 : (bytes > 2 ? 128 : 4096));

	TransformImprecise result;

	#ifdef __OXC_EXT_F64

		U32 elems = bytes / sizeof(TransformPreciseDouble);

		if(i + 1 >= elems)
			return;
			
		TransformPreciseDouble cam = getAtUniform<TransformPreciseDouble>(objectTransformGlobal, 0);

		[loop]
		for(uint j = 0; j < loops; ++j) {

			TransformPreciseDouble obj = getAtUniform<TransformPreciseDouble>(objectTransformGlobal, ((i + j) % (elems - 1)) + 1);
			result.pos = (float3)(obj.pos - cam.pos);

			setAtUniform(objectTransformLocal, i + 1, result);
		}

	#else

		U32 elems = bytes /  sizeof(TransformPreciseFixed);

		if(i + 1 >= elems)
			return;
			
		TransformPreciseFixed cam = getAtUniform<TransformPreciseFixed>(objectTransformGlobal, 0);
		U64x3 unpackedCam = cam.pos; //fixedPointUnpack(cam.pos);

		[loop]
		for(uint j = 0; j < loops; ++j) {

			TransformPreciseFixed obj = getAtUniform<TransformPreciseFixed>(objectTransformGlobal, ((i + j) % (elems - 1)) + 1);
			U64x3 unpackedObj = obj.pos; //fixedPointUnpack(obj.pos);

			result.pos = fixedPointToFloat(fixedPointSub(unpackedObj, unpackedCam));
			setAtUniform(objectTransformLocal, i + 1, result);
		}

	#endif
}

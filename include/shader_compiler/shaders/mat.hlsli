R"(
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

//shader_compiler/shaders/mat.hlsli
//
//F32x4x4 transforms, projections and the 4x4 inverse. Mirrors types/math/mat.h on the CPU side;
//the conventions (row major, row vectors, left handed, [0,1] depth) are documented there.

#pragma once
#include "@types.hlsli"

//F32x4x4 helpers such as transforms, perspective, etc.
//https://github.com/Oxsomi/core2/blob/master/include/types/mat.hpp
//and https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixperspectivefovlh

F32x4x4 F32x4x4_scale(F32x4 scale) {
	return F32x4x4(
		scale.x, 0, 0, 0,
		0, scale.y, 0, 0,
		0, 0, scale.z, 0,
		0, 0, 0, scale.w
	);
}

F32x4x4 F32x4x4_scale(F32 x, F32 y, F32 z, F32 w = 1) { return F32x4x4_scale(F32x4(x, y, z, w)); }

F32x4x4 F32x4x4_translate(F32x3 translate) {
	return F32x4x4(
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		translate.x, translate.y, translate.z, 1
	);
}

F32x4x4 F32x4x4_translate(F32 x, F32 y, F32 z) { return F32x4x4_translate(F32x3(x, y, z)); }

F32x4x4 F32x4x4_rotateX(F32 rad) {
	F32x4x4 res = F32x4x4_scale(1.xxxx);
	res[1][1] = cos(rad);	res[2][1] = -sin(rad);
	res[1][2] = sin(rad);	res[2][2] = cos(rad);
	return res;
}

F32x4x4 F32x4x4_rotateY(F32 rad) {
	F32x4x4 res = F32x4x4_scale(1.xxxx);
	res[0][0] = cos(rad);	res[0][2] = -sin(rad);
	res[2][0] = sin(rad);	res[2][2] = cos(rad);
	return res;
}

F32x4x4 F32x4x4_rotateZ(F32 rad) {
	//Was transposed relative to rotateX/rotateY: those send Y->+Z and Z->+X, so Z has to send X->+Y to
	//complete the cycle, and it was sending it to -Y. Matches F32x4x4_rotateZ in types/math/mat.h.
	F32x4x4 res = F32x4x4_scale(1.xxxx);
	res[0][0] = cos(rad);	res[0][1] = sin(rad);
	res[1][0] = -sin(rad);	res[1][1] = cos(rad);
	return res;
}

F32x4x4 F32x4x4_rotate(F32x3 rotate) {
	return mul(F32x4x4_rotateZ(rotate.z), mul(F32x4x4_rotateY(rotate.y), F32x4x4_rotateX(rotate.x)));
}

F32x4x4 F32x4x4_rotate(F32 x, F32 y, F32 z) { return F32x4x4_rotate(F32x3(x, y, z)); }

F32x4x4 F32x4x4_transform(F32x3 position, F32x3 rotation, F32x3 scale) {
	return mul(F32x4x4_scale(F32x4(scale, 1)), mul(F32x4x4_rotate(rotation), F32x4x4_translate(-position)));
}

F32x4x4 F32x4x4_view(F32x3 position, F32x3 rotation) {
	return mul(F32x4x4_rotate(rotation), F32x4x4_translate(-position));
}

F32x4x4 F32x4x4_perspective(F32 fovYRad, F32 aspect, F32 near, F32 far) {
	F32 scale = 1 / tan(fovYRad / 2);
	return F32x4x4(
		scale / aspect,		0,		0,								0,
		0,					scale,	0,								0,
		0,					0,		far / (far - near),				1,
		0,					0,		-near * far / (far - near),		0
	);
}

F32x4x4 F32x4x4_construct(F32x3 x, F32x3 y, F32x3 z, F32x3 eye) {
	//The last row is the eye projected onto the basis. It used to read dot(x, eye.x), which is a dot
	//against the scalar eye.x broadcast to a float3 - never what was meant. Matches types/math/mat.h.
	return F32x4x4(
		x.x,				y.x,				z.x,				0,
		x.y,				y.y,				z.y,				0,
		x.z,				y.z,				z.z,				0,
		-dot(x, eye),		-dot(y, eye),		-dot(z, eye),		1
	);
}

F32x4x4 F32x4x4_lookDir(F32x3 eye, F32x3 direction, F32x3 up) {
	F32x3 z = normalize(direction);
	//cross(up, z), not cross(z, up): the latter is the mirrored basis, and disagreed with lookAt below
	F32x3 x = normalize(cross(normalize(up), z));
	F32x3 y = cross(z, x);
	return F32x4x4_construct(x, y, z, eye);
}

F32x4x4 F32x4x4_lookAt(F32x3 eye, F32x3 center, F32x3 up) {
	F32x3 z = normalize(center - eye);
	F32x3 x = normalize(cross(normalize(up), z));
	F32x3 y = cross(z, x);
	return F32x4x4_construct(x, y, z, eye);
}

//Quick conversion

static const F32 F32_pi = 3.1415926535;
static const F32 F32_degToRad = F32_pi / 180;
static const F32 F32_radToDeg = 180 / F32_pi;

//Inverting a 4x4 matrix
//https://gist.github.com/mattatz/86fff4b32d198d0928d0fa4ff32cf6fa

F32x4x4 inverseSlow(F32x4x4 m) {

	F32 n11 = m[0][0], n12 = m[1][0], n13 = m[2][0], n14 = m[3][0];
	F32 n21 = m[0][1], n22 = m[1][1], n23 = m[2][1], n24 = m[3][1];
	F32 n31 = m[0][2], n32 = m[1][2], n33 = m[2][2], n34 = m[3][2];
	F32 n41 = m[0][3], n42 = m[1][3], n43 = m[2][3], n44 = m[3][3];

	F32 t11 = n23 * n34 * n42 - n24 * n33 * n42 + n24 * n32 * n43 - n22 * n34 * n43 - n23 * n32 * n44 + n22 * n33 * n44;
	F32 t12 = n14 * n33 * n42 - n13 * n34 * n42 - n14 * n32 * n43 + n12 * n34 * n43 + n13 * n32 * n44 - n12 * n33 * n44;
	F32 t13 = n13 * n24 * n42 - n14 * n23 * n42 + n14 * n22 * n43 - n12 * n24 * n43 - n13 * n22 * n44 + n12 * n23 * n44;
	F32 t14 = n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 + n12 * n24 * n33 + n13 * n22 * n34 - n12 * n23 * n34;

	F32 det = n11 * t11 + n21 * t12 + n31 * t13 + n41 * t14;
	F32 idet = 1 / det;

	F32x4x4 ret;

	ret[0][0] = t11;
	ret[0][1] = n24 * n33 * n41 - n23 * n34 * n41 - n24 * n31 * n43 + n21 * n34 * n43 + n23 * n31 * n44 - n21 * n33 * n44;
	ret[0][2] = n22 * n34 * n41 - n24 * n32 * n41 + n24 * n31 * n42 - n21 * n34 * n42 - n22 * n31 * n44 + n21 * n32 * n44;
	ret[0][3] = n23 * n32 * n41 - n22 * n33 * n41 - n23 * n31 * n42 + n21 * n33 * n42 + n22 * n31 * n43 - n21 * n32 * n43;

	ret[1][0] = t12;
	ret[1][1] = n13 * n34 * n41 - n14 * n33 * n41 + n14 * n31 * n43 - n11 * n34 * n43 - n13 * n31 * n44 + n11 * n33 * n44;
	ret[1][2] = n14 * n32 * n41 - n12 * n34 * n41 - n14 * n31 * n42 + n11 * n34 * n42 + n12 * n31 * n44 - n11 * n32 * n44;
	ret[1][3] = n12 * n33 * n41 - n13 * n32 * n41 + n13 * n31 * n42 - n11 * n33 * n42 - n12 * n31 * n43 + n11 * n32 * n43;

	ret[2][0] = t13;
	ret[2][1] = n14 * n23 * n41 - n13 * n24 * n41 - n14 * n21 * n43 + n11 * n24 * n43 + n13 * n21 * n44 - n11 * n23 * n44;
	ret[2][2] = n12 * n24 * n41 - n14 * n22 * n41 + n14 * n21 * n42 - n11 * n24 * n42 - n12 * n21 * n44 + n11 * n22 * n44;
	ret[2][3] = n13 * n22 * n41 - n12 * n23 * n41 - n13 * n21 * n42 + n11 * n23 * n42 + n12 * n21 * n43 - n11 * n22 * n43;

	ret[3][0] = t14;
	ret[3][1] = n13 * n24 * n31 - n14 * n23 * n31 + n14 * n21 * n33 - n11 * n24 * n33 - n13 * n21 * n34 + n11 * n23 * n34;
	ret[3][2] = n14 * n22 * n31 - n12 * n24 * n31 - n14 * n21 * n32 + n11 * n24 * n32 + n12 * n21 * n34 - n11 * n22 * n34;
	ret[3][3] = n12 * n23 * n31 - n13 * n22 * n31 + n13 * n21 * n32 - n11 * n23 * n32 - n12 * n21 * n33 + n11 * n22 * n33;

	[unroll]
	for(U32 i = 0; i < 4; ++i)
		ret[i] *= idet.xxxx;

	return ret;
}

)"

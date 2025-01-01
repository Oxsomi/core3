R"(
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

#pragma once

#ifdef __OXC_EXT_16BITTYPES

	typedef float16_t F16;
	typedef float16_t2 F16x2;
	typedef float16_t3 F16x3;
	typedef float16_t4 F16x4;

	//Float16 matrices

	typedef float16_t4x4 F16x4x4;
	typedef float16_t3x4 F16x3x4;
	typedef float16_t2x4 F16x2x4;

	typedef float16_t4x3 F16x4x3;
	typedef float16_t3x3 F16x3x3;
	typedef float16_t2x3 F16x2x3;

	typedef float16_t4x2 F16x4x2;
	typedef float16_t3x2 F16x3x2;
	typedef float16_t2x2 F16x2x2;

	typedef int16_t I16;
	typedef int16_t2 I16x2;
	typedef int16_t3 I16x3;
	typedef int16_t4 I16x4;

	typedef uint16_t U16;
	typedef uint16_t2 U16x2;
	typedef uint16_t3 U16x3;
	typedef uint16_t4 U16x4;

	//Int16 matrices

	typedef int16_t4x4 I16x4x4;
	typedef int16_t3x4 I16x3x4;
	typedef int16_t2x4 I16x2x4;

	typedef int16_t4x3 I16x4x3;
	typedef int16_t3x3 I16x3x3;
	typedef int16_t2x3 I16x2x3;

	typedef int16_t4x2 I16x4x2;
	typedef int16_t3x2 I16x3x2;
	typedef int16_t2x2 I16x2x2;

	//Uint16 matrices

	typedef uint16_t4x4 U16x4x4;
	typedef uint16_t3x4 U16x3x4;
	typedef uint16_t2x4 U16x2x4;

	typedef uint16_t4x3 U16x4x3;
	typedef uint16_t3x3 U16x3x3;
	typedef uint16_t2x3 U16x2x3;

	typedef uint16_t4x2 U16x4x2;
	typedef uint16_t3x2 U16x3x2;
	typedef uint16_t2x2 U16x2x2;

#endif

#ifdef __OXC_EXT_F64

	typedef double F64;
	typedef double2 F64x2;
	typedef double3 F64x3;
	typedef double4 F64x4;

	//Float64 matrices

	typedef float64_t4x4 F64x4x4;
	typedef float64_t3x4 F64x3x4;
	typedef float64_t2x4 F64x2x4;

	typedef float64_t4x3 F64x4x3;
	typedef float64_t3x3 F64x3x3;
	typedef float64_t2x3 F64x2x3;

	typedef float64_t4x2 F64x4x2;
	typedef float64_t3x2 F64x3x2;
	typedef float64_t2x2 F64x2x2;

#endif

typedef uint U32;
typedef float F32;
typedef int I32;

typedef uint2 U32x2;
typedef float2 F32x2;
typedef int2 I32x2;

typedef uint3 U32x3;
typedef float3 F32x3;
typedef int3 I32x3;

typedef uint4 U32x4;
typedef float4 F32x4;
typedef int4 I32x4;

//Matrices

//Float matrices

typedef float4x4 F32x4x4;
typedef float3x4 F32x3x4;
typedef float2x4 F32x2x4;

typedef float4x3 F32x4x3;
typedef float3x3 F32x3x3;
typedef float2x3 F32x2x3;

typedef float4x2 F32x4x2;
typedef float3x2 F32x3x2;
typedef float2x2 F32x2x2;

//Int matrices

typedef int4x4 I32x4x4;
typedef int3x4 I32x3x4;
typedef int2x4 I32x2x4;

typedef int4x3 I32x4x3;
typedef int3x3 I32x3x3;
typedef int2x3 I32x2x3;

typedef int4x2 I32x4x2;
typedef int3x2 I32x3x2;
typedef int2x2 I32x2x2;

//Uint matrices

typedef uint4x4 U32x4x4;
typedef uint3x4 U32x3x4;
typedef uint2x4 U32x2x4;

typedef uint4x3 U32x4x3;
typedef uint3x3 U32x3x3;
typedef uint2x3 U32x2x3;

typedef uint4x2 U32x4x2;
typedef uint3x2 U32x3x2;
typedef uint2x2 U32x2x2;

#ifdef __OXC_EXT_I64

	typedef uint64_t U64;
	typedef uint64_t2 U64x2;
	typedef uint64_t3 U64x3;
	typedef uint64_t4 U64x4;

	typedef int64_t I64;
	typedef int64_t2 I64x2;
	typedef int64_t3 I64x3;
	typedef int64_t4 I64x4;

	//Uint64 matrices

	typedef uint64_t4x4 U64x4x4;
	typedef uint64_t3x4 U64x3x4;
	typedef uint64_t2x4 U64x2x4;

	typedef uint64_t4x3 U64x4x3;
	typedef uint64_t3x3 U64x3x3;
	typedef uint64_t2x3 U64x2x3;

	typedef uint64_t4x2 U64x4x2;
	typedef uint64_t3x2 U64x3x2;
	typedef uint64_t2x2 U64x2x2;

	//Int64 matrices

	typedef int64_t4x4 I64x4x4;
	typedef int64_t3x4 I64x3x4;
	typedef int64_t2x4 I64x2x4;

	typedef int64_t4x3 I64x4x3;
	typedef int64_t3x3 I64x3x3;
	typedef int64_t2x3 I64x2x3;

	typedef int64_t4x2 I64x4x2;
	typedef int64_t3x2 I64x3x2;
	typedef int64_t2x2 I64x2x2;

#endif

//Bool

typedef bool Bool;

typedef bool2 Boolx2;
typedef bool3 Boolx3;
typedef bool4 Boolx4;

//Binding graphics shader outputs (e.g. uv : _bind(0))

#define _bind(x) TEXCOORD##x
#define _flat nointerpolation

//Indirect draws

struct IndirectDraw {
	U32 vertexCount, instanceCount, vertexOffset, instanceOffset;
};

struct IndirectDrawIndexed {

	U32 indexCount, instanceCount, indexOffset;
	I32 vertexOffset;

	U32 instanceOffset;
	U32 padding[3];			//For alignment
};

struct IndirectDispatch {
	U32 x, y, z, pad;
};

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
	F32x4x4 res = F32x4x4_scale(1.xxxx);
	res[0][0] = cos(rad);	res[0][1] = -sin(rad);
	res[1][0] = sin(rad);	res[1][1] = cos(rad);
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
	return F32x4x4(
		x.x,				y.x,				z.x,				0,
		x.y,				y.y,				z.y,				0,
		x.z,				y.z,				z.z,				0,
		-dot(x, eye.x),		-dot(y, eye.y),		-dot(z, eye.z),		1
	);
}

F32x4x4 F32x4x4_lookDir(F32x3 eye, F32x3 direction, F32x3 up) {
	F32x3 z = normalize(direction);
	F32x3 x = normalize(cross(z, normalize(up)));
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

U32 U32_fromF32(F32 f) { return asuint(f); }
U32x2 U32x2_fromF32x2(F32x2 f) { return asuint(f); }
U32x3 U32x3_fromF32x3(F32x3 f) { return asuint(f); }
U32x4 U32x4_fromF32x4(F32x4 f) { return asuint(f); }

F32 F32_fromU32(U32 u) { return asfloat(u); }
F32x2 F32x2_fromU32x2(U32x2 u) { return asfloat(u); }
F32x3 F32x3_fromU32x3(U32x3 u) { return asfloat(u); }
F32x4 F32x4_fromU32x4(U32x4 u) { return asfloat(u); }

//Fixed point math

#ifdef __OXC_EXT_I64

	U64x3 fixedPointUnpack(U32x4 packed) {
		U32x3 hi = (packed.www >> U32x3(0, 10, 20)) & ((1 << 10) - 1);
		U64x3 hiShift = (U64x3) hi << 32;
		return hiShift | packed.xyz;
	}

	U32x4 fixedPointPack(U64x3 unpacked) {
	
		U32x3 lo = (U32x3) unpacked;

		unpacked >>= 32;
		U32x3 hi = ((U32x3) unpacked) << U32x3(0, 10, 20);

		return U32x4(lo, dot(hi, 1.xxx));
	}

	U64x3 fixedPointSub(U64x3 a, U64x3 b) { return a - b; }
	U64x3 fixedPointAdd(U64x3 a, U64x3 b) { return a + b; }

	static const U32 fixedPointFraction = 4;		//1/16th cm
	static const U32 fixedPointShift = 41;			//42 bits (sign is excluded here)

	static const F32 fixedPointMultiplier = 1.f / (1 << fixedPointFraction);
	static const U64 fixedPointMask = ((U64)1 << (fixedPointShift + 1)) - 1;

	F32x3 fixedPointToFloat(U64x3 value) {

		Boolx3 sign = (Boolx3)(value >> fixedPointShift);
		U64x3 correctedForSign = value ^ fixedPointMask.xxx;
		++correctedForSign;

		value = select(sign, correctedForSign, value);
		return (F32x3)value * select(sign, -fixedPointMultiplier, fixedPointMultiplier);
	}

	U64x3 fixedPointFromFloat(F32x3 v) {

		Boolx3 sign = v < 0;
		v = abs(v);

		v *= (1 << fixedPointFraction).xxx;

		U64x3 res = (U64x3) v;
		U64x3 correctedForSign = (res - 1) ^ fixedPointMask.xxx;
		return select(sign, correctedForSign, res);
	}

#endif

)"

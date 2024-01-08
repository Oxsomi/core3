#pragma once

#ifdef _ENABLE_F16

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

#endif

#ifdef _ENABLE_IU16

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

#ifdef _ENABLE_F64

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
typedef uint64_t U64;

typedef float F32;
typedef int I32;
typedef int64_t I64;

typedef uint2 U32x2;
typedef uint64_t2 U64x2;

typedef float2 F32x2;
typedef int2 I32x2;
typedef int64_t2 I64x2;

typedef uint3 U32x3;
typedef uint64_t3 U64x3;

typedef float3 F32x3;
typedef int3 I32x3;
typedef int64_t3 I64x3;

typedef uint4 U32x4;
typedef uint64_t4 U64x4;

typedef float4 F32x4;
typedef int4 I32x4;
typedef int64_t4 I64x4;

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

//Bool

typedef bool Bool;

//Binding graphics shader outputs (e.g. uv : _bind(0))

#define _bind(x) TEXCOORD##x

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
		1, 0, 0, translate.x,
		0, 1, 0, translate.y,
		0, 0, 1, translate.z,
		0, 0, 0, 1
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
	return F32x4x4_rotateX(rotate.x) * F32x4x4_rotateY(rotate.y) * F32x4x4_rotateZ(rotate.z);
}

F32x4x4 F32x4x4_rotate(F32 x, F32 y, F32 z) { return F32x4x4_rotate(F32x3(x, y, z)); }

F32x4x4 F32x4x4_transform(F32x3 position, F32x3 rotation, F32x3 scale) {
	return F32x4x4_translate(-position) * F32x4x4_rotate(rotation) * F32x4x4_scale(F32x4(scale, 1));
}

F32x4x4 F32x4x4_view(F32x3 position, F32x3 rotation) {
	return F32x4x4_translate(-position) * F32x4x4_rotate(rotation);
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

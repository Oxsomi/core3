#pragma once

#ifdef _ENABLE_F16
    typedef float16_t F16;
    typedef float16_t2 F16x2;
    typedef float16_t3 F16x3;
    typedef float16_t4 F16x4;
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

#endif

#ifdef _ENABLE_F64
    typedef double F64;
    typedef double2 F64x2;
    typedef double3 F64x3;
    typedef double4 F64x4;
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

typedef bool Bool;

#define _globalId SV_DispatchThreadID

#include "math/math.h"

const F32 F32_e		= 2.718281828459045f;
const F32 F32_pi		= 3.141592653589793f;
const F32 F32_radToDeg = 57.2957795131f;
const F32 F32_degToRad = 0.01745329251f;

const U64 Ki			= 1 << 10;
const U64 Mi			= 1 << 20;
const U64 Gi			= 1 << 30;
const U64 Ti			= 1 << 30;
const U64 Pi			= (U64)1 << 40;

const U64 K				= 1'000;
const U64 M				= 1'000'000;
const U64 B				= 1'000'000'000;
const U64 T				= 1'000'000'000'000;
const U64 P				= 1'000'000'000'000'000;

const Ns mus			= 1'000;
const Ns ms				= 1'000'000;
const Ns seconds		= 1'000'000'000;
const Ns mins			= 60'000'000'000;
const Ns hours			= 3'600'000'000'000;
const Ns days			= 86'400'000'000'000;
const Ns weeks			= 604'800'000'000'000;

const U8 U8_MIN			= 0;
const U16 U16_MIN		= 0;
const U32 U32_MIN		= 0;
const U64 U64_MIN		= 0;

const I8  I8_MIN		= 0x80;
const I16 I16_MIN		= 0x8000;
const I32 I32_MIN		= 0x80000000;
const I64 I64_MIN		= 0x8000000000000000;

const U8  U8_MAX		= 0xFF;
const U16 U16_MAX		= 0xFFFF;
const U32 U32_MAX		= 0xFFFFFFFF;
const U64 U64_MAX		= 0xFFFFFFFFFFFFFFFF;

const I8  I8_MAX		= 0x7F;
const I16 I16_MAX		= 0x7FFF;
const I32 I32_MAX		= 0x7FFFFFFF;
const I64 I64_MAX		= 0x7FFFFFFFFFFFFFFF;

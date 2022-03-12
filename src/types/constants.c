#include "math_helper.h"

const f32 Math_e		= 2.718281828459045f;
const f32 Math_pi		= 3.141592653589793f;
const f32 Math_radToDeg = 57.2957795131f;
const f32 Math_degToRad = 0.01745329251f;

const usz Ki			= 1 << 10;
const usz Mi			= 1 << 20;
const usz Gi			= 1 << 30;
const usz Ti			= 1 << 30;
const usz Pi			= (usz)1 << 40;

const usz K				= 1'000;
const usz M				= 1'000'000;
const usz B				= 1'000'000'000;
const usz T				= 1'000'000'000'000;

const ns mus			= 1'000;
const ns ms				= 1'000'000;
const ns seconds		= 1'000'000'000;
const ns mins			= 60'000'000'000;
const ns hours			= 3'600'000'000'000;
const ns days			= 86'400'000'000'000;
const ns wweeks			= 604'800'000'000'000;

const u8 u8_MIN			= 0;
const u16 u16_MIN		= 0;
const u32 u32_MIN		= 0;
const u64 u64_MIN		= 0;
const usz usz_MIN		= 0;

const usz sz_BYTES		= sizeof(usz);
const usz sz_BITS		= sizeof(usz) * 8;

const i8  i8_MIN		= 0x80;
const i16 i16_MIN		= 0x8000;
const i32 i32_MIN		= 0x80000000;
const i64 i64_MIN		= 0x8000000000000000;
const isz isz_MIN		= (isz)((usz)1 << (sizeof(usz) * 8 - 1));

const u8  u8_MAX		= 0xFF;
const u16 u16_MAX		= 0xFFFF;
const u32 u32_MAX		= 0xFFFFFFFF;
const u64 u64_MAX		= 0xFFFFFFFFFFFFFFFF;

const i8  i8_MAX		= 0x7F;
const i16 i16_MAX		= 0x7FFF;
const i32 i32_MAX		= 0x7FFFFFFF;
const i64 i64_MAX		= 0x7FFFFFFFFFFFFFFF;

const usz usz_MAX		= (usz) 0xFFFFFFFFFFFFFFFF;
const isz isz_MAX		= (isz)(((usz)1 << (sizeof(usz) * 8 - 1)) - 1);
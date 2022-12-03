#include "math/math.h"
#include <float.h>

const F32 F32_E				= 2.718281828459045f;
const F32 F32_PI			= 3.141592653589793f;
const F32 F32_RAD_TO_DEG	= 57.2957795131f;
const F32 F32_DEG_TO_RAD	= 0.01745329251f;

const U64 KIBI			= 1 << 10;
const U64 MIBI			= 1 << 20;
const U64 GIBI			= 1 << 30;
const U64 TIBI			= (U64)1 << 40;
const U64 PEBI			= (U64)1 << 50;

const U64 KILO			= 1'000;
const U64 MEGA			= 1'000'000;
const U64 GIGA			= 1'000'000'000;
const U64 TERA			= 1'000'000'000'000;
const U64 PETA			= 1'000'000'000'000'000;

const Ns MU				= 1'000;
const Ns MS				= 1'000'000;
const Ns SECOND			= 1'000'000'000;
const Ns MIN			= 60'000'000'000;
const Ns HOUR			= 3'600'000'000'000;
const Ns DAY			= 86'400'000'000'000;
const Ns WEEK			= 604'800'000'000'000;

const U8 U8_MIN			= 0;
const C8 C8_MIN			= 0;
const U16 U16_MIN		= 0;
const U32 U32_MIN		= 0;
const U64 U64_MIN		= 0;

const I8  I8_MIN		= 0x80;
const I16 I16_MIN		= 0x8000;
const I32 I32_MIN		= 0x80000000;
const I64 I64_MIN		= 0x8000000000000000;

const U8  U8_MAX		= 0xFF;
const C8  C8_MAX		= 0xFF;
const U16 U16_MAX		= 0xFFFF;
const U32 U32_MAX		= 0xFFFFFFFF;
const U64 U64_MAX		= 0xFFFFFFFFFFFFFFFF;

const I8  I8_MAX		= 0x7F;
const I16 I16_MAX		= 0x7FFF;
const I32 I32_MAX		= 0x7FFFFFFF;
const I64 I64_MAX		= 0x7FFFFFFFFFFFFFFF;

const F32 F32_MAX		= FLT_MAX;
const F32 F32_MIN		= -FLT_MAX;

#pragma once
#include <stdint.h>
#include <stdbool.h>

//Types

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float  f32;
typedef double f64;

typedef size_t usz;
typedef ptrdiff_t isz;

typedef u64 ns;		//Time since Unix epoch
typedef i64 dns;	//Delta ns

typedef char c8;

//Constants

extern const usz Ki;
extern const usz Mi;
extern const usz Gi;
extern const usz Ti;
extern const usz Pi;

extern const usz K;
extern const usz M;
extern const usz B;
extern const usz T;

extern const ns mus;
extern const ns ms;
extern const ns seconds;
extern const ns mins;
extern const ns hours;
extern const ns days;
extern const ns weeks;

extern const u8 u8_MIN;
extern const u16 u16_MIN;
extern const u32 u32_MIN;
extern const u64 u64_MIN;
extern const usz usz_MIN;

extern const usz sz_BYTES;
extern const usz sz_BITS;

extern const i8  i8_MIN;
extern const i16 i16_MIN;
extern const i32 i32_MIN;
extern const i64 i64_MIN;
extern const isz isz_MIN;

extern const u8  u8_MAX;
extern const u16 u16_MAX;
extern const u32 u32_MAX;
extern const u64 u64_MAX;

extern const i8  i8_MAX;
extern const i16 i16_MAX;
extern const i32 i32_MAX;
extern const i64 i64_MAX;

extern const usz usz_MAX;
extern const isz isz_MAX;

extern const f32 f32_MIN;
extern const f32 f32_MAX;

struct Buffer {
	u8 *ptr;
	usz siz;
};
#pragma once
#include <stdint.h>
#include <stdbool.h>

//A hint to show that something is implementation dependent
//For ease of implementing a new implementation

#define impl

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

typedef u64 ns;		/// Time since Unix epoch in ns
typedef i64 dns;	/// Delta ns

typedef char c8;

//Constants

extern const u64 Ki;
extern const u64 Mi;
extern const u64 Gi;
extern const u64 Ti;
extern const u64 Pi;

extern const u64 K;
extern const u64 M;
extern const u64 B;
extern const u64 T;
extern const u64 P;

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

extern const i8  i8_MIN;
extern const i16 i16_MIN;
extern const i32 i32_MIN;
extern const i64 i64_MIN;

extern const u8  u8_MAX;
extern const u16 u16_MAX;
extern const u32 u32_MAX;
extern const u64 u64_MAX;

extern const i8  i8_MAX;
extern const i16 i16_MAX;
extern const i32 i32_MAX;
extern const i64 i64_MAX;

struct Buffer {
	u8 *ptr;
	u64 siz;
};
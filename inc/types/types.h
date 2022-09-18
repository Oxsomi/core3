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

//Char

inline c8 c8_toLower(c8 c) {
	return (c8) tolower(c);
}

inline c8 c8_toUpper(c8 c) {
	return (c8) toupper(c);
}

inline c8 c8_transform(c8 c, enum StringTransform transform) {
	return transform == StringTransform_None ? c : (
		transform == StringTransform_Lower ? c8_toLower(c) :
		c8_toUpper(c)
	);
}

inline c8 c8_isBin(c8 c) { return c == '0' || c == '1'; }
inline c8 c8_isOct(c8 c) { return c >= '0' && c <= '7'; }
inline c8 c8_isDec(c8 c) { return c >= '0' && c <= '9'; }

inline bool c8_isUpperCase(c8 c) { return c >= 'A' && c <= 'Z'; }
inline bool c8_isLowerCase(c8 c) { return c >= 'a' && c <= 'z'; }
inline bool c8_isUpperCaseHex(c8 c) { return c >= 'A' && c <= 'F'; }
inline bool c8_isLowerCaseHex(c8 c) { return c >= 'a' && c <= 'f'; }

inline c8 c8_isHex(c8 c) { return c8_isDec(c) || c8_isUpperCaseHex(c) || c8_isLowerCaseHex(c); }
inline c8 c8_isNyto(c8 c) { return c8_isDec(c) || c8_isUpperCase(c) || c8_isLowerCase(c) || c == '_' || c == '$'; }
inline c8 c8_isAlphaNumeric(c8 c) { return c8_isNyto(c) && c != '$'; }

inline u8 c8_bin(c8 c) { return c == '0' ? 0 : (c == '1' ? 1 : u8_MAX); }
inline u8 c8_oct(c8 c) { return c8_isOct(c) ? c - '0' : u8_MAX; }
inline u8 c8_dec(c8 c) { return c8_isDec(c) ? c - '0' : u8_MAX; }

inline u8 c8_hex(c8 c) {

	if (c8_isDec(c))
		return c - '0';

	if (c8_isUpperCaseHex(c))
		return c - 'A' + 10;

	if (c8_isLowerCaseHex(c))
		return c - 'a' + 10;

	return u8_MAX;
}

inline u8 c8_nyto(c8 c) {

	if (c8_isDec(c))
		return c - '0';

	if (c8_isUpperCase(c))
		return c - 'A' + 10;

	if (c8_isLowerCase(c))
		return c - 'a' + 36;

	if (c == '_')
		return 62;

	return c == '$' ? 63 : u8_MAX;
}

inline c8 c8_createBin(u8 v) { return (v == 0 ? '0' : (v == 1 ? '1' : '\0')); }
inline c8 c8_createOct(u8 v) { return v < 8 ? '0' + v : '\0'; }
inline c8 c8_createDec(u8 v) { return v < 10 ? '0' + v : '\0'; }
inline c8 c8_createHex(u8 v) { return v < 10 ? '0' + v : (v < 16 ? 'A' + v - 10 : '\0'); }

inline c8 c8_createNyto(u8 v) { 
	return v < 10 ? '0' + v : (
		v < 36 ? 'A' + v - 10 : (
			v < 62 ? 'a' + v - 36 : (
				v == 62 ? '_' : (
					v == 63 ? '$' : '\0'
				)
			)
		)
	); 
}

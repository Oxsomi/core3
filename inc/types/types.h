#pragma once
#include <stdint.h>
#include <stdbool.h>

//A hint to show that something is implementation dependent
//For ease of implementing a new implementation

#define impl

//A hint to show that the end user should define these

#define user_impl

//Types

typedef int8_t  I8;
typedef int16_t I16;
typedef int32_t I32;
typedef int64_t I64;

typedef uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;

typedef float  F32;

#if _StrictFloat
	#if FLT_EVAL_METHOD != 0
		#error Flt eval method should be 0 to indicate consistent behavior
	#endif
#endif

typedef U64 Ns;		/// Time since Unix epoch in Ns
typedef I64 DNs;	/// Delta Ns

typedef char C8;

typedef bool Bool;

//Constants

extern const U64 Ki;
extern const U64 Mi;
extern const U64 Gi;
extern const U64 Ti;
extern const U64 Pi;

extern const U64 K;
extern const U64 M;
extern const U64 B;
extern const U64 T;
extern const U64 P;

extern const Ns mus;
extern const Ns ms;
extern const Ns seconds;
extern const Ns mins;
extern const Ns hours;
extern const Ns days;
extern const Ns weeks;

extern const U8 U8_MIN;
extern const U16 U16_MIN;
extern const U32 U32_MIN;
extern const U64 U64_MIN;

extern const I8  I8_MIN;
extern const I16 I16_MIN;
extern const I32 I32_MIN;
extern const I64 I64_MIN;

extern const U8  U8_MAX;
extern const U16 U16_MAX;
extern const U32 U32_MAX;
extern const U64 U64_MAX;

extern const I8  I8_MAX;
extern const I16 I16_MAX;
extern const I32 I32_MAX;
extern const I64 I64_MAX;

extern const F32 F32_MIN;
extern const F32 F32_MAX;

//TODO: Buffer constant possibility

struct Buffer {
	U8 *ptr;
	U64 siz;
};

//Char

C8 C8_toLower(C8 c);
C8 C8_toUpper(C8 c);

enum StringCase {
	StringCase_Sensitive,			//Prefer when possible; avoids transforming the character
	StringCase_Insensitive
};

enum StringTransform {
	StringTransform_None,
	StringTransform_Lower,
	StringTransform_Upper
};

inline C8 C8_transform(C8 c, enum StringTransform transform) {
	return transform == StringTransform_None ? c : (
		transform == StringTransform_Lower ? C8_toLower(c) :
		C8_toUpper(c)
	);
}

inline Bool C8_isBin(C8 c) { return c == '0' || c == '1'; }
inline Bool C8_isOct(C8 c) { return c >= '0' && c <= '7'; }
inline Bool C8_isDec(C8 c) { return c >= '0' && c <= '9'; }

inline Bool C8_isUpperCase(C8 c) { return c >= 'A' && c <= 'Z'; }
inline Bool C8_isLowerCase(C8 c) { return c >= 'a' && c <= 'z'; }
inline Bool C8_isUpperCaseHex(C8 c) { return c >= 'A' && c <= 'F'; }
inline Bool C8_isLowerCaseHex(C8 c) { return c >= 'a' && c <= 'f'; }
inline Bool C8_isWhitespace(C8 c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

inline Bool C8_isHex(C8 c) { return C8_isDec(c) || C8_isUpperCaseHex(c) || C8_isLowerCaseHex(c); }
inline Bool C8_isNyto(C8 c) { return C8_isDec(c) || C8_isUpperCase(c) || C8_isLowerCase(c) || c == '_' || c == '$'; }
inline Bool C8_isAlphaNumeric(C8 c) { return C8_isNyto(c) && c != '$'; }

inline U8 C8_bin(C8 c) { return c == '0' ? 0 : (c == '1' ? 1 : U8_MAX); }
inline U8 C8_oct(C8 c) { return C8_isOct(c) ? c - '0' : U8_MAX; }
inline U8 C8_dec(C8 c) { return C8_isDec(c) ? c - '0' : U8_MAX; }

inline U8 C8_hex(C8 c) {

	if (C8_isDec(c))
		return c - '0';

	if (C8_isUpperCaseHex(c))
		return c - 'A' + 10;

	if (C8_isLowerCaseHex(c))
		return c - 'a' + 10;

	return U8_MAX;
}

inline U8 C8_nyto(C8 c) {

	if (C8_isDec(c))
		return c - '0';

	if (C8_isUpperCase(c))
		return c - 'A' + 10;

	if (C8_isLowerCase(c))
		return c - 'a' + 36;

	if (c == '_')
		return 62;

	return c == '$' ? 63 : U8_MAX;
}

inline C8 C8_createBin(U8 v) { return (v == 0 ? '0' : (v == 1 ? '1' : (C8)U8_MAX)); }
inline C8 C8_createOct(U8 v) { return v < 8 ? '0' + v : (C8)U8_MAX; }
inline C8 C8_createDec(U8 v) { return v < 10 ? '0' + v : (C8)U8_MAX; }
inline C8 C8_createHex(U8 v) { return v < 10 ? '0' + v : (v < 16 ? 'A' + v - 10 : (C8)U8_MAX); }

//Nytodecimal: 0-9A-Za-z_$

inline C8 C8_createNyto(U8 v) { 
	return v < 10 ? '0' + v : (
		v < 36 ? 'A' + v - 10 : (
			v < 62 ? 'a' + v - 36 : (
				v == 62 ? '_' : (
					v == 63 ? '$' : (C8)U8_MAX
				)
			)
		)
	); 
}

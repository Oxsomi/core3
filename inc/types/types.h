#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "cfg/config.h"

//A hint to show that something is implementation dependent
//For ease of implementing a new implementation
//These should never be directly called by anyone else than the main library the impl is for.

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

typedef float F32;

#if !_RELAX_FLOAT
	#if FLT_EVAL_METHOD != 0
		#error Flt eval method should be 0 to indicate consistent behavior
	#endif
#endif

typedef U64 Ns;		/// Time since Unix epoch in Ns
typedef I64 DNs;	/// Delta Ns

typedef char C8;

typedef bool Bool;

//Constants

extern const U64 KIBI;
extern const U64 MIBI;
extern const U64 GIBI;
extern const U64 TIBI;
extern const U64 PEBI;

extern const U64 KILO;
extern const U64 MEGA;
extern const U64 GIGA;
extern const U64 TERA;
extern const U64 PETA;

extern const Ns MU;
extern const Ns MS;
extern const Ns SECOND;
extern const Ns MIN;
extern const Ns HOUR;
extern const Ns DAY;
extern const Ns WEEK;

extern const U8 U8_MIN;
extern const C8 C8_MIN;
extern const U16 U16_MIN;
extern const U32 U32_MIN;
extern const U64 U64_MIN;

extern const I8  I8_MIN;
extern const I16 I16_MIN;
extern const I32 I32_MIN;
extern const I64 I64_MIN;

extern const U8  U8_MAX;
extern const C8  C8_MAX;
extern const U16 U16_MAX;
extern const U32 U32_MAX;
extern const U64 U64_MAX;

extern const I8  I8_MAX;
extern const I16 I16_MAX;
extern const I32 I32_MAX;
extern const I64 I64_MAX;

extern const F32 F32_MIN;
extern const F32 F32_MAX;

//Buffer (more functions in types/buffer.h)

typedef struct Buffer {
	U8 *ptr;
	U64 lengthAndRefBits;		//refBits: [ b31 isRef, b30 isConst ]. Length should be max 48 bits
} Buffer;

U64 Buffer_length(Buffer buf);

Bool Buffer_isRef(Buffer buf);
Bool Buffer_isConstRef(Buffer buf);

Buffer Buffer_createManagedPtr(void *ptr, U64 length);

//Use this instead of simply copying the Buffer to a new location
//A copy like this is only fine if the other doesn't get freed.
//In all other cases, createRefFromBuffer should be called on the one that shouldn't be freeing.
//If it needs to be refcounted, RefPtr should be used.
//
Buffer Buffer_createRefFromBuffer(Buffer buf, Bool isConst);

//Char

C8 C8_toLower(C8 c);
C8 C8_toUpper(C8 c);

typedef enum EStringCase {
	EStringCase_Sensitive,			//Prefer when possible; avoids transforming the character
	EStringCase_Insensitive
} EStringCase;

typedef enum EStringTransform {
	EStringTransform_None,
	EStringTransform_Lower,
	EStringTransform_Upper
} EStringTransform;

C8 C8_transform(C8 c, EStringTransform transform);

Bool C8_isBin(C8 c);
Bool C8_isOct(C8 c);
Bool C8_isDec(C8 c);

Bool C8_isUpperCase(C8 c);
Bool C8_isLowerCase(C8 c);
Bool C8_isUpperCaseHex(C8 c);
Bool C8_isLowerCaseHex(C8 c);
Bool C8_isWhitespace(C8 c);

Bool C8_isHex(C8 c);
Bool C8_isNyto(C8 c);
Bool C8_isAlphaNumeric(C8 c);
Bool C8_isAlpha(C8 c);

Bool C8_isValidAscii(C8 c);
Bool C8_isValidFileName(C8 c);

U8 C8_bin(C8 c);
U8 C8_oct(C8 c);
U8 C8_dec(C8 c);

U8 C8_hex(C8 c);
U8 C8_nyto(C8 c);

C8 C8_createBin(U8 v);
C8 C8_createOct(U8 v);
C8 C8_createDec(U8 v);
C8 C8_createHex(U8 v);

//Nytodecimal: 0-9A-Za-z_$

C8 C8_createNyto(U8 v);

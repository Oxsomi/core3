#include "types/types.h"
#include <ctype.h>

U64 Buffer_length(Buffer buf) {
	return buf.lengthAndRefBits << 2 >> 2;
}

Buffer Buffer_createManagedPtr(void *ptr, U64 length) {

	if((U64)ptr >> 48 || length >> 48 || !ptr || !length)
		return (Buffer) { 0 };

	return (Buffer) { .ptr = ptr, .lengthAndRefBits = length };
}

Buffer Buffer_createRefFromBuffer(Buffer buf, Bool isConst) {

	if(!buf.ptr)
		return (Buffer) { 0 };

	Buffer copy = buf;
	copy.lengthAndRefBits |= ((U64)1 << 63) | ((U64)isConst << 62);
	return copy;
}

Bool Buffer_isRef(Buffer buf) {
	return buf.lengthAndRefBits >> 62;
}

Bool Buffer_isConstRef(Buffer buf) {
	return (buf.lengthAndRefBits >> 62) == 3;
}

C8 C8_toLower(C8 c) {
	return (C8) tolower(c);
}

C8 C8_toUpper(C8 c) {
	return (C8) toupper(c);
}

C8 C8_transform(C8 c, EStringTransform transform) {
	return transform == EStringTransform_None ? c : (
		transform == EStringTransform_Lower ? C8_toLower(c) :
		C8_toUpper(c)
	);
}

Bool C8_isBin(C8 c) { return c == '0' || c == '1'; }
Bool C8_isOct(C8 c) { return c >= '0' && c <= '7'; }
Bool C8_isDec(C8 c) { return c >= '0' && c <= '9'; }

Bool C8_isUpperCase(C8 c) { return c >= 'A' && c <= 'Z'; }
Bool C8_isLowerCase(C8 c) { return c >= 'a' && c <= 'z'; }
Bool C8_isUpperCaseHex(C8 c) { return c >= 'A' && c <= 'F'; }
Bool C8_isLowerCaseHex(C8 c) { return c >= 'a' && c <= 'f'; }
Bool C8_isWhitespace(C8 c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

Bool C8_isHex(C8 c) { return C8_isDec(c) || C8_isUpperCaseHex(c) || C8_isLowerCaseHex(c); }
Bool C8_isNyto(C8 c) { return C8_isDec(c) || C8_isUpperCase(c) || C8_isLowerCase(c) || c == '_' || c == '$'; }
Bool C8_isAlphaNumeric(C8 c) { return C8_isDec(c) || C8_isUpperCase(c) || C8_isLowerCase(c); }
Bool C8_isAlpha(C8 c) { return C8_isUpperCase(c) || C8_isLowerCase(c); }

Bool C8_isValidAscii(C8 c) { return (c >= 0x20 && c < 0x7F) || c == '\t' || c == '\n' || c == '\r'; }
Bool C8_isValidFileName(C8 c) { return C8_isAlphaNumeric(c) || c == ' ' || c == '-' || c == '.' || c == '_'; }

U8 C8_bin(C8 c) { return c == '0' ? 0 : (c == '1' ? 1 : U8_MAX); }
U8 C8_oct(C8 c) { return C8_isOct(c) ? c - '0' : U8_MAX; }
U8 C8_dec(C8 c) { return C8_isDec(c) ? c - '0' : U8_MAX; }

U8 C8_hex(C8 c) {

	if (C8_isDec(c))
		return c - '0';

	if (C8_isUpperCaseHex(c))
		return c - 'A' + 10;

	if (C8_isLowerCaseHex(c))
		return c - 'a' + 10;

	return U8_MAX;
}

U8 C8_nyto(C8 c) {

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

C8 C8_createBin(U8 v) { return (v == 0 ? '0' : (v == 1 ? '1' : C8_MAX)); }
C8 C8_createOct(U8 v) { return v < 8 ? '0' + v : C8_MAX; }
C8 C8_createDec(U8 v) { return v < 10 ? '0' + v : C8_MAX; }
C8 C8_createHex(U8 v) { return v < 10 ? '0' + v : (v < 16 ? 'A' + v - 10 : C8_MAX); }

//Nytodecimal: 0-9A-Za-z_$

C8 C8_createNyto(U8 v) { 
	return v < 10 ? '0' + v : (
		v < 36 ? 'A' + v - 10 : (
			v < 62 ? 'a' + v - 36 : (
				v == 62 ? '_' : (
					v == 63 ? '$' : C8_MAX
					)
				)
			)
		); 
}

/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

//platforms/generic/platform_env.c

#include "platforms/platform.h"
#include "types/container/string.h"
#include "types/base/string_read_helper.h"
#include "types/base/error.h"

Bool Platform_getEnv(CharString name, const Allocator *alloc, CharString *result, Error *e_rr) {

	Bool s_uccess = true;
	CharString nameCopy = CharString_createNull();

	if(!result)
		retError(clean, Error_nullPointer(2, "Platform_getEnv()::result is required"));

	if(result->ptr)
		retError(clean, Error_invalidParameter(2, 0, "Platform_getEnv()::result must be empty, it would leak"));

	if(CharString_isEmpty(name) || CharString_containsSensitive(&name, '=', 0, 0))
		retError(clean, Error_invalidParameter(0, 0, "Platform_getEnv()::name must be non-empty and hold no '='"));

	//A NUL inside the name would silently look up a shorter one.

	if(CharString_containsSensitive(&name, '\0', 0, 0))
		retError(clean, Error_invalidParameter(0, 1, "Platform_getEnv()::name holds a NUL"));

	if(!CharString_isNullTerminated(name)) {
		gotoIfError3(clean, CharString_createCopy(name, alloc, &nameCopy, e_rr));
		name = nameCopy;
	}

	gotoIfError3(clean, Platform_getEnvExt(name, alloc, result, e_rr));

clean:
	CharString_free(&nameCopy, alloc);
	return s_uccess;
}

Bool Platform_hasEnv(CharString name, const Allocator *alloc, Bool *result, Error *e_rr) {

	Bool s_uccess = true;
	CharString value = CharString_createNull();

	if(!result)
		retError(clean, Error_nullPointer(2, "Platform_hasEnv()::result is required"));

	gotoIfError3(clean, Platform_getEnv(name, alloc, &value, e_rr));
	*result = !!value.ptr;

clean:
	CharString_free(&value, alloc);
	return s_uccess;
}

Bool Platform_parseEnvBool(CharString value, Bool *result) {

	if(!result)
		return false;

	if(CharString_equalsCStringSensitive(&value, "1") || CharString_equalsCStringInsensitive(&value, "true"))
		*result = true;

	else if(CharString_equalsCStringSensitive(&value, "0") || CharString_equalsCStringInsensitive(&value, "false"))
		*result = false;

	else return false;

	return true;
}

//parseHex takes "0x" alone as zero, so the prefix has to be followed by a digit to count.

Bool Platform_parseEnvU64(CharString value, U64 *result) {

	U64 parsed = 0;

	if(!result)
		return false;

	if(CharString_startsWithCStringInsensitive(&value, "0x", 0)) {
		if(CharString_length(value) <= 2 || !CharString_parseHex(value, &parsed))
			return false;
	}

	else if(!CharString_isDec(value) || !CharString_parseDec(value, &parsed))
		return false;

	*result = parsed;
	return true;
}

Bool Platform_parseEnvI64(CharString value, I64 *result) {

	I64 parsed = 0;

	if(!result || !CharString_isSignedNumber(value) || !CharString_parseDecSigned(value, &parsed))
		return false;

	*result = parsed;
	return true;
}

Bool Platform_parseEnvF64(CharString value, F64 *result) {

	F64 parsed = 0;

	if(!result || !CharString_parseDouble(value, &parsed))
		return false;

	*result = parsed;
	return true;
}

//One body for the four typed readers: read, and when set, parse with the grammar above or fail naming what it wanted.

#define PLATFORM_GET_ENV_TYPED(T, name, alloc, result, e_rr, wants) {                                                   \
	Bool s_uccess = true;                                                                                               \
																														\
	CharString value = CharString_createNull();                                                                         \
	if(!result)                                                                                                         \
		retError(clean, Error_nullPointer(2, "Platform_getEnv" #T "()::result is required"));                           \
																														\
	gotoIfError3(clean, Platform_getEnv(name, alloc, &value, e_rr));                                                    \
																														\
	if(value.ptr && !Platform_parseEnv##T(value, result))                                                               \
		retError(clean, Error_invalidParameter(0, 2, "Platform_getEnv" #T "() value isn't " wants));                    \
clean:                                                                                                                  \
	CharString_free(&value, alloc);                                                                                     \
	return s_uccess;                                                                                                    \
}

Bool Platform_getEnvBool(CharString name, const Allocator *alloc, Bool *result, Error *e_rr)
	PLATFORM_GET_ENV_TYPED(Bool, name, alloc, result, e_rr, "0, 1, true or false")

Bool Platform_getEnvU64(CharString name, const Allocator *alloc, U64 *result, Error *e_rr)
	PLATFORM_GET_ENV_TYPED(U64, name, alloc, result, e_rr, "a decimal or 0x hex U64")

Bool Platform_getEnvI64(CharString name, const Allocator *alloc, I64 *result, Error *e_rr)
	PLATFORM_GET_ENV_TYPED(I64, name, alloc, result, e_rr, "a decimal I64")

Bool Platform_getEnvF64(CharString name, const Allocator *alloc, F64 *result, Error *e_rr)
	PLATFORM_GET_ENV_TYPED(F64, name, alloc, result, e_rr, "a decimal F64")

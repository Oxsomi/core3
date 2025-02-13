/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "platforms/ext/listx.h"
#include "platforms/platform.h"
#include "platforms/log.h"
#include "types/base/c8.h"
#include "types/container/buffer.h"
#include "tools/oxc3_cli/cli.h"
#include "tools/package_cli/packager.h"

#ifdef CLI_SHADER_COMPILER
	#include "shader_compiler/compiler.h"
#endif

Bool CLI_package(ParsedArgs args) {

	//Parse encryption key

	U32 encryptionKeyV[8] = { 0 };
	U32 *encryptionKey = NULL;			//Only if we have aes should encryption key be set.
	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;

	if (args.parameters & EOperationHasParameter_AES) {

		CharString key = CharString_createNull();

		if (
			(ParsedArgs_getArg(args, EOperationHasParameter_AESShift, &key)).genericError ||
			!CharString_isHex(key)
		) {
			Log_errorLnx("Invalid parameter sent to -aes. Expecting key in hex (32 bytes)");
			return false;
		}

		U64 off = CharString_startsWithStringInsensitive(key, CharString_createRefCStrConst("0x"), 0) ? 2 : 0;

		if (CharString_length(key) - off != 64) {
			Log_errorLnx("Invalid parameter sent to -aes. Expecting key in hex (32 bytes)");
			return false;
		}

		for (U64 i = off; i + 1 < CharString_length(key); ++i) {

			U8 v0 = C8_hex(key.ptr[i]);
			U8 v1 = C8_hex(key.ptr[++i]);

			v0 = (v0 << 4) | v1;
			*((U8*)encryptionKeyV + ((i - off) >> 1)) = v0;
		}

		encryptionKey = encryptionKeyV;
	}

	//Get input

	CharString input = (CharString) { 0 };
	gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &input))

	//Check if output is valid

	CharString output = (CharString) { 0 };
	gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &output))

	//Get compile settings

	Bool multipleModes = false;
	U64 compileModeU64 = 0;
	U64 threadCount = 0;
	CharString includeDir = (CharString) { 0 };
	ECompilerWarning extraWarnings = (ECompilerWarning) 0;
	Bool merge = !(args.flags & EOperationFlags_Split);

	#ifdef CLI_SHADER_COMPILER

		gotoIfError3(clean, CLI_parseCompileTypes(args, &compileModeU64, &multipleModes))
		gotoIfError3(clean, CLI_parseThreads(args, &threadCount, 1))

		if (args.parameters & EOperationHasParameter_IncludeDir)
			gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_IncludeDirShift, &includeDir))

		extraWarnings = CLI_getExtraWarnings(args);

	#endif

	gotoIfError3(clean, Packager_package(
		input,
		output,
		encryptionKey,
		multipleModes,
		compileModeU64,
		threadCount,
		includeDir,
		merge,
		extraWarnings,
		true,
		!!(args.flags & EOperationFlags_Debug),
		!!(args.flags & EOperationFlags_IgnoreEmptyFiles),
		Platform_instance->alloc,
		&err
	))

clean:

	if(encryptionKey)
		Buffer_unsetAllBits(Buffer_createRef(encryptionKeyV, sizeof(encryptionKeyV)));

	return s_uccess;
}

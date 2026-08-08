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

//tools/oxc3_cli/encrypt.c

#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/container/log.h"
#include "types/base/error.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "tools/oxc3_cli/cli.h"

Bool CLI_encryptDo(const ParsedArgs *args) {

	if(!args) return false;

	//A key is required, but it may come from any of -aes / -aes-file (params) or --aes-stdin (a flag), validated by
	//CLI_getAesKey later. The required-parameter bitmask can't express "one of", so enforce it here; else convert
	//would silently write a plaintext archive when no key was given.

	if(!(args->parameters & EOperationHasParameter_AnyAES) && !(args->flags & EOperationFlags_AESStdin)) {
		Log_errorLnx("CLI_encryptDo requires a key via -aes, -aes-file or --aes-stdin.");
		return false;
	}

	ParsedArgs argsMut = *args;

	const Bool generateOutput = !(argsMut.parameters & EOperationHasParameter_Output);
	const U64 generatedOutputIndex = 1;
	CharString tmpString = CharString_createNull();
	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;

	//Modify arguments so it can be passed to oiCA convert function.

	if (generateOutput) {
		const CharString oiCA = CharString_createRefCStrConst(".oiCA");
		gotoIfError3(clean, CharString_createCopy(*argsMut.args.ptr, Platform_instance->alloc, &tmpString, e_rr));
		gotoIfError3(clean, CharString_appendString(&tmpString, &oiCA, Platform_instance->alloc, e_rr));
		gotoIfError3(clean, ListCharString_insert(&argsMut.args, generatedOutputIndex, tmpString, Platform_instance->alloc, e_rr));
	}

	const ParsedArgs caArgs = (ParsedArgs) {
		.operation = EOperation_FileTo,
		.format = EFormat_oiCA,
		.flags = EOperationFlags_Uncompressed,
		.parameters = argsMut.parameters | EOperationHasParameter_Output,
		.args = argsMut.args
	};

	s_uccess = CLI_convertTo(&caArgs);

clean:

	if(!s_uccess) {
		Log_errorLnx("CLI_encryptDo failed.");
		Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_NewLine);
	}

	if(generateOutput)
		ListCharString_popLocation(&argsMut.args, generatedOutputIndex, NULL, NULL);

	CharString_free(&tmpString, Platform_instance->alloc);

	return s_uccess;
}

Bool CLI_encryptUndo(const ParsedArgs *args) {

	if(!args) return false;

	//A key is required, but it may come from any of -aes / -aes-file (params) or --aes-stdin (a flag).

	if(!(args->parameters & EOperationHasParameter_AnyAES) && !(args->flags & EOperationFlags_AESStdin)) {
		Log_errorLnx("CLI_encryptUndo requires a key via -aes, -aes-file or --aes-stdin.");
		return false;
	}

	const ParsedArgs caArgs = (ParsedArgs) {
		.operation = EOperation_FileFrom,
		.format = EFormat_oiCA,
		.parameters = args->parameters,
		.args = args->args
	};

	return CLI_convertFrom(&caArgs);
}

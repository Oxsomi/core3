/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/buffer.h"
#include "platforms/log.h"
#include "platforms/ext/listx.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/stringx.h"
#include "cli.h"

Bool CLI_encryptDo(ParsedArgs args) {

	Bool generateOutput = !(args.parameters & EOperationHasParameter_Output);
	U64 generatedOutputIndex = 1;
	CharString tmpString = CharString_createNull();
	Bool b = false;
	Error err = Error_none();

	//Modify arguments so it can be passed to oiCA convert function.

	if (generateOutput) {

		_gotoIfError(clean, CharString_createCopyx(*((const CharString*)args.args.ptr), &tmpString));
		_gotoIfError(clean, CharString_appendStringx(&tmpString, CharString_createConstRefCStr(".oiCA")));

		_gotoIfError(
			clean, 
			List_insertx(&args.args, generatedOutputIndex, Buffer_createConstRef(&tmpString, sizeof(tmpString)))
		);
	}

	ParsedArgs caArgs = (ParsedArgs) {
		.operation = EOperation_FileTo,
		.format = EFormat_oiCA,
		.flags = EOperationFlags_Uncompressed,
		.parameters = args.parameters | EOperationHasParameter_Output,
		.args = args.args
	};
	
	b = CLI_convertTo(caArgs);

clean:

	if(!b) {
		Log_errorLnx("CLI_encryptDo failed.");
		Error_printx(err, ELogLevel_Error, ELogOptions_NewLine);
	}

	if(generateOutput)
		List_popLocation(&args.args, generatedOutputIndex, Buffer_createNull());

	CharString_freex(&tmpString);

	return b;
}

Bool CLI_encryptUndo(ParsedArgs args) {

	ParsedArgs caArgs = (ParsedArgs) {
		.operation = EOperation_FileFrom,
		.format = EFormat_oiCA,
		.parameters = args.parameters,
		.args = args.args
	};

	return CLI_convertFrom(caArgs);
}

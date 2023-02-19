/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
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
	String tmpString = String_createNull();
	Bool b = false;
	Error err = Error_none();

	//Modify arguments so it can be passed to oiCA convert function.

	if (generateOutput) {

		_gotoIfError(clean, String_createCopyx(*((const String*)args.args.ptr), &tmpString));
		_gotoIfError(clean, String_appendStringx(&tmpString, String_createConstRefUnsafe(".oiCA")));

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
		Log_error(String_createConstRefUnsafe("CLI_encryptDo failed."), ELogOptions_NewLine);
		Error_printx(err, ELogLevel_Error, ELogOptions_NewLine);
	}

	if(generateOutput)
		List_popLocation(&args.args, generatedOutputIndex, Buffer_createNull());

	String_freex(&tmpString);

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

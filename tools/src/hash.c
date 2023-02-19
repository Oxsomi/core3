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
#include "platforms/ext/stringx.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/file.h"
#include "platforms/log.h"
#include "operations.h"
#include "cli.h"

Bool CLI_hash(ParsedArgs args, Bool isFile) {

	String str = String_createNull();
	Buffer buf = Buffer_createNull();
	String tmp = String_createNull();
	String tmpi = String_createNull();
	Bool success = false;

	Error err = ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &str);

	if (err.genericError)
		goto clean;

	if(!isFile)
		buf = String_bufferConst(str);

	else _gotoIfError(clean, File_read(str, 1 * SECOND, &buf));

	switch(args.format) {

		case EFormat_SHA256: {

			U32 output[8] = { 0 };
			Buffer_sha256(buf, output);

			//Stringify

			for (U8 i = 0; i < 8; ++i) {
				_gotoIfError(clean, String_createHexx(output[i], 8, &tmpi));
				_gotoIfError(clean, String_popFrontCount(&tmpi, 2));
				_gotoIfError(clean, String_appendStringx(&tmp, tmpi));
				String_freex(&tmpi);
			}

			break;
		}

		case EFormat_CRC32C: {
			U32 output = Buffer_crc32c(buf);
			_gotoIfError(clean, String_createHexx(output, 8, &tmp));
			_gotoIfError(clean, String_popFrontCount(&tmp, 2));
			break;
		}

		default:
			Log_error(String_createConstRefUnsafe("Unsupported format"), ELogOptions_NewLine);
			goto clean;
	}

	Log_debug(String_createConstRefUnsafe("Hash: "), ELogOptions_NewLine);
	Log_debug(String_createConstRefUnsafe("0x"), ELogOptions_None);
	Log_debug(tmp, ELogOptions_NewLine);

	success = true;

clean:

	if(err.genericError) {
		Log_error(String_createConstRefUnsafe("Failed to convert hash to string!"), ELogOptions_NewLine);
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);
	}

	Buffer_freex(&buf);
	String_freex(&tmp);
	String_freex(&tmpi);
	return success;
}

Bool CLI_hashFile(ParsedArgs args) {
	return CLI_hash(args, true);
}

Bool CLI_hashString(ParsedArgs args) {
	return CLI_hash(args, false);
}

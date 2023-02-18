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

#include "types/string.h"
#include "types/buffer.h"
#include "types/error.h"
#include "platforms/log.h"
#include "platforms/file.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/errorx.h"
#include "cli.h"

typedef enum RandType {

	RandType_Key,
	RandType_Char,
	RandType_Num,
	RandType_Bytes

} RandType;

Bool CLI_rand(ParsedArgs args, RandType type) {

	//Parse arguments

	U64 n = 1;

	if (args.parameters & EOperationHasParameter_Number) {

		String str = String_createNull();

		if (
			ParsedArgs_getArg(args, EOperationHasParameter_NumberShift, &str).genericError ||
			!String_parseDec(str, &n) ||
			(n >> 32)
		) {
			Log_debug(String_createConstRefUnsafe("Invalid argument -n <string>, uint expected."), ELogOptions_NewLine);
			return false;
		}
	}

	U64 l = args.operation == EOperation_RandKey ? 32 : 1;

	if (args.parameters & EOperationHasParameter_Length) {

		String str = String_createNull();

		if (
			ParsedArgs_getArg(args, EOperationHasParameter_LengthShift, &str).genericError ||
			!String_parseDec(str, &l) ||
			(l >> 32)
		) {
			Log_debug(String_createConstRefUnsafe("Invalid argument -l <string>, uint expected."), ELogOptions_NewLine);
			return false;
		}
	}

	//Temp output

	Buffer outputFile = Buffer_createNull();
	Buffer tmp = Buffer_createNull();
	String outputString = String_createNull();
	String errorString = String_createNull();
	String tmpString = String_createNull();
	Error err = Error_none();

	//Generate data

	U64 bytesToGenerate = l;
	U8 outputAsBase = 16;

	switch (args.operation) {

		case EOperation_RandKey:
			break;

		case EOperation_RandChar:
			bytesToGenerate *= 8;		//Better probability distribution
			break;

		default:
			Log_debug(String_createConstRefUnsafe("Invalid operation"), ELogOptions_NewLine);
			return false;
	}

	if(outputAsBase == 256)
		_gotoIfError(clean, Buffer_createUninitializedBytesx(n * bytesToGenerate, &outputFile));

	Buffer outputFilePtr = Buffer_createRefFromBuffer(outputFile, false);

	for (U64 i = 0; i < n; ++i) {

		_gotoIfError(clean, Buffer_createUninitializedBytesx(bytesToGenerate, &tmp));

		if(!Buffer_csprng(tmp))
			_gotoIfError(clean, Error_invalidOperation(0));

		if(outputAsBase == 256)
			_gotoIfError(clean, Buffer_appendBuffer(&outputFilePtr, tmp))

		else {

			switch (args.operation) {

				case EOperation_RandKey:

					for(U64 j = 0; j < Buffer_length(tmp); ++j) {
			
						U8 v = tmp.ptr[j];
						U8 prefix = 2;

						switch (outputAsBase) {

							case 16:
								_gotoIfError(clean, String_createHexx(v, 2, &tmpString));
								break;
						}

						_gotoIfError(clean, String_popFrontCount(&tmpString, prefix));
						_gotoIfError(clean, String_appendStringx(&outputString, tmpString));

						String_freex(&tmpString);
					}

					break;

				case EOperation_RandChar: {

					//Build up all options

					String options = String_createNull();

					Bool anyCharFlags = args.flags & EOperationFlags_RandomChar;
					Bool pickAll = !anyCharFlags;

					if (args.parameters & EOperationHasParameter_Character) {
					
						String str = String_createNull();

						if (ParsedArgs_getArg(args, EOperationHasParameter_CharacterShift, &str).genericError) {
							Log_debug(String_createConstRefUnsafe("Invalid argument -c <string>."), ELogOptions_NewLine);
							return false;
						}

						_gotoIfError(clean, String_appendStringx(&options, str));

						if(str.length)
							pickAll = false;
					}

					if(pickAll || anyCharFlags)
						for(C8 c = 0x21; c < 0x7F; ++c) {

							//Check what type of character to see if we should skip

							if (!pickAll) {

								if(
									C8_isUpperCase(c) && 
									!(args.flags & (EOperationFlags_Alpha | EOperationFlags_Uppercase | EOperationFlags_Alphanumeric))
								)
									continue;

								if(
									C8_isLowerCase(c) && 
									!(args.flags & (EOperationFlags_Alpha | EOperationFlags_Lowercase | EOperationFlags_Alphanumeric))
								)
									continue;

								if(C8_isDec(c) && !(args.flags & (EOperationFlags_Number | EOperationFlags_Alphanumeric)))
									continue;
								
								if(!C8_isAlphaNumeric(c) && !(args.flags & EOperationFlags_Symbols))
									continue;
							}

							//Append

							_gotoIfError(clean, String_appendx(&options, c));
						}

					//We use a U64 per character, because if for example we generate 256 values (1 U8).
					//Then subdivide it over 120, we'd be left with 26 values.
					//This means that the first 26 characters would be picked 50% more often than others.
					//To keep this to a minimum, we use a U64, because in the same scenario, the error rate is super small.
					//We do this to avoid having to reroll the randomness.

					for (U64 i = 0, j = Buffer_length(tmp); i < j; i += 8) {

						U64 v = *(const U64*)(tmp.ptr + i);
						v %= options.length;

						_gotoIfError(clean, String_appendx(&outputString, options.ptr[v]));
					}

					break;
				}
			}

			if(i != (n - 1) || !(args.parameters & EOperationHasParameter_Output))
				_gotoIfError(clean, String_appendStringx(&outputString, String_newLine()));
		}

		Buffer_freex(&tmp);
	}

	//Write to output

	if(outputString.ptr)
		outputFile = String_bufferConst(outputString);

	if (args.parameters & EOperationHasParameter_Output) {

		String outputPath = String_createNull();
		
		if (ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &outputPath).genericError) {
			Log_debug(String_createConstRefUnsafe("Invalid argument -o <string>, file path expected."), ELogOptions_NewLine);
			return false;
		}

		errorString = String_createConstRefUnsafe("Couldn't write to output file");
		_gotoIfError(clean, File_write(outputFile, outputPath, 1 * SECOND));
	}

	else {
		Log_debug(String_createConstRefUnsafe("Random operation returned:"), ELogOptions_NewLine);
		Log_debug(outputString, ELogOptions_None);
	}

clean:

	if(err.genericError) {

		if(errorString.length)
			Log_debug(errorString, ELogOptions_NewLine);

		else Error_printx(err, ELogLevel_Error, ELogOptions_NewLine);
	}

	Buffer_freex(&tmp);
	Buffer_freex(&outputFile);
	String_freex(&outputString);
	String_freex(&tmpString);
	return !err.genericError;
}

Bool CLI_randKey(ParsedArgs args) {
	return CLI_rand(args, RandType_Key);
}

Bool CLI_randChar(ParsedArgs args) {
	return CLI_rand(args, RandType_Char);
}

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
#include "math/math.h"
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
	RandType_Data

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
			Log_errorLn("Invalid argument -n <string>, uint expected.");
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
			Log_errorLn("Invalid argument -l <string>, uint expected.");
			return false;
		}
	}

	U64 b = 0;		//Unused except rand num

	if (args.parameters & EOperationHasParameter_Bit) {

		String str = String_createNull();

		if (
			ParsedArgs_getArg(args, EOperationHasParameter_BitShift, &str).genericError ||
			!String_parseDec(str, &b) ||
			(b >> 16)
		) {
			Log_errorLn("Invalid argument -b <string>, ushort expected.");
			return false;
		}
	}

	//Temp output

	Buffer outputFile = Buffer_createNull();
	Buffer tmp = Buffer_createNull();
	String outputString = String_createNull();
	const C8 *errorString = NULL;
	String tmpString = String_createNull();
	Error err = Error_none();

	//Generate data

	U64 bytesToGenerate = l;
	U64 outputAsBase = 16;

	switch (args.operation) {

		case EOperation_RandKey:
			break;

		case EOperation_RandData:
			outputAsBase = args.parameters & EOperationHasParameter_Output ? 256 : 16;
			break;

		case EOperation_RandNum: {

			Bool hasMultiFlag = false;
			EOperationFlags local = args.flags & EOperationFlags_RandNum;

			if (local & EOperationFlags_Nyto) {
				outputAsBase = 64;
				hasMultiFlag |= local & ~EOperationFlags_Nyto;
			}

			if (local & EOperationFlags_Hex)
				hasMultiFlag |= local & ~EOperationFlags_Hex;

			if (local & EOperationFlags_Oct) {
				outputAsBase = 8;
				hasMultiFlag |= local & ~EOperationFlags_Oct;
			}

			if (local & EOperationFlags_Bin) {
				outputAsBase = 2;
				hasMultiFlag |= local & ~EOperationFlags_Bin;
			}

			if(!local)
				outputAsBase = 10;

			if (hasMultiFlag) {
				Log_errorLn("Invalid argument. Can only pick one base.");
				return false;
			}
			
			bytesToGenerate *= 8;		//Better probability distribution
			break;
		}

		case EOperation_RandChar:
			bytesToGenerate *= 8;		//Better probability distribution
			break;

		default:
			Log_errorLn("Invalid operation.");
			return false;
	}

	//Default for random number. Default length to how many bytes it takes to represent the number.

	U64 maxLen = l;

	if (b) {

		switch (outputAsBase) {

			case 2:		maxLen = b;					break;
			case 8:		maxLen = (b + 2) / 3;		break;
			case 16:	maxLen = (b + 3) >> 2;		break;
			case 64:	maxLen = (b + 5) / 6;		break;

			default: {

				if (b > 64) {
					Log_errorLn("Decimal numbers can only support up to 64 bit (if -b is set).");
					return false;
				}

				F32 f = (F32)((U64)1 << U64_min(63, b));

				if(b == 64)		//Shift by 64 is illegal ofc
					f *= 2;

				maxLen = (U64) F32_ceil(F32_log10(f));
				break;
			}
		}

		if(args.parameters & EOperationHasParameter_Length)
			l = U64_min(l, maxLen);

		else l = maxLen;

		bytesToGenerate = l * 8;		//Better probability distribution
	}

	//Buffer

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
				case EOperation_RandData:

					for(U64 j = 0, k = Buffer_length(tmp); j < k; ++j) {
			
						U8 v = tmp.ptr[j];
						U8 prefix = 2;

						switch (outputAsBase) {

							case 16:
								_gotoIfError(clean, String_createHexx(v, 2, &tmpString));
								break;
						}

						_gotoIfError(clean, String_popFrontCount(&tmpString, prefix));
						_gotoIfError(clean, String_appendStringx(&outputString, tmpString));

						if(args.operation != EOperation_RandKey || bytesToGenerate != 32)
							if(j != (k - 1) && !((j + 1) & 15))
								_gotoIfError(clean, String_appendx(&outputString, ' '));

						if(j != (k - 1) && !((j + 1) & 63))
							_gotoIfError(clean, String_appendStringx(&outputString, String_newLine()));

						String_freex(&tmpString);
					}

					break;

				case EOperation_RandNum:
				case EOperation_RandChar: {

					//Build up all options

					String options = String_createNull();

					//Random char

					if(args.operation == EOperation_RandChar) {

						Bool anyCharFlags = args.flags & EOperationFlags_RandChar;
						Bool pickAll = !anyCharFlags;

						if (args.parameters & EOperationHasParameter_Character) {
					
							String str = String_createNull();

							if (ParsedArgs_getArg(args, EOperationHasParameter_CharacterShift, &str).genericError) {
								Log_errorLn("Invalid argument -c <string>.");
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
										!(args.flags & (
											EOperationFlags_Alpha | EOperationFlags_Uppercase | EOperationFlags_Alphanumeric
										))
									)
										continue;

									if(
										C8_isLowerCase(c) && 
										!(args.flags & (
											EOperationFlags_Alpha | EOperationFlags_Lowercase | EOperationFlags_Alphanumeric)
										)
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
					}

					//Random number

					else for(U8 i = 0; i < (U8) outputAsBase; ++i)
						_gotoIfError(clean, String_appendx(&options, C8_createNyto(i)));

					//Base10 is limited to 1 U64.
					//We can immediately return this (as long as we clamp it)

					if (outputAsBase == 10) {
						
						U64 v = *(const U64*)tmp.ptr;

						if(b != 64)
							v &= ((U64)1 << b) - 1;

						_gotoIfError(clean, String_createDecx(v, false, &tmpString));
						_gotoIfError(clean, String_appendStringx(&outputString, tmpString));
						String_freex(&tmpString);
					}

					//We use a U64 per character, because if for example we generate 256 values (1 U8).
					//Then subdivide it over 120, we'd be left with 26 values.
					//This means that the first 26 characters would be picked 50% more often than others.
					//To keep this to a minimum, we use a U64, because in the same scenario, the error rate is super small.
					//We do this to avoid having to reroll the randomness.

					else for (U64 i = 0, j = Buffer_length(tmp); i < j; i += 8) {

						U64 v = *(const U64*)(tmp.ptr + i);
						v %= options.length;

						//Ensure we stay within our bit limit
						
						if(!i && b)
							switch (outputAsBase) {
								case 8:		if(b % 3) v &= (1 << (b % 3)) - 1;		break;
								case 16:	if(b % 4) v &= (1 << (b % 4)) - 1;		break;
								case 64:	if(b % 6) v &= (1 << (b % 6)) - 1;		break;
							}

						//

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
			Log_errorLn("Invalid argument -o <string>, file path expected.");
			return false;
		}

		errorString = "Couldn't write to output file";
		_gotoIfError(clean, File_write(outputFile, outputPath, 1 * SECOND));
	}

	else {
		Log_debugLn("Random operation returned:");
		Log_debug(ELogOptions_None, "%.*s", outputString.length, outputString.ptr);
	}

clean:

	if(err.genericError) {

		if(errorString)
			Log_errorLn(errorString);

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

Bool CLI_randData(ParsedArgs args) {
	return CLI_rand(args, RandType_Data);
}

Bool CLI_randNum(ParsedArgs args) {
	return CLI_rand(args, RandType_Num);
}

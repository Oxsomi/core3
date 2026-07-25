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

//tools/oxc3_cli/rand.c

#include "tools/oxc3_cli/cli.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/base/error.h"
#include "types/base/c8.h"
#include "types/base/mathi.h"
#include "types/base/mathf.h"
#include "types/base/string_mut.h"
#include "platforms/logx.h"
#include "platforms/platform.h"
#include "platforms/file.h"
#include "types/base/string_read.h"
#include "types/container/log.h"
#include "types/base/constants.h"

Bool CLI_rand(const ParsedArgs *args) {

	if(!args) return false;

	//Temp output

	Buffer outputFile = Buffer_createNull();
	Buffer tmp = Buffer_createNull();
	CharString outputString = CharString_createNull();
	const C8 *errorString = NULL;
	CharString tmpString = CharString_createNull();
	CharString options = CharString_createNull();
	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;
	const Allocator *alloc = Platform_instance->alloc;
	const CharString newLine = CharString_newLine();

	//Parse arguments

	U64 n = 1;

	if (args->parameters & EOperationHasParameter_CountArg) {

		CharString str = CharString_createNull();

		if (
			ParsedArgs_getArg(args, EOperationHasParameter_CountShift, &str).genericError ||
			!CharString_parseU64(str, &n) ||
			(n >> 32)
		) {
			errorString = "Invalid argument -number <string>, uint expected.";
			retError(clean, Error_invalidParameter(0, 0, "CLI_rand() -number <string>, uint expected."));
		}
	}

	U64 l = args->operation == EOperation_RandKey ? 32 : 1;

	if (args->parameters & EOperationHasParameter_Length) {

		CharString str = CharString_createNull();

		if (
			ParsedArgs_getArg(args, EOperationHasParameter_LengthShift, &str).genericError ||
			!CharString_parseU64(str, &l) ||
			(l >> 32)
		) {
			errorString = "Invalid argument -length <string>, uint expected.";
			retError(clean, Error_invalidParameter(0, 0, "CLI_rand() -length <string>, uint expected."));
		}
	}

	U64 b = 0;        //Unused except rand num

	if (args->parameters & EOperationHasParameter_Bit) {

		CharString str = CharString_createNull();

		if (
			ParsedArgs_getArg(args, EOperationHasParameter_BitShift, &str).genericError ||
			!CharString_parseU64(str, &b) ||
			(b >> 16)
		) {
			errorString = "Invalid argument -bits <string>, ushort expected.";
			retError(clean, Error_invalidParameter(0, 0, "CLI_rand() -bits <string>, ushort expected."));
		}
	}

	//Generate data

	U64 bytesToGenerate = l;
	U64 outputAsBase = 16;

	switch (args->operation) {

		case EOperation_RandKey:
			break;

		case EOperation_RandData:
			outputAsBase = args->parameters & EOperationHasParameter_Output ? 256 : 16;
			break;

		case EOperation_RandNum: {

			Bool hasMultiFlag = false;
			EOperationFlags local = args->flags & EOperationFlags_RandNum;

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
				errorString = "Invalid argument. Can only pick one base.";
				retError(clean, Error_invalidParameter(0, 0, "CLI_rand() Invalid argument. Can only pick one base."));
			}

			bytesToGenerate *= 8;        //Better probability distribution
			break;
		}

		case EOperation_RandChar:
			bytesToGenerate *= 8;        //Better probability distribution
			break;

		default:
			errorString = "Invalid operation";
			retError(clean, Error_invalidOperation(0, "CLI_rand() Invalid operation"));
	}

	//Default for random number. Default length to how many bytes it takes to represent the number.

	U64 maxLen = l;

	if (b) {

		switch (outputAsBase) {

			case 2:        maxLen = b;                    break;
			case 8:        maxLen = (b + 2) / 3;        break;
			case 16:    maxLen = (b + 3) >> 2;        break;
			case 64:    maxLen = (b + 5) / 6;        break;

			default: {

				if (b > 64) {
					errorString = "Decimal numbers can only support up to 64 bit (if -bits is set).";
					retError(clean, Error_invalidParameter(
						0, 0, "CLI_rand() Decimal numbers can only support up to 64 bit (if -bits is set)."
					));
				}

				F64 f = (F64)((U64)1 << U64_min(63, b));

				if(b == 64)        //Shift by 64 is illegal ofc
					f *= 2;

				maxLen = (U64) F64_ceil(F64_log10(f));
				break;
			}
		}

		if(args->parameters & EOperationHasParameter_Length)
			l = U64_min(l, maxLen);

		else l = maxLen;

		bytesToGenerate = l * 8;        //Better probability distribution
	}

	//Buffer

	if(outputAsBase == 256)
		gotoIfError3(clean, Buffer_createUninitializedBytes(n * bytesToGenerate, alloc, &outputFile, e_rr));

	Buffer outputFilePtr = Buffer_createRefFromBuffer(outputFile, false);

	for (U64 i = 0; i < n; ++i) {

		gotoIfError3(clean, Buffer_createUninitializedBytes(bytesToGenerate, alloc, &tmp, e_rr));

		if(!Buffer_csprng(tmp))
			retError(clean, Error_invalidOperation(0, "CLI_rand() Buffer_csprng failed"));

		if(outputAsBase == 256) {
			gotoIfError3(clean, Buffer_appendBuffer(&outputFilePtr, tmp, e_rr));
		}

		else {

			switch (args->operation) {

				default:
					errorString = "Invalid operation";
					retError(clean, Error_invalidOperation(0, "CLI_rand() Invalid operation"));

				case EOperation_RandKey:
				case EOperation_RandData:

					for(U64 j = 0, k = Buffer_length(tmp); j < k; ++j) {

						U8 v = tmp.ptr[j];
						U8 prefix = 2;

						if(outputAsBase == 16)
							gotoIfError3(clean, CharString_createHex(&(CharStringCreateNumber) {
								.v = v, .leadingZeros = 2, .allocator = alloc, .result = &tmpString
							}, e_rr));

						gotoIfError3(clean, CharString_popFrontCount(&tmpString, prefix, e_rr));
						gotoIfError3(clean, CharString_appendString(&outputString, &tmpString, alloc, e_rr));

						if(args->operation != EOperation_RandKey || bytesToGenerate != 32)
							if(j != (k - 1) && !((j + 1) & 15))
								gotoIfError3(clean, CharString_append(&outputString, ' ', alloc, e_rr));

						if(j != (k - 1) && !((j + 1) & 63) && bytesToGenerate != 64)
							gotoIfError3(clean, CharString_appendString(&outputString, &newLine, alloc, e_rr));

						CharString_free(&tmpString, alloc);
					}

					break;

				case EOperation_RandNum:
				case EOperation_RandChar: {

					//Build up all options

					//Random char

					if(args->operation == EOperation_RandChar) {

						Bool anyCharFlags = args->flags & EOperationFlags_RandChar;
						Bool pickAll = !anyCharFlags;

						if (args->parameters & EOperationHasParameter_Character) {

							CharString str = CharString_createNull();

							if (ParsedArgs_getArg(args, EOperationHasParameter_CharacterShift, &str).genericError) {
								errorString = "Invalid argument -chars <string>.";
								retError(clean, Error_invalidParameter(
									0, 0, "CLI_rand() Invalid argument -chars <string>."
								));
							}

							gotoIfError3(clean, CharString_appendString(&options, &str, alloc, e_rr));

							if(CharString_length(str))
								pickAll = false;
						}

						if(pickAll || anyCharFlags)
							for(C8 c = 0x21; c < 0x7F; ++c) {

								//Check what type of character to see if we should skip

								if (!pickAll) {

									if(
										C8_isUpperCase(c) &&
										!(args->flags & (
											EOperationFlags_Alpha | EOperationFlags_Uppercase | EOperationFlags_Alphanumeric
										))
									)
										continue;

									if(
										C8_isLowerCase(c) &&
										!(args->flags & (
											EOperationFlags_Alpha | EOperationFlags_Lowercase | EOperationFlags_Alphanumeric)
										)
									)
										continue;

									if(C8_isDec(c) && !(args->flags & (EOperationFlags_Number | EOperationFlags_Alphanumeric)))
										continue;

									if(!C8_isAlphaNumeric(c) && !(args->flags & EOperationFlags_Symbols))
										continue;
								}

								//Append

								gotoIfError3(clean, CharString_append(&options, c, alloc, e_rr));
							}
					}

					//Random number

					else for(U8 j = 0; j < (U8) outputAsBase; ++j)
						gotoIfError3(clean, CharString_append(&options, C8_createNyto(j), alloc, e_rr));

					//Base10 is limited to 1 U64.
					//We can immediately return this (as long as we clamp it)

					if (outputAsBase == 10) {

						U64 v = *(const U64*)tmp.ptr;

						if(b != 64)
							v &= ((U64)1 << b) - 1;

						gotoIfError3(clean, CharString_createDec(&(CharStringCreateNumber) {
							.v = v, .leadingZeros = false, .allocator = alloc, .result = &tmpString
						}, e_rr));
						gotoIfError3(clean, CharString_appendString(&outputString, &tmpString, alloc, e_rr));
						CharString_free(&tmpString, alloc);
					}

					//We use a U64 per character, because if for example we generate 256 values (1 U8).
					//Then subdivide it over 120, we'd be left with 26 values.
					//This means that the first 26 characters would be picked 50% more often than others.
					//To keep this to a minimum, we use a U64, because in the same scenario, the error rate is tiny.
					//We do this to avoid having to re-roll the randomness.

					else for (U64 k = 0, j = Buffer_length(tmp); k < j; k += 8) {

						U64 v = *(const U64*)(tmp.ptr + k);
						v %= CharString_length(options);

						//Ensure we stay within our bit limit

						if(!k && b)
							switch (outputAsBase) {
								case 8:        if(b % 3) v &= (1 << (b % 3)) - 1;        break;
								case 16:    if(b % 4) v &= (1 << (b % 4)) - 1;        break;
								case 64:    if(b % 6) v &= (1 << (b % 6)) - 1;        break;
							}

						gotoIfError3(clean, CharString_append(&outputString, options.ptr[v], alloc, e_rr));
					}

					break;
				}
			}

			if(i != (n - 1) || !(args->parameters & EOperationHasParameter_Output))
				gotoIfError3(clean, CharString_appendString(&outputString, &newLine, alloc, e_rr));
		}

		Buffer_free(&tmp, alloc);
	}

	//Write to output

	if(outputString.ptr)
		outputFile = CharString_bufferConst(outputString);

	if (args->parameters & EOperationHasParameter_Output) {

		CharString outputPath = CharString_createNull();
		RefPtrType fhType = FileHandle_makeType(alloc);

		if (ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &outputPath).genericError) {
			errorString = "Invalid argument -output <string>, file path expected.";
			retError(clean, Error_invalidParameter(
				0, 0, "CLI_rand() Invalid argument -output <string>, file path expected."
			));
		}

		errorString = "Couldn't write to output file";
		gotoIfError3(clean, File_write(&outputFile, &outputPath, 0, 0, 1 * SECOND, true, &fhType, e_rr));
	}

	else {
		Log_debugLnx("Random operation returned:");
		Log_debugx(ELogOptions_None, "%.*s", CharString_length(outputString), outputString.ptr);
	}

clean:

	if(!s_uccess) {

		if(errorString)
			Log_errorLnx(errorString);

		else Error_print(alloc, &err, ELogLevel_Error, ELogOptions_NewLine);
	}

	CharString_free(&options, alloc);
	Buffer_free(&tmp, alloc);
	Buffer_free(&outputFile, alloc);
	CharString_free(&outputString, alloc);
	CharString_free(&tmpString, alloc);
	return s_uccess;
}

Bool CLI_randKey(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_rand(args);
}

Bool CLI_randChar(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_rand(args);
}

Bool CLI_randData(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_rand(args);
}

Bool CLI_randNum(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_rand(args);
}

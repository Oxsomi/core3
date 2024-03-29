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
#include "types/string.h"
#include "types/error.h"
#include "platforms/platform.h"
#include "platforms/log.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/listx.h"
#include "platforms/ext/stringx.h"
#include "cli.h"

void CLI_showHelp(EOperationCategory category, EOperation op, EFormat f) {

	Bool invalidOp = op == EOperation_Invalid;
	Bool invalidCat = category == EOperationCategory_Invalid;
	Bool invalidF = f == EFormat_Invalid;

	//Show all categories

	if (invalidCat) {

		Log_debug(ELogOptions_None, "All categories:\n\n");

		for(U64 i = EOperationCategory_Start; i < EOperationCategory_End; ++i)
			Log_debugLn(
				"%s: %s", 
				EOperationCategory_names[i - 1], EOperationCategory_description[i - 1]
			);

		return;
	}

	//Show all operators

	if(invalidOp) {

		Log_debug(ELogOptions_None, "All operations:\n\n");

		for(U64 i = 0; i < EOperation_Invalid; ++i) {

			Operation opVal = Operation_values[i];

			if(opVal.category != category)
				continue;

			Log_debugLn(
				"%s %s %s", 
				EOperationCategory_names[opVal.category - 1], opVal.name,
				opVal.isFormatLess ? "" : "-f <format> ...{format dependent syntax}"
			);

			Log_debug(ELogOptions_None, "%s\n\n", opVal.desc);
		}

		return;
	}

	//Show all options for that operation

	Operation opVal = Operation_values[op];

	if (invalidF && !opVal.isFormatLess) {

		Log_debugLn(
			"Please use syntax:\n%s %s -f <format> ...{format dependent syntax}", 
			EOperationCategory_names[category - 1],
			opVal.name
		);

		Log_debug(ELogOptions_None, "All formats:\n\n");

		for (U64 i = 0; i < EFormat_Invalid; ++i) {

			Bool containsCat = false;

			for(U64 j = 0; j < 4; ++j)
				if (Format_values[i].supportedCategories[j] == category) {
					containsCat = true;
					break;
				}

			if(!containsCat)
				continue;

			Log_debugLn("%s: %s", Format_values[i].name, Format_values[i].desc);
		}

		return;
	}

	//Show more about the current operation and format

	Log_debugLn(
		"Please use syntax:\n%s %s %s %s", 
		EOperationCategory_names[category - 1],
		opVal.name,
		opVal.isFormatLess ? "" : "-f",
		opVal.isFormatLess ? "" : Format_values[f].name
	);

	Format format = (Format) { 0 };

	if(!opVal.isFormatLess)
		format = Format_values[f];

	else format = (Format) {
		.optionalParameters = opVal.optionalParameters,
		.requiredParameters = opVal.requiredParameters,
		.operationFlags		= opVal.operationFlags
	};

	if(format.requiredParameters | format.optionalParameters) {

		Log_debug(ELogOptions_None, "With the following parameters:\n\n");

		for(U64 i = EOperationHasParameter_InputShift; i < EOperationHasParameter_Count; ++i)

			if (((format.requiredParameters | format.optionalParameters) >> i) & 1)
				Log_debugLn(
					"%s:\t%s\t%s",
					EOperationHasParameter_names[i],
					EOperationHasParameter_descriptions[i],
					(format.requiredParameters >> i) & 1 ? "\t(required)" : ""
				);
	}

	if((format.requiredParameters | format.optionalParameters) && format.operationFlags)
		Log_debugLn("");

	if(format.operationFlags) {

		Log_debug(ELogOptions_None, "With the following flags:\n\n");

		for(U64 i = EOperationFlags_None; i < EOperationFlags_Count; ++i)

			if ((format.operationFlags >> i) & 1) 
				Log_debugLn(
					"%s:\t%s",
					EOperationFlags_names[i],
					EOperationFlags_descriptions[i]
				);
	}
}

Bool CLI_helpOperation(ParsedArgs args) {

	Error err = Error_none();
	CharStringList split = (CharStringList) { 0 };

	if(args.parameters & EOperationHasParameter_Input)
		_gotoIfError(clean, CharString_splitx(*(const CharString*)args.args.ptr, ':', EStringCase_Sensitive, &split));

	if(split.length > 0)
		for (EOperationCategory cat = EOperationCategory_Start; cat < EOperationCategory_End; ++cat) {

			CharString catStr = CharString_createConstRefCStr(EOperationCategory_names[cat - 1]);

			if(CharString_equalsString(split.ptr[0], catStr, EStringCase_Insensitive)) {

				EOperation operation = EOperation_Invalid;

				//Find operation

				if(split.length > 1)
					for(U64 i = 0; i < EOperation_Invalid; ++i)

						if (
							cat == Operation_values[i].category &&
							CharString_equalsString(
								split.ptr[1], 
								CharString_createConstRefCStr(Operation_values[i].name),
								EStringCase_Insensitive
							)
						) {
							operation = i;
							break;
						}

				//Find format

				EFormat format = EFormat_Invalid;

				if (split.length > 2 && operation != EOperation_Invalid) {

					for(U64 i = 0; i < EFormat_Invalid; ++i)

						if (
							CharString_equalsString(
								split.ptr[2], 
								CharString_createConstRefCStr(Format_values[i].name),
								EStringCase_Insensitive
							)
						) {
							format = i;
							break;
						}

					Bool supportsFormat = false;

					if(format != EFormat_Invalid)
						for(U8 i = 0; i < 4; ++i)
							if (Format_values[format].supportedCategories[i] == cat) {
								supportsFormat = true;
								break;
							}

					if(!supportsFormat)
						format = EFormat_Invalid;
				}

				CLI_showHelp(cat, operation, format);
				goto clean;
			}
		}

	CLI_showHelp(EOperationCategory_Invalid, EOperation_Invalid, EFormat_Invalid);

clean:
	CharStringList_freex(&split);
	return !err.genericError;
}

Bool CLI_execute(CharStringList arglist) {

	//Show help

	if (arglist.length < 1) {
		CLI_showHelp(EOperationCategory_Invalid, EOperation_Invalid, EFormat_Invalid);
		return false;
	}

	//Grab category

	CharString arg0 = arglist.ptr[0];

	EOperationCategory category = EOperationCategory_Invalid;

	for(U64 i = EOperationCategory_Start; i < EOperationCategory_End; ++i)

		if (CharString_equalsString(
			arg0, 
			CharString_createConstRefCStr(EOperationCategory_names[i - 1]),
			EStringCase_Insensitive
		)) {
			category = i;
			break;
		}

	if (category == EOperationCategory_Invalid) {
		CLI_showHelp(EOperationCategory_Invalid, EOperation_Invalid, EFormat_Invalid);
		return false;
	}

	//Show help

	if (arglist.length < 2) {
		CLI_showHelp(category, EOperation_Invalid, EFormat_Invalid);
		return false;
	}

	//Grab operation

	CharString arg1 = arglist.ptr[1];

	EOperation operation = EOperation_Invalid;

	for(U64 i = 0; i < EOperation_Invalid; ++i)

		if (
			category == Operation_values[i].category &&
			CharString_equalsString(
				arg1, 
				CharString_createConstRefCStr(Operation_values[i].name),
				EStringCase_Insensitive
			)
		) {
			operation = i;
			break;
		}

	if (operation == EOperation_Invalid) {
		CLI_showHelp(category, EOperation_Invalid, EFormat_Invalid);
		return false;
	}

	//Parse command line options

	ParsedArgs args = (ParsedArgs) { 0 };
	args.args = List_createEmpty(sizeof(CharString));
	args.operation = operation;

	Error err = List_reservex(&args.args, 100);
	_gotoIfError(clean, err);

	//Grab all flags

	for(U64 i = 0; i < EOperationFlags_Count; ++i)
		for(U64 j = 2; j < arglist.length; ++j)

			if (CharString_equalsString(
				arglist.ptr[j],
				CharString_createConstRefCStr(EOperationFlags_names[i]),
				EStringCase_Insensitive
			)) {

				if ((args.flags >> i) & 1) {
					Log_errorLn("Duplicate flag: %.*s.", CharString_length(arglist.ptr[j]), arglist.ptr[j].ptr);
					goto clean;
				}

				args.flags |= (EOperationFlags)(1 << i);
				continue;			//Don't break, find duplicate arguments
			}

	//Grab all params

	args.format = EFormat_Invalid;

	for(U64 i = 0; i < EOperationHasParameter_Count; ++i)
		for(U64 j = 2; j < arglist.length; ++j)
			if (CharString_equalsString(
				arglist.ptr[j],
				CharString_createConstRefCStr(EOperationHasParameter_names[i]),
				EStringCase_Insensitive
			)) {

				EOperationHasParameter param = (EOperationHasParameter)(1 << i);

				//Check neighbor and save as list entry

				if (
					j + 1 >= arglist.length ||
					CharString_getAt(arglist.ptr[j + 1], 0) == '-'
				) {
					Log_errorLn("Parameter is missing argument: %.*s.", CharString_length(arglist.ptr[j]), arglist.ptr[j].ptr);
					goto clean;
				}

				else {

					//Parse file format

					if (param == EOperationHasParameter_FileFormat) {

						for (U64 k = 0; k < EFormat_Invalid; ++k)
							if (CharString_equalsString(
								arglist.ptr[j + 1],
								CharString_createConstRefCStr(Format_values[k].name),
								EStringCase_Insensitive
							)) {
								args.format = (EFormat) k;
								break;
							}

						break;
					}

					//Mark as present

					if (args.parameters & param) {
						Log_errorLn("Duplicate parameter: %.*s.", CharString_length(arglist.ptr[j]), arglist.ptr[j].ptr);
						goto clean;
					}

					args.parameters |= param;

					//Store param for parsing later

					_gotoIfError(clean, List_pushBackx(
						&args.args, 
						Buffer_createConstRef(&arglist.ptr[j + 1], sizeof(CharString))
					));
				}

				++j;			//Skip next argument
				continue;		//Don't break, we wanna detect duplicates!
			}

	//Check if format is supported

	Bool formatLess = Operation_values[args.operation].isFormatLess;
	Bool supportsFormat = formatLess;

	if(args.format != EFormat_Invalid)
		for(U8 i = 0; i < 4; ++i)
			if (Format_values[args.format].supportedCategories[i] == category) {
				supportsFormat = true;
				break;
			}

	//Invalid usage

	if(
		(args.format == EFormat_Invalid && !formatLess) || 
		!supportsFormat
	) {
		CLI_showHelp(category, operation, EFormat_Invalid);
		goto clean;
	}

	//Find invalid flags or parameters that couldn't be matched

	for (U64 j = 2; j < arglist.length; ++j) {

		if (CharString_getAt(arglist.ptr[j], 0) == '-') {

			//Check for parameters

			if (CharString_getAt(arglist.ptr[j], 1) != '-') {

				U64 i = 0;

				for (; i < EOperationHasParameter_Count; ++i)
					if (CharString_equalsString(
						arglist.ptr[j],
						CharString_createConstRefCStr(EOperationHasParameter_names[i]),
						EStringCase_Insensitive
					))
						break;

				if(i == EOperationHasParameter_Count) {
					Log_errorLn("Invalid parameter is present: %.*s.", CharString_length(arglist.ptr[j]), arglist.ptr[j].ptr);
					CLI_showHelp(category, operation, args.format);
					goto clean;
				}

				++j;
				continue;
			}

			//Check for flags

			U64 i = 0;

			for (; i < EOperationFlags_Count; ++i)
				if (CharString_equalsString(
					arglist.ptr[j],
					CharString_createConstRefCStr(EOperationFlags_names[i]),
					EStringCase_Insensitive
				))
					break;

			if(i == EOperationFlags_Count) {
				Log_errorLn("Invalid flag is present: %.*s.", CharString_length(arglist.ptr[j]), arglist.ptr[j].ptr);
				CLI_showHelp(category, operation, args.format);
				goto clean;
			}

			continue;
		}

		Log_errorLn("Invalid argument is present: %.*s", CharString_length(arglist.ptr[j]), arglist.ptr[j].ptr);
		CLI_showHelp(category, operation, args.format);
		goto clean;
	}

	//Check that parameters passed are valid

	Format format = (Format) { 0 };

	if(!formatLess)
		format = Format_values[args.format];

	else {

		Operation op = Operation_values[args.operation];

		format = (Format) {
			.optionalParameters = op.optionalParameters,
			.requiredParameters = op.requiredParameters,
			.operationFlags		= op.operationFlags
		};
	}

	EOperationHasParameter reqParam = format.requiredParameters;
	EOperationHasParameter optParam = format.optionalParameters;
	EOperationFlags opFlags			= format.operationFlags;

	//It must have all required params

	if ((args.parameters & reqParam) != reqParam) {
		Log_errorLn("Required parameter is missing.");
		CLI_showHelp(category, operation, args.format);
		goto clean;
	}

	//It has some parameter that we don't support

	if (args.parameters & ~(reqParam | optParam)) {
		Log_errorLn("Unsupported parameter is present.");
		CLI_showHelp(category, operation, args.format);
		goto clean;
	}

	//It has some flag we don't support

	if (args.flags & ~opFlags) {
		Log_errorLn("Unsupported flag is present.");
		CLI_showHelp(category, operation, args.format);
		goto clean;
	}

	//Now we can enter the function

	Bool res = Operation_values[operation].func(args);
	List_freex(&args.args);
	return res;

clean:

	if(err.genericError)
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);

	List_freex(&args.args);
	return false;
}

#include "types/buffer.h"
#include "types/string.h"
#include "types/error.h"
#include "platforms/platform.h"
#include "platforms/log.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/listx.h"
#include "cli.h"

void CLI_showHelp(EOperationCategory category, EOperation op, EFormat f) {

	Bool invalidOp = op == EOperation_Invalid;
	Bool invalidCat = category == EOperationCategory_Invalid;
	Bool invalidF = f == EFormat_Invalid;

	//Show all categories

	if (invalidCat) {

		Log_debug(String_createConstRefUnsafe("All categories:"), ELogOptions_NewLine);
		Log_debug(String_createNull(), ELogOptions_NewLine);

		for(U64 i = 0; i < EOperationCategory_Invalid; ++i) {

			Log_debug(
				String_createConstRefUnsafe(EOperationCategory_names[i]), 
				ELogOptions_None
			);

			Log_debug(String_createConstRefUnsafe(": "), ELogOptions_None);

			Log_debug(
				String_createConstRefUnsafe(EOperationCategory_description[i]), 
				ELogOptions_None
			);

			Log_debug(String_createNull(), ELogOptions_NewLine);
		}

		return;
	}

	//Show all operators

	if(invalidOp) {

		Log_debug(String_createConstRefUnsafe("All operations:"), ELogOptions_NewLine);
		Log_debug(String_createNull(), ELogOptions_NewLine);

		for(U64 i = 0; i < EOperation_Invalid; ++i) {

			if(Operation_values[i].category != category)
				continue;

			Log_debug(
				String_createConstRefUnsafe(EOperationCategory_names[Operation_values[i].category]), 
				ELogOptions_None
			);

			Log_debug(String_createConstRefUnsafe(" "), ELogOptions_None);

			Log_debug(
				String_createConstRefUnsafe(Operation_values[i].name), 
				ELogOptions_None
			);

			Log_debug(String_createConstRefUnsafe(" -f <format> ...{format dependent syntax}"), ELogOptions_NewLine);

			Log_debug(
				String_createConstRefUnsafe(Operation_values[i].desc), 
				ELogOptions_NewLine
			);

			Log_debug(String_createNull(), ELogOptions_NewLine);
		}

		return;
	}

	//Show all options for that operation

	if (invalidF) {

		Log_debug(String_createConstRefUnsafe("Please use syntax: "), ELogOptions_NewLine);

		Log_debug(
			String_createConstRefUnsafe(EOperationCategory_names[category]), 
			ELogOptions_None
		);

		Log_debug(String_createConstRefUnsafe(" "), ELogOptions_None);

		Log_debug(
			String_createConstRefUnsafe(Operation_values[op].name), 
			ELogOptions_None
		);

		Log_debug(String_createConstRefUnsafe(" -f <format> ...{format dependent syntax}"), ELogOptions_NewLine);

		Log_debug(String_createConstRefUnsafe("All formats:"), ELogOptions_NewLine);
		Log_debug(String_createNull(), ELogOptions_NewLine);

		for (U64 i = 0; i < EFormat_Invalid; ++i) {

			Log_debug(
				String_createConstRefUnsafe(Format_values[i].name), 
				ELogOptions_None
			);

			Log_debug(String_createConstRefUnsafe(": "), ELogOptions_NewLine);

			Log_debug(
				String_createConstRefUnsafe(Format_values[i].desc), 
				ELogOptions_NewLine
			);

			Log_debug(String_createNull(), ELogOptions_NewLine);
		}

		return;
	}

	//Show more about the current operation and format

	Log_debug(String_createConstRefUnsafe("Please use syntax: "), ELogOptions_NewLine);

	Log_debug(
		String_createConstRefUnsafe(EOperationCategory_names[category]), 
		ELogOptions_None
	);

	Log_debug(String_createConstRefUnsafe(" "), ELogOptions_None);

	Log_debug(
		String_createConstRefUnsafe(Operation_values[op].name), 
		ELogOptions_None
	);

	Log_debug(String_createConstRefUnsafe(" -f "), ELogOptions_None);

	Format format = Format_values[f];

	Log_debug(
		String_createConstRefUnsafe(format.name), 
		ELogOptions_NewLine
	);

	Log_debug(String_createConstRefUnsafe("With the following parameters:"), ELogOptions_NewLine);
	Log_debug(String_createNull(), ELogOptions_NewLine);

	for(U64 i = EOperationHasParameter_InputShift; i < EOperationHasParameter_Count; ++i)

		if (((format.requiredParameters | format.optionalParameters) >> i) & 1) {

			Log_debug(String_createConstRefUnsafe(EOperationHasParameter_names[i]), ELogOptions_None);

			Log_debug(String_createConstRefUnsafe(":\t"), ELogOptions_None);

			Log_debug(String_createConstRefUnsafe(EOperationHasParameter_descriptions[i]), ELogOptions_None);

			Bool req = (format.requiredParameters >> i) & 1;

			Log_debug(String_createConstRefUnsafe(req ? "\t(required)" : ""), ELogOptions_NewLine);
		}


	Log_debug(String_createNull(), ELogOptions_NewLine);
	Log_debug(String_createConstRefUnsafe("With the following flags:"), ELogOptions_NewLine);
	Log_debug(String_createNull(), ELogOptions_NewLine);

	for(U64 i = EOperationFlags_None; i < EOperationFlags_Count; ++i)

		if ((format.operationFlags >> i) & 1) {

			Log_debug(String_createConstRefUnsafe(EOperationFlags_names[i]), ELogOptions_None);

			Log_debug(String_createConstRefUnsafe(":\t"), ELogOptions_None);

			Log_debug(String_createConstRefUnsafe(EOperationFlags_descriptions[i]), ELogOptions_NewLine);
		}
}

Bool CLI_execute(StringList arglist) {

	Bool success = true;

	//Show help

	if (Platform_instance.args.length < 1) {
		CLI_showHelp(EOperationCategory_Invalid, EOperation_Invalid, EFormat_Invalid);
		return false;
	}

	//Grab category

	String arg0 = Platform_instance.args.ptr[0];

	EOperationCategory category = EOperationCategory_Invalid;

	for(U64 i = 0; i < EOperationCategory_Invalid; ++i)

		if (String_equalsString(
			arg0, 
			String_createConstRefUnsafe(EOperationCategory_names[i]),
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

	if (Platform_instance.args.length < 2) {
		CLI_showHelp(category, EOperation_Invalid, EFormat_Invalid);
		return false;
	}

	//Grab operation

	String arg1 = Platform_instance.args.ptr[1];

	EOperation operation = EOperation_Invalid;

	for(U64 i = 0; i < EOperation_Invalid; ++i)

		if (String_equalsString(
			arg1, 
			String_createConstRefUnsafe(Operation_values[i].name),
			EStringCase_Insensitive
		)) {
			operation = i;
			break;
		}

	if (operation == EOperation_Invalid) {
		CLI_showHelp(category, EOperation_Invalid, EFormat_Invalid);
		return false;
	}

	//Parse command line options

	ParsedArgs args = (ParsedArgs) { 0 };
	args.args = List_createEmpty(sizeof(String));

	Error err = List_reservex(&args.args, 100);
	_gotoIfError(clean, err);

	//Grab all flags

	for(U64 i = 0; i < EOperationFlags_Count; ++i)
		for(U64 j = 2; j < Platform_instance.args.length; ++j)

			if (String_equalsString(
				Platform_instance.args.ptr[j],
				String_createConstRefUnsafe(EOperationFlags_names[i]),
				EStringCase_Insensitive
			)) {

				if ((args.flags >> i) & 1) {
					Log_debug(String_createConstRefUnsafe("Duplicate flag: "), ELogOptions_None);
					Log_debug(Platform_instance.args.ptr[j], ELogOptions_NewLine);
					_gotoIfError(clean, Error_alreadyDefined(0));
				}

				args.flags |= (EOperationFlags)(1 << i);
				continue;			//Don't break, find duplicate arguments
			}

	//Grab all params

	args.format = EFormat_Invalid;

	for(U64 i = 0; i < EOperationHasParameter_Count; ++i)
		for(U64 j = 2; j < Platform_instance.args.length; ++j)
			if (String_equalsString(
				Platform_instance.args.ptr[j],
				String_createConstRefUnsafe(EOperationHasParameter_names[i]),
				EStringCase_Insensitive
			)) {

				EOperationHasParameter param = (EOperationHasParameter)(1 << i);

				//Check neighbor and save as list entry

				if (
					j + 1 >= Platform_instance.args.length ||
					String_getAt(Platform_instance.args.ptr[j + 1], 0) == '-'
				) {
					Log_debug(String_createConstRefUnsafe("Parameter is missing argument: "), ELogOptions_None);
					Log_debug(Platform_instance.args.ptr[j], ELogOptions_NewLine);
					_gotoIfError(clean, Error_outOfBounds(0, 0, j + 1, Platform_instance.args.length));
				}

				else {

					//Parse file format

					if (param == EOperationHasParameter_FileFormat) {

						for (U64 k = 0; k < EFormat_Invalid; ++k)
							if (String_equalsString(
								Platform_instance.args.ptr[j + 1],
								String_createConstRefUnsafe(Format_values[k].name),
								EStringCase_Insensitive
							)) {
								args.format = (EFormat) k;
								break;
							}

						break;
					}

					//Mark as present

					if (args.parameters & param) {
						Log_debug(String_createConstRefUnsafe("Duplicate parameter: "), ELogOptions_None);
						Log_debug(Platform_instance.args.ptr[j], ELogOptions_NewLine);
						_gotoIfError(clean, Error_alreadyDefined(0));
					}

					args.parameters |= param;

					//Store param for parsing later

					_gotoIfError(clean, List_pushBackx(
						&args.args, 
						Buffer_createConstRef(&Platform_instance.args.ptr[j + 1], sizeof(String))
					));
				}

				++j;			//Skip next argument
				continue;		//Don't break, we wanna detect duplicates!
			}

	//Invalid usage

	if(args.format == EFormat_Invalid) {
		CLI_showHelp(category, operation, EFormat_Invalid);
		_gotoIfError(clean, Error_invalidOperation(0));
	}

	//Find invalid flags or parameters that couldn't be matched

	for (U64 j = 2; j < Platform_instance.args.length; ++j) {

		if (String_getAt(Platform_instance.args.ptr[j], 0) == '-') {

			//Check for parameters

			if (String_getAt(Platform_instance.args.ptr[j], 1) != '-') {

				U64 i = 0;

				for (; i < EOperationHasParameter_Count; ++i)
					if (String_equalsString(
						Platform_instance.args.ptr[j],
						String_createConstRefUnsafe(EOperationHasParameter_names[i]),
						EStringCase_Insensitive
					))
						break;

				if(i == EOperationHasParameter_Count) {
					Log_debug(String_createConstRefUnsafe("Invalid parameter is present: "), ELogOptions_None);
					Log_debug(Platform_instance.args.ptr[j], ELogOptions_NewLine);
					CLI_showHelp(category, operation, args.format);
					_gotoIfError(clean, Error_invalidParameter((U32)j, 0, 0));
				}

				++j;
				continue;
			}

			//Check for flags

			U64 i = 0;

			for (; i < EOperationFlags_Count; ++i)
				if (String_equalsString(
					Platform_instance.args.ptr[j],
					String_createConstRefUnsafe(EOperationFlags_names[i]),
					EStringCase_Insensitive
				))
					break;

			if(i == EOperationFlags_Count) {
				Log_debug(String_createConstRefUnsafe("Invalid flag is present: "), ELogOptions_None);
				Log_debug(Platform_instance.args.ptr[j], ELogOptions_NewLine);
				CLI_showHelp(category, operation, args.format);
				_gotoIfError(clean, Error_invalidParameter((U32)j, 0, 0));
			}

			continue;
		}

		Log_debug(String_createConstRefUnsafe("Invalid argument is present: "), ELogOptions_None);
		Log_debug(Platform_instance.args.ptr[j], ELogOptions_NewLine);
		CLI_showHelp(category, operation, args.format);
		_gotoIfError(clean, Error_invalidParameter((U32)j, 0, 0));
	}

	//Check that parameters passed are valid

	Format f = Format_values[args.format];

	//It must have all required params

	if ((args.parameters & f.requiredParameters) != f.requiredParameters) {
		Log_debug(String_createConstRefUnsafe("Required parameter is missing."), ELogOptions_NewLine);
		CLI_showHelp(category, operation, args.format);
		_gotoIfError(clean, Error_invalidOperation(1));
	}

	//It has some parameter that we don't support

	if (args.parameters & ~(f.requiredParameters | f.optionalParameters)) {
		Log_debug(String_createConstRefUnsafe("Unsupported parameter is present."), ELogOptions_NewLine);
		CLI_showHelp(category, operation, args.format);
		_gotoIfError(clean, Error_invalidOperation(2));
	}

	//It has some flag we don't support

	if (args.parameters & ~f.operationFlags) {
		Log_debug(String_createConstRefUnsafe("Unsupported flag is present."), ELogOptions_NewLine);
		CLI_showHelp(category, operation, args.format);
		_gotoIfError(clean, Error_invalidOperation(3));
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

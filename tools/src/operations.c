#include "types/error.h"
#include "types/buffer.h"
#include "types/string.h"
#include "cli.h"

//Parameters

const C8 *EOperationHasParameter_names[] = {
	"-f",
	"-i",
	"-o",
	"-aes",
	"-split"
};

const C8 *EOperationHasParameter_descriptions[] = {
	"File format",
	"Input file/folder (relative)",
	"Output file/folder (relative)",
	"Encryption key (32-byte, hex or Nyto)",
	"Split by character (defaulted to newline)"
};

//Flags

const C8 *EOperationFlags_names[] = {
	"--sha256",
	"--uncompressed",
	"--ascii",
	"--utf8",
	"--full-date",
	"--date",
	"--not-recursive"
};

const C8 *EOperationFlags_descriptions[] = {
	"Includes 256-bit hashes instead of 32-bit ones into file if applicable.",
	"Keep the data uncompressed (default is compressed).",
	"Indicates the input files should be threated as ASCII. If 1 file; splits by enter, otherwise 1 entry/file.",
	"Indicates the input files should be threated as UTF8. If 1 file; splits by enter, otherwise 1 entry/file.",
	"Includes full file timestamp (Ns)",
	"Includes MS-DOS timestamp (YYYY-MM-dd HH-mm-ss (each two seconds))",
	"If folder is selected, blocks recursive file searching. Can be handy if only the direct directory should be included.",
};

//Operations

const C8 *EOperationCategory_names[] = {
	"convert"
};

const C8 *EOperationCategory_description[] = {
	"Converting between OxC3 types and non native types."
};

Operation Operation_values[2];
Format Format_values[2];

void Operations_init() {

	Format_values[EFormat_oiCA] = (Format) {
		.name = "oiCA",
		.desc = "Oxsomi Compressed Archive; a file table with file data.",
		.operationFlags = EOperationFlags_Default | EOperationFlags_Date | EOperationFlags_FullDate,
		.optionalParameters = EOperationHasParameter_AES,
		.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_Output,
		.flags = EFormatFlags_SupportFolders
	},

	Format_values[EFormat_oiDL] = (Format) { 
		.name = "oiDL",
		.desc = "Oxsomi Data List; an indexed list of data, can be text (ASCII/UTF8) or binary data.",
		.operationFlags = EOperationFlags_Default | EOperationFlags_Ascii | EOperationFlags_UTF8,
		.optionalParameters = EOperationHasParameter_AES | EOperationHasParameter_SplitBy,
		.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_Output,
		.flags = EFormatFlags_SupportFiles | EFormatFlags_SupportFolders
	},

	Operation_values[EOperation_ConvertTo] = (Operation) { 
		.category = EOperationCategory_Convert, 
		.name = "to", 
		.desc = "Converting from a non native file format to a native file format.", 
		.func = &CLI_convertTo
	};

	Operation_values[EOperation_ConvertFrom] = (Operation) { 
		.category = EOperationCategory_Convert, 
		.name = "from", 
		.desc = "Converting to a non native file format from a native file format.", 
		.func = &CLI_convertFrom
	};
}

Error ParsedArgs_getArg(ParsedArgs args, EOperationHasParameter parameterId, String *arg) {

	if(!arg)
		return Error_nullPointer(2, 0);

	if(!parameterId)
		return Error_unsupportedOperation(0);

	if(!((args.parameters >> parameterId) & 1))
		return Error_notFound(0, 1, 0);

	U64 ourLoc = 0;

	for(U64 j = EOperationHasParameter_InputShift; j < parameterId; ++j)
		if((args.parameters >> j) & 1)
			++ourLoc;

	Buffer res = Buffer_createNull();
	Error err = List_get(args.args, ourLoc, &res);

	if(err.genericError)
		return err;

	*arg = *(String*)res.ptr;
	return Error_none();
}

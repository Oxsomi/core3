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
	"-split",
	"-n",
	"-l",
	"-c",
	"-b",
	"-e",
	"-s"
};

const C8 *EOperationHasParameter_descriptions[] = {
	"File format",
	"Input file/folder (relative)",
	"Output file/folder (relative)",
	"Encryption key (32-byte hex)",
	"Split by character (defaulted to newline)",
	"Number of elements",
	"Length of each element",
	"Characters to include",
	"Bit count",
	"Entry index or path",
	"Start offset"
};

//Flags

const C8 *EOperationFlags_names[] = {
	"--sha256",
	"--uncompressed",
	"--ascii",
	"--utf8",
	"--full-date",
	"--date",
	"--not-recursive",
	"--alpha",
	"--alphanumeric",
	"--numbers",
	"--symbols",
	"--lowercase",
	"--uppercase",
	"--nyto",
	"--hex",
	"--bin",
	"--oct"
};

const C8 *EOperationFlags_descriptions[] = {
	"Includes 256-bit hashes instead of 32-bit ones into file if applicable.",
	"Keep the data uncompressed (default is compressed).",
	"Indicates the input files should be threated as ASCII. If 1 file; splits by enter, otherwise 1 entry/file.",
	"Indicates the input files should be threated as UTF8. If 1 file; splits by enter, otherwise 1 entry/file.",
	"Includes full file timestamp (Ns)",
	"Includes MS-DOS timestamp (YYYY-MM-dd HH-mm-ss (each two seconds))",
	"If folder is selected, blocks recursive file searching. Can be handy if only the direct directory should be included.",
	"Include alpha characters (A-Za-z).",
	"Include alphanumeric characters (A-Za-z0-9).",
	"Include number characters (0-9).",
	"Include symbols (<0x20, 0x7F> excluding alphanumeric).",
	"Include lower alpha characters (a-z).",
	"Include upper alpha characters (A-Z).",
	"Encode using nytodecimal (0-9A-Za-z_$).",
	"Encode using hexadecimal (0-9A-F).",
	"Encode using binary (0-1).",
	"Encode using octadecimal (0-7)."
};

//Operations

const C8 *EOperationCategory_names[] = {
	"file",
	"hash",
	"rand"
};

const C8 *EOperationCategory_description[] = {
	"File utilities such as file conversions, encryption, compression, etc.",
	"Converting a file or string to a hash.",
	"Generating random data."
};

Operation Operation_values[12];
Format Format_values[4];

void Operations_init() {

	//Convert operation

	Format_values[EFormat_oiCA] = (Format) {
		.name = "oiCA",
		.desc = "Oxsomi Compressed Archive; a file table with file data.",
		.operationFlags = EOperationFlags_Default | EOperationFlags_Date | EOperationFlags_FullDate,
		.optionalParameters = EOperationHasParameter_AES,
		.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_Output,
		.flags = EFormatFlags_SupportFiles | EFormatFlags_SupportFolders,
		.supportedCategories = { EOperationCategory_File }
	};

	Format_values[EFormat_oiDL] = (Format) { 
		.name = "oiDL",
		.desc = "Oxsomi Data List; an indexed list of data, can be text (ASCII/UTF8) or binary data.",
		.operationFlags = EOperationFlags_Default | EOperationFlags_Ascii | EOperationFlags_UTF8,
		.optionalParameters = EOperationHasParameter_AES | EOperationHasParameter_SplitBy,
		.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_Output,
		.flags = EFormatFlags_SupportFiles | EFormatFlags_SupportFolders,
		.supportedCategories = { EOperationCategory_File }
	};

	Operation_values[EOperation_FileTo] = (Operation) { 
		.category = EOperationCategory_File, 
		.name = "to", 
		.desc = "Converting from a non native file format to a native file format.", 
		.func = &CLI_convertTo
	};

	Operation_values[EOperation_FileFrom] = (Operation) { 
		.category = EOperationCategory_File, 
		.name = "from", 
		.desc = "Converting to a non native file format from a native file format.", 
		.func = &CLI_convertFrom
	};

	//Encryption

	Operation_values[EOperation_FileEncr] = (Operation) { 

		.category = EOperationCategory_File, 
		.name = "encr", 
		.desc = "Encrypt a file or folder.", 
		.func = &CLI_encryptDo,

		.isFormatLess = true,

		.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_AES,
		.optionalParameters = EOperationHasParameter_Output
	};

	Operation_values[EOperation_FileDecr] = (Operation) { 

		.category = EOperationCategory_File, 
		.name = "decr", 
		.desc = "Decrypt a file or folder.", 
		.func = &CLI_encryptUndo,

		.isFormatLess = true,

		.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_AES | EOperationHasParameter_Output
	};

	//Inspection

	Operation_values[EOperation_FileHeader] = (Operation) { 

		.category = EOperationCategory_File, 
		.name = "header", 
		.desc = "Inspect the file header of oiXX files.", 
		.func = &CLI_inspectHeader,

		.isFormatLess = true,
		.requiredParameters = EOperationHasParameter_Input
	};

	Operation_values[EOperation_FileData] = (Operation) { 

		.category = EOperationCategory_File, 
		.name = "data", 
		.desc = "Inspect the file data of oiXX files.", 
		.func = &CLI_inspectData,

		.isFormatLess = true,
		.requiredParameters = EOperationHasParameter_Input,
		.optionalParameters = 
			EOperationHasParameter_AES | EOperationHasParameter_Output | 
			EOperationHasParameter_Entry | EOperationHasParameter_StartOffset | EOperationHasParameter_Length
	};

	//Hash category

	Format_values[EFormat_CRC32C] = (Format) {
		.name = "CRC32C",
		.desc = "CRC32 Castagnoli (32-bit hash)",
		.operationFlags = EOperationFlags_None,
		.optionalParameters = EOperationHasParameter_None,
		.requiredParameters = EOperationHasParameter_Input,
		.flags = EFormatFlags_SupportFiles | EFormatFlags_SupportAsString,
		.supportedCategories = { EOperationCategory_Hash }
	};

	Format_values[EFormat_SHA256] = (Format) { 
		.name = "SHA256",
		.desc = "SHA256 (256-bit hash)",
		.operationFlags = EOperationFlags_None,
		.optionalParameters = EOperationHasParameter_None,
		.requiredParameters = EOperationHasParameter_Input,
		.flags = EFormatFlags_SupportFiles | EFormatFlags_SupportAsString,
		.supportedCategories = { EOperationCategory_Hash }
	};

	Operation_values[EOperation_HashString] = (Operation) { 
		.category = EOperationCategory_Hash, 
		.name = "string", 
		.desc = "Hashing a string.", 
		.func = &CLI_hashString
	};

	Operation_values[EOperation_HashFile] = (Operation) { 
		.category = EOperationCategory_Hash, 
		.name = "file", 
		.desc = "Hashing a file.", 
		.func = &CLI_hashFile
	};

	//Random operations

	Operation_values[EOperation_RandKey] = (Operation) { 

		.category = EOperationCategory_Rand, 

		.name = "key", 
		.desc = "Generating a key for AES256 (or other purposes), in hex format.", 

		.func = &CLI_randKey,

		.isFormatLess = true,

		.optionalParameters = EOperationHasParameter_Number | EOperationHasParameter_Length | EOperationHasParameter_Output
	};

	Operation_values[EOperation_RandChar] = (Operation) { 

		.category = EOperationCategory_Rand, 

		.name = "char", 
		.desc = "Generating a random sequence of characters.", 

		.func = &CLI_randChar,

		.isFormatLess = true,

		.optionalParameters = 
			EOperationHasParameter_Number | EOperationHasParameter_Length | 
			EOperationHasParameter_Output | EOperationHasParameter_Character,
		
		.operationFlags = EOperationFlags_RandChar
	};

	Operation_values[EOperation_RandData] = (Operation) { 

		.category = EOperationCategory_Rand, 

		.name = "data", 
		.desc = "Generating random bytes. As hexdump if no output is specified.", 

		.func = &CLI_randData,

		.isFormatLess = true,

		.optionalParameters = EOperationHasParameter_Number | EOperationHasParameter_Length | EOperationHasParameter_Output
	};

	Operation_values[EOperation_RandNum] = (Operation) { 

		.category = EOperationCategory_Rand, 

		.name = "num", 
		.desc = "Generating random numbers (in text form).", 

		.func = &CLI_randNum,

		.isFormatLess = true,

		.optionalParameters = 
			EOperationHasParameter_Number | EOperationHasParameter_Length | EOperationHasParameter_Output | 
			EOperationHasParameter_Bit,

		.operationFlags = EOperationFlags_RandNum
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

/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "tools/cli.h"
#include "types/base/error.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "platforms/log.h"

Bool CLI_info(ParsedArgs args) {

	(void)args;

	Log_debugLnx(

		"OxC3 (Oxsomi core %"PRIu32".%"PRIu32".%03"PRIu32"), a general framework and toolset for cross-platform applications.\n"
		"Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)"
		"%s",

		(U32) (OXC3_MAJOR + 3),
		(U32) OXC3_MINOR,
		(U32) OXC3_PATCH,

		args.operation != EOperation_InfoLicense ? "" :
		"\n\n"
		"This program is free software: you can redistribute it and/or modify\n"
		"it under the terms of the GNU General Public License as published by\n"
		"the Free Software Foundation, either version 3 of the License, or\n"
		"(at your option) any later version.\n"
		"\n"
		"This program is distributed in the hope that it will be useful,\n"
		"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		"GNU General Public License for more details.\n"
		"\n"
		"You should have received a copy of the GNU General Public License\n"
		"along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.\n"
		"Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.\n"
		"To prevent this a separate license will have to be requested at contact@osomi.net for a premium;\n"
		"This is called dual licensing."
	);

	return true;
}

//Parameters

const C8 *EOperationHasParameter_names[] = {
	"-format",
	"-input",
	"-output",
	"-aes",
	"-split",
	"-count",
	"-length",
	"-chars",
	"-bits",
	"-entry",
	"-start",
	"-compile-output",
	"-threads",
	"-compile-type",
	"-include-dir",
	"-input2",
	"-graphics-api"
};

const C8 *EOperationHasParameter_descriptions[] = {
	"File format",
	"Input string or path (relative)",
	"Output path (relative)",
	"Encryption key (32-byte hex)",
	"Split by character (defaulted to newline) or split audio source (left/right/combine)",
	"Number of elements",
	"Length of each element",
	"Characters to include",
	"Bit count",
	"Entry index or path",
	"Start offset",
	"Shader output mode (spv, dxil or all; also allows multiple such as dxil,spv)",
	"Thread count (0 = all, 50% = 50% of all threads, 4 = 4 threads)",
	"Shader compile mode (preprocess, includes, reflect, compile)",
	"Set extra include path",
	"Input file to merge with",
	"Graphics api to use. Default is either all or the native one depending on command."
};

//Flags

const C8 *EOperationFlags_names[EOperationFlags_Count] = {
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
	"--oct",
	"--debug",
	"--ignore-empty-files",
	"--includes",
	"--split",
	"--warn-unused-registers",
	"--warn-unused-constants",
	"--warn-buffer-padding",
	"--verbose"
};

const C8 *EOperationFlags_descriptions[EOperationFlags_Count] = {
	"Includes 256-bit hashes instead of 32-bit ones into file if applicable.",
	"Keep the data uncompressed (default is compressed).",
	"Indicates the input files should be treated as ASCII. If 1 file; splits by enter, otherwise 1 entry/file.",
	"Indicates the input files should be treated as UTF8. If 1 file; splits by enter, otherwise 1 entry/file.",
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
	"Binary mode.",
	"Encode using octadecimal (0-7).",
	"Include more debug information.",
	"Ignore error when an empty source file is encountered.",
	"Display includes.",
	"Split up every binary target into its own oiSH file (.dxil.oiSH, .spv.oiSH, etc.).",
	"Warn when unused registers are present in the final binary.",
	"Warn when unused constants are present in the final binary.",
	"Warn when buffer padding is present in the final binary.",
	"Print full information to the console."
};

//Operations

const C8 *EOperationCategory_names[] = {

	"file",

	#ifdef CLI_SHADER_COMPILER
		"compile",
	#endif

	#ifdef CLI_GRAPHICS
		"graphics",
	#endif

	"audio",

	"hash",
	"rand",
	"info",
	"profile",
	"help"
};

const C8 *EOperationCategory_description[] = {

	"File utilities such as file conversions, encryption, compression, etc.",

	#ifdef CLI_SHADER_COMPILER
		"Compile shaders or to intermediate binary (Chimera).",
	#endif

	#ifdef CLI_GRAPHICS
		"Graphics operations such as showing devices.",
	#endif

	"Audio operations such as showing devices.",

	"Converting a file or string to a hash.",
	"Generating random data.",
	"Information about the tool.",
	"Profiles operations on the current system.",
	"Help about the instructions in the tool."
};

Operation Operation_values[EOperation_Invalid];
Format Format_values[EFormat_Invalid];

void Operations_init() {

	//Convert operation

	Format_values[EFormat_oiCA] = (Format) {
		.name = "oiCA",
		.desc = "Oxsomi Compressed Archive; a file table with file data.",
		.operationFlags = EOperationFlags_Default | EOperationFlags_Date | EOperationFlags_FullDate,
		.optionalParameters = EOperationHasParameter_AES | EOperationHasParameter_Input2,
		.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_Output,
		.flags = EFormatFlags_SupportFiles | EFormatFlags_SupportFolders,
		.supportedCategories = { EOperationCategory_File }
	};

	Format_values[EFormat_oiDL] = (Format) {
		.name = "oiDL",
		.desc = "Oxsomi Data List; an indexed list of data, can be text (ASCII/UTF8) or binary data.",
		.operationFlags = EOperationFlags_Default | EOperationFlags_Ascii | EOperationFlags_UTF8,
		.optionalParameters = EOperationHasParameter_AES | EOperationHasParameter_SplitBy | EOperationHasParameter_Input2,
		.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_Output,
		.flags = EFormatFlags_SupportFiles | EFormatFlags_SupportFolders,
		.supportedCategories = { EOperationCategory_File }
	};

	Format_values[EFormat_oiSH] = (Format) {
		.name = "oiSH",
		.desc = "Oxsomi SHader; compiled shader binaries by entrypoint and metadata.",
		.operationFlags = EOperationFlags_None,
		.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_Output | EOperationHasParameter_Input2,
		.flags = EFormatFlags_SupportFiles,
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

	Operation_values[EOperation_FileCombine] = (Operation) {
		.category = EOperationCategory_File,
		.name = "combine",
		.desc = "Combine multiple files of one type into one.",
		.func = &CLI_fileCombine
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
		.operationFlags = EOperationFlags_Bin | EOperationFlags_Includes,
		.requiredParameters = EOperationHasParameter_Input,
		.optionalParameters =
			EOperationHasParameter_AES | EOperationHasParameter_Output |
			EOperationHasParameter_Entry | EOperationHasParameter_StartOffset | EOperationHasParameter_Length |
			EOperationHasParameter_ShaderOutputMode
	};

	//Hash category

	Format_values[EFormat_CRC32C] = (Format) {
		.name = "CRC32C",
		.desc = "CRC32 Castagnoli (32-bit hash)",
		.operationFlags = EOperationFlags_None,
		.optionalParameters = EOperationHasParameter_None,
		.requiredParameters = EOperationHasParameter_Input,
		.flags = EFormatFlags_SupportFiles | EFormatFlags_SupportFolders | EFormatFlags_SupportAsString,
		.supportedCategories = { EOperationCategory_Hash }
	};

	Format_values[EFormat_MD5] = (Format) {
		.name = "MD5",
		.desc = "MD5 (128-bit hash)",
		.operationFlags = EOperationFlags_None,
		.optionalParameters = EOperationHasParameter_None,
		.requiredParameters = EOperationHasParameter_Input,
		.flags = EFormatFlags_SupportFiles | EFormatFlags_SupportFolders | EFormatFlags_SupportAsString,
		.supportedCategories = { EOperationCategory_Hash }
	};

	Format_values[EFormat_FNV1A64] = (Format) {
		.name = "FNV1A64",
		.desc = "Fowler-Noll-Vo-1A 64-bit hash",
		.operationFlags = EOperationFlags_None,
		.optionalParameters = EOperationHasParameter_None,
		.requiredParameters = EOperationHasParameter_Input,
		.flags = EFormatFlags_SupportFiles | EFormatFlags_SupportFolders | EFormatFlags_SupportAsString,
		.supportedCategories = { EOperationCategory_Hash }
	};

	Format_values[EFormat_SHA256] = (Format) {
		.name = "SHA256",
		.desc = "SHA256 (256-bit hash)",
		.operationFlags = EOperationFlags_None,
		.optionalParameters = EOperationHasParameter_None,
		.requiredParameters = EOperationHasParameter_Input,
		.flags = EFormatFlags_SupportFiles | EFormatFlags_SupportFolders | EFormatFlags_SupportAsString,
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

		.optionalParameters = EOperationHasParameter_CountArg | EOperationHasParameter_Length | EOperationHasParameter_Output
	};

	Operation_values[EOperation_RandChar] = (Operation) {

		.category = EOperationCategory_Rand,

		.name = "char",
		.desc = "Generating a random sequence of characters.",

		.func = &CLI_randChar,

		.isFormatLess = true,

		.optionalParameters =
			EOperationHasParameter_CountArg | EOperationHasParameter_Length |
			EOperationHasParameter_Output | EOperationHasParameter_Character,

		.operationFlags = EOperationFlags_RandChar
	};

	Operation_values[EOperation_RandData] = (Operation) {

		.category = EOperationCategory_Rand,

		.name = "data",
		.desc = "Generating random bytes. As hexdump if no output is specified.",

		.func = &CLI_randData,

		.isFormatLess = true,

		.optionalParameters = EOperationHasParameter_CountArg | EOperationHasParameter_Length | EOperationHasParameter_Output
	};

	Operation_values[EOperation_RandNum] = (Operation) {

		.category = EOperationCategory_Rand,

		.name = "num",
		.desc = "Generating random numbers (in text form).",

		.func = &CLI_randNum,

		.isFormatLess = true,

		.optionalParameters =
			EOperationHasParameter_CountArg | EOperationHasParameter_Length | EOperationHasParameter_Output |
			EOperationHasParameter_Bit,

		.operationFlags = EOperationFlags_RandNum
	};

	//Package file for virtual file system

	Operation_values[EOperation_Package] = (Operation) {

		.category = EOperationCategory_File,

		.name = "package",
		.desc = "Package files such as shaders, textures and models into an oiCA as Oxsomi file formats.",

		.func = &CLI_package,

		.isFormatLess = true,

		.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_Output,
		.optionalParameters =
			EOperationHasParameter_AES | EOperationHasParameter_ThreadCount |
			EOperationHasParameter_IncludeDir | EOperationHasParameter_ShaderOutputMode,

		.operationFlags =
			EOperationFlags_Debug | EOperationFlags_Split |
			EOperationFlags_CompilerWarnings | EOperationFlags_IgnoreEmptyFiles
	};

	//Compile shaders

	#ifdef CLI_SHADER_COMPILER

		Format_values[EFormat_HLSL] = (Format) {

			.name = "HLSL",
			.desc = "High Level Shading Language; Microsoft's shading language for DirectX and Vulkan.",

			.operationFlags =
				EOperationFlags_Debug | EOperationFlags_Split |
				EOperationFlags_CompilerWarnings | EOperationFlags_IgnoreEmptyFiles,

			.requiredParameters =
				EOperationHasParameter_Input | EOperationHasParameter_Output,

			.optionalParameters =
				EOperationHasParameter_ThreadCount | EOperationHasParameter_IncludeDir |
				EOperationHasParameter_ShaderCompileMode | EOperationHasParameter_ShaderOutputMode,

			.flags = EFormatFlags_SupportFiles | EFormatFlags_SupportFolders,
			.supportedCategories = { EOperationCategory_Compile }
		};

		Operation_values[EOperation_CompileShader] = (Operation) {
			.category = EOperationCategory_Compile,
			.name = "shaders",
			.desc = "Compile shader from text to application ready format",
			.func = &CLI_compileShader
		};

	#endif

	//List graphics devices

	#ifdef CLI_GRAPHICS

		Operation_values[EOperation_GraphicsDevices] = (Operation) {

			.category = EOperationCategory_Graphics,

			.name = "devices",
			.desc = "Shows graphics devices using the active graphics API(s).",

			.func = &CLI_graphicsDevices,

			.isFormatLess = true,

			.optionalParameters = EOperationHasParameter_Entry | EOperationHasParameter_CountArg | EOperationHasParameter_GraphicsApi,
			.operationFlags = EOperation_Verbose
		};

	#endif

	//Audio operations
	
	Operation_values[EOperation_AudioDevices] = (Operation) {
		.category = EOperationCategory_Audio,
		.name = "devices",
		.desc = "Shows audio devices using the active audio API.",
		.func = &CLI_audioDevices,
		.isFormatLess = true
	};

	Format_values[EFormat_WAV] = (Format) {

		.name = "WAV",
		.desc = "Waveform Audio Format",

		.operationFlags =
			EOperationFlags_Debug | EOperationFlags_Split |
			EOperationFlags_CompilerWarnings | EOperationFlags_IgnoreEmptyFiles,

		.requiredParameters =
			EOperationHasParameter_Input | EOperationHasParameter_Output,

		.optionalParameters = EOperationHasParameter_Bit | EOperationHasParameter_SplitBy,

		.flags = EFormatFlags_SupportFiles | EFormatFlags_SupportFolders,
		.supportedCategories = { EOperationCategory_Audio }
	};
	
	Operation_values[EOperation_AudioConvert] = (Operation) {
		.category = EOperationCategory_Audio,
		.name = "convert",
		.desc = "Convert audio to other format.",
		.func = &CLI_audioConvert,
		.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_Output
	};

	//License for the tool

	Operation_values[EOperation_InfoLicense] = (Operation) {

		.category = EOperationCategory_Info,

		.name = "license",
		.desc = "Shows the license.",

		.func = &CLI_info,

		.isFormatLess = true
	};

	Operation_values[EOperation_InfoAbout] = (Operation) {

		.category = EOperationCategory_Info,

		.name = "about",
		.desc = "Shows information about the tool.",

		.func = &CLI_info,

		.isFormatLess = true
	};

	//Profile

	Operation_values[EOperation_ProfileCast] = (Operation) {

		.category = EOperationCategory_Profile,

		.name = "cast",
		.desc = "Profiles casting operations from random halfs/floats/doubles to other float types.",

		.func = &CLI_profileCast,

		.isFormatLess = true
	};

	Operation_values[EOperation_ProfileRNG] = (Operation) {

		.category = EOperationCategory_Profile,

		.name = "rng",
		.desc = "Profiles generating random numbers using CSPRNG (Cryptographically Secure pseudo RNG).",

		.func = &CLI_profileRNG,

		.isFormatLess = true
	};

	Operation_values[EOperation_ProfileCRC32C] = (Operation) {

		.category = EOperationCategory_Profile,

		.name = "crc32c",
		.desc = "Profiles hashing random data using crc32c.",

		.func = &CLI_profileCRC32C,

		.isFormatLess = true
	};

	Operation_values[EOperation_ProfileFNV1A64] = (Operation) {

		.category = EOperationCategory_Profile,

		.name = "fnv1a64",
		.desc = "Profiles hashing random data using fnv1a64.",

		.func = &CLI_profileFNV1A64,

		.isFormatLess = true
	};

	Operation_values[EOperation_ProfileSHA256] = (Operation) {

		.category = EOperationCategory_Profile,

		.name = "sha256",
		.desc = "Profiles hashing random data using sha256.",

		.func = &CLI_profileSHA256,

		.isFormatLess = true
	};

	Operation_values[EOperation_ProfileMD5] = (Operation) {

		.category = EOperationCategory_Profile,

		.name = "md5",
		.desc = "Profiles hashing random data using md5.",

		.func = &CLI_profileMD5,

		.isFormatLess = true
	};

	Operation_values[EOperation_ProfileAES256] = (Operation) {

		.category = EOperationCategory_Profile,

		.name = "aes256",
		.desc = "Profiles encrypting and decrypting random data using aes256-gcm.",

		.func = &CLI_profileAES256,

		.isFormatLess = true
	};

	Operation_values[EOperation_ProfileAES128] = (Operation) {

		.category = EOperationCategory_Profile,

		.name = "aes128",
		.desc = "Profiles encrypting and decrypting random data using aes128-gcm.",

		.func = &CLI_profileAES128,

		.isFormatLess = true
	};

	//Help operations

	Operation_values[EOperation_HelpCategories] = (Operation) {

		.category = EOperationCategory_Help,

		.name = "categories",
		.desc = "Help to see all categories.",

		.func = &CLI_helpOperation,

		.isFormatLess = true
	};

	Operation_values[EOperation_HelpOperations] = (Operation) {

		.category = EOperationCategory_Help,

		.name = "operations",
		.desc = "Help to see all operations in the category mentioned by -input.",

		.func = &CLI_helpOperation,

		.isFormatLess = true,

		.requiredParameters = EOperationHasParameter_Input
	};

	Operation_values[EOperation_HelpOperation] = (Operation) {

		.category = EOperationCategory_Help,

		.name = "operation",
		.desc = "Help to see all information about the operation mentioned by -input (category:operation or category).",

		.func = &CLI_helpOperation,

		.isFormatLess = true,

		.requiredParameters = EOperationHasParameter_Input
	};

	Operation_values[EOperation_HelpFormat] = (Operation) {

		.category = EOperationCategory_Help,

		.name = "format",
		.desc = "Help to see all information about the format mentioned by -input (category:operation:format).",

		.func = &CLI_helpOperation,

		.isFormatLess = true,

		.requiredParameters = EOperationHasParameter_Input
	};
}

Error ParsedArgs_getArg(ParsedArgs args, EOperationHasParameter parameterId, CharString *arg) {

	if(!arg || !parameterId)
		return Error_nullPointer(!arg ? 2 : 0, "ParsedArgs_getArg()::arg and parameterId are required");

	if(!((args.parameters >> parameterId) & 1))
		return Error_notFound(0, 1, "ParsedArgs_getArg()::parameterId not found");

	U64 ourLoc = 0;

	for(U64 j = EOperationHasParameter_InputShift; j < parameterId; ++j)
		if((args.parameters >> j) & 1)
			++ourLoc;

	const Error err = ListCharString_get(args.args, ourLoc, arg);

	if(err.genericError)
		return err;

	return Error_none();
}

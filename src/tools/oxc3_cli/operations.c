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

//tools/oxc3_cli/operations.c

#include "tools/oxc3_cli/cli.h"
#include "types/base/error.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "platforms/logx.h"

Bool CLI_info(const ParsedArgs *args) {

	if(!args) return false;

	Log_debugLnx(

		"OxC3 (Oxsomi core %"PRIu32".%"PRIu32".%03"PRIu32"), a general framework and toolset for cross-platform applications.\n"
		"Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)"
		"%s",

		(U32) OXC3_MAJOR,
		(U32) OXC3_MINOR,
		(U32) OXC3_PATCH,

		args->operation != EOperation_InfoLicense ? "" :
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
	"-graphics-api",
	"-type",
	"-oiCA",
	"-aes-file"
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
	"Shader compile mode (compile)",
	"Set extra include path",
	"Input file to merge with",
	"Graphics api to use. Default is either all or the native one depending on command.",
	"Numeric type (e.g. a float format: F8, F16, F32, F64, BF16, TF19, PXR24, FP24).",
	"Operate inside the given oiCA archive instead of the working directory.",
	"Read the 32-byte AES key from a file (64/66-char hex or a raw 32-byte binary) instead of a plaintext argument."
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
	"--verbose",
	"--fixed",
	"--aes-stdin"
};

const C8 *EOperationFlags_descriptions[EOperationFlags_Count] = {
	"Includes 256-bit hashes instead of 32-bit ones into file if applicable.",
	"Store data uncompressed (the current default; compression is a work in progress and not supported yet).",
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
	"Print full information to the console.",
	"Emit a fixed-point value instead of a float format (float convert).",
	"Read the 32-byte AES key (hex) from one line of stdin instead of a plaintext argument."
};

//Operations

const C8 *EOperationCategory_names[] = {

	"file",

	#ifdef CLI_SHADER_COMPILER
		"compile",
		"shader",
	#endif

	#ifdef CLI_GRAPHICS
		"graphics",
	#endif

	"audio",

	"hash",
	"rand",
	"float",
	"time",
	"info",
	"devices",
	"profile",
	"help"
};

const C8 *EOperationCategory_description[] = {

	"File utilities such as file conversions, encryption, compression, etc.",

	#ifdef CLI_SHADER_COMPILER
		"Compile shaders or to intermediate binary (Chimera).",
		"Shader tools: compile, reflect, inspect (entrypoints/includes/features), disassemble and assemble.",
	#endif

	#ifdef CLI_GRAPHICS
		"Graphics operations such as showing devices.",
	#endif

	"Audio operations such as showing devices.",

	"Converting a file or string to a hash.",
	"Generating random data.",
	"Float format conversion and inspection.",
	"Time conversion (epoch nanoseconds <-> ISO 8601).",
	"Information about the tool.",
	"This machine's hardware: CPU, graphics and audio devices.",
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
		.optionalParameters =
			EOperationHasParameter_AES | EOperationHasParameter_AESFile |
			EOperationHasParameter_Input2,
		.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_Output,
		.flags = EFormatFlags_SupportFiles | EFormatFlags_SupportFolders,
		.supportedCategories = { EOperationCategory_File }
	};

	Format_values[EFormat_oiDL] = (Format) {
		.name = "oiDL",
		.desc = "Oxsomi Data List; an indexed list of data, can be text (ASCII/UTF8) or binary data.",
		.operationFlags = EOperationFlags_Default | EOperationFlags_Ascii | EOperationFlags_UTF8,
		.optionalParameters =
			EOperationHasParameter_AES | EOperationHasParameter_AESFile |
			EOperationHasParameter_SplitBy | EOperationHasParameter_Input2,
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

		.requiredParameters = EOperationHasParameter_Input,
		.optionalParameters =
			EOperationHasParameter_Output | EOperationHasParameter_AES |
			EOperationHasParameter_AESFile
	};

	Operation_values[EOperation_FileDecr] = (Operation) {

		.category = EOperationCategory_File,
		.name = "decr",
		.desc = "Decrypt a file or folder.",
		.func = &CLI_encryptUndo,

		.isFormatLess = true,

		.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_Output,
		.optionalParameters =
			EOperationHasParameter_AES | EOperationHasParameter_AESFile
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
		.operationFlags = EOperationFlags_Bin | EOperationFlags_Includes | EOperationFlags_Verbose,
		.requiredParameters = EOperationHasParameter_Input,
		.optionalParameters =
			EOperationHasParameter_AES | EOperationHasParameter_AESFile |
			EOperationHasParameter_Output |
			EOperationHasParameter_Entry | EOperationHasParameter_StartOffset | EOperationHasParameter_Length |
			EOperationHasParameter_ShaderOutputMode
	};

	//File utilities (also work on virtual "//" paths)

	Operation_values[EOperation_FileList] = (Operation) {
		.category = EOperationCategory_File,
		.name = "list", .desc = "List the entries of a directory (defaults to the working directory).",
		.func = &CLI_fileList, .isFormatLess = true,
		.optionalParameters =
			EOperationHasParameter_Input | EOperationHasParameter_oiCA |
			EOperationHasParameter_AES | EOperationHasParameter_AESFile
	};

	Operation_values[EOperation_FileTree] = (Operation) {
		.category = EOperationCategory_File,
		.name = "tree", .desc = "List the entries of a directory recursively (defaults to the working directory).",
		.func = &CLI_fileTree, .isFormatLess = true,
		.optionalParameters =
			EOperationHasParameter_Input | EOperationHasParameter_oiCA |
			EOperationHasParameter_AES | EOperationHasParameter_AESFile
	};

	Operation_values[EOperation_FileStat] = (Operation) {
		.category = EOperationCategory_File,
		.name = "stat", .desc = "Show type, size, access and modified time of a file or folder.",
		.func = &CLI_fileStat, .isFormatLess = true,
		.requiredParameters = EOperationHasParameter_Input,
		.optionalParameters =
			EOperationHasParameter_oiCA |
			EOperationHasParameter_AES | EOperationHasParameter_AESFile
	};

	Operation_values[EOperation_FileCount] = (Operation) {
		.category = EOperationCategory_File,
		.name = "count", .desc = "Count the files and folders under a path (defaults to the working directory).",
		.func = &CLI_fileCount, .isFormatLess = true,
		.operationFlags = EOperationFlags_NonRecursive,
		.optionalParameters =
			EOperationHasParameter_Input | EOperationHasParameter_oiCA |
			EOperationHasParameter_AES | EOperationHasParameter_AESFile
	};

	Operation_values[EOperation_FileCopy] = (Operation) {
		.category = EOperationCategory_File,
		.name = "copy", .desc = "Copy a file to a new location.",
		.func = &CLI_fileCopy, .isFormatLess = true,
		.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_Output,
		.optionalParameters =
			EOperationHasParameter_oiCA |
			EOperationHasParameter_AES | EOperationHasParameter_AESFile
	};

	Operation_values[EOperation_FileMove] = (Operation) {
		.category = EOperationCategory_File,
		.name = "move", .desc = "Move a file into a destination directory.",
		.func = &CLI_fileMove, .isFormatLess = true,
		.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_Output
	};

	Operation_values[EOperation_FileDelete] = (Operation) {
		.category = EOperationCategory_File,
		.name = "del", .desc = "Delete a file or folder (recursive for folders).",
		.func = &CLI_fileDelete, .isFormatLess = true,
		.requiredParameters = EOperationHasParameter_Input,
		.optionalParameters =
			EOperationHasParameter_oiCA | EOperationHasParameter_Output |
			EOperationHasParameter_AES | EOperationHasParameter_AESFile
	};

	Operation_values[EOperation_FileMkdir] = (Operation) {
		.category = EOperationCategory_File,
		.name = "mkdir", .desc = "Create a folder (creating parent folders as needed).",
		.func = &CLI_fileMkdir, .isFormatLess = true,
		.requiredParameters = EOperationHasParameter_Input
	};

	Operation_values[EOperation_FileTouch] = (Operation) {
		.category = EOperationCategory_File,
		.name = "touch", .desc = "Create an empty file.",
		.func = &CLI_fileTouch, .isFormatLess = true,
		.requiredParameters = EOperationHasParameter_Input
	};

	Operation_values[EOperation_FileCmp] = (Operation) {
		.category = EOperationCategory_File,
		.name = "cmp", .desc = "Byte-compare two files and report the first difference.",
		.func = &CLI_fileCmp, .isFormatLess = true,
		.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_Input2
	};

	Operation_values[EOperation_FileDiff] = (Operation) {
		.category = EOperationCategory_File,
		.name = "diff", .desc = "Structurally compare two archives (oiCA or oiDL): added / removed / modified entries.",
		.func = &CLI_fileDiff, .isFormatLess = true,
		.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_Input2
	};

	Operation_values[EOperation_FileWipe] = (Operation) {
		.category = EOperationCategory_File,
		.name = "wipe", .desc = "Overwrite a file's contents with zeros.",
		.func = &CLI_fileWipe, .isFormatLess = true,
		.requiredParameters = EOperationHasParameter_Input
	};

	Operation_values[EOperation_FileHexdump] = (Operation) {
		.category = EOperationCategory_File,
		.name = "hexdump", .desc = "Print a hex + ASCII dump of a file or region.",
		.func = &CLI_fileHexdump, .isFormatLess = true,
		.requiredParameters = EOperationHasParameter_Input,
		.optionalParameters = EOperationHasParameter_StartOffset | EOperationHasParameter_Length
	};

	Operation_values[EOperation_FileGmac] = (Operation) {
		.category = EOperationCategory_File,
		.name = "gmac", .desc = "Compute an AES-GMAC authentication tag over a file (-aes / -aes-file / --aes-stdin key).",
		.func = &CLI_fileGmac, .isFormatLess = true,
		.requiredParameters = EOperationHasParameter_Input,
		.optionalParameters =
			EOperationHasParameter_AES | EOperationHasParameter_AESFile
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

	#ifdef CLI_SHADER_COMPILER

		//Package file for virtual file system

		Operation_values[EOperation_Package] = (Operation) {

			.category = EOperationCategory_File,

			.name = "package",
			.desc = "Package files such as shaders, textures and models into an oiCA as Oxsomi file formats.",

			.func = &CLI_package,

			.isFormatLess = true,

			.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_Output,
			.optionalParameters =
				EOperationHasParameter_AES | EOperationHasParameter_AESFile |
				EOperationHasParameter_ThreadCount |
				EOperationHasParameter_IncludeDir | EOperationHasParameter_ShaderOutputMode,

			.operationFlags =
				EOperationFlags_Debug | EOperationFlags_Split |
				EOperationFlags_CompilerWarnings | EOperationFlags_IgnoreEmptyFiles
		};

		//Compile shaders

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

		//Shader category: compile + reflect + inspect + (dis)assemble

		Operation_values[EOperation_ShaderCompile] = (Operation) {
			.category = EOperationCategory_Shader,
			.name = "compile",
			.desc = "Compile a shader to an oiSH; the source format is detected from the input extension (.hlsl).",
			.func = &CLI_compileShader,
			.isFormatLess = true,
			.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_Output,
			.optionalParameters =
				EOperationHasParameter_ThreadCount | EOperationHasParameter_IncludeDir |
				EOperationHasParameter_ShaderCompileMode | EOperationHasParameter_ShaderOutputMode,
			.operationFlags =
				EOperationFlags_Debug | EOperationFlags_Split |
				EOperationFlags_CompilerWarnings | EOperationFlags_IgnoreEmptyFiles
		};

		Operation_values[EOperation_ShaderReflect] = (Operation) {
			.category = EOperationCategory_Shader,
			.name = "reflect",
			.desc = "Produce a reflection-only oiSH from a shader source (no compiled binary; not usable for a pipeline).",
			.func = &CLI_shaderReflect,
			.isFormatLess = true,
			.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_Output,
			.optionalParameters = EOperationHasParameter_ThreadCount | EOperationHasParameter_IncludeDir,
			.operationFlags =
				EOperationFlags_Debug | EOperationFlags_CompilerWarnings | EOperationFlags_IgnoreEmptyFiles
		};

		Operation_values[EOperation_ShaderEntrypoints] = (Operation) {
			.category = EOperationCategory_Shader,
			.name = "entrypoints",
			.desc = "List the entrypoints (name + stage) declared in an oiSH shader.",
			.func = &CLI_shaderEntrypoints,
			.isFormatLess = true,
			.requiredParameters = EOperationHasParameter_Input,
			.operationFlags = EOperationFlags_Verbose
		};

		Operation_values[EOperation_ShaderIncludes] = (Operation) {
			.category = EOperationCategory_Shader,
			.name = "includes",
			.desc = "List the include files (relative path + CRC32C) an oiSH shader was compiled from.",
			.func = &CLI_shaderIncludes,
			.isFormatLess = true,
			.requiredParameters = EOperationHasParameter_Input
		};

		Operation_values[EOperation_ShaderFeatureSet] = (Operation) {
			.category = EOperationCategory_Shader,
			.name = "feature_set",
			.desc = "Show the extensions, shader models and binary types used across an oiSH shader's binaries.",
			.func = &CLI_shaderFeatureSet,
			.isFormatLess = true,
			.requiredParameters = EOperationHasParameter_Input
		};

		Operation_values[EOperation_ShaderDisassemble] = (Operation) {
			.category = EOperationCategory_Shader,
			.name = "disassemble",
			.desc = "Disassemble a standalone .spv or .dxil binary to text (stdout, or -output <file>).",
			.func = &CLI_shaderDisassemble,
			.isFormatLess = true,
			.requiredParameters = EOperationHasParameter_Input,
			.optionalParameters = EOperationHasParameter_Output
		};

		Operation_values[EOperation_ShaderAssemble] = (Operation) {
			.category = EOperationCategory_Shader,
			.name = "assemble",
			.desc = "Assemble SPIR-V text (.spv.txt) into a .spv binary. DXIL assembly isn't supported yet.",
			.func = &CLI_shaderAssemble,
			.isFormatLess = true,
			.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_Output
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
			.operationFlags = EOperationFlags_Verbose
		};

		Operation_values[EOperation_GraphicsCreateDevice] = (Operation) {

			.category = EOperationCategory_Graphics,

			.name = "create",
			.desc = "Create a graphics device using the specified graphics API, useful for testing purposes.",

			.func = &CLI_graphicsCreate,

			.isFormatLess = true,

			.optionalParameters = EOperationHasParameter_Entry | EOperationHasParameter_CountArg | EOperationHasParameter_GraphicsApi
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

	Operation_values[EOperation_DevicesCPU] = (Operation) {

		.category = EOperationCategory_Devices,

		.name = "cpu",
		.desc = "Shows this machine's CPU: logical cores, physical memory and hardware capability flags.",

		.func = &CLI_cpuDevices,

		.isFormatLess = true
	};

	Operation_values[EOperation_DevicesAll] = (Operation) {

		.category = EOperationCategory_Devices,

		.name = "all",
		.desc = "Dumps CPU + graphics devices + audio devices in one go (handy for support / bug reports).",

		.func = &CLI_infoAll,

		.isFormatLess = true
	};

	//Profile

	Operation_values[EOperation_ProfileCast] = (Operation) {

		.category = EOperationCategory_Profile,

		.name = "cast",
		.desc = "Profiles casting operations from random halfs/floats/doubles to other float types.",

		.func = &CLI_profileCast,

		.optionalParameters = EOperationHasParameter_ThreadCount | EOperationHasParameter_Length,
		.isFormatLess = true
	};

	Operation_values[EOperation_ProfileRNG] = (Operation) {

		.category = EOperationCategory_Profile,

		.name = "rng",
		.desc = "Profiles generating random numbers using CSPRNG (Cryptographically Secure pseudo RNG).",

		.func = &CLI_profileRNG,

		.optionalParameters = EOperationHasParameter_ThreadCount | EOperationHasParameter_Length,
		.isFormatLess = true
	};

	Operation_values[EOperation_ProfileCRC32C] = (Operation) {

		.category = EOperationCategory_Profile,

		.name = "crc32c",
		.desc = "Profiles hashing random data using crc32c.",

		.func = &CLI_profileCRC32C,

		.optionalParameters = EOperationHasParameter_ThreadCount | EOperationHasParameter_Length,
		.isFormatLess = true
	};

	Operation_values[EOperation_ProfileFNV1A64] = (Operation) {

		.category = EOperationCategory_Profile,

		.name = "fnv1a64",
		.desc = "Profiles hashing random data using fnv1a64.",

		.func = &CLI_profileFNV1A64,

		.optionalParameters = EOperationHasParameter_ThreadCount | EOperationHasParameter_Length,
		.isFormatLess = true
	};

	Operation_values[EOperation_ProfileSHA256] = (Operation) {

		.category = EOperationCategory_Profile,

		.name = "sha256",
		.desc = "Profiles hashing random data using sha256.",

		.func = &CLI_profileSHA256,

		.optionalParameters = EOperationHasParameter_ThreadCount | EOperationHasParameter_Length,
		.isFormatLess = true
	};

	Operation_values[EOperation_ProfileMD5] = (Operation) {

		.category = EOperationCategory_Profile,

		.name = "md5",
		.desc = "Profiles hashing random data using md5.",

		.func = &CLI_profileMD5,

		.optionalParameters = EOperationHasParameter_ThreadCount | EOperationHasParameter_Length,
		.isFormatLess = true
	};

	Operation_values[EOperation_ProfileAES256] = (Operation) {

		.category = EOperationCategory_Profile,

		.name = "aes256",
		.desc = "Profiles encrypting and decrypting random data using aes256-gcm.",

		.func = &CLI_profileAES256,

		.optionalParameters = EOperationHasParameter_ThreadCount | EOperationHasParameter_Length,
		.isFormatLess = true
	};

	Operation_values[EOperation_ProfileAES128] = (Operation) {

		.category = EOperationCategory_Profile,

		.name = "aes128",
		.desc = "Profiles encrypting and decrypting random data using aes128-gcm.",

		.func = &CLI_profileAES128,

		.optionalParameters = EOperationHasParameter_ThreadCount | EOperationHasParameter_Length,
		.isFormatLess = true
	};

	Operation_values[EOperation_ProfileMemcpy] = (Operation) {

		.category = EOperationCategory_Profile,

		.name = "memcpy",
		.desc = "Profiles memory copy bandwidth (Buffer_memcpy).",

		.func = &CLI_profileMemcpy,

		.optionalParameters = EOperationHasParameter_ThreadCount | EOperationHasParameter_Length,
		.isFormatLess = true
	};

	Operation_values[EOperation_ProfileMemset] = (Operation) {

		.category = EOperationCategory_Profile,

		.name = "memset",
		.desc = "Profiles memory clear bandwidth (Buffer_unsetAllBits).",

		.func = &CLI_profileMemset,

		.optionalParameters = EOperationHasParameter_ThreadCount | EOperationHasParameter_Length,
		.isFormatLess = true
	};

	Operation_values[EOperation_ProfileVec] = (Operation) {

		.category = EOperationCategory_Profile,

		.name = "vec",
		.desc = "Profiles 128-bit float SIMD throughput (vec4f add / mul / fma).",

		.func = &CLI_profileVec,

		.optionalParameters = EOperationHasParameter_ThreadCount | EOperationHasParameter_Length,
		.isFormatLess = true
	};

	Operation_values[EOperation_ProfileAll] = (Operation) {

		.category = EOperationCategory_Profile,

		.name = "all",
		.desc = "Runs every profile in sequence (cast, rng, hashes, aes, memcpy, memset, vec).",

		.func = &CLI_profileAll,

		.optionalParameters = EOperationHasParameter_ThreadCount | EOperationHasParameter_Length,
		.isFormatLess = true
	};

	//Float format conversion / inspection

	Operation_values[EOperation_FloatConvert] = (Operation) {
		.category = EOperationCategory_Float,
		.name = "convert",
		.desc = "Convert a decimal value to a float format (-type F8/F16/F32/F64/BF16/TF19/PXR24/FP24) or --fixed.",
		.func = &CLI_floatConvert,
		.isFormatLess = true,
		.requiredParameters = EOperationHasParameter_Input,
		.optionalParameters = EOperationHasParameter_Type,
		.operationFlags = EOperationFlags_Fixed
	};

	Operation_values[EOperation_FloatDissect] = (Operation) {
		.category = EOperationCategory_Float,
		.name = "dissect",
		.desc = "Show sign/exponent/mantissa/class of a value (decimal or 0x bits) in a float format (-type).",
		.func = &CLI_floatDissect,
		.isFormatLess = true,
		.requiredParameters = EOperationHasParameter_Input,
		.optionalParameters = EOperationHasParameter_Type
	};

	//Time conversion

	Operation_values[EOperation_TimeNow] = (Operation) {
		.category = EOperationCategory_Time,
		.name = "now",
		.desc = "Print the current UTC time as ISO 8601 and Unix-epoch nanoseconds.",
		.func = &CLI_timeNow,
		.isFormatLess = true
	};

	Operation_values[EOperation_TimeConvert] = (Operation) {
		.category = EOperationCategory_Time,
		.name = "convert",
		.desc = "Convert between Unix-epoch nanoseconds and ISO 8601 (auto-detected from -input).",
		.func = &CLI_timeConvert,
		.isFormatLess = true,
		.requiredParameters = EOperationHasParameter_Input
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

Bool ParsedArgs_getArg(const ParsedArgs *args, EOperationHasParameter parameterId, CharString *arg, Error *e_rr) {

	Bool s_uccess = true;

	if(!args || !arg || !parameterId)
		retError(clean, Error_nullPointer(!args ? 0 : (!arg ? 2 : 1), "ParsedArgs_getArg()::args, arg and parameterId are required"));

	if(!((args->parameters >> parameterId) & 1))
		retError(clean, Error_notFound(0, 1, "ParsedArgs_getArg()::parameterId not found"));

	U64 ourLoc = 0;

	for(U64 j = EOperationHasParameter_InputShift; j < parameterId; ++j)
		if((args->parameters >> j) & 1)
			++ourLoc;

	gotoIfError3(clean, ListCharString_get(args->args, ourLoc, arg, e_rr));

clean:
	return s_uccess;
}

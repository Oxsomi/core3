/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#pragma once
#include "platforms/ext/listx.h"
#include "types/container/string.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Parameters

typedef enum EOperationHasParameter {

	//As shifts instead of masks

	EOperationHasParameter_FileFormatShift = 0,
	EOperationHasParameter_InputShift,
	EOperationHasParameter_OutputShift,
	EOperationHasParameter_AESShift,

	EOperationHasParameter_SplitByShift,

	EOperationHasParameter_CountShift,
	EOperationHasParameter_LengthShift,
	EOperationHasParameter_CharacterShift,

	EOperationHasParameter_BitShift,

	EOperationHasParameter_EntryShift,
	EOperationHasParameter_StartOffsetShift,

	EOperationHasParameter_ShaderOutputModeShift,
	EOperationHasParameter_ThreadCountShift,
	EOperationHasParameter_ShaderCompileModeShift,
	EOperationHasParameter_IncludeDirShift,

	EOperationHasParameter_Input2Shift,				//If two inputs are specified, this specifies input2

	EOperationHasParameter_GraphicsApiShift,

	EOperationHasParameter_CountEnum,				//How many enums there are

	EOperationHasParameter_Start				= EOperationHasParameter_FileFormatShift,

	//As mask

	EOperationHasParameter_None					= 0,
	EOperationHasParameter_FileFormat			= 1 << EOperationHasParameter_FileFormatShift,
	EOperationHasParameter_Input				= 1 << EOperationHasParameter_InputShift,
	EOperationHasParameter_Output				= 1 << EOperationHasParameter_OutputShift,
	EOperationHasParameter_AES					= 1 << EOperationHasParameter_AESShift,

	EOperationHasParameter_SplitBy				= 1 << EOperationHasParameter_SplitByShift,

	EOperationHasParameter_CountArg				= 1 << EOperationHasParameter_CountShift,		//--count
	EOperationHasParameter_Length				= 1 << EOperationHasParameter_LengthShift,

	EOperationHasParameter_Character			= 1 << EOperationHasParameter_CharacterShift,

	EOperationHasParameter_Bit					= 1 << EOperationHasParameter_BitShift,

	EOperationHasParameter_Entry				= 1 << EOperationHasParameter_EntryShift,
	EOperationHasParameter_StartOffset			= 1 << EOperationHasParameter_StartOffsetShift,

	EOperationHasParameter_ShaderOutputMode		= 1 << EOperationHasParameter_ShaderOutputModeShift,
	EOperationHasParameter_ThreadCount			= 1 << EOperationHasParameter_ThreadCountShift,
	EOperationHasParameter_ShaderCompileMode	= 1 << EOperationHasParameter_ShaderCompileModeShift,
	EOperationHasParameter_IncludeDir			= 1 << EOperationHasParameter_IncludeDirShift,

	EOperationHasParameter_Input2				= 1 << EOperationHasParameter_Input2Shift,

	EOperationHasParameter_GraphicsApi			= 1 << EOperationHasParameter_GraphicsApiShift

} EOperationHasParameter;

extern const C8 *EOperationHasParameter_names[];
extern const C8 *EOperationHasParameter_descriptions[];

//Flags

typedef enum EOperationFlags {

	EOperationFlags_None				= 0,

	//Default flags

	EOperationFlags_SHA256				= 1 << 0,
	EOperationFlags_Uncompressed		= 1 << 1,

	EOperationFlags_Default				=
	EOperationFlags_SHA256 | EOperationFlags_Uncompressed,

	//CharString flags

	EOperationFlags_Ascii				= 1 << 2,
	EOperationFlags_UTF8				= 1 << 3,

	//File flags

	EOperationFlags_FullDate			= 1 << 4,
	EOperationFlags_Date				= 1 << 5,

	//Folder

	EOperationFlags_NonRecursive		= 1 << 6,

	//Random char

	EOperationFlags_Alpha				= 1 << 7,
	EOperationFlags_Alphanumeric		= 1 << 8,
	EOperationFlags_Number				= 1 << 9,
	EOperationFlags_Symbols				= 1 << 10,
	EOperationFlags_Lowercase			= 1 << 11,
	EOperationFlags_Uppercase			= 1 << 12,

	EOperationFlags_RandChar			=
		EOperationFlags_Alpha | EOperationFlags_Alphanumeric | EOperationFlags_Number |
		EOperationFlags_Symbols | EOperationFlags_Lowercase | EOperationFlags_Uppercase,

	//Random number

	EOperationFlags_Nyto				= 1 << 13,
	EOperationFlags_Hex					= 1 << 14,
	EOperationFlags_Bin					= 1 << 15,		//This is also used by OxC3 file data (for oiSH)
	EOperationFlags_Oct					= 1 << 16,

	EOperationFlags_RandNum =
		EOperationFlags_Oct | EOperationFlags_Bin | EOperationFlags_Hex | EOperationFlags_Nyto,

	//Compilation

	EOperationFlags_Debug				= 1 << 17,
	EOperationFlags_IgnoreEmptyFiles	= 1 << 18,
	EOperationFlags_Includes			= 1 << 19,		//Used to signal to OxC3 file data that includes should be requested
	EOperationFlags_Split				= 1 << 20,		//Split multiple binary types (SPV, DXIL, etc.) into one per file

	EOperationFlags_WarnUnusedRegisters	= 1 << 21,
	EOperationFlags_WarnUnusedConstants	= 1 << 22,
	EOperationFlags_WarnBufferPadding	= 1 << 23,

	EOperationFlags_CompilerWarnings	= 7 << 21,

	EOperation_Verbose					= 1 << 24,

	EOperationFlags_Count				= 25

} EOperationFlags;

extern const C8 *EOperationFlags_names[EOperationFlags_Count];
extern const C8 *EOperationFlags_descriptions[EOperationFlags_Count];

//Operations

typedef enum EOperation {

	EOperation_FileTo,
	EOperation_FileFrom,
	EOperation_FileCombine,

	EOperation_HashFile,
	EOperation_HashString,

	EOperation_RandKey,
	EOperation_RandChar,
	EOperation_RandData,
	EOperation_RandNum,

	EOperation_FileEncr,
	EOperation_FileDecr,

	EOperation_FileHeader,
	EOperation_FileData,

	EOperation_Package,

	#ifdef CLI_SHADER_COMPILER
		EOperation_CompileShader,
	#endif

	//EOperation_CompileChimera,

	EOperation_GraphicsDevices,

	EOperation_InfoLicense,
	EOperation_InfoAbout,

	EOperation_ProfileCast,
	EOperation_ProfileRNG,
	EOperation_ProfileCRC32C,
	EOperation_ProfileFNV1A64,
	EOperation_ProfileSHA256,
	EOperation_ProfileMD5,
	EOperation_ProfileAES256,
	EOperation_ProfileAES128,

	EOperation_HelpCategories,
	EOperation_HelpOperations,
	EOperation_HelpOperation,
	EOperation_HelpFormat,

	EOperation_Invalid

} EOperation;

typedef enum EOperationCategory {

	EOperationCategory_Invalid,
	EOperationCategory_File,

	#ifdef CLI_SHADER_COMPILER
		EOperationCategory_Compile,
	#endif

	#ifdef CLI_GRAPHICS
		EOperationCategory_Graphics,
	#endif

	EOperationCategory_Hash,
	EOperationCategory_Rand,
	EOperationCategory_Info,
	EOperationCategory_Profile,
	EOperationCategory_Help,
	EOperationCategory_End,

	EOperationCategory_Start = EOperationCategory_File

} EOperationCategory;

extern const C8 *EOperationCategory_names[];
extern const C8 *EOperationCategory_description[];

typedef enum EFormat {

	EFormat_oiCA,
	EFormat_oiDL,
	EFormat_oiSH,

	EFormat_SHA256,
	EFormat_CRC32C,
	EFormat_MD5,
	EFormat_FNV1A64,

	EFormat_HLSL,

	EFormat_Invalid

} EFormat;

typedef struct ParsedArgs {
	EOperation operation;
	EFormat format;
	EOperationFlags flags;
	EOperationHasParameter parameters;
	ListCharString args;					//Use parameter flags to extract from low to high
} ParsedArgs;

typedef Bool (*OperationFunc)(ParsedArgs);

Error ParsedArgs_getArg(ParsedArgs args, EOperationHasParameter parameterId, CharString *arg);

typedef enum EFormatFlags {
	EFormatFlags_None				= 0,
	EFormatFlags_SupportFiles		= 1 << 0,
	EFormatFlags_SupportFolders		= 1 << 1,
	EFormatFlags_SupportAsString	= 1 << 2
} EFormatFlags;

typedef struct Operation {

	EOperationCategory category;
	const C8 *name, *desc;
	OperationFunc func;

	//If set, this will be used instead of a per format setting.

	Bool isFormatLess;
	EOperationFlags operationFlags;
	EOperationHasParameter requiredParameters, optionalParameters;

} Operation;

typedef struct Format {
	const C8 *name, *desc;
	EOperationFlags operationFlags;
	EOperationHasParameter requiredParameters, optionalParameters;
	EFormatFlags flags;
	EOperationCategory supportedCategories[4];
} Format;

extern Operation Operation_values[EOperation_Invalid];
extern Format Format_values[EFormat_Invalid];

void Operations_init();

#ifdef __cplusplus
	}
#endif


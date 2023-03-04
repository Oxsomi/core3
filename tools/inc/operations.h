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

#pragma once
#include "types/list.h"

typedef struct CharString CharString;

//Parameters

typedef enum EOperationHasParameter {

	//As shifts instead of masks

	EOperationHasParameter_FileFormatShift = 0,
	EOperationHasParameter_InputShift,
	EOperationHasParameter_OutputShift,
	EOperationHasParameter_AESShift,

	EOperationHasParameter_SplitByShift,

	EOperationHasParameter_NumberShift,
	EOperationHasParameter_LengthShift,
	EOperationHasParameter_CharacterShift,

	EOperationHasParameter_BitShift,

	EOperationHasParameter_EntryShift,
	EOperationHasParameter_StartOffsetShift,

	EOperationHasParameter_Count,	

	EOperationHasParameter_Start		= EOperationHasParameter_FileFormatShift,

	//As mask

	EOperationHasParameter_None			= 0,
	EOperationHasParameter_FileFormat	= 1 << EOperationHasParameter_FileFormatShift,
	EOperationHasParameter_Input		= 1 << EOperationHasParameter_InputShift,
	EOperationHasParameter_Output		= 1 << EOperationHasParameter_OutputShift,
	EOperationHasParameter_AES			= 1 << EOperationHasParameter_AESShift,

	EOperationHasParameter_SplitBy		= 1 << EOperationHasParameter_SplitByShift,

	EOperationHasParameter_Number		= 1 << EOperationHasParameter_NumberShift,
	EOperationHasParameter_Length		= 1 << EOperationHasParameter_LengthShift,

	EOperationHasParameter_Character	= 1 << EOperationHasParameter_CharacterShift,

	EOperationHasParameter_Bit			= 1 << EOperationHasParameter_BitShift,

	EOperationHasParameter_Entry		= 1 << EOperationHasParameter_EntryShift,
	EOperationHasParameter_StartOffset	= 1 << EOperationHasParameter_StartOffsetShift

} EOperationHasParameter;

extern const C8 *EOperationHasParameter_names[];
extern const C8 *EOperationHasParameter_descriptions[];

//Flags

typedef enum EOperationFlags {

	EOperationFlags_None			= 0,

	//Default flags

	EOperationFlags_SHA256			= 1 << 0,
	EOperationFlags_Uncompressed	= 1 << 1,

	EOperationFlags_Default			= 
	EOperationFlags_SHA256 | EOperationFlags_Uncompressed,

	//CharString flags

	EOperationFlags_Ascii			= 1 << 2,
	EOperationFlags_UTF8			= 1 << 3,

	//File flags

	EOperationFlags_FullDate		= 1 << 4,
	EOperationFlags_Date			= 1 << 5,

	//Folder

	EOperationFlags_NonRecursive	= 1 << 6,

	//Random char

	EOperationFlags_Alpha			= 1 << 7,
	EOperationFlags_Alphanumeric	= 1 << 8,
	EOperationFlags_Number			= 1 << 9,
	EOperationFlags_Symbols			= 1 << 10,
	EOperationFlags_Lowercase		= 1 << 11,
	EOperationFlags_Uppercase		= 1 << 12,

	EOperationFlags_RandChar		= 
		EOperationFlags_Alpha | EOperationFlags_Alphanumeric | EOperationFlags_Number |
		EOperationFlags_Symbols | EOperationFlags_Lowercase | EOperationFlags_Uppercase,

	//Random number

	EOperationFlags_Nyto			= 1 << 13,
	EOperationFlags_Hex				= 1 << 14,
	EOperationFlags_Bin				= 1 << 15,
	EOperationFlags_Oct				= 1 << 16,

	EOperationFlags_RandNum =
		EOperationFlags_Oct | EOperationFlags_Bin | EOperationFlags_Hex | EOperationFlags_Nyto,

	EOperationFlags_Count			= 17

} EOperationFlags;

extern const C8 *EOperationFlags_names[];
extern const C8 *EOperationFlags_descriptions[];

//Operations

typedef enum EOperation {

	EOperation_FileTo,
	EOperation_FileFrom,

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

	EOperation_LicenseShow,

	EOperation_Invalid

} EOperation;

typedef enum EOperationCategory {
	EOperationCategory_Invalid,
	EOperationCategory_File,
	EOperationCategory_Hash,
	EOperationCategory_Rand,
	EOperationCategory_License,
	EOperationCategory_End,
	EOperationCategory_Start = EOperationCategory_File
} EOperationCategory;

extern const C8 *EOperationCategory_names[];
extern const C8 *EOperationCategory_description[];

typedef enum EFormat {
	EFormat_oiCA,
	EFormat_oiDL,
	EFormat_SHA256,
	EFormat_CRC32C,
	EFormat_Invalid
} EFormat;

typedef struct ParsedArgs {
	EOperation operation;
	EFormat format;
	EOperationFlags flags;
	EOperationHasParameter parameters;
	List args;					//[CharString]; use parameter flags to extract from low to high
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

extern Operation Operation_values[14];
extern Format Format_values[4];

void Operations_init();

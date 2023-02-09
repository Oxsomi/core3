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

#pragma once
#include "types/list.h"

typedef struct String String;

//TODO: Package folder
//TODO: Inspect

//Parameters

typedef enum EOperationHasParameter {

	//As shifts instead of masks

	EOperationHasParameter_FileFormatShift = 0,
	EOperationHasParameter_InputShift,
	EOperationHasParameter_OutputShift,
	EOperationHasParameter_AESShift,

	EOperationHasParameter_SplitByShift,

	EOperationHasParameter_Count,	
	EOperationHasParameter_Start		= EOperationHasParameter_FileFormatShift,

	//As mask

	EOperationHasParameter_None			= 0,
	EOperationHasParameter_FileFormat	= 1 << EOperationHasParameter_FileFormatShift,
	EOperationHasParameter_Input		= 1 << EOperationHasParameter_InputShift,
	EOperationHasParameter_Output		= 1 << EOperationHasParameter_OutputShift,
	EOperationHasParameter_AES			= 1 << EOperationHasParameter_AESShift,

	EOperationHasParameter_SplitBy		= 1 << EOperationHasParameter_SplitByShift

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

	//String flags

	EOperationFlags_Ascii			= 1 << 2,
	EOperationFlags_UTF8			= 1 << 3,

	//File flags

	EOperationFlags_FullDate		= 1 << 4,
	EOperationFlags_Date			= 1 << 5,

	//Folder

	EOperationFlags_NonRecursive	= 1 << 6,

	EOperationFlags_Count			= 7

} EOperationFlags;

extern const C8 *EOperationFlags_names[];
extern const C8 *EOperationFlags_descriptions[];

//Operations

typedef enum EOperation {
	EOperation_ConvertTo,
	EOperation_ConvertFrom,
	EOperation_Invalid
} EOperation;

typedef enum EOperationCategory {
	EOperationCategory_Convert,
	EOperationCategory_Invalid
} EOperationCategory;

extern const C8 *EOperationCategory_names[];
extern const C8 *EOperationCategory_description[];

typedef enum EFormat {
	EFormat_oiCA,
	EFormat_oiDL,
	EFormat_Invalid
} EFormat;

typedef struct ParsedArgs {
	EFormat format;
	EOperationFlags flags;
	EOperationHasParameter parameters;
	List args;					//[String]; use parameter flags to extract from low to high
} ParsedArgs;

typedef Bool (*OperationFunc)(ParsedArgs);

Error ParsedArgs_getArg(ParsedArgs args, EOperationHasParameter parameterId, String *arg);

typedef struct Operation {
	EOperationCategory category;
	const C8 *name, *desc;
	OperationFunc func;
} Operation;

typedef enum EFormatFlags {
	EFormatFlags_None			= 0,
	EFormatFlags_SupportFiles	= 1 << 0,
	EFormatFlags_SupportFolders	= 1 << 1
} EFormatFlags;

typedef struct Format {
	const C8 *name, *desc;
	EOperationFlags operationFlags;
	EOperationHasParameter requiredParameters, optionalParameters;
	EFormatFlags flags;
} Format;

extern Operation Operation_values[2];
extern Format Format_values[2];

void Operations_init();

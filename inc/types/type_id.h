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
#include "types.h"

typedef enum EDataTypePrimitive {

	EDataTypePrimitive_Int,
	EDataTypePrimitive_Float,
	EDataTypePrimitive_Bool,
	EDataTypePrimitive_Char,

	EDataTypePrimitive_Custom,
	EDataTypePrimitive_Enum,
	EDataTypePrimitive_String,
	EDataTypePrimitive_Container

} EDataTypePrimitive;

typedef enum EDataType {

	EDataType_UInt			= 0b0000,
	EDataType_Int			= 0b0001,
	EDataType_Float			= 0b0011,
	EDataType_Bool			= 0b0100,
	EDataType_Char			= 0b1100,

	EDataType_Custom		= 0b1000,
	EDataType_Enum			= 0b1010,
	EDataType_String		= 0b1100,
	EDataType_Interface		= 0b1110,

	EDataType_IsSigned		= 0b0001

} EDataType;

EDataTypePrimitive EDataType_getPrimitive(EDataType type);
Bool EDataType_isSigned(EDataType type);

#define _makeTypeId(libId, subId, typeId, elementCount, dataTypeBytes, dataType)										\
((libId << 24) | (subId << 22) | (typeId << 13) | ((elementCount - 1) << 7) | ((dataTypeBytes - 1) << 4) | dataType)

#define _LIBRARYID_DEFAULT 0xC3
#define _TYPESIZE_UNDEF 7

typedef enum ETypeId {

	ETypeId_I8					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 0, 1, 1, EDataType_Int),
	ETypeId_I16					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 1, 1, 2, EDataType_Int),
	ETypeId_I32					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 2, 1, 4, EDataType_Int),
	ETypeId_I64					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 3, 1, 8, EDataType_Int),

	ETypeId_U8					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 4, 1, 1, EDataType_UInt),
	ETypeId_U16					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 5, 1, 2, EDataType_UInt),
	ETypeId_U32					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 6, 1, 4, EDataType_UInt),
	ETypeId_U64					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 7, 1, 8, EDataType_UInt),

	ETypeId_F32					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 8, 1, 4, EDataType_Float),

	ETypeId_Ns					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 9 , 1, 8, EDataType_UInt),
	ETypeId_DNs					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 10, 1, 8, EDataType_Int),

	ETypeId_C8					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 11, 1, 1, EDataType_Char),
	ETypeId_Bool				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 12, 1, 1, EDataType_Bool),

	ETypeId_Buffer				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 13, 1, _TYPESIZE_UNDEF, EDataType_Custom),

	ETypeId_EStringCase			= _makeTypeId(_LIBRARYID_DEFAULT, 0, 14, 1, 1, EDataType_Enum),
	ETypeId_EStringTransform	= _makeTypeId(_LIBRARYID_DEFAULT, 0, 15, 1, 1, EDataType_Enum),

	ETypeId_I32x2				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 16, 2, 4, EDataType_Int),
	ETypeId_I32x4				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 17, 4, 4, EDataType_Int),
	ETypeId_F32x2				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 18, 2, 4, EDataType_Float),
	ETypeId_F32x4				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 19, 4, 4, EDataType_Float),

	ETypeId_Transform			= _makeTypeId(_LIBRARYID_DEFAULT, 0, 20, 12, 4, EDataType_Float),
	ETypeId_PackedTransform		= _makeTypeId(_LIBRARYID_DEFAULT, 0, 21, 8 , 4, EDataType_UInt),
	ETypeId_Transform2D			= _makeTypeId(_LIBRARYID_DEFAULT, 0, 22, 4 , 4, EDataType_Float),

	ETypeId_TilemapTransform	= _makeTypeId(_LIBRARYID_DEFAULT, 0, 23, 1, 8, EDataType_UInt),

	ETypeId_EMirrored			= _makeTypeId(_LIBRARYID_DEFAULT, 0, 24, 1, 1, EDataType_Enum),
	ETypeId_ERotated			= _makeTypeId(_LIBRARYID_DEFAULT, 0, 25, 1, 1, EDataType_Enum),
	ETypeId_EFormatStatus		= _makeTypeId(_LIBRARYID_DEFAULT, 0, 26, 1, 1, EDataType_Enum),

	ETypeId_ShortString			= _makeTypeId(_LIBRARYID_DEFAULT, 0, 27, 32, 1, EDataType_Char),
	ETypeId_LongString			= _makeTypeId(_LIBRARYID_DEFAULT, 0, 28, 64, 1, EDataType_Char),

	ETypeId_String				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 29, 1, _TYPESIZE_UNDEF, EDataType_String),
	ETypeId_StringList			= _makeTypeId(_LIBRARYID_DEFAULT, 0, 30, 1, _TYPESIZE_UNDEF, EDataType_Custom),

	ETypeId_Quat				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 31, 4, 4, EDataType_Float),
	ETypeId_Quat16				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 32, 4, 2, EDataType_UInt),

	ETypeId_List				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 33, 1, _TYPESIZE_UNDEF, EDataType_Custom),

	ETypeId_EGenericError		= _makeTypeId(_LIBRARYID_DEFAULT, 0, 34, 1, 4, EDataType_Enum),
	ETypeId_EErrorParamValues	= _makeTypeId(_LIBRARYID_DEFAULT, 0, 35, 1, 1, EDataType_Enum),

	ETypeId_Error				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 36, 1, _TYPESIZE_UNDEF, EDataType_Custom),
	ETypeId_Stacktrace			= _makeTypeId(_LIBRARYID_DEFAULT, 0, 37, 1, _TYPESIZE_UNDEF, EDataType_Custom),
	ETypeId_BitRef				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 38, 1, _TYPESIZE_UNDEF, EDataType_Custom),
	ETypeId_Allocator			= _makeTypeId(_LIBRARYID_DEFAULT, 0, 39, 1, _TYPESIZE_UNDEF, EDataType_Interface),

	ETypeId_EDataTypePrimitive	= _makeTypeId(_LIBRARYID_DEFAULT, 0, 40, 1, 1, EDataType_Enum),
	ETypeId_EDataType			= _makeTypeId(_LIBRARYID_DEFAULT, 0, 41, 1, 1, EDataType_Enum),
	ETypeId_TypeId				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 42, 1, 4, EDataType_Enum)

} ETypeId;

EDataType ETypeId_getDataType(ETypeId id);
U8 ETypeId_getDataTypeBytes(ETypeId id);
U8 ETypeId_getElements(ETypeId id);
Bool ETypeId_hasValidSize(ETypeId id);
U64 ETypeId_getBytes(ETypeId id);
U8 ETypeId_getLibrarySubId(ETypeId id);
U8 ETypeId_getLibraryId(ETypeId id);

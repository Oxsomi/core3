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

#define _makeTypeId(libId, subId, typeId, elementCount, dataTypeBytes, dataType) \
((libId << 24) | (subId << 22) | (typeId << 13) | ((elementCount - 1) << 7) | ((dataTypeBytes - 1) << 4) | dataType)

#define _LIBRARYID_DEFAULT 0xC3
#define _TYPESIZE_UNDEF 7

typedef enum TypeId {

	TypeId_I8					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 0, 1, 1, EDataType_Int),
	TypeId_I16					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 1, 1, 2, EDataType_Int),
	TypeId_I32					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 2, 1, 4, EDataType_Int),
	TypeId_I64					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 3, 1, 8, EDataType_Int),

	TypeId_U8					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 4, 1, 1, EDataType_UInt),
	TypeId_U16					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 5, 1, 2, EDataType_UInt),
	TypeId_U32					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 6, 1, 4, EDataType_UInt),
	TypeId_U64					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 7, 1, 8, EDataType_UInt),

	TypeId_F32					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 8, 1, 4, EDataType_Float),

	TypeId_Ns					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 9 , 1, 8, EDataType_UInt),
	TypeId_DNs					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 10, 1, 8, EDataType_Int),

	TypeId_C8					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 11, 1, 1, EDataType_Char),
	TypeId_Bool					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 12, 1, 1, EDataType_Bool),

	TypeId_Buffer				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 13, 1, _TYPESIZE_UNDEF, EDataType_Custom),

	TypeId_EStringCase			= _makeTypeId(_LIBRARYID_DEFAULT, 0, 14, 1, 1, EDataType_Enum),
	TypeId_EStringTransform		= _makeTypeId(_LIBRARYID_DEFAULT, 0, 15, 1, 1, EDataType_Enum),

	TypeId_I32x2				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 16, 2, 4, EDataType_Int),
	TypeId_I32x4				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 17, 4, 4, EDataType_Int),
	TypeId_F32x2				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 18, 2, 4, EDataType_Float),
	TypeId_F32x4				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 19, 4, 4, EDataType_Float),

	TypeId_Transform			= _makeTypeId(_LIBRARYID_DEFAULT, 0, 20, 12, 4, EDataType_Float),
	TypeId_PackedTransform		= _makeTypeId(_LIBRARYID_DEFAULT, 0, 21, 8 , 4, EDataType_UInt),
	TypeId_Transform2D			= _makeTypeId(_LIBRARYID_DEFAULT, 0, 22, 4 , 4, EDataType_Float),

	TypeId_TilemapTransform		= _makeTypeId(_LIBRARYID_DEFAULT, 0, 23, 1, 8, EDataType_UInt),

	TypeId_EMirrored			= _makeTypeId(_LIBRARYID_DEFAULT, 0, 24, 1, 1, EDataType_Enum),
	TypeId_ERotated				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 25, 1, 1, EDataType_Enum),
	TypeId_EFormatStatus		= _makeTypeId(_LIBRARYID_DEFAULT, 0, 26, 1, 1, EDataType_Enum),

	TypeId_ShortString			= _makeTypeId(_LIBRARYID_DEFAULT, 0, 27, 32, 1, EDataType_Char),
	TypeId_LongString			= _makeTypeId(_LIBRARYID_DEFAULT, 0, 28, 64, 1, EDataType_Char),

	TypeId_String				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 29, 1, _TYPESIZE_UNDEF, EDataType_String),
	TypeId_StringList			= _makeTypeId(_LIBRARYID_DEFAULT, 0, 30, 1, _TYPESIZE_UNDEF, EDataType_Custom),

	TypeId_Quat					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 31, 4, 4, EDataType_Float),
	TypeId_Quat16				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 32, 4, 2, EDataType_UInt),

	TypeId_List					= _makeTypeId(_LIBRARYID_DEFAULT, 0, 33, 1, _TYPESIZE_UNDEF, EDataType_Custom),

	TypeId_EGenericError		= _makeTypeId(_LIBRARYID_DEFAULT, 0, 34, 1, 4, EDataType_Enum),
	TypeId_EErrorParamValues	= _makeTypeId(_LIBRARYID_DEFAULT, 0, 35, 1, 1, EDataType_Enum),

	TypeId_Error				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 36, 1, _TYPESIZE_UNDEF, EDataType_Custom),
	TypeId_Stacktrace			= _makeTypeId(_LIBRARYID_DEFAULT, 0, 37, 1, _TYPESIZE_UNDEF, EDataType_Custom),
	TypeId_BitRef				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 38, 1, _TYPESIZE_UNDEF, EDataType_Custom),
	TypeId_Allocator			= _makeTypeId(_LIBRARYID_DEFAULT, 0, 39, 1, _TYPESIZE_UNDEF, EDataType_Interface),

	TypeId_EDataTypePrimitive	= _makeTypeId(_LIBRARYID_DEFAULT, 0, 40, 1, 1, EDataType_Enum),
	TypeId_EDataType			= _makeTypeId(_LIBRARYID_DEFAULT, 0, 41, 1, 1, EDataType_Enum),
	TypeId_TypeId				= _makeTypeId(_LIBRARYID_DEFAULT, 0, 42, 1, 4, EDataType_Enum)

} TypeId;

EDataType TypeId_getDataType(TypeId id);
U8 TypeId_getDataTypeBytes(TypeId id);
U8 TypeId_getElements(TypeId id);
Bool TypeId_hasValidSize(TypeId id);
U64 TypeId_getBytes(TypeId id);
U8 TypeId_getLibrarySubId(TypeId id);
U8 TypeId_getLibraryId(TypeId id);

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

inline EDataTypePrimitive EDataType_getPrimitive(EDataType type) { return (EDataTypePrimitive)(type >> 1); }
inline Bool EDataType_isSigned(EDataType type) { return type & EDataType_IsSigned; }

#define _makeTypeId(libId, subId, typeId, elementCount, dataTypeBytes, dataType) \
((libId << 24) | (subId << 22) | (typeId << 13) | ((elementCount - 1) << 7) | ((dataTypeBytes - 1) << 4) | dataType)

#define _LibraryId_OxC3 0xC3
#define _TypeSize_Undef 7

typedef enum TypeId {

	TypeId_I8					= _makeTypeId(_LibraryId_OxC3, 0, 0, 1, 1, EDataType_Int),
	TypeId_I16					= _makeTypeId(_LibraryId_OxC3, 0, 1, 1, 2, EDataType_Int),
	TypeId_I32					= _makeTypeId(_LibraryId_OxC3, 0, 2, 1, 4, EDataType_Int),
	TypeId_I64					= _makeTypeId(_LibraryId_OxC3, 0, 3, 1, 8, EDataType_Int),

	TypeId_U8					= _makeTypeId(_LibraryId_OxC3, 0, 4, 1, 1, EDataType_UInt),
	TypeId_U16					= _makeTypeId(_LibraryId_OxC3, 0, 5, 1, 2, EDataType_UInt),
	TypeId_U32					= _makeTypeId(_LibraryId_OxC3, 0, 6, 1, 4, EDataType_UInt),
	TypeId_U64					= _makeTypeId(_LibraryId_OxC3, 0, 7, 1, 8, EDataType_UInt),

	TypeId_F32					= _makeTypeId(_LibraryId_OxC3, 0, 8, 1, 4, EDataType_Float),

	TypeId_Ns					= _makeTypeId(_LibraryId_OxC3, 0, 9 , 1, 8, EDataType_UInt),
	TypeId_DNs					= _makeTypeId(_LibraryId_OxC3, 0, 10, 1, 8, EDataType_Int),

	TypeId_C8					= _makeTypeId(_LibraryId_OxC3, 0, 11, 1, 1, EDataType_Char),
	TypeId_Bool					= _makeTypeId(_LibraryId_OxC3, 0, 12, 1, 1, EDataType_Bool),

	TypeId_Buffer				= _makeTypeId(_LibraryId_OxC3, 0, 13, 1, _TypeSize_Undef, EDataType_Custom),

	TypeId_EStringCase			= _makeTypeId(_LibraryId_OxC3, 0, 14, 1, 1, EDataType_Enum),
	TypeId_EStringTransform		= _makeTypeId(_LibraryId_OxC3, 0, 15, 1, 1, EDataType_Enum),

	TypeId_I32x2				= _makeTypeId(_LibraryId_OxC3, 0, 16, 2, 4, EDataType_Int),
	TypeId_I32x4				= _makeTypeId(_LibraryId_OxC3, 0, 17, 4, 4, EDataType_Int),
	TypeId_F32x2				= _makeTypeId(_LibraryId_OxC3, 0, 18, 2, 4, EDataType_Float),
	TypeId_F32x4				= _makeTypeId(_LibraryId_OxC3, 0, 19, 4, 4, EDataType_Float),

	TypeId_Transform			= _makeTypeId(_LibraryId_OxC3, 0, 20, 12, 4, EDataType_Float),
	TypeId_PackedTransform		= _makeTypeId(_LibraryId_OxC3, 0, 21, 8 , 4, EDataType_UInt),
	TypeId_Transform2D			= _makeTypeId(_LibraryId_OxC3, 0, 22, 4 , 4, EDataType_Float),

	TypeId_TilemapTransform		= _makeTypeId(_LibraryId_OxC3, 0, 23, 1, 8, EDataType_UInt),

	TypeId_EMirrored			= _makeTypeId(_LibraryId_OxC3, 0, 24, 1, 1, EDataType_Enum),
	TypeId_ERotated				= _makeTypeId(_LibraryId_OxC3, 0, 25, 1, 1, EDataType_Enum),
	TypeId_EFormatStatus		= _makeTypeId(_LibraryId_OxC3, 0, 26, 1, 1, EDataType_Enum),

	TypeId_ShortString			= _makeTypeId(_LibraryId_OxC3, 0, 27, 32, 1, EDataType_Char),
	TypeId_LongString			= _makeTypeId(_LibraryId_OxC3, 0, 28, 64, 1, EDataType_Char),

	TypeId_String				= _makeTypeId(_LibraryId_OxC3, 0, 29, 1, _TypeSize_Undef, EDataType_String),
	TypeId_StringList			= _makeTypeId(_LibraryId_OxC3, 0, 30, 1, _TypeSize_Undef, EDataType_Custom),

	TypeId_Quat					= _makeTypeId(_LibraryId_OxC3, 0, 31, 4, 4, EDataType_Float),
	TypeId_Quat16				= _makeTypeId(_LibraryId_OxC3, 0, 32, 4, 2, EDataType_UInt),

	TypeId_List					= _makeTypeId(_LibraryId_OxC3, 0, 33, 1, _TypeSize_Undef, EDataType_Custom),

	TypeId_EGenericError		= _makeTypeId(_LibraryId_OxC3, 0, 34, 1, 4, EDataType_Enum),
	TypeId_EErrorParamValues	= _makeTypeId(_LibraryId_OxC3, 0, 35, 1, 1, EDataType_Enum),

	TypeId_Error				= _makeTypeId(_LibraryId_OxC3, 0, 36, 1, _TypeSize_Undef, EDataType_Custom),
	TypeId_Stacktrace			= _makeTypeId(_LibraryId_OxC3, 0, 37, 1, _TypeSize_Undef, EDataType_Custom),
	TypeId_BitRef				= _makeTypeId(_LibraryId_OxC3, 0, 38, 1, _TypeSize_Undef, EDataType_Custom),
	TypeId_Allocator			= _makeTypeId(_LibraryId_OxC3, 0, 39, 1, _TypeSize_Undef, EDataType_Interface),

	TypeId_EDataTypePrimitive	= _makeTypeId(_LibraryId_OxC3, 0, 40, 1, 1, EDataType_Enum),
	TypeId_EDataType			= _makeTypeId(_LibraryId_OxC3, 0, 41, 1, 1, EDataType_Enum),
	TypeId_TypeId				= _makeTypeId(_LibraryId_OxC3, 0, 42, 1, 4, EDataType_Enum)

} TypeId;

inline EDataType TypeId_getDataType(TypeId id) { return (EDataType)(id & 0xF); }
inline U8 TypeId_getDataTypeBytes(TypeId id) { return ((id >> 4) & 7) + 1; }
inline U8 TypeId_getElements(TypeId id) { return ((id >> 7) & 63) + 1; }

inline Bool TypeId_hasValidSize(TypeId id) { return TypeId_getDataTypeBytes(id) != _TypeSize_Undef; }

inline U64 TypeId_getBytes(TypeId id) { 
	return TypeId_hasValidSize(id) ? (U64)TypeId_getDataTypeBytes(id) * TypeId_getElements(id) : U64_MAX;
}

inline U8 TypeId_getLibrarySubId(TypeId id) { return (U8)((id >> 22) & 3); }
inline U8 TypeId_getLibraryId(TypeId id) { return (U8)((id >> 24) & 0xF); }

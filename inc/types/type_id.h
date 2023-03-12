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
#include "types.h"

typedef struct Allocator Allocator;
typedef struct Error Error;
typedef struct CharString CharString;

typedef enum EDataType {

	EDataType_UInt			= 0b000,
	EDataType_Int			= 0b001,
	EDataType_Float			= 0b011,
	EDataType_Bool			= 0b100,
	EDataType_Char			= 0b110,

	EDataType_IsSigned		= 0b001

} EDataType;

typedef enum EDataTypeStride {

	EDataTypeStride_8,
	EDataTypeStride_16,
	EDataTypeStride_32,
	EDataTypeStride_64

} EDataTypeStride;

Bool EDataType_isSigned(EDataType type);

//Layout is as follows:
//U8 library id
//U8 type id
//U9 padding
//U2 width
//U2 height
//U2 dataTypeStride (EDataTypeStride)
//U3 dataType (EDataType)

#define _makeTypeId(libId, typeId, width, height, dataTypeStride, dataType)										\
((libId << 24) | ((typeId) << 16) | ((width - 1) << 7) | ((height - 1) << 5) | (dataTypeStride << 3) | dataType)

#define _LIBRARYID_DEFAULT 0xC3		//OxC0-OxCF are reserved for default library.

//Vector expands for ints

#define _ETypeIdXIntVec(start, prefix, dataType, w) 																	\
ETypeId_##prefix##8##x##w			= _makeTypeId(_LIBRARYID_DEFAULT, start + 0, w, 1, EDataTypeStride_8 , dataType),	\
ETypeId_##prefix##16##x##w			= _makeTypeId(_LIBRARYID_DEFAULT, start + 1, w, 1, EDataTypeStride_16, dataType),	\
ETypeId_##prefix##32##x##w			= _makeTypeId(_LIBRARYID_DEFAULT, start + 2, w, 1, EDataTypeStride_32, dataType),	\
ETypeId_##prefix##64##x##w			= _makeTypeId(_LIBRARYID_DEFAULT, start + 3, w, 1, EDataTypeStride_64, dataType)

#define _ETypeIdXIntVecN(start, prefix, dataType)																		\
_ETypeIdXIntVec(start + 0, prefix, dataType, 2),																		\
_ETypeIdXIntVec(start + 4, prefix, dataType, 3),																		\
_ETypeIdXIntVec(start + 8, prefix, dataType, 4)

//Matrix expands for floats

#define _ETypeIdXIntMatWH(start, prefix, dataType, w, h) 																\
ETypeId_##prefix##8##x##w##x##h		= _makeTypeId(_LIBRARYID_DEFAULT, start + 0, w, h, EDataTypeStride_8 , dataType),	\
ETypeId_##prefix##16##x##w##x##h	= _makeTypeId(_LIBRARYID_DEFAULT, start + 1, w, h, EDataTypeStride_16, dataType),	\
ETypeId_##prefix##32##x##w##x##h	= _makeTypeId(_LIBRARYID_DEFAULT, start + 2, w, h, EDataTypeStride_32, dataType),	\
ETypeId_##prefix##64##x##w##x##h	= _makeTypeId(_LIBRARYID_DEFAULT, start + 3, w, h, EDataTypeStride_64, dataType)

#define _ETypeIdXIntMatW(start, prefix, dataType, w) 																	\
_ETypeIdXIntMatWH(start + 0, prefix, dataType, w, 2),																	\
_ETypeIdXIntMatWH(start + 4, prefix, dataType, w, 3),																	\
_ETypeIdXIntMatWH(start + 8, prefix, dataType, w, 4)

#define _ETypeIdXIntMat(start, prefix, dataType) 																		\
_ETypeIdXIntMatW(start + 0 , prefix, dataType, 2),																		\
_ETypeIdXIntMatW(start + 12, prefix, dataType, 3),																		\
_ETypeIdXIntMatW(start + 24, prefix, dataType, 4)

//Int expand

#define _ETypeIdXInt(start, prefix, dataType)																			\
ETypeId_##prefix##8					= _makeTypeId(_LIBRARYID_DEFAULT, start + 0 , 1, 1, EDataTypeStride_8 , dataType),	\
ETypeId_##prefix##16				= _makeTypeId(_LIBRARYID_DEFAULT, start + 1 , 1, 1, EDataTypeStride_16, dataType),	\
ETypeId_##prefix##32				= _makeTypeId(_LIBRARYID_DEFAULT, start + 2 , 1, 1, EDataTypeStride_32, dataType),	\
ETypeId_##prefix##64				= _makeTypeId(_LIBRARYID_DEFAULT, start + 3 , 1, 1, EDataTypeStride_64, dataType)

//Float expand

#define _ETypeIdFloat(start, prefix, dataType)																			\
ETypeId_F16							= _makeTypeId(_LIBRARYID_DEFAULT, start + 1, 1, 1, EDataTypeStride_16, dataType),	\
ETypeId_F32							= _makeTypeId(_LIBRARYID_DEFAULT, start + 2, 1, 1, EDataTypeStride_32, dataType),	\
ETypeId_F64							= _makeTypeId(_LIBRARYID_DEFAULT, start + 3, 1, 1, EDataTypeStride_64, dataType)

//Float vector expand

#define _ETypeIdFloatVec(start, w) 																						\
ETypeId_F16x##w			= _makeTypeId(_LIBRARYID_DEFAULT, start + 0, w, 1, EDataTypeStride_16, EDataType_Float),		\
ETypeId_F32x##w			= _makeTypeId(_LIBRARYID_DEFAULT, start + 1, w, 1, EDataTypeStride_32, EDataType_Float),		\
ETypeId_F64x##w			= _makeTypeId(_LIBRARYID_DEFAULT, start + 2, w, 1, EDataTypeStride_64, EDataType_Float)

#define _ETypeIdFloatVecN(start)																						\
_ETypeIdFloatVec(start + 0, 2),																							\
_ETypeIdFloatVec(start + 3, 3),																							\
_ETypeIdFloatVec(start + 6, 4)

//Float matrix expand

#define _ETypeIdFloatMatWH(start, w, h) 																				\
ETypeId_F16x##w##x##h	= _makeTypeId(_LIBRARYID_DEFAULT, start + 0, w, h, EDataTypeStride_16, EDataType_Float),		\
ETypeId_F32x##w##x##h	= _makeTypeId(_LIBRARYID_DEFAULT, start + 1, w, h, EDataTypeStride_32, EDataType_Float),		\
ETypeId_F64x##w##x##h	= _makeTypeId(_LIBRARYID_DEFAULT, start + 2, w, h, EDataTypeStride_64, EDataType_Float)

#define _ETypeIdFloatMatW(start, w) 																					\
_ETypeIdFloatMatWH(start + 0, w, 2),																					\
_ETypeIdFloatMatWH(start + 3, w, 3),																					\
_ETypeIdFloatMatWH(start + 6, w, 4)

#define _ETypeIdFloatMat(start) 																						\
_ETypeIdFloatMatW(start + 0 , 2),																						\
_ETypeIdFloatMatW(start + 9 , 3),																						\
_ETypeIdFloatMatW(start + 18, 4)

//All possible types

typedef enum ETypeId {

	ETypeId_C8									= _makeTypeId(_LIBRARYID_DEFAULT, 0, 1, 1, EDataTypeStride_8 , EDataType_Char),
	ETypeId_Bool								= _makeTypeId(_LIBRARYID_DEFAULT, 1, 1, 1, EDataTypeStride_8 , EDataType_Bool),

	_ETypeIdXInt(2, I, EDataType_Int),			//I<8/16/32/64>
	_ETypeIdXInt(6, U, EDataType_UInt),			//U<8/16/32/64>

	_ETypeIdFloat(10, F, EDataType_Float),		//F<16/32/64>

	_ETypeIdXIntVecN(13, I, EDataType_Int),		//I<8/16/32/64>x<2/3/4>
	_ETypeIdXIntVecN(25, U, EDataType_UInt),	//I<8/16/32/64>x<2/3/4>
	_ETypeIdFloatVecN(37),						//F<16/32/64>x<2/3/4>

	_ETypeIdXIntMat(46, I, EDataType_Int),		//I<8/16/32/64>x<2/3/4>x<2/3/4>
	_ETypeIdXIntMat(82, U, EDataType_UInt),		//U<8/16/32/64>x<2/3/4>x<2/3/4>
	_ETypeIdFloatMat(118),						//F<16/32/64>x<2/3/4>x<2/3/4>

	ETypeId_Max = 145,

	ETypeId_Undefined	= 0xFFFFFFFF

} ETypeId;

typedef U16 ETypeIdShort;	//Cuts off the info part to more efficiently store type id.
typedef U8 ETypeIdOxC3;		//Type id for OxC3; only if standard types are used only (no extensions).

EDataType ETypeId_getDataType(ETypeId id);
U8 ETypeId_getDataTypeBytes(ETypeId id);
U8 ETypeId_getWidth(ETypeId id);
U8 ETypeId_getHeight(ETypeId id);
U8 ETypeId_getElements(ETypeId id);
U64 ETypeId_getBytes(ETypeId id);
U8 ETypeId_getLibraryId(ETypeId id);
U8 ETypeId_getLibraryTypeId(ETypeId id);

//Before using registry and string conversions it has to be created.
//In OxC3_platforms dependencies this is done automatically, 
// but not in projects that use OxC3_types only.
//Any registerType or string conversion will return Error_invalidOperation if these are not created.

Error ETypeId_create(Allocator alloc);
Bool ETypeId_free(Allocator alloc);

typedef Error (*ETypeIdStringifyFunc)(ETypeId typeId, Buffer data, Allocator alloc, CharString *tmp);

Error ETypeId_stringify(ETypeId typeId, Buffer data, Allocator alloc, CharString *stringified);

Error ETypeId_registerTypeId(ETypeId id, CharString name, ETypeIdStringifyFunc stringifyFunc, Allocator alloc);

Error ETypeId_registerType(
	U8 libraryId, 
	U8 typeId, 
	U8 width, 
	U8 height, 
	EDataTypeStride dataTypeStride, 
	EDataType dataType, 
	CharString name,
	ETypeIdStringifyFunc stringifyFunc,
	ETypeId *result,
	Allocator alloc
);

Error ETypeId_asString(ETypeId id, CharString *result);
Error ETypeId_fromString(CharString str, ETypeId *id);

Error ETypeId_getTypeFromDesc(U16 desc, U8 libraryId, ETypeId *res);
Error ETypeId_getTypeFromShortType(U8 libraryId, U8 libraryTypeId, ETypeId *res);

Error ETypeId_getBaseType(ETypeId id, ETypeId *res);		//The base type of F32x4 would be F32, F32x4x4 would be F32x4
Error ETypeId_getPureType(ETypeId id, ETypeId *res);		//The full basic type. For example F32x4x4 goes to F32


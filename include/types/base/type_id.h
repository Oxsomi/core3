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

//types/base/type_id.h

#pragma once
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum EDataType {

	EDataType_UInt           = 0b000,
	EDataType_Int            = 0b001,
	EDataType_Float          = 0b011,
	EDataType_Bool           = 0b100,        //Is actually 1 bit (even if stride is 8)
	EDataType_Char           = 0b110,

	EDataType_IsSigned       = 0b001,
	EDataType_Object         = 0b101

} EDataType;

typedef enum EDataTypeStride {
	EDataTypeStride_8,
	EDataTypeStride_16,
	EDataTypeStride_32,
	EDataTypeStride_64
} EDataTypeStride;

static inline Bool EDataType_isSigned(EDataType type) { return type & EDataType_IsSigned; }

//Layout is as follows:
//U13 library id (0x1000-0x1FFF are reserved for default library)
//U10 type id (optional for non objects)
//U2 width
//U2 height
//U2 dataTypeStride (EDataTypeStride)
//U3 dataType (EDataType)

#define makeTypeId(libId, typeId, width, height, dataTypeStride, dataType)                                                \
	((U32)(                                                                                                               \
		((libId) << 19) | ((typeId) << 9) |                                                                               \
		(((width) - 1) << 7) | (((height) - 1) << 5) | ((dataTypeStride) << 3) | (dataType)                               \
	))

#define makeObjectId(libId, typeId, properties) \
	(((libId) << 19) | ((typeId) << 9) | ((properties) << 3) | EDataType_Object)

#define LIBRARYID_DEFAULT 0x1C30

//Vector expands for ints

#define ETIDAsg(...) = __VA_ARGS__

#define ETypeIdXIntVec(prefix, dataType, w)                                                                               \
ETypeId_##prefix##8##x##w    ETIDAsg(makeTypeId(LIBRARYID_DEFAULT, 0, w, 1, EDataTypeStride_8 , dataType)),               \
ETypeId_##prefix##16##x##w    ETIDAsg(makeTypeId(LIBRARYID_DEFAULT, 0, w, 1, EDataTypeStride_16, dataType)),              \
ETypeId_##prefix##32##x##w    ETIDAsg(makeTypeId(LIBRARYID_DEFAULT, 0, w, 1, EDataTypeStride_32, dataType)),              \
ETypeId_##prefix##64##x##w    ETIDAsg(makeTypeId(LIBRARYID_DEFAULT, 0, w, 1, EDataTypeStride_64, dataType))

#define ETypeIdXIntVecN(prefix, dataType)                                                                                 \
ETypeIdXIntVec(prefix, dataType, 2),                                                                                      \
ETypeIdXIntVec(prefix, dataType, 3),                                                                                      \
ETypeIdXIntVec(prefix, dataType, 4)

//Matrix expands for floats

#define ETypeIdXIntMatWH(prefix, dataType, w, h)                                                                          \
ETypeId_##prefix##8##x##w##x##h        ETIDAsg(makeTypeId(LIBRARYID_DEFAULT, 0, w, h, EDataTypeStride_8 , dataType)),     \
ETypeId_##prefix##16##x##w##x##h    ETIDAsg(makeTypeId(LIBRARYID_DEFAULT, 0, w, h, EDataTypeStride_16, dataType)),        \
ETypeId_##prefix##32##x##w##x##h    ETIDAsg(makeTypeId(LIBRARYID_DEFAULT, 0, w, h, EDataTypeStride_32, dataType)),        \
ETypeId_##prefix##64##x##w##x##h    ETIDAsg(makeTypeId(LIBRARYID_DEFAULT, 0, w, h, EDataTypeStride_64, dataType))

#define ETypeIdXIntMatW(prefix, dataType, h)                                                                              \
ETypeIdXIntMatWH(prefix, dataType, 1, h),                                                                                 \
ETypeIdXIntMatWH(prefix, dataType, 2, h),                                                                                 \
ETypeIdXIntMatWH(prefix, dataType, 3, h),                                                                                 \
ETypeIdXIntMatWH(prefix, dataType, 4, h)

#define ETypeIdXIntMat(prefix, dataType)                                                                                  \
ETypeIdXIntMatW(prefix, dataType, 2),                                                                                     \
ETypeIdXIntMatW(prefix, dataType, 3),                                                                                     \
ETypeIdXIntMatW(prefix, dataType, 4)

//Int expand

#define ETypeIdXInt(prefix, dataType)                                                                                     \
ETypeId_##prefix##8        ETIDAsg(makeTypeId(LIBRARYID_DEFAULT, 0, 1, 1, EDataTypeStride_8 , dataType)),                 \
ETypeId_##prefix##16    ETIDAsg(makeTypeId(LIBRARYID_DEFAULT, 0, 1, 1, EDataTypeStride_16, dataType)),                    \
ETypeId_##prefix##32    ETIDAsg(makeTypeId(LIBRARYID_DEFAULT, 0, 1, 1, EDataTypeStride_32, dataType)),                    \
ETypeId_##prefix##64    ETIDAsg(makeTypeId(LIBRARYID_DEFAULT, 0, 1, 1, EDataTypeStride_64, dataType))

//Float expand

#define ETypeIdFloat(prefix, dataType)                                                                                    \
ETypeId_F16                ETIDAsg(makeTypeId(LIBRARYID_DEFAULT, 0, 1, 1, EDataTypeStride_16, dataType)),                 \
ETypeId_F32                ETIDAsg(makeTypeId(LIBRARYID_DEFAULT, 0, 1, 1, EDataTypeStride_32, dataType)),                 \
ETypeId_F64                ETIDAsg(makeTypeId(LIBRARYID_DEFAULT, 0, 1, 1, EDataTypeStride_64, dataType))

//Float vector expand

#define ETypeIdFloatVec(w)                                                                                                \
ETypeId_F16x##w            ETIDAsg(makeTypeId(LIBRARYID_DEFAULT, 0, w, 1, EDataTypeStride_16, EDataType_Float)),          \
ETypeId_F32x##w            ETIDAsg(makeTypeId(LIBRARYID_DEFAULT, 0, w, 1, EDataTypeStride_32, EDataType_Float)),          \
ETypeId_F64x##w            ETIDAsg(makeTypeId(LIBRARYID_DEFAULT, 0, w, 1, EDataTypeStride_64, EDataType_Float))

#define ETypeIdFloatVecN() ETypeIdFloatVec(2), ETypeIdFloatVec(3), ETypeIdFloatVec(4)

//Float matrix expand

#define ETypeIdFloatMatWH(w, h)                                                                                           \
ETypeId_F16x##w##x##h    ETIDAsg(makeTypeId(LIBRARYID_DEFAULT, 0, w, h, EDataTypeStride_16, EDataType_Float)),            \
ETypeId_F32x##w##x##h    ETIDAsg(makeTypeId(LIBRARYID_DEFAULT, 0, w, h, EDataTypeStride_32, EDataType_Float)),            \
ETypeId_F64x##w##x##h    ETIDAsg(makeTypeId(LIBRARYID_DEFAULT, 0, w, h, EDataTypeStride_64, EDataType_Float))

#define ETypeIdFloatMatW(w)                                                                                               \
ETypeIdFloatMatWH(1, w), ETypeIdFloatMatWH(2, w), ETypeIdFloatMatWH(3, w), ETypeIdFloatMatWH(4, w)

#define ETypeIdFloatMat()                                                                                                 \
ETypeIdFloatMatW(2), ETypeIdFloatMatW(3), ETypeIdFloatMatW(4)

//All possible types

typedef enum ETypeId_ {

	ETypeId_C8                                    = makeTypeId(LIBRARYID_DEFAULT, 0, 1, 1, EDataTypeStride_8, EDataType_Char),
	ETypeId_B1                                    = makeTypeId(LIBRARYID_DEFAULT, 0, 1, 1, EDataTypeStride_8, EDataType_Bool),

	ETypeIdXInt(I, EDataType_Int),                //I<8/16/32/64>
	ETypeIdXInt(U, EDataType_UInt),               //U<8/16/32/64>

	ETypeIdFloat(F, EDataType_Float),             //F<16/32/64>

	ETypeId_B1x2                                  = makeTypeId(LIBRARYID_DEFAULT, 0, 2, 1, EDataTypeStride_8, EDataType_Bool),
	ETypeId_B1x3                                  = makeTypeId(LIBRARYID_DEFAULT, 0, 3, 1, EDataTypeStride_8, EDataType_Bool),
	ETypeId_B1x4                                  = makeTypeId(LIBRARYID_DEFAULT, 0, 4, 1, EDataTypeStride_8, EDataType_Bool),

	ETypeIdXIntVecN(I, EDataType_Int),            //I<8/16/32/64>x<2/3/4>
	ETypeIdXIntVecN(U, EDataType_UInt),           //U<8/16/32/64>x<2/3/4>
	ETypeIdFloatVecN(),                           //F<16/32/64>x<2/3/4>

	ETypeId_B1x1x2                                = makeTypeId(LIBRARYID_DEFAULT, 0, 1, 2, EDataTypeStride_8, EDataType_Bool),
	ETypeId_B1x2x2                                = makeTypeId(LIBRARYID_DEFAULT, 0, 2, 2, EDataTypeStride_8, EDataType_Bool),
	ETypeId_B1x3x2                                = makeTypeId(LIBRARYID_DEFAULT, 0, 3, 2, EDataTypeStride_8, EDataType_Bool),
	ETypeId_B1x4x2                                = makeTypeId(LIBRARYID_DEFAULT, 0, 4, 2, EDataTypeStride_8, EDataType_Bool),

	ETypeId_B1x1x3                                = makeTypeId(LIBRARYID_DEFAULT, 0, 1, 3, EDataTypeStride_8, EDataType_Bool),
	ETypeId_B1x2x3                                = makeTypeId(LIBRARYID_DEFAULT, 0, 2, 3, EDataTypeStride_8, EDataType_Bool),
	ETypeId_B1x3x3                                = makeTypeId(LIBRARYID_DEFAULT, 0, 3, 3, EDataTypeStride_8, EDataType_Bool),
	ETypeId_B1x4x3                                = makeTypeId(LIBRARYID_DEFAULT, 0, 4, 3, EDataTypeStride_8, EDataType_Bool),

	ETypeId_B1x1x4                                = makeTypeId(LIBRARYID_DEFAULT, 0, 1, 4, EDataTypeStride_8, EDataType_Bool),
	ETypeId_B1x2x4                                = makeTypeId(LIBRARYID_DEFAULT, 0, 2, 4, EDataTypeStride_8, EDataType_Bool),
	ETypeId_B1x3x4                                = makeTypeId(LIBRARYID_DEFAULT, 0, 3, 4, EDataTypeStride_8, EDataType_Bool),
	ETypeId_B1x4x4                                = makeTypeId(LIBRARYID_DEFAULT, 0, 4, 4, EDataTypeStride_8, EDataType_Bool),

	ETypeIdXIntMat(I, EDataType_Int),             //I<8/16/32/64>x<1/2/3/4>x<2/3/4>
	ETypeIdXIntMat(U, EDataType_UInt),            //U<8/16/32/64>x<1/2/3/4>x<2/3/4>
	ETypeIdFloatMat(),                            //F<16/32/64>x<1/2/3/4>x<2/3/4>

	ETypeId_Max = 193,

	ETypeId_Undefined    = 0xFFFFFFFF

} ETypeId_;             //Needs all bits, but upper bit isn't always considered the same when casting due to enum shenanigans

typedef U32 ETypeId;    //Exception to the 'enum is ETypeId and value is TypeId' rule, for compatibility only

#undef ETIDAsg
#define ETIDAsg(...)

static const ETypeId ETypeId_arr[ETypeId_Max] = {

	ETypeId_C8, ETypeId_B1,

	ETypeIdXInt(I, EDataType_Int),                 //I<8/16/32/64>
	ETypeIdXInt(U, EDataType_UInt),                //U<8/16/32/64>

	ETypeIdFloat(F, EDataType_Float),             //F<16/32/64>

	ETypeId_B1x2,
	ETypeId_B1x3,
	ETypeId_B1x4,

	ETypeIdXIntVecN(I, EDataType_Int),            //I<8/16/32/64>x<2/3/4>
	ETypeIdXIntVecN(U, EDataType_UInt),           //U<8/16/32/64>x<2/3/4>
	ETypeIdFloatVecN(),                           //F<16/32/64>x<2/3/4>

	ETypeId_B1x1x2, ETypeId_B1x2x2, ETypeId_B1x3x2, ETypeId_B1x4x2,
	ETypeId_B1x1x3, ETypeId_B1x2x3, ETypeId_B1x3x3, ETypeId_B1x4x3,
	ETypeId_B1x1x4, ETypeId_B1x2x4, ETypeId_B1x3x4, ETypeId_B1x4x4,

	ETypeIdXIntMat(I, EDataType_Int),             //I<8/16/32/64>x<1/2/3/4>x<2/3/4>
	ETypeIdXIntMat(U, EDataType_UInt),            //U<8/16/32/64>x<1/2/3/4>x<2/3/4>
	ETypeIdFloatMat()                             //F<16/32/64>x<1/2/3/4>x<2/3/4>
};

typedef U8 TypeIdShort;

TypeIdShort ETypeId_toShortId(ETypeId id);

#undef ETIDAsg

static inline EDataType ETypeId_getDataType(ETypeId id) { return (EDataType)(id & 7); }
static inline EDataTypeStride ETypeId_getDataTypeStride(ETypeId id) { return (EDataTypeStride)((id >> 3) & 3); }
static inline Bool ETypeId_isObject(ETypeId id) { return ETypeId_getDataType(id) == EDataType_Object; }

static inline U8 ETypeId_getDataTypeBytes(ETypeId id) {

	EDataType type = ETypeId_getDataType(id);

	if (type == EDataType_Char || type == EDataType_Bool)
		return 1;

	return ETypeId_isObject(id) ? 0 : (1 << type);
}

static inline U8 ETypeId_getHeight(ETypeId id) { return ETypeId_isObject(id) ? 0 : (((id >> 5) & 3) + 1); }
static inline U8 ETypeId_getWidth(ETypeId id) { return ETypeId_isObject(id) ? 0 : (((id >> 7) & 3) + 1); }

static inline U8 ETypeId_getElements(ETypeId id) {
	return ETypeId_isObject(id) ? 0 : ETypeId_getWidth(id) * ETypeId_getHeight(id);
}

static inline U64 ETypeId_getBytes(ETypeId id) {

	U64 siz = ETypeId_isObject(id) ? 0 : (U64)ETypeId_getDataTypeBytes(id) * ETypeId_getElements(id);

	if (ETypeId_getDataType(id) == EDataType_Bool)    //Bits, not bytes
		return (siz + 7) >> 3;

	return siz;
}

static inline U16 ETypeId_getLibraryId(ETypeId id) { return (U16)(id >> 19); }
static inline U16 ETypeId_getLibraryTypeId(ETypeId id) { return (U16)((id >> 9) & ((1 << 10) - 1)); }

#ifdef __cplusplus
	}
#endif

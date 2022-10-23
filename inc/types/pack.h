#pragma once
#include "types/types.h"
#include "math/quat.h"

//U64 packing

U64 U64_pack21x3(U32 x, U32 y, U32 z);
U32 U64_unpack21x3(U64 packed, U8 off);
Bool U64_setPacked21x3(U64 *packed, U8 off, U32 v);

Bool U64_pack20x3u4(U64 *dst, U32 x, U32 y, U32 z, U8 u4);
U32 U64_unpack20x3(U64 packed, U8 off);
Bool U64_setPacked20x3(U64 *packed, U8 off, U32 v);

Bool U64_getBit(U64 packed, U8 off);
Bool U64_setBit(U64 *packed, U8 off, Bool b);

//U32 packing

Bool U32_getBit(U32 packed, U8 off);
Bool U32_setBit(U32 *packed, U8 off, Bool b);

//Compressing quaternions

struct Quat16 {
	I16 arr[4];
};

Quat Quat_unpack(struct Quat16 q);
struct Quat16 Quat_pack(Quat q);

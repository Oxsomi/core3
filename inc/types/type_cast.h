#pragma once
#include "math/math.h"
#include "error.h"

//Conversions

Error F32_fromBits(U64 v, F32 *res);

Error I8_fromUInt(U64 v, I8 *res);
Error I8_fromInt(I64 v, I8 *res);
Error I8_fromFloat(F32 v, I8 *res);

Error I16_fromUInt(U64 v, I16 *res);
Error I16_fromInt(I64 v, I16 *res);
Error I16_fromFloat(F32 v, I16 *res);

Error I32_fromUInt(U64 v, I32 *res);
Error I32_fromInt(I64 v, I32 *res);
Error I32_fromFloat(F32 v, I32 *res);

Error I64_fromUInt(U64 v, I64 *res);
Error I64_fromFloat(F32 v, I64 *res);

//Cast to uints

Error U8_fromUInt(U64 v, U8 *res);
Error U8_fromInt(I64 v, U8 *res);
Error U8_fromFloat(F32 v, U8 *res);

Error U16_fromUInt(U64 v, U16 *res);
Error U16_fromInt(I64 v, U16 *res);
Error U16_fromFloat(F32 v, U16 *res);

Error U32_fromUInt(U64 v, U32 *res);
Error U32_fromInt(I64 v, U32 *res);
Error U32_fromFloat(F32 v, U32 *res);

Error U64_fromInt(I64 v, U64 *res);
Error U64_fromFloat(F32 v, U64 *res);

//Cast to floats

Error F32_fromInt(I64 v, F32 *res);
Error F32_fromUInt(U64 v, F32 *res);

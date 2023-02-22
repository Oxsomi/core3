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

#include "types/type_cast.h"

//Conversions

Error F32_fromBits(U64 v, F32 *res) {

	if(!res)
		return Error_nullPointer(1);

	if(v > U32_MAX)
		return Error_overflow(0, v, U32_MAX);

	U32 bits = (U32) v;
	F32 r = *(const F32*) &bits;

	if(!F32_isValid(r))
		return Error_NaN(0);

	if(v == (1 << 31))	//Signed zero
		return Error_invalidParameter(0, 0);

	*res = r;
	return Error_none();
}

//Cast to ints

#define _CastFromU(type, toBits, ...) {											\
																				\
	if(!res)																	\
		return Error_nullPointer(1);											\
																				\
	__VA_ARGS__																	\
																				\
	if(v > type##_MAX)															\
		return Error_overflow(0, toBits, (U64) type##_MAX);						\
																				\
	*res = (type) v;															\
	return Error_none();														\
}

#define _CastFromI(type, castType, toBits, ...) _CastFromU(type, toBits, __VA_ARGS__ 		\
	if(v < type##_MIN)																		\
		return Error_underflow(0, toBits, (castType) type##_MIN);							\
)

#define _CastFromF(type) _CastFromI(type, type, *(const U32*)&v)

Error I8_fromUInt(U64 v, I8 *res)		_CastFromU(I8, (U64)v)
Error I8_fromInt(I64 v, I8 *res)		_CastFromI(I8, U8, (U64)v)
Error I8_fromFloat(F32 v, I8 *res)		_CastFromF(I8) 

Error I16_fromUInt(U64 v, I16 *res)		_CastFromU(I16, (U64)v)
Error I16_fromInt(I64 v, I16 *res)		_CastFromI(I16, U16, (U64)v)
Error I16_fromFloat(F32 v, I16 *res)	_CastFromF(I16) 

Error I32_fromUInt(U64 v, I32 *res)		_CastFromU(I32, (U64)v)
Error I32_fromInt(I64 v, I32 *res) 		_CastFromI(I32, U32, (U64)v)
Error I32_fromFloat(F32 v, I32 *res)	_CastFromF(I32)

Error I64_fromUInt(U64 v, I64 *res) {

	if(!res)
		return Error_nullPointer(1);

	if(v > (U64) I64_MAX)
		return Error_overflow(0, (U64)v, (U64) I64_MAX);

	return Error_none();
}

Error I64_fromFloat(F32 v, I64 *res)	_CastFromF(I64)

//Cast to uints

Error U8_fromUInt(U64 v, U8 *res)		_CastFromU(U8, (U64)v)
Error U8_fromInt(I64 v, U8 *res)		_CastFromI(U8, U8, (U64)(I64)v)
Error U8_fromFloat(F32 v, U8 *res)		_CastFromF(U8)

Error U16_fromUInt(U64 v, U16 *res)		_CastFromU(U16, (U64)v)
Error U16_fromInt(I64 v, U16 *res) 		_CastFromI(U16, U16, (U64)(I64)v)
Error U16_fromFloat(F32 v, U16 *res)	_CastFromF(U16)

Error U32_fromUInt(U64 v, U32 *res)		_CastFromU(U32, (U64)v)
Error U32_fromInt(I64 v, U32 *res)		_CastFromI(U32, U32, (U64)(I64)v)
Error U32_fromFloat(F32 v, U32 *res)	_CastFromF(U32)

Error U64_fromInt(I64 v, U64 *res) {

	if(!res)
		return Error_nullPointer(1);

	if(v < (I64) U64_MAX)
		return Error_underflow(0, (U64)v, 0);

	return Error_none();
}


Error U64_fromFloat(F32 v, U64 *res)	_CastFromF(U64)

//Cast to floats

#define CastToF32 {															\
																			\
	if(!res)																\
		return Error_nullPointer(1);										\
																			\
	F32 r = (F32) v;														\
																			\
	if(!F32_isValid(r))														\
		return Error_NaN(0);												\
																			\
	if(r != v)																\
		return Error_notFound(0, 0);										\
																			\
	*res = r;																\
	return Error_none();													\
}

Error F32_fromInt(I64 v, F32 *res)  CastToF32
Error F32_fromUInt(U64 v, F32 *res) CastToF32

//Endianness, because sometimes it's needed

U16 U16_swapEndianness(U16 v) { return (v >> 8) | (v << 8); }
U32 U32_swapEndianness(U32 v) { return ((U32)U16_swapEndianness((U16)v) << 16) | U16_swapEndianness((U16)(v >> 16)); }
U64 U64_swapEndianness(U64 v) { return ((U64)U32_swapEndianness((U32)v) << 32) | U32_swapEndianness((U32)(v >> 32)); }

I16 I16_swapEndianness(I16 v) { return (I16) U16_swapEndianness((U16)v); }
I32 I32_swapEndianness(I32 v) { return (I32) U32_swapEndianness((U32)v); }
I64 I64_swapEndianness(I64 v) { return (I64) U64_swapEndianness((U64)v); }

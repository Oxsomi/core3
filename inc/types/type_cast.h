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

I16 I16_swapEndianness(I16 v);
I32 I32_swapEndianness(I32 v);
I64 I64_swapEndianness(I64 v);
U16 U16_swapEndianness(U16 v);
U32 U32_swapEndianness(U32 v);
U64 U64_swapEndianness(U64 v);

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

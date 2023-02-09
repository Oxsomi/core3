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

typedef struct Quat16 {
	I16 arr[4];
} Quat16;

Quat Quat_unpack(Quat16 q);
Quat16 Quat_pack(Quat q);

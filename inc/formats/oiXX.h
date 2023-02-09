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
#include "types/string.h"

typedef enum EXXCompressionType {
	EXXCompressionType_None,							//--uncompressed
	EXXCompressionType_Brotli11,						//(default)
	EXXCompressionType_Brotli1,							//--fast-compress
	EXXCompressionType_Count
} EXXCompressionType;

typedef enum EXXEncryptionType {
	EXXEncryptionType_None,								//(default)
	EXXEncryptionType_AES256GCM,						//--aes <32-byte key (in hex or nyto)>
	EXXEncryptionType_Count
} EXXEncryptionType;

typedef enum EXXDataSizeType {		//Can be represented as a 2-bit array for example
	EXXDataSizeType_U8,
	EXXDataSizeType_U16,
	EXXDataSizeType_U32,
	EXXDataSizeType_U64
} EXXDataSizeType;

static const U8 SIZE_BYTE_TYPE[4] = { 1, 2, 4, 8 };

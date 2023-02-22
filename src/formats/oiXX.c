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

#include "types/error.h"
#include "types/buffer.h"
#include "formats/oiXX.h"

Error Buffer_consumeSizeType(Buffer *buf, EXXDataSizeType type, U64 *result) {

	if(!buf || !result)
		return Error_nullPointer(!buf ? 0 : 2);

	*result = 0;		//This is ok, as little endian a U8 would be stored in the first bytes not the last

	switch (type) {
		case EXXDataSizeType_U8:		return Buffer_consume(buf, result, 1);
		case EXXDataSizeType_U16:		return Buffer_consume(buf, result, 2);
		case EXXDataSizeType_U32:		return Buffer_consume(buf, result, 4);
		case EXXDataSizeType_U64:		return Buffer_consume(buf, result, 8);
		default:
			return Error_invalidEnum(1, (U64)type, (U64)EXXDataSizeType_U64);
	}
}

U64 Buffer_forceReadSizeType(const U8 *ptr, EXXDataSizeType type) {

	if(!ptr)
		return 0;

	switch (type) {
		case EXXDataSizeType_U8:		return *ptr;
		case EXXDataSizeType_U16:		return *(const U16*)ptr;
		case EXXDataSizeType_U32:		return *(const U32*)ptr;
		case EXXDataSizeType_U64:		return *(const U64*)ptr;
		default:						return 0;
	}
}

U64 Buffer_forceWriteSizeType(U8 *ptr, EXXDataSizeType type, U64 result) {

	if(!ptr)
		return 0;

	switch (type) {
		case EXXDataSizeType_U8:		*ptr = (U8) result;				return sizeof(U8);
		case EXXDataSizeType_U16:		*(U16*)ptr = (U16) result;		return sizeof(U16);
		case EXXDataSizeType_U32:		*(U32*)ptr = (U32) result;		return sizeof(U32);
		case EXXDataSizeType_U64:		*(U64*)ptr = result;			return sizeof(U64);
		default:						return 0;
	}
}

EXXDataSizeType EXXDataSizeType_getRequiredType(U64 v) {
	return v <= U8_MAX ? EXXDataSizeType_U8 : (v <= U16_MAX ? EXXDataSizeType_U16 : (
		v <= U32_MAX ? EXXDataSizeType_U32 : EXXDataSizeType_U64
	));
}

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

#include "types/type_id.h"

EDataTypePrimitive EDataType_getPrimitive(EDataType type) { return (EDataTypePrimitive)(type >> 1); }
Bool EDataType_isSigned(EDataType type) { return type & EDataType_IsSigned; }

EDataType ETypeId_getDataType(ETypeId id) { return (EDataType)(id & 0xF); }
U8 ETypeId_getDataTypeBytes(ETypeId id) { return ((id >> 4) & 7) + 1; }
U8 ETypeId_getElements(ETypeId id) { return ((id >> 7) & 63) + 1; }

Bool ETypeId_hasValidSize(ETypeId id) { return ETypeId_getDataTypeBytes(id) != _TYPESIZE_UNDEF; }

U64 ETypeId_getBytes(ETypeId id) { 
	return ETypeId_hasValidSize(id) ? (U64)ETypeId_getDataTypeBytes(id) * ETypeId_getElements(id) : U64_MAX;
}

U8 ETypeId_getLibrarySubId(ETypeId id) { return (U8)((id >> 22) & 3); }
U8 ETypeId_getLibraryId(ETypeId id) { return (U8)((id >> 24) & 0xF); }

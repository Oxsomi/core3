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
#include "types/types.h"

typedef struct Error Error;
typedef struct List List;

Error List_createx(U64 length, U64 stride, List *result);
Error List_createNullBytesx(U64 length, U64 stride, List *result);
Error List_createRepeatedx(U64 length, U64 stride, Buffer data, List *result);
Error List_createCopyx(List list, List *result);

Error List_createSubsetReversex(List list, U64 index, U64 length, List *result);
Error List_createReversex(List list, List *result);

Error List_findx(List list, Buffer buf, List *result);

Error List_eraseAllx(List *list, Buffer buf);
Error List_insertx(List *list, U64 index, Buffer buf);
Error List_pushAllx(List *list, List other);
Error List_insertAllx(List *list, List other, U64 offset);

Error List_reservex(List *list, U64 capacity);
Error List_resizex(List *list, U64 size);
Error List_shrinkToFitx(List *list);

Error List_pushBackx(List *list, Buffer buf);

Bool List_freex(List *result);

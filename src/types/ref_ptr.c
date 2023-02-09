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

#include "types/ref_ptr.h"

RefPtr RefPtr_create(void *ptr, Allocator alloc, ObjectFreeFunc free) {
	return (RefPtr) {
		.refCount = 1,
		.ptr = ptr,
		.alloc = alloc,
		.free = free
	};
}

Bool RefPtr_add(RefPtr *ptr) {

	if(!ptr || !ptr->refCount)
		return false;

	++ptr->refCount;
	return true;
}

Bool RefPtr_sub(RefPtr *ptr) {

	if(!ptr || !ptr->refCount)
		return false;

	Bool b = true;

	if(!(--ptr->refCount)) {
		b = ptr->free(ptr->ptr, ptr->alloc);
		*ptr = (RefPtr) { 0 };
	}

	return b;
}

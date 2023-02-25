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

#include "types/buffer.h"
#include "types/time.h"

#ifdef _WIN32

	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
	#include <bcrypt.h>

	Bool Buffer_csprng(Buffer target) {

		if(!Buffer_length(target) || Buffer_isConstRef(target))
			return false;

		NTSTATUS stat = BCryptGenRandom(
			0, 
			(U8*)target.ptr, 
			(ULONG) Buffer_length(target),
			BCRYPT_USE_SYSTEM_PREFERRED_RNG
		);

		return BCRYPT_SUCCESS(stat);
	}

#else
	#error Other platform random number generation not supported yet.
#endif

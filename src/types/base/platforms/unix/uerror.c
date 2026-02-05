/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

#include "types/base/platform_types.h"
#include "types/base/error.h"

#if _PLATFORM_TYPE != PLATFORM_ANDROID

    #include <execinfo.h>

    void Error_captureStackTrace(void **stack, U8 stackSize, U8 skipTmp) {

        if(!stack || !stackSize)
            return;

        void *tmpStack[128];
        I32 count = backtrace(tmpStack, 128);

        if(count <= 0 || ((U64)stackSize + skipTmp + 1) > 128) {
            stack[0] = NULL;
            return;
        }

        U64 j = (U64)skipTmp + 1;

        for(U64 i = j; i < 128 && i < (U32) count && i < j + stackSize; ++i)
            stack[i - j] = tmpStack[i];

        if(count - j < stackSize)
            stack[count - j] = NULL;
    }

#endif

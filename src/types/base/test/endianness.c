/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/base/endianness.h"
#include "shared.h"

void Test_endianness(Test *test) {

	Test_setModule(test, "Endianness");

	U16 be16 = U16_swapEndianness(0x1234);
	U32 be32 = U32_swapEndianness(0x12345678);
	U64 be64 = U64_swapEndianness(0x123456789ABCDEF0);

	Test_assert(test, "U16_swapEndianness", NULL, be16 == 0x3412);
	Test_assert(test, "U32_swapEndianness", NULL, be32 == 0x78563412);
	Test_assert(test, "U64_swapEndianness", NULL, be64 == 0xF0DEBC9A78563412);
}

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

#include "test_bmp_shared.h"
#include "types/container/test/basic_alloc.h"

int main() {

	const Allocator alloc = BasicAllocator_instance;
	Test t = (Test){ 0 };
	t.alloc = &alloc;

	Test_BMPRoundTripBGRA8(&t);
	Test_BMPRoundTripBGR8(&t);
	Test_BMPRoundTripFlipped(&t);
	Test_BMPRoundTripTopDown(&t);
	Test_BMPRoundTripStridePadding(&t);
	Test_BMPRoundTripPixelDensity(&t);
	Test_BMPRoundTripHeaderFields(&t);

	Test_BMPPixelContentFlipped(&t);
	Test_BMPPixelContentNoFlip(&t);

	Test_BMPWriteSizeConsistency(&t);
	Test_BMPWriteSizeConsistencyBGR8(&t);

	Test_BMPWriteZeroDimensions(&t);
	Test_BMPWriteNegativePixelsPerMetre(&t);
	Test_BMPWriteWrongFormat(&t);

	Test_BMPReadInvalidMagic(&t);
	Test_BMPReadZeroWidth(&t);
	Test_BMPReadZeroHeight(&t);
	Test_BMPReadUnsupportedBitCount(&t);
	Test_BMPReadUnsupportedCompression(&t);
	Test_BMPReadUnsupportedCompressionRLE4(&t);

	BasicAllocator_checkLeakedMem(&t);
	return Test_end(&t);
}

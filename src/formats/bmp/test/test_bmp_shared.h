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

#pragma once
#include "types/test/test.h"

void Test_BMPRoundTripBGRA8(Test *t);
void Test_BMPRoundTripBGR8(Test *t);
void Test_BMPRoundTripFlipped(Test *t);
void Test_BMPRoundTripTopDown(Test *t);
void Test_BMPRoundTripStridePadding(Test *t);
void Test_BMPRoundTripPixelDensity(Test *t);
void Test_BMPRoundTripHeaderFields(Test *t);

void Test_BMPPixelContentFlipped(Test *t);
void Test_BMPPixelContentNoFlip(Test *t);

void Test_BMPWriteZeroDimensions(Test *t);
void Test_BMPWriteNegativePixelsPerMetre(Test *t);
void Test_BMPWriteWrongFormat(Test *t);
void Test_BMPReadInvalidMagic(Test *t);
void Test_BMPReadZeroWidth(Test *t);
void Test_BMPReadZeroHeight(Test *t);
void Test_BMPReadUnsupportedBitCount(Test *t);
void Test_BMPReadUnsupportedCompression(Test *t);
void Test_BMPReadUnsupportedCompressionRLE4(Test *t);

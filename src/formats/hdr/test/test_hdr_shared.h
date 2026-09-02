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

//formats/hdr/test/test_hdr_shared.h

#pragma once
#include "types/test/test.h"

void Test_HDRRoundTripBasic(Test *t);
void Test_HDRRoundTripKeepRGBEMatchesDecoded(Test *t);
void Test_HDRRoundTripSourceIsRGBE(Test *t);
void Test_HDRRoundTripFlatScanline(Test *t);
void Test_HDRRoundTripWideScanline(Test *t);
void Test_HDRRoundTripRunLength(Test *t);
void Test_HDRRoundTripExactZero(Test *t);
void Test_HDRRoundTripDynamicRange(Test *t);

void Test_HDRWriteZeroDimensions(Test *t);
void Test_HDRWriteOversizedDimensions(Test *t);
void Test_HDRWriteShortInput(Test *t);

void Test_HDRReadInvalidMagic(Test *t);
void Test_HDRReadMissingFormat(Test *t);
void Test_HDRReadUnsupportedFormat(Test *t);
void Test_HDRReadZeroRepeat(Test *t);
void Test_HDRReadUnwritableSink(Test *t);

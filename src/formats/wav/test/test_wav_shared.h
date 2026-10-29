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

//formats/wav/test/test_wav_shared.h

#pragma once
#include "types/test/test.h"

void Test_WAVReadAdditionalChunksAndRates(Test *t);
void Test_WAVWriteAdditionalRates(Test *t);

void Test_WAVRoundTripStereo16(Test *t);
void Test_WAVRoundTripMonoF32(Test *t);
void Test_WAVRoundTripMono8(Test *t);
void Test_WAVRoundTripMono64(Test *t);

void Test_WAVWriteInvalidFreq(Test *t);
void Test_WAVWriteInvalidStride(Test *t);
void Test_WAVWriteUnalignedLength(Test *t);

void Test_WAVReadInvalidMagic(Test *t);
void Test_WAVReadTruncated(Test *t);

void Test_WAVConvertStereoToMono(Test *t);
void Test_WAVConvertStereoLeftOnly(Test *t);
void Test_WAVConvertMisalignedSrc(Test *t);

void Test_WAVRoundTripMonoPCM32(Test *t);
void Test_WAVRoundTripStereoPCM32(Test *t);
void Test_WAVConvertPCM32ToF32(Test *t);
void Test_WAVConvertStereoPCM32ToMono(Test *t);

void Test_WAVReadExtended(Test *t);
void Test_WAVOddLengthPadding(Test *t);

void Test_WAVCvtTable(Test *t);
void Test_WAVCvtUnalignedIndex(Test *t);
void Test_WAVAvgTable(Test *t);
void Test_WAVConvertExpand(Test *t);
void Test_WAVConvertFloatToPCM(Test *t);
void Test_WAVConvertAverageExtremes(Test *t);
void Test_WAVConvertInvalidInfo(Test *t);

void Test_WAVReadOffsetAndTrailing(Test *t);
void Test_WAVReadChunkBounds(Test *t);
void Test_WAVReadFmtChunk(Test *t);
void Test_WAVWriteFailedWrites(Test *t);
void Test_WAVWriteLimits(Test *t);

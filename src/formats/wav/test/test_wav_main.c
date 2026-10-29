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

//formats/wav/test/test_wav_main.c

#include "types/test/test.h"
#include "test_wav_shared.h"
#include "types/container/test/basic_alloc.h"

OXC3_TEST_MAIN(formats_wav) {

	const Allocator alloc = BasicAllocator_instance;
	Test t = (Test){ 0 };
	t.alloc = &alloc;

	//WAVFile_cvt
	Test_WAVCvtTable(&t);
	Test_WAVCvtUnalignedIndex(&t);

	//WAVFile_avg
	Test_WAVAvgTable(&t);

	//WAV round-trips
	Test_WAVRoundTripStereo16(&t);
	Test_WAVRoundTripMonoF32(&t);
	Test_WAVRoundTripMono8(&t);
	Test_WAVRoundTripMono64(&t);
	Test_WAVRoundTripMonoPCM32(&t);
	Test_WAVRoundTripStereoPCM32(&t);

	//WAV_write error cases
	Test_WAVWriteInvalidFreq(&t);
	Test_WAVWriteInvalidStride(&t);
	Test_WAVWriteUnalignedLength(&t);

	//WAV_read error cases
	Test_WAVReadInvalidMagic(&t);
	Test_WAVReadTruncated(&t);

	//WAV_read extensible header
	Test_WAVReadExtended(&t);

	//WAV_read bounds and fmt chunk
	Test_WAVReadOffsetAndTrailing(&t);
	Test_WAVReadChunkBounds(&t);
	Test_WAVReadFmtChunk(&t);
	Test_WAVReadAdditionalChunksAndRates(&t);

	//WAV_write failures and limits
	Test_WAVWriteFailedWrites(&t);
	Test_WAVWriteLimits(&t);
	Test_WAVWriteAdditionalRates(&t);

	//WAV odd-length padding
	Test_WAVOddLengthPadding(&t);

	//WAVFile_convert
	Test_WAVConvertStereoToMono(&t);
	Test_WAVConvertStereoLeftOnly(&t);
	Test_WAVConvertMisalignedSrc(&t);
	Test_WAVConvertPCM32ToF32(&t);
	Test_WAVConvertStereoPCM32ToMono(&t);
	Test_WAVConvertExpand(&t);
	Test_WAVConvertFloatToPCM(&t);
	Test_WAVConvertAverageExtremes(&t);
	Test_WAVConvertInvalidInfo(&t);

	BasicAllocator_checkLeakedMem(&t);
	return Test_end(&t);
}

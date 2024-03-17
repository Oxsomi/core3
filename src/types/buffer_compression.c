/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/buffer.h"
#include "types/error.h"
#include "types/allocation_buffer.h"
#include "types/allocator.h"

/*

static const U8 BROTLI_MIN_QUALITY_FOR_BLOCK_SPLIT = 4;
static const U64 BROTLI_WINDOW_SIZE = 22;

//https://cran.r-project.org/web/packages/brotli/vignettes/brotli-2015-09-22.pdf
//Brotli with a 22 bit window size.

Error Brotli_compress(
	Buffer target,
	EBufferCompressionHint hint,
	U8 quality,
	Allocator allocator,
	Buffer *output
) {

	if(quality != 11)
		return Error_invalidParameter(2, 0, 0);

	Bool isUTF8 = Buffer_isUtf8(target, 0.75);

	//Setup constants

	U8 maxInputBlockBits = quality < BROTLI_MIN_QUALITY_FOR_BLOCK_SPLIT ? 14 : 16;

	if(quality >= 9 && BROTLI_WINDOW_SIZE > maxInputBlockBits)
		maxInputBlockBits = (U8) U64_min(BROTLI_WINDOW_SIZE, 21);

	U64 maxBackwardDistance = ((U64)1 << BROTLI_WINDOW_SIZE) - 16;

	//Allocate requirements

	Error err = Error_none();
	AllocationBuffer ringBuffer = (AllocationBuffer) { 0 };

	gotoIfError(
		clean,
		AllocationBuffer_create(
			(U64_max(BROTLI_WINDOW_SIZE, maxInputBlockBits) + 7) >> 3,
			allocator, &ringBuffer
		)
	);

	//TODO: Literal cost mask?



clean:

	AllocationBuffer_free(&ringBuffer, allocator);

	return err;
}

Error Brotli_decompress(
	Buffer target,
	U8 quality,
	Allocator allocator,
	Buffer *output
) {

	if(quality != 11)
		return Error_invalidParameter(2, 0, 0);

}

Error Buffer_compress(
	Buffer target,
	EBufferCompressionType type,
	EBufferCompressionHint hint,
	Allocator allocator,
	Buffer *output
) {

	if(!Buffer_length(target))
		return Error_nullPointer(0, 0);

	if(type >= EBufferCompressionType_Count)
		return Error_invalidEnum(1, 0, (U64)type, (U64)EBufferCompressionType_Count);

	if(hint >= EBufferCompressionHint_Count)
		return Error_invalidEnum(2, 0, (U64)hint, (U64)EBufferCompressionHint_Count);

	if(!output)
		return Error_nullPointer(4, 0);

	return Brotli_compress(target, hint, type == EBufferCompressionType_Brotli11 ? 11 : 1, allocator, output);
}

Error Buffer_decompress(
	Buffer target,
	EBufferCompressionType type,
	Allocator allocator,
	Buffer *output
) {

	if(!Buffer_length(target))
		return Error_nullPointer(0, 0);

	if(type >= EBufferCompressionType_Count)
		return Error_invalidEnum(1, 0, (U64)type, (U64)EBufferCompressionType_Count);

	if(!output)
		return Error_nullPointer(3, 0);

	return Brotli_decompress(target, type == EBufferCompressionType_Brotli11 ? 11 : 1, allocator, output);
}
*/

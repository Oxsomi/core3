/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/container/list_impl.h"
#include "formats/wav/wav.h"
#include "formats/wav/headers.h"
#include "types/math/math.h"
#include "platforms/file.h"

Bool WAV_write(
	Stream *stream,
	Stream *inputStream,
	U64 outputStreamOffset,
	U64 inputStreamOffset,
	U64 streamLength,
	Bool isStereo,
	U32 freq,
	U16 stride,
	U64 *dataOutput,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!stream || stream->isReadonly)
		retError(clean, Error_nullPointer(0, "WAV_write()::stream is required"))

	if(!dataOutput) {

		if(!inputStream || !inputStream->isReadonly)
			retError(clean, Error_nullPointer(0, "WAV_write()::inputStream is required"))

		if (inputStreamOffset >= inputStream->handle.fileSize)
			retError(clean, Error_outOfBounds(
				0, inputStreamOffset, inputStream->handle.fileSize, "WAV_write()::inputStreamOffset is out of bounds"
			))

		if(!streamLength)
			streamLength = inputStream->handle.fileSize - inputStreamOffset;

		if (inputStreamOffset + streamLength > inputStream->handle.fileSize)
			retError(clean, Error_outOfBounds(
				0, inputStreamOffset + streamLength, inputStream->handle.fileSize,
				"WAV_write()::inputStreamOffset + streamLength is out of bounds"
			))
	}

	else if(!streamLength)
		retError(clean, Error_invalidParameter(
			0, 0, "WAV_write() has to have a streamLength if input stream is missing"
		))

	switch (freq) {

		case   8000:
		case  11025:
		case  22050:
		case  32000:
		case  44100:
		case  48000:
		case  96000:
		case 192000:
			break;

		default:
			retError(clean, Error_invalidParameter(
				0, 0, "WAV_write() wav file has invalid frequency; must be one of "
				"8KHz, 11.025KHz, 22.05KHz, 32KHz, 44.1KHz, 48KHz, 96KHz, 192KHz"
			))
	}

	switch (stride) {
		case 8: case 16: case 24: case 32: case 64:
			break;
		default:
			retError(clean, Error_invalidParameter(
				0, 0, "WAV_write() wav file has invalid stride. Must be 8, 16, 24, 32 or 64"
			))
	}

	U16 bytesPerBlock = ((isStereo ? 2 : 1) * stride) >> 3;

	if(streamLength % bytesPerBlock)
		retError(clean, Error_invalidParameter(
			0, 0, "WAV_write() stream length didn't match bytes per block"
		))

	//RIFFHeader

	RIFFHeader header = (RIFFHeader) {
		.section = (RIFFSection) {
			.magicNumber = RIFFHeader_magic,
			.size = (U32) (sizeof(U32) + sizeof(RIFFFmtHeader) + sizeof(RIFFDataHeader) + streamLength)
		},
		.magicNumberFile = RIFFWAVHeader_magic
	};

	gotoIfError3(clean, Stream_write(
		stream,
		Buffer_createRefConst(&header, sizeof(header)),
		0,
		outputStreamOffset,
		0,
		false,
		e_rr
	))

	outputStreamOffset += sizeof(header);

	//RIFFFmtHeader

	RIFFFmtHeader fmtHeader = (RIFFFmtHeader) {

		.section = (RIFFSection) {
			.magicNumber = RIFFFmtHeader_magic,
			.size = (U32)(sizeof(RIFFFmtHeader) - sizeof(RIFFSection))
		},
	
		.format = stride > 24 ? ERIFFAudioFormat_IEEE754 : ERIFFAudioFormat_PCM,
		.channels = isStereo ? 2 : 1,

		.frequency = freq,

		.bytesPerSecond = bytesPerBlock * freq,
		.bytesPerBlock = bytesPerBlock,
		.stride = stride
	};

	gotoIfError3(clean, Stream_write(
		stream,
		Buffer_createRefConst(&fmtHeader, sizeof(fmtHeader)),
		0,
		outputStreamOffset,
		0,
		false,
		e_rr
	))

	outputStreamOffset += sizeof(fmtHeader);

	//RIFFDataHeader

	RIFFDataHeader dataHeader = (RIFFDataHeader) {
		.magicNumber = RIFFDataHeader_magic,
		.size = (U32) streamLength
	};

	gotoIfError3(clean, Stream_write(
		stream,
		Buffer_createRefConst(&dataHeader, sizeof(dataHeader)),
		0,
		outputStreamOffset,
		0,
		false,
		e_rr
	))

	outputStreamOffset += sizeof(dataHeader);

	//Data

	if(!dataOutput)
		gotoIfError3(clean, Stream_writeStream(
			stream,
			inputStream,
			inputStreamOffset,
			outputStreamOffset,
			streamLength,
			e_rr
		))

	else *dataOutput = outputStreamOffset;

clean:
	return s_uccess;
}

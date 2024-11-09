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
#include "platforms/file.h"
#include "platforms/platform.h"

Bool WAV_read(Stream *stream, U64 off, U64 len, Allocator allocator, WAVFile *result, Error *e_rr) {

	(void) allocator;

	Bool s_uccess = true;

	if(!stream || !result)
		retError(clean, Error_nullPointer(!stream ? 0 : 5, "WAV_read()::stream and result are required"))

	if(off >= stream->handle.fileSize)
		retError(clean, Error_outOfBounds(0, off, stream->handle.fileSize, "WAV_read()::stream out of bounds"))

	if(!len)
		len = stream->handle.fileSize - off;

	if(off + len > stream->handle.fileSize)
		retError(clean, Error_outOfBounds(1, off + len, stream->handle.fileSize, "WAV_read()::stream out of bounds"))
		
	RIFFHeader header;
	gotoIfError3(clean, Stream_read(stream, Buffer_createRef(&header, sizeof(header)), off, 0, 0, false, e_rr))
	off += sizeof(RIFFHeader);

	if(header.section.magicNumber != RIFFHeader_magic)
		retError(clean, Error_invalidParameter(0, 0, "WAV_read() invalid RIFF file"))

	if(header.section.size + 8 > len)
		retError(clean, Error_outOfBounds(1, header.section.size + 8, len, "WAV_read()::section out of bounds"))

	if(header.magicNumberFile != RIFFWAVHeader_magic)
		retError(clean, Error_invalidParameter(0, 0, "WAV_read() invalid wav file"))

	Bool hasFmt = false;
	Bool hasData = false;

	while (off < len) {

		RIFFSection section;
		gotoIfError3(clean, Stream_read(stream, Buffer_createRef(&section, sizeof(section)), off, 0, 0, false, e_rr))
		off += sizeof(section);

		if(section.size > len - off)
			retError(clean, Error_outOfBounds(1, section.size, len - off, "WAV_read()::section out of bounds"))

		switch(section.magicNumber) {

			case RIFFFmtHeader_magic: {

				if(hasFmt)
					retError(clean, Error_invalidParameter(0, 0, "WAV_read() wav file has more than one fmt section"))

				Buffer buf =  Buffer_createRef(&result->fmt, sizeof(result->fmt));
				gotoIfError3(clean, Stream_read(stream, buf, off - sizeof(section), 0, 0, false, e_rr))

				switch (result->fmt.frequency) {

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
							0, 0, "WAV_read() wav file has invalid frequency; must be one of "
							"8KHz, 11.025KHz, 22.05KHz, 32KHz, 44.1KHz, 48KHz, 96KHz, 192KHz"
						))
				}

				if(result->fmt.channels < 1 || result->fmt.channels > 2)
					retError(clean, Error_invalidState(0, "WAV_read() channel count unsupported"))
				
				if((U64)result->fmt.frequency * result->fmt.bytesPerBlock != result->fmt.bytesPerSecond)
					retError(clean, Error_invalidState(0, "WAV_read() bytesPerSecond is invalid"))
				
				if(((U64)result->fmt.channels * result->fmt.stride) >> 3 != result->fmt.bytesPerBlock)
					retError(clean, Error_invalidState(0, "WAV_read() bytesPerBlock is invalid"))

				if(
					result->fmt.stride != 8 && result->fmt.stride != 16 && result->fmt.stride != 24 &&		//Int
					result->fmt.stride != 32 && result->fmt.stride != 64									//Float ext
				)
					retError(clean, Error_invalidState(0, "WAV_read() bit count unsupported"))

				if(result->fmt.stride >= 32 && result->fmt.format != ERIFFAudioFormat_IEEE754)
					retError(clean, Error_invalidState(
						0, "WAV_read() format unsupported for 32 or 64 bit depth"
					))

				if(result->fmt.stride <= 24 && result->fmt.format != ERIFFAudioFormat_PCM)
					retError(clean, Error_invalidState(
						0, "WAV_read() format unsupported for 8 or 16 bit depth"
					))

				hasFmt = true;
				break;
			}

			case RIFFDataHeader_magic:

				if(hasData)
					retError(clean, Error_invalidParameter(0, 0, "WAV_read() wav file has more than one fmt section"))

				result->dataLength = section.size;
				result->dataStart = off;
				hasData = true;
				break;

			//Unsupported, but just handle it quietly

			case RIFFInfoHeader_magic:
			case RIFFCsetHeader_magic:
			case RIFFPlstHeader_magic:
			case RIFFCueHeader_magic:
			case RIFFFactHeader_magic:
			case RIFFCDifHeader_magic:
			case RIFFListHeader_magic:
			case RIFFId3Header_magic:
			case RIFFBextHeader_magic:
			case RIFFIxmlHeader_magic:
			case RIFFXMPHeader_magic:
			case RIFFCartHeader_magic:
			case RIFFSmplHeader_magic:

			case RIFFFllrHeader_magic:
			case RIFFJunkHeader_magic:
			case RIFFjunkHeader_magic:
			case RIFFPadHeader_magic:
				break;

			default:
				retError(clean, Error_invalidParameter(0, 0, "WAV_read() invalid riff section"))
		}

		off += section.size;
	}

	if(!hasData || !hasFmt)
		retError(clean, Error_invalidParameter(0, 0, "WAV_read() wav file has no fmt or data section"))

	if(result->dataLength % result->fmt.bytesPerBlock)
		retError(clean, Error_invalidParameter(
			0, 0, "WAV_write() dataLength didn't match bytes per block"
		))

clean:
	return s_uccess;
}

Bool WAV_readx(Stream *stream, U64 off, U64 len, WAVFile *result, Error *e_rr) {
	return WAV_read(stream, off, len, Platform_instance->alloc, result, e_rr);
}

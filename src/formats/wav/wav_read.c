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

//formats/wav/wav_read.c

#include "types/container/stream.h"
#include "formats/wav/wav_file.h"
#include "formats/wav/wav_headers.h"

Bool WAV_read(StreamRef *streamRef, U64 off, U64 len, const Allocator *alloc, WAVFile *result, Error *e_rr) {

	Bool s_uccess = true;
	StreamCursor cursorRead = (StreamCursor) { 0 };

	if (!result)
		retError(clean, Error_nullPointer(5, "WAV_read()::result is required"));

	gotoIfError3(clean, StreamCursor_create(streamRef, 0, false, alloc, &cursorRead, e_rr));

	OxStream *stream = RefPtr_data(streamRef, OxStream);

	if (off >= stream->size)
		retError(clean, Error_outOfBounds(1, off, stream->size, "WAV_read()::stream out of bounds"));

	if(!len)
		len = stream->size - off;

	if(off + len > stream->size)
		retError(clean, Error_outOfBounds(1, off + len, stream->size, "WAV_read()::stream out of bounds"));
		
	RIFFHeader header;
	gotoIfError3(clean, StreamCursor_read(
		&cursorRead, Buffer_createRef(&header, sizeof(header)), off, 0, 0, false, alloc, e_rr
	));

	off += sizeof(RIFFHeader);

	if(header.section.magicNumber != RIFFHeader_magic)
		retError(clean, Error_invalidParameter(0, 0, "WAV_read() invalid RIFF file"));

	if(header.section.size + 8 > len)
		retError(clean, Error_outOfBounds(1, header.section.size + 8, len, "WAV_read()::section out of bounds"));

	if(header.magicNumberFile != RIFFWAVHeader_magic)
		retError(clean, Error_invalidParameter(0, 0, "WAV_read() invalid wav file"));

	Bool hasFmt = false;
	Bool hasData = false;

	while (off < len) {

		RIFFSection section;
		gotoIfError3(clean, StreamCursor_read(
			&cursorRead, Buffer_createRef(&section, sizeof(section)), off, 0, 0, false, alloc, e_rr
		));

		off += sizeof(section);

		if(section.size > len - off)
			retError(clean, Error_outOfBounds(1, section.size, len - off, "WAV_read()::section out of bounds"));

		switch(section.magicNumber) {

			case RIFFFmtHeader_magic: {

				if(hasFmt)
					retError(clean, Error_invalidParameter(0, 0, "WAV_read() wav file has more than one fmt section"));

				Buffer buf =  Buffer_createRef(&result->fmt, sizeof(result->fmt));
				gotoIfError3(clean, StreamCursor_read(&cursorRead, buf, off - sizeof(section), 0, 0, false, alloc, e_rr));

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
						));
				}

				if(result->fmt.channels < 1 || result->fmt.channels > 2)
					retError(clean, Error_invalidState(0, "WAV_read() channel count unsupported"));
				
				if((U64)result->fmt.frequency * result->fmt.bytesPerBlock != result->fmt.bytesPerSecond)
					retError(clean, Error_invalidState(0, "WAV_read() bytesPerSecond is invalid"));
				
				if(((U64)result->fmt.channels * result->fmt.stride) >> 3 != result->fmt.bytesPerBlock)
					retError(clean, Error_invalidState(0, "WAV_read() bytesPerBlock is invalid"));

				if(
					result->fmt.stride != 8 && result->fmt.stride != 16 && result->fmt.stride != 24 &&        //Int
					result->fmt.stride != 32 && result->fmt.stride != 64                                    //Float ext
				)
					retError(clean, Error_invalidState(0, "WAV_read() bit count unsupported"));

				if (result->fmt.format == 0xFFFE) {        //Extended
					
					if(result->fmt.section.size < sizeof(RIFFFmtHeader) + sizeof(RIFFFmtExtended))
						retError(clean, Error_invalidState(0, "WAV_read() is of unexpected size"));

					RIFFFmtExtended ext;
					buf = Buffer_createRef(&ext, sizeof(ext));
					U64 extOff = off - sizeof(section) + sizeof(RIFFFmtHeader);
					gotoIfError3(clean, StreamCursor_read(&cursorRead, buf, extOff, 0, 0, false, alloc, e_rr));

					if (ext.cbSize != 22)
						retError(clean, Error_invalidState(0, "WAV_read() ext is malformed"));

					if (ext.bitsPerSample > result->fmt.stride)
						retError(clean, Error_invalidState(0, "WAV_read() bitsPerSample is larger than stride"));

					if (
						ext.bitsPerSample != 8 &&
						ext.bitsPerSample != 16 &&
						ext.bitsPerSample != 24 &&
						ext.bitsPerSample != 32 &&
						ext.bitsPerSample != 64
					)
						retError(clean, Error_invalidParameter(0, 0, "WAV_read() unsupported ext bits per sample"));

					static const U8 expectedGuidSuffix[12] = {
						0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71
					};

					U8 realSuffix[12];        //U32 is in the header because it's easier that way, this is the rest
					buf = Buffer_createRef(realSuffix, sizeof(realSuffix));
					gotoIfError3(clean, StreamCursor_read(
						&cursorRead, buf, extOff + sizeof(ext), 0, 0, false, alloc, e_rr
					));

					Buffer cmp = Buffer_createRefConst(expectedGuidSuffix, sizeof(expectedGuidSuffix));

					if(Buffer_neq(buf, cmp))
						retError(clean, Error_invalidParameter(0, 0, "WAV_read() unsupported GUID"));

					switch (ext.guid0) {

						case 1:  //KSDATAFORMAT_SUBTYPE_PCM
							result->fmt.format = ERIFFAudioFormat_PCM;
							break;

						case 3:  //KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
							result->fmt.format = ERIFFAudioFormat_IEEE754;
							break;

						default:
							retError(clean, Error_invalidParameter(
								0, 0, "WAV_read() unsupported WAVE_FORMAT_EXTENSIBLE subformat"
							));
					}
				}

				if(result->fmt.stride > 32 && result->fmt.format != ERIFFAudioFormat_IEEE754)
					retError(clean, Error_invalidState(
						0, "WAV_read() PCM format unsupported for 64 bit depth"
					));

				if(result->fmt.stride <= 24 && result->fmt.format != ERIFFAudioFormat_PCM)
					retError(clean, Error_invalidState(
						0, "WAV_read() format unsupported for 8 or 16 bit depth"
					));

				hasFmt = true;
				break;
			}

			case RIFFDataHeader_magic:

				if(hasData)
					retError(clean, Error_invalidParameter(0, 0, "WAV_read() wav file has more than one fmt section"));

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
				retError(clean, Error_invalidParameter(0, 0, "WAV_read() invalid riff section"));
		}

		off += (section.size + 1) & ~1;
	}

	if(!hasData || !hasFmt)
		retError(clean, Error_invalidParameter(0, 0, "WAV_read() wav file has no fmt or data section"));

	if(result->dataLength % result->fmt.bytesPerBlock)
		retError(clean, Error_invalidParameter(
			0, 0, "WAV_read() dataLength didn't match bytes per block"
		));

clean:
	StreamCursor_close(&cursorRead, alloc);
	return s_uccess;
}

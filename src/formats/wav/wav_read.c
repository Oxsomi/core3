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

	if(len > stream->size - off)
		retError(clean, Error_outOfBounds(1, off + len, stream->size, "WAV_read()::stream out of bounds"));

	//The file is [start, start + len); every offset below is absolute in the stream, so every bound is too

	U64 end = off + len;

	if(len < sizeof(RIFFHeader))
		retError(clean, Error_outOfBounds(1, len, sizeof(RIFFHeader), "WAV_read()::stream is too small for a RIFF header"));

	RIFFHeader header;
	gotoIfError3(clean, StreamCursor_read(
		&cursorRead, Buffer_createRef(&header, sizeof(header)), off, 0, 0, false, alloc, e_rr
	));

	if(header.section.magicNumber != RIFFHeader_magic)
		retError(clean, Error_invalidParameter(0, 0, "WAV_read() invalid RIFF file"));

	if((U64) header.section.size + 8 > len)
		retError(clean, Error_outOfBounds(1, (U64) header.section.size + 8, len, "WAV_read()::section out of bounds"));

	if(header.magicNumberFile != RIFFWAVHeader_magic)
		retError(clean, Error_invalidParameter(0, 0, "WAV_read() invalid wav file"));

	//Bytes after the RIFF chunk aren't part of the file

	end = off + (U64) header.section.size + 8;
	off += sizeof(RIFFHeader);

	Bool hasFmt = false;
	Bool hasData = false;

	while (off < end) {

		if(end - off < sizeof(RIFFSection))
			retError(clean, Error_outOfBounds(1, end - off, sizeof(RIFFSection), "WAV_read()::section header out of bounds"));

		RIFFSection section;
		gotoIfError3(clean, StreamCursor_read(
			&cursorRead, Buffer_createRef(&section, sizeof(section)), off, 0, 0, false, alloc, e_rr
		));

		off += sizeof(section);

		if(section.size > end - off)
			retError(clean, Error_outOfBounds(1, section.size, end - off, "WAV_read()::section out of bounds"));

		switch(section.magicNumber) {

			case RIFFFmtHeader_magic: {

				if(hasFmt)
					retError(clean, Error_invalidParameter(0, 0, "WAV_read() wav file has more than one fmt section"));

				//The chunk can be bigger than the header (cbSize, extensible); the rest is skipped by the chunk size

				if(section.size < sizeof(RIFFFmtHeader) - sizeof(RIFFSection))
					retError(clean, Error_invalidState(0, "WAV_read()::fmt section is too small"));

				Buffer buf =  Buffer_createRef(&result->fmt, sizeof(result->fmt));
				gotoIfError3(clean, StreamCursor_read(&cursorRead, buf, off - sizeof(section), 0, 0, false, alloc, e_rr));

				if(!result->fmt.frequency)
					retError(clean, Error_invalidParameter(0, 0, "WAV_read() frequency must be positive"));

				if(result->fmt.channels < 1 || result->fmt.channels > 2)
					retError(clean, Error_invalidState(0, "WAV_read() channel count unsupported"));
				
				if((U64) result->fmt.frequency * result->fmt.bytesPerBlock != result->fmt.bytesPerSecond)
					retError(clean, Error_invalidState(0, "WAV_read() bytesPerSecond is invalid"));
				
				if(((U64) result->fmt.channels * result->fmt.stride) >> 3 != result->fmt.bytesPerBlock)
					retError(clean, Error_invalidState(0, "WAV_read() bytesPerBlock is invalid"));

				if (
					result->fmt.stride != 8 && result->fmt.stride != 16 && result->fmt.stride != 24 &&        //Int
					result->fmt.stride != 32 && result->fmt.stride != 64                                      //Float ext
				)
					retError(clean, Error_invalidState(0, "WAV_read() bit count unsupported"));

				if (result->fmt.format == 0xFFFE) {        //Extended
					
					//WAVEFORMATEX body (16) + cbSize and the 22 bytes it counts
					if(section.size < sizeof(RIFFFmtHeader) - sizeof(RIFFSection) + 2 + 22)
						retError(clean, Error_invalidState(0, "WAV_read() is of unexpected size"));

					RIFFFmtExtended ext;
					buf = Buffer_createRef(&ext, sizeof(ext));
					U64 extOff = off - sizeof(section) + sizeof(RIFFFmtHeader);
					gotoIfError3(clean, StreamCursor_read(&cursorRead, buf, extOff, 0, 0, false, alloc, e_rr));

					if (ext.cbSize != 22)
						retError(clean, Error_invalidState(0, "WAV_read() ext is malformed"));

					//The container (stride) decides how samples are read. Valid bits below the stride say the sample is
					// MSB justified in it, which reads correctly as a full stride sample, so it is accepted as such.

					if (ext.bitsPerSample == 0 || ext.bitsPerSample > result->fmt.stride)
						retError(clean, Error_invalidState(0, "WAV_read() bitsPerSample must be in [1, stride]"));

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

				if(result->fmt.format != ERIFFAudioFormat_PCM && result->fmt.format != ERIFFAudioFormat_IEEE754)
					retError(clean, Error_invalidState(0, "WAV_read() format must be PCM or IEEE float"));

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
					retError(clean, Error_invalidParameter(0, 0, "WAV_read() wav file has more than one data section"));

				result->dataLength = section.size;
				result->dataStart = off;
				hasData = true;
				break;

			//RIFF readers skip unknown chunks, including their alignment byte.

			default:
				break;
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

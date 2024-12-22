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

#pragma once
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

//WAV/RIFF headers
//Combo between
// https://docs.fileformat.com/audio/wav/
// https://en.wikipedia.org/wiki/WAV
// https://exiftool.org/TagNames/RIFF.html

typedef struct RIFFSection {
	U32 magicNumber;					//E.g. "RIFF" (RIFFHeader_magic)
	U32 size;							//Size excluding RIFFSection
} RIFFSection;

typedef struct RIFFHeader {
	RIFFSection section;				//"RIFF"
	U32 magicNumberFile;				//E.g. "WAVE" (RIFFWAVHeader_magic)
} RIFFHeader;

typedef enum ERIFFAudioFormat {
	ERIFFAudioFormat_PCM		= 1,	//Ints (U8, U16)
	ERIFFAudioFormat_IEEE754	= 3		//Floats (F32, F64)
} ERIFFAudioFormat;

typedef U16 RIFFAudioFormatType;		//ERIFFAudioFormat

typedef struct RIFFFmtHeader {

	RIFFSection section;				//"fmt " (RIFFFmtHeader_magic)

	RIFFAudioFormatType format;
	U16 channels;

	U32 frequency;						//Hz (/ second)

	U32 bytesPerSecond;					//frequency * bytesPerBlock

	U16 bytesPerBlock;					//(channels * bitsPerSample) >> 3
	U16 stride;							//8, 16, 24, 32 or 64

} RIFFFmtHeader;

typedef RIFFSection RIFFDataHeader;		//"data" (RIFFDataHeader_magic)

#define RIFFHeader_magic     C8x4('R', 'I', 'F', 'F')
#define RIFFWAVHeader_magic  C8x4('W', 'A', 'V', 'E')
#define RIFFFmtHeader_magic  C8x4('f', 'm', 't', ' ')
#define RIFFDataHeader_magic C8x4('d', 'a', 't', 'a')

#define RIFFInfoHeader_magic C8x4('I', 'N', 'F', 'O')
#define RIFFCsetHeader_magic C8x4('C', 'S', 'E', 'T')
#define RIFFPlstHeader_magic C8x4('p', 'l', 's', 't')
#define RIFFCueHeader_magic  C8x4('c', 'u', 'e', ' ')
#define RIFFFactHeader_magic C8x4('f', 'a', 'c', 't')
#define RIFFCDifHeader_magic C8x4('C', 'D', 'i', 'f')
#define RIFFListHeader_magic C8x4('L', 'I', 'S', 'T')
#define RIFFId3Header_magic  C8x4('i', 'd', '3', ' ')
#define RIFFBextHeader_magic C8x4('b', 'e', 'x', 't')
#define RIFFIxmlHeader_magic C8x4('i', 'X', 'M', 'L')
#define RIFFXMPHeader_magic  C8x4('_', 'P', 'M', 'X')
#define RIFFCartHeader_magic C8x4('c', 'a', 'r', 't')
#define RIFFSmplHeader_magic C8x4('s', 'm', 'p', 'l')

#define RIFFFllrHeader_magic C8x4('F', 'L', 'L', 'R')
#define RIFFJunkHeader_magic C8x4('J', 'U', 'N', 'K')
#define RIFFjunkHeader_magic C8x4('j', 'u', 'n', 'k')
#define RIFFPadHeader_magic  C8x4('P', 'A', 'D', ' ')

#ifdef __cplusplus
	}
#endif

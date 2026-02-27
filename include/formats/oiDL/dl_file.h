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
#include "formats/oiXX/oiXX.h"
#include "types/container/list_predeclare.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct RefPtr RefPtr;
typedef struct RefPtrType RefPtrType;
typedef RefPtr StreamRef;

typedef enum EDLDataType {
	EDLDataType_Data,									//(default, Buffer)
	EDLDataType_String,									//--string (CharString)
	EDLDataType_Count
} EDLDataType;

typedef U8 DLDataType;			//EDLDataType

typedef enum EDLSettingsFlags {
	EDLSettingsFlags_None				= 0,
	EDLSettingsFlags_HideMagicNumber	= 1 << 0,		//Only valid if the oiDL can be 100% confidently detected otherwise
	EDLSettingsFlags_Invalid			= 0xFE
} EDLSettingsFlags;

typedef U8 DLSettingsFlags;		//EDLSettingsFlags

typedef struct DLSettings {

	//Is compared as U64[7], which works because in DLFile::settings it's 8-byte aligned

	XXCompressionType compressionType;
	XXEncryptionType encryptionType;
	DLDataType dataType;
	DLSettingsFlags flags;
	U32 chunkSize;				//0 = default

	U32 encryptionKey[8];
	U32 iv[3];					//This is readonly, write will always generate new one.
	U32 padding;

} DLSettings;

//Check docs/oiDL.md for the file spec

typedef struct DLEntryStream {		//So that we don't have to add a lot of refs to the same stream
	StreamRef *stream;
	U64 dataOff;					//Offset of data (not the real location in the stream, needs to apply chunking)
	U64 len;
} DLEntryStream;

TList(DLEntryStream);

typedef struct DLFile {

	//These entries don't necessarily store the actual data.
	// First, entryStreams[i].stream should be checked to ensure it's not lazily loaded instead.

	union {
		ListBuffer entryBuffers;
		ListCharString entryStrings;
	};

	ListDLEntryStream entryStreams;	//Needs to be equal to the size of entryBuffers or entryString

	Buffer cache;				//Keep small entries in here up to 1MiB, so they reference this buffer.
	DLSettings settings;		//Keep this at 8-byte alignment!
	U64 readLength;				//How many bytes were read for this file

} DLFile;

static const U32 DLFile_smallLen = 32768;
static const U32 DLFile_medLen = 131072;

static inline U64 DLFile_entryCount(const DLFile *file) {
	return file ? file->entryStreams.length : 0;
}

static inline Bool DLFile_isAllocated(const DLFile *file) {
	return file && file->entryStreams.ptr;
}

static inline Bool DLFile_isFullyLoaded(const DLFile *file, U64 i) {
	return file && i < file->entryStreams.length && !file->entryStreams.ptr[i].stream;
}

static inline U64 DLFile_entrySize(const DLFile *file, U64 i) {

	if (!file || i >= file->entryStreams.length)
		return 0;

	DLEntryStream stream = file->entryStreams.ptr[i];

	if (stream.stream)
		return stream.len;

	return
		file->settings.dataType == EDLDataType_String ?
		CharString_length(file->entryStrings.ptr[i]) :
		Buffer_length(file->entryBuffers.ptr[i]);
}

#ifdef __cplusplus
	}
#endif

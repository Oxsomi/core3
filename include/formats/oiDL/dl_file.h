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

//formats/oiDL/dl_file.h

#pragma once
#include "types/container/string.h"
#include "types/container/list_predeclare.h"
#include "formats/oiXX/oiXX.h"

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

Bool DLFile_create(const DLSettings *settings, U64 cacheSize, const Allocator *alloc, DLFile *dlFile, Error *e_rr);
void DLFile_free(DLFile *dlFile, const Allocator *alloc);

Bool DLFile_createCopy(const DLFile *dlFile, const Allocator *alloc, DLFile *copy, Error *e_rr);
Bool DLFile_reserve(DLFile *dlFile, U64 reserve, const Allocator *alloc, Error *e_rr);

//Init cache if it doesn't exist yet, size == 0 reserves default size.
Bool DLFile_initCache(DLFile *dlFile, U64 size, const Allocator *alloc, Error *e_rr);

//Important note: StreamRef at startOffset shouldn't be contained in a StreamCursor at this moment,
// or you might risk overwriting it.
Bool DLFile_write(
	const DLFile *dlFile,
	const Allocator *alloc,
	StreamRef *result,		//Pass NULL to calculate length only (*startOffset), won't work if compression is used.

	//NULL if not encrypted, otherwise must be valid for the DLFile's stream lifetime
	// This can outlast the DLFile if it's the stream is referenced elsewhere.
	const RefPtrType *encryptionStreamType,

	I32x4 iv,		//Unused if not a subfile or not encrypted, otherwise should contain valid unique iv

	U64 *startOffset,
	Error *e_rr
);

Bool DLFile_read(
	StreamRef *file,
	U64 *startOffset,
	const U32 encryptionKey[8],		//Must be NULL if no encryption, else must be valid
	I32x4 iv,						//If it's a subFile and encrypted, needs to be the valid IV used to encrypt
	Bool isSubFile,					//Sets HideMagicNumber flag and allows leftover data after the oiDL
	Bool forceKeepInStreams,		//Useful to pass around streams and not deal with buffers/strings
	const Allocator *alloc,

	//NULL if not encrypted, otherwise must be valid for the DLFile's stream lifetime
	// This can outlast the DLFile if it's the stream is referenced elsewhere.
	const RefPtrType *encryptionStreamType,	

	DLFile *dlFile,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif

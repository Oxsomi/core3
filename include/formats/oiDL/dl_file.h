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
typedef RefPtr StreamRef;

typedef enum EDLDataType {
	EDLDataType_Data,									//(default, Buffer)
	EDLDataType_String,									//--string (CharString)
	EDLDataType_Count
} EDLDataType;

typedef U8 DLDataType;			//EDLDataType

typedef enum EDLSettingsFlags {
	EDLSettingsFlags_None				= 0,
	EDLSettingsFlags_UseSHA256			= 1 << 0,		//--sha256
	EDLSettingsFlags_HideMagicNumber	= 1 << 1,		//Only valid if the oiDL can be 100% confidently detected otherwise
	EDLSettingsFlags_Invalid			= 0xFC
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
	U64 startOff;					//Offset of start in stream
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

Bool DLFile_create(const DLSettings *settings, U64 cacheSize, const Allocator *alloc, DLFile *dlFile, Error *e_rr);
void DLFile_free(DLFile *dlFile, const Allocator *alloc);

Bool DLFile_loadedStringAt(const DLFile *dlFile, U64 i, CharString *string, Error *e_rr);
Bool DLFile_loadedBufferAt(const DLFile *dlFile, U64 i, Buffer *buffer, Error *e_rr);

//Move from stream into memory permanently and close stream.
Bool DLFile_loadEntry(const DLFile *dlFile, U64 i, const Allocator *alloc, Error *e_rr);

//Load contents into stream (using cache), also works fine for loaded entries
Bool DLFile_loadStream(
	const DLFile *dlFile,
	U64 i,
	Buffer cache,					//Pass empty buffer for default
	StreamCursor *writeCursor,
	U64 writeOffset,
	const Allocator *alloc,
	Error *e_rr
);

//Currently quite slow!
U64 DLFile_findLoadedString(const DLFile *dlFile, U64 start, U64 end, const CharString *string);

//Turn raw buffer list / char string list / stream list into a DLFile.

Bool DLFile_createBufferList(
	const DLSettings *settings,
	ListBuffer *buffers,			//Moves ListBuffer to DLFile, clears ListBuffer after.
	const Allocator *alloc,
	DLFile *dlFile,
	Error *e_rr
);

Bool DLFile_createStringList(
	const DLSettings *settings,
	ListCharString *strings,		//Moves ListBuffer to DLFile, clears ListBuffer after.
	const Allocator *alloc,
	DLFile *dlFile,
	Error *e_rr
);

Bool DLFile_createStreamList(
	const DLSettings *settings,
	ListDLEntryStream *streams,		//Moves ListBuffer to DLFile, clears ListBuffer after.
	const Allocator *alloc,
	DLFile *dlFile,
	Error *e_rr
);

//Move entry to dlFile, so afterwards entry is empty
Bool DLFile_addEntry(DLFile *dlFile, Buffer *entry, const Allocator *alloc, Error *e_rr);

//Move entry to dlFile, so afterwards entry is empty
Bool DLFile_addEntryString(DLFile *dlFile, CharString *entry, const Allocator *alloc, Error *e_rr);

//Add lazy entry for large entries
Bool DLFile_addEntryStream(
	DLFile *dlFile,
	StreamRef *stream,
	U64 startOff,
	U64 dataOff,
	U64 len,
	const Allocator *alloc,
	Error *e_rr
);

//Important note: StreamRef at startOffset shouldn't be contained in a StreamCursor at this moment,
// or you might risk overwriting it.
Bool DLFile_write(const DLFile *dlFile, const Allocator *alloc, StreamRef *result, U64 *startOffset, Error *e_rr);

Bool DLFile_read(
	StreamRef *file,
	U64 startOffset,
	const U32 encryptionKey[8],		//Must be NULL if no encryption, else must be valid
	Bool isSubFile,					//Sets HideMagicNumber flag and allows leftover data after the oiDL
	const Allocator *alloc,
	DLFile *dlFile,
	Error *e_rr
);

Bool DLFile_combine(const DLFile *a, const DLFile *b, const Allocator *alloc, DLFile *combined, Error *e_rr);
//TODO: Bool DLFile_split(const DLFile *a, const Buffer *bitset, const Allocator *alloc, DLFile *split, Error *e_rr);

//File headers

//File spec (docs/oiDL.md)

typedef enum EDLVersion {
	EDLVersion_V1_0
} EDLVersion;

typedef U8 DLVersion;		//EDLVersion

typedef enum EDLFlags {

	EDLFlags_None 					= 0,

	EDLFlags_UseSHA256				= 1 << 0,		//Whether SHA256 (1) or CRC32C (0) is used as hash

	EDLFlags_IsString				= 1 << 1,		//If true; must be a valid UTF8 string

	//Chunk size of AES for multi threading. 0 = 128KiB, 1 = 1MiB, 2 = 8MiB, 3 = 64MiB

	EDLFlags_UseAESChunksA			= 1 << 2,
	EDLFlags_UseAESChunksB			= 1 << 3,

	EDLFlags_HasExtendedData		= 1 << 4,		//Extended data

	EDLFlags_AESChunkMask			= EDLFlags_UseAESChunksA | EDLFlags_UseAESChunksB,
	EDLFlags_AESChunkShift			= 2

} EDLFlags;

typedef U8 DLFlags;			//EDLFlags

typedef struct DLHeader {
	DLVersion version;		//major.minor (%10 = minor, /10 = major (+1 to get real major))
	DLFlags flags;
	U8 type;				//(EXXCompressionType << 4) | EXXEncryptionType. Each enum should be <Count (see oiXX.md).
	U8 sizeTypes;			//EXXDataSizeTypes: entryType | (uncompressedSizType << 2) | (dataType << 4) (must be < (1 << 6)).
} DLHeader;

static const U32 DLHeader_chunkSizes[] = { 131072, 1048576, 8388608, 67108864 };
static const U8 DLHeader_chunkSizesShifts[] = { 17, 20, 23, 26 };

typedef struct DLExtraInfo {

	//Identifier to ensure the extension is detected.
	//0x0 - 0x1FFFFFFF are version headers, others are extensions.
	U32 extendedMagicNumber;

	U16 extendedHeader;			//If extensions want to add extra data to the header
	U16 perEntryExtendedData;	//What to store per entry besides a DataSizeType

} DLExtraInfo;

#define DLHeader_MAGIC 0x4C44696F

#ifdef __cplusplus
	}
#endif

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

//tools/oxc3_cli/inspect_header.c

#include "types/base/error.h"
#include "types/base/mathi.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/container/log.h"
#include "formats/oiCA/ca_file.h"
#include "formats/oiCA/ca_headers.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_headers.h"
#include "formats/oiSH/sh_headers.h"
#include "formats/oiXX/oiXX.h"
#include "formats/bmp/bmp_file.h"
#include "formats/bmp/bmp_headers.h"
#include "formats/dds/dds_file.h"
#include "formats/dds/dds_headers.h"
#include "formats/wav/wav_file.h"
#include "formats/wav/wav_headers.h"
#include "types/container/texture_format.h"
#include "types/container/stream.h"
#include "types/container/memory_stream.h"
#include "platforms/logx.h"
#include "platforms/file.h"
#include "platforms/platform.h"
#include "tools/oxc3_cli/cli.h"
#include "types/base/constants.h"

typedef struct VersionString {
	C8 v[5];        //XX.Y
} VersionString;

VersionString VersionCharString_get(U8 version) {

	VersionString res = (VersionString) { 0 };

	const U8 major = 1 + version / 10;
	const U8 minor = version % 10;
	U8 off = 0;

	if (major / 10)
		res.v[off++] = '0' + major / 10;

	res.v[off++] = '0' + major % 10;
	res.v[off++] = '.';
	res.v[off++] = '0' + minor;
	return res;
}

void XXFile_printType(U8 type) {

	//Compression is currently unsupported (only EXXCompressionType_None), so a set compression nibble is unexpected.

	if (type >> 4)
		Log_debugLnx("Compression type: Unrecognized (compression is unsupported).");

	if (type & 0xF) {
		const C8 *typeStr = (type & 0xF) == EXXEncryptionType_AES256GCM ? "AES256GCM" : "Unrecognized";
		Log_debugLnx("Encryption type: %s.", typeStr);
	}
}

void XXFile_printVersion(U8 v) {
	VersionString str = VersionCharString_get(v);
	Log_debugLnx("Version: %s", str.v);
}

static const C8 *dataTypes[] = {
	"U8 (8-bit number).",
	"U16 (16-bit number)",
	"U32 (32-bit number)",
	"U64 (64-bit number)"
};

//Image / audio formats are parsed via their stream readers (header only).
//BMP has a 2-byte magic; DDS/WAV are 4-byte.

static Bool CLI_inspectMediaHeader(StreamRef *stream, U16 magic16, U32 magic32, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	ListSubResourceData subs = (ListSubResourceData) { 0 };

	if(magic16 == BMP_MAGIC) {

		BMPInfo info = (BMPInfo) { 0 };
		U64 off = 0, dataOff = 0;
		gotoIfError3(clean, BMP_read(stream, &off, &dataOff, &info, alloc, e_rr));

		Log_debugLnx(
			"Detected BMP file:\n\t%"PRIu32" x %"PRIu32", format %s\n\t%"PRIi32" x %"PRIi32" pixels/meter%s",
			info.w, info.h, ETextureFormatId_name[info.textureFormatId],
			info.xPixPerM, info.yPixPerM, info.discardAlpha ? "\n\tAlpha discarded (stored as RGB8)" : ""
		);
	}

	else if(magic32 == ddsMagic) {

		DDSInfo info = (DDSInfo) { 0 };
		U64 off = 0;
		gotoIfError3(clean, DDS_read(stream, &off, &info, alloc, &subs, e_rr));

		Log_debugLnx(
			"Detected DDS file:\n\t%"PRIu32" x %"PRIu32" x %"PRIu32"\n\t%"PRIu32" mips, %"PRIu32" layers\n\t"
			"format %s, texture type %"PRIu32", %"PRIu64" subresources",
			info.w, info.h, info.l, info.mips, info.layers,
			ETextureFormatId_name[info.textureFormatId], (U32) info.type, subs.length
		);
	}

	else {        //RIFFHeader_magic (WAV)

		WAVFile wav = (WAVFile) { 0 };
		gotoIfError3(clean, WAV_read(stream, 0, 0, alloc, &wav, e_rr));

		const C8 *fmt =
			wav.fmt.format == ERIFFAudioFormat_IEEE754 ? "IEEE754 float" :
			(wav.fmt.format == ERIFFAudioFormat_PCM ? "PCM" : "unknown");

		Log_debugLnx(
			"Detected WAV file:\n\t%s, %"PRIu16" channel(s), %"PRIu32" Hz, %"PRIu16"-bit\n\t"
			"%"PRIu16" bytes/block, %"PRIu32" bytes/second, %"PRIu32" data bytes",
			fmt, wav.fmt.channels, wav.fmt.frequency, wav.fmt.stride,
			wav.fmt.bytesPerBlock, wav.fmt.bytesPerSecond, wav.dataLength
		);
	}

clean:
	ListSubResourceData_freeUnderlying(&subs, alloc);
	return s_uccess;
}

//Inspect header is a lightweight checker for the header.
//To get a more detailed view you can use OxC3 file data instead.
//Viewing the header doesn't require a key (if encrypted), but the data does.

Bool CLI_inspectHeader(const ParsedArgs *args) {

	if(!args) return false;

	Error err = Error_none();
	Bool success = false;

	const Allocator *alloc = Platform_instance->alloc;
	RefPtrType fileHandleType = FileHandle_makeType(alloc);
	RefPtrType streamType = FileStream_makeType(alloc);
	RefPtrType memStreamType = MemoryStream_makeType(alloc);

	CharString path = CharString_createNull();
	StreamRef *stream = NULL;
	StreamCursor cursor = (StreamCursor) { 0 };
	U8 hdr[512];
	Buffer buf = Buffer_createNull(), virtualBuf = Buffer_createNull();

	if (!ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &path, &err)) {
		Log_errorLnx("Invalid argument -input <string>.");
		goto clean;
	}

	CLI_ensureVirtualLoaded(&path);        //If it's a "//section/..." path, load the section so File_* can resolve it

	//We only ever read a small, bounded header region via a stream cursor - never the whole (possibly huge) file.
	//The 4-byte magic dispatches; the image/audio parsers then read what they need straight from the same stream.

	FileInfo info = (FileInfo) { 0 };
	if (!File_getInfo(&path, &info, alloc, &err)) {
		Log_errorLnx("Invalid file path.");
		goto clean;
	}

	const U64 size = info.fileSize;
	FileInfo_free(&info, alloc);

	if (size < 4) {
		Log_errorLnx("File has to start with a magic number.");
		goto clean;
	}

	//File_open(Stream) isn't supported on virtual files, so for those we read the (in-memory, embedded) file into a
	//buffer and wrap it in a memory stream; physical files stream straight from disk (bounded read below).

	if (File_isVirtual(path)) {

		if (!File_read(&path, 100 * MS, 0, 0, &fileHandleType, &virtualBuf, &err)) {
			Log_errorLnx("Couldn't read virtual file.");
			goto clean;
		}

		if (!MemoryStream_createFromBuffer(&virtualBuf, EMemoryStreamFlags_None, &memStreamType, &stream, &err))
			goto clean;
	}

	else if (!File_openStream(&path, 100 * MS, EFileOpenType_Read, false, &fileHandleType, &streamType, &stream, &err)) {
		Log_errorLnx("Couldn't open file.");
		goto clean;
	}

	if (!StreamCursor_create(stream, 64 * KIBI, false, alloc, &cursor, &err))
		goto clean;

	//Every supported header (oiXX header + counts, or just the magic the image/audio parsers key off) fits in hdr

	const U64 headerRead = U64_min(size, (U64) sizeof(hdr));

	if (!StreamCursor_read(&cursor, Buffer_createRef(hdr, sizeof(hdr)), 0, 0, headerRead, false, alloc, &err))
		goto clean;

	buf = Buffer_createRefConst(hdr, headerRead);

	//Image / audio formats are read via their own stream parsers rather than the oiXX header layout below

	{
		const U16 magic16 = *(const U16*)buf.ptr;
		const U32 magic32 = *(const U32*)buf.ptr;

		if (magic16 == BMP_MAGIC || magic32 == ddsMagic || magic32 == RIFFHeader_magic) {
			success = CLI_inspectMediaHeader(stream, magic16, magic32, alloc, &err);
			goto clean;
		}
	}

	U64 reqLen = 0;

	switch (*(const U32*)buf.ptr) {

		case CAHeader_MAGIC:    reqLen = sizeof(CAHeader);                    break;
		case DLHeader_MAGIC:    reqLen = sizeof(DLHeader) + sizeof(U32);    break;
		case SHHeader_MAGIC:    reqLen = sizeof(SHHeader) + sizeof(U32);    break;
		case SBHeader_MAGIC:    reqLen = sizeof(SBHeader) + sizeof(U32);    break;
		default:
			Log_errorLnx("File wasn't recognized.");
			goto clean;
	}

	if (Buffer_length(buf) < reqLen) {
		Log_errorLnx("File wasn't the right size.");
		goto clean;
	}

	switch (*(const U32*)buf.ptr) {

		//oiSH header

		case SHHeader_MAGIC: {

			const SHHeader shHeader = *(const SHHeader*)(buf.ptr + sizeof(U32));

			Log_debugLnx("Detected oiSH file with following info:");

			XXFile_printVersion(shHeader.version);

			Log_debugLnx(
				"Compiler version: %"PRIu32".%"PRIu32".%"PRIu32,
				OXC3_GET_MAJOR(shHeader.compilerVersion),
				OXC3_GET_MINOR(shHeader.compilerVersion),
				OXC3_GET_PATCH(shHeader.compilerVersion)
			);

			Log_debugLnx(
				"Spirv size type uses %s (if present).",
				dataTypes[(shHeader.sizeTypes >> 0) & 3]
			);

			Log_debugLnx(
				"DXIL size type uses %s (if present).",
				dataTypes[(shHeader.sizeTypes >> 2) & 3]
			);

			Log_debugLnx("Hashes: Source: %08"PRIX64", Contents: %08"PRIX64, shHeader.sourceHash, shHeader.hash);

			Log_debugLnx(
				"With %"PRIu16" binaries, %"PRIu16" stages, %"PRIu16" defines, "
				"%"PRIu16" stored semantics, %"PRIu16" includes, %"PRIu16" arrayDimCount, %"PRIu16" registerNameCount and "
				"%"PRIu16" uniformNameCount",
				shHeader.binaryCount,
				shHeader.stageCount,
				shHeader.uniqueDefines,
				shHeader.semanticCount,
				shHeader.includeFileCount,
				shHeader.arrayDimCount,
				shHeader.registerNameCount,
				shHeader.uniformNameCount
			);

			break;
		}

		//oiSB header

		case SBHeader_MAGIC: {

			const SBHeader sbHeader = *(const SBHeader*)(buf.ptr + sizeof(U32));

			Log_debugLnx("Detected oiSB file with following info:");

			XXFile_printVersion(sbHeader.version);

			if(sbHeader.flags & ESBFlag_IsTightlyPacked)
				Log_debugLnx("\tFlag: Is tightly packed");

			Log_debugLnx(
				"With %"PRIu16" arrays, %"PRIu16" structs, %"PRIu16" vars and %"PRIu32" bufferSize",
				sbHeader.arrays,
				sbHeader.structs,
				sbHeader.vars,
				sbHeader.bufferSize
			);

			break;
		}

		//oiCA header

		case CAHeader_MAGIC: {

			const CAHeader caHeader = *(const CAHeader*)buf.ptr;

			Log_debugLnx("Detected oiCA file with following info:");

			XXFile_printVersion(caHeader.version);

			//Type (oiCA stores only an encryption type; the archive itself isn't compressed)

			XXFile_printType(caHeader.type);

			//Extended data sits right after the header, before the file/directory counts

			if (caHeader.flags & ECAFlags_HasExtendedData) {

				reqLen += sizeof(CAExtraInfo);

				if (Buffer_length(buf) < reqLen) {
					Log_errorLnx("File wasn't the right size.");
					goto clean;
				}

				CAExtraInfo extraInfo = (CAExtraInfo) { 0 };
				Buffer_memcpy(
					Buffer_createRef(&extraInfo, sizeof(extraInfo)),
					Buffer_createRefConst(buf.ptr + sizeof(CAHeader), sizeof(extraInfo))
				);

				Log_debugLnx("Extended magic number: %08X", extraInfo.extendedMagicNumber);
				Log_debugLnx("Extended header size: %"PRIu32, (U32)extraInfo.headerExtensionSize);
				Log_debugLnx("Extended per directory size: %"PRIu32, (U32)extraInfo.directoryExtensionSize);
				Log_debugLnx("Extended per file size: %"PRIu32, (U32)extraInfo.fileExtensionSize);
			}

			//File and directory counts (their byte size depends on the *CountLong flags)

			const U64 fileCountPtr = reqLen;
			reqLen += caHeader.flags & ECAFlags_FilesCountLong ? sizeof(U32) : sizeof(U16);

			const U64 dirCountPtr = reqLen;
			reqLen += caHeader.flags & ECAFlags_DirectoriesCountLong ? sizeof(U16) : sizeof(U8);

			if (Buffer_length(buf) < reqLen) {
				Log_errorLnx("File wasn't the right size.");
				goto clean;
			}

			const U64 fileCount = Buffer_forceReadSizeType(
				buf.ptr + fileCountPtr,
				caHeader.flags & ECAFlags_FilesCountLong ? EXXDataSizeType_U32 : EXXDataSizeType_U16
			);

			const U64 dirCount = Buffer_forceReadSizeType(
				buf.ptr + dirCountPtr,
				caHeader.flags & ECAFlags_DirectoriesCountLong ? EXXDataSizeType_U16 : EXXDataSizeType_U8
			);

			Log_debugLnx("Directory count: %"PRIu64, dirCount);
			Log_debugLnx("File count: %"PRIu64, fileCount);

			//Flags (ECAFlags, one bit each)

			static const C8 *flags[] = {
				"\tFiles store a date.",
				"\tFiles store an extended date (up to a nanosecond).",
				"\tFile has an extended data section.",
				"\tDirectory count is stored as a U16 (16-bit) instead of a U8 (8-bit).",
				"\tFile count is stored as a U32 (32-bit) instead of a U16 (16-bit)."
			};

			if(caHeader.flags) {

				Log_debugLnx("Flags: ");

				for(U64 i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i)
					if((caHeader.flags >> i) & 1)
						Log_debugLnx(flags[i]);
			}

			break;
		}

		//oiDL header

		case DLHeader_MAGIC: {

			const DLHeader dlHeader = *(const DLHeader*)(buf.ptr + sizeof(U32));

			Log_debugLnx("Detected oiDL file with following info:");

			XXFile_printVersion(dlHeader.version);

			//AES chunking

			const U32 aesChunking = dlHeader.flags & EDLFlags_AESChunkMask;

			if (aesChunking) {

				static const C8 *chunking[] = {
					"10 MiB",
					"50 MiB",
					"100 MiB"
				};

				Log_debugLnx(
					"AES Chunking uses %s per chunk.",
					chunking[(aesChunking >> EDLFlags_AESChunkShift) - 1]
				);
			}

			//Types

			XXFile_printType(dlHeader.type);

			//File sizes

			Log_debugLnx("Entry count size type uses %s", dataTypes[dlHeader.sizeTypes & 3]);

			if (dlHeader.type >> 4)
				Log_debugLnx("Compression size type uses %s", dataTypes[(dlHeader.sizeTypes >> 2) & 3]);

			Log_debugLnx("Buffer size type uses %s", dataTypes[(dlHeader.sizeTypes >> 4) & 3]);

			reqLen += (U64)1 << (dlHeader.sizeTypes & 3);

			if (Buffer_length(buf) < reqLen) {
				Log_errorLnx("File wasn't the right size.");
				goto clean;
			}

			//Entry count

			const U64 entryCount = Buffer_forceReadSizeType(
				buf.ptr + sizeof(U32) + sizeof(DLHeader),
				(EXXDataSizeType)(dlHeader.sizeTypes & 3)
			);

			Log_debugLnx("Entry count: %"PRIu64, entryCount);

			//Extended data

			if (dlHeader.flags & EDLFlags_HasExtendedData) {

				if (Buffer_length(buf) < reqLen) {
					Log_errorLnx("File wasn't the right size.");
					goto clean;
				}

				DLExtraInfo extraInfo = (DLExtraInfo) { 0 };
				Buffer_memcpy(
					Buffer_createRef(&extraInfo, sizeof(extraInfo)),
					Buffer_createRefConst(buf.ptr + reqLen, sizeof(extraInfo))
				);

				Log_debugLnx("Extended magic number: %08X", extraInfo.extendedMagicNumber);
				Log_debugLnx("Extended header size: %"PRIu32, (U32)extraInfo.extendedHeader);
				Log_debugLnx("Extended per entry size: %"PRIu32, (U32)extraInfo.perEntryExtendedData);
			}

			//Flags

			static const C8 *flags[] = {
				"\tHash is SHA256 instead of CRC32C (a 256-bit hash instead of a 32-bit one).",
				"\tFile is a string array rather than a data array.",
				"\tFile is UTF8 encoded rather than ASCII.",
				NULL,
				NULL,
				"\tFile has an extended data section.",
				"\tUnrecognized flag"
			};

			if(dlHeader.flags & ~EDLFlags_AESChunkMask) {

				Log_debugLnx("Flags: ");

				for (U64 i = 0; i < sizeof(dlHeader.flags) << 3; ++i) {

					const C8 *flag = flags[U64_min(i, sizeof(flags) / sizeof(flags[0]) - 1)];

					if((dlHeader.flags >> i) & 1 && flag)
						Log_debugLnx(flag);
				}
			}

			break;
		}

		//Invalid

		default:
			Log_errorLnx("File had unrecognized magic number");
			goto clean;
	}

	success = true;

clean:

	if(err.genericError)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_NewLine);

	StreamCursor_close(&cursor, alloc);
	RefPtr_dec(&stream);
	Buffer_free(&virtualBuf, alloc);
	return success;
}

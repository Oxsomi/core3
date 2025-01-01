/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "platforms/ext/listx.h"
#include "types/base/error.h"
#include "types/math/math.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "formats/oiCA/ca_file.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiSH/headers.h"
#include "platforms/log.h"
#include "platforms/file.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/errorx.h"
#include "tools/cli.h"

typedef struct VersionString {
	C8 v[5];		//XX.Y
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

	if (type >> 4) {

		const C8 *typeStr = "Unrecognized";

		switch (type >> 4) {
			case EXXCompressionType_Brotli11:	typeStr = "Brotli:11";		break;
			case EXXCompressionType_Brotli1:	typeStr = "Brotli:1";		break;
		}

		Log_debugLnx("Compression type: %s.", typeStr);
	}

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

//Inspect header is a lightweight checker for the header.
//To get a more detailed view you can use OxC3 file data instead.
//Viewing the header doesn't require a key (if encrypted), but the data does.

Bool CLI_inspectHeader(ParsedArgs args) {

	Buffer buf = Buffer_createNull();
	Error err;
	Bool success = false;

	CharString path = CharString_createNull();
	CharString tmp = CharString_createNull();

	if ((err = ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &path)).genericError) {
		Log_errorLnx("Invalid argument -input <string>.");
		goto clean;
	}

	if (!File_readx(path, 100 * MS, 0, 0, &buf, &err)) {
		Log_errorLnx("Invalid file path.");
		goto clean;
	}

	if (Buffer_length(buf) < 4) {
		Log_errorLnx("File has to start with magic number.");
		goto clean;
	}

	U64 reqLen = 0;

	switch (*(const U32*)buf.ptr) {

		case CAHeader_MAGIC:	reqLen = sizeof(CAHeader);					break;
		case DLHeader_MAGIC:	reqLen = sizeof(DLHeader) + sizeof(U32);	break;
		case SHHeader_MAGIC:	reqLen = sizeof(SHHeader) + sizeof(U32);	break;
		case SBHeader_MAGIC:	reqLen = sizeof(SBHeader) + sizeof(U32);	break;
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
				"With %"PRIu16" binaries, %"PRIu16" stages, %"PRIu16" uniforms, "
				"%"PRIu16" stored semantics, %"PRIu16" includes, %"PRIu16" arrayDimCount and %"PRIu16" registerNameCount",
				shHeader.binaryCount,
				shHeader.stageCount,
				shHeader.uniqueUniforms,
				shHeader.semanticCount,
				shHeader.includeFileCount,
				shHeader.arrayDimCount,
				shHeader.registerNameCount
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

			//File sizes

			Log_debugLnx(
				"Data size type uses %s.",
				dataTypes[(caHeader.flags >> ECAFlags_FileSizeType_Shift) & ECAFlags_FileSizeType_Mask]
			);

			if (caHeader.type >> 4)
				Log_debugLnx(
					"Compression size type uses %s.",
					dataTypes[(caHeader.flags >> ECAFlags_CompressedSizeType_Shift) & ECAFlags_CompressedSizeType_Mask]
				);

			//File and directory count

			const U64 fileCountPtr = reqLen;
			reqLen += caHeader.flags & ECAFlags_FilesCountLong ? sizeof(U32) : sizeof(U16);

			const U64 dirCountPtr = reqLen;
			reqLen += caHeader.flags & ECAFlags_DirectoriesCountLong ? sizeof(U16) : sizeof(U8);

			if (Buffer_length(buf) < reqLen) {
				Log_errorLnx("File wasn't the right size.");
				goto clean;
			}

			//Entry count

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

			//AES chunking

			const U32 aesChunking = caHeader.flags & ECAFlags_AESChunkMask;

			if (aesChunking) {

				static const C8 *chunking[] = {
					"10 MiB",
					"100 MiB",
					"500 MiB"
				};

				Log_debugLnx(
					"AES Chunking uses %s per chunk.",
					chunking[(aesChunking >> ECAFlags_AESChunkShift) - 1]
				);
			}

			//Types

			XXFile_printType(caHeader.type);

			//Extended data

			if (caHeader.flags & ECAFlags_HasExtendedData) {

				if (Buffer_length(buf) < reqLen) {
					Log_errorLnx("File wasn't the right size.");
					goto clean;
				}

				CAExtraInfo extraInfo = (CAExtraInfo) { 0 };
				Buffer_copy(
					Buffer_createRef(&extraInfo, sizeof(extraInfo)),
					Buffer_createRefConst(buf.ptr + reqLen, sizeof(extraInfo))
				);

				Log_debugLnx("Extended magic number: %08X", extraInfo.extendedMagicNumber);
				Log_debugLnx("Extended header size: %"PRIu32, (U32)extraInfo.headerExtensionSize);
				Log_debugLnx("Extended per directory size: %"PRIu32, (U32)extraInfo.directoryExtensionSize);
				Log_debugLnx("Extended per file size: %"PRIu32, (U32)extraInfo.fileExtensionSize);
			}

			//Flags

			static const C8 *flags[] = {
				"\tHash is SHA256 instead of CRC32C (a 256-bit hash instead of a 32-bit one).",
				"\tFiles store a date in interval of two seconds or up to a nanosecond.",
				"\tFile stores a full date, up to nanoseconds.",
				NULL,
				NULL,
				NULL,
				NULL,
				"\tFile has an extended data section.",
				NULL,
				NULL,
				"\tFile stores directory references/count as a U16 (16-bit) as opposed to U8 (8-bit).",
				"\tFile stores file references/count as a U32 (32-bit) as opposed to U16 (16-bit).",
				"\tUnrecognized flag"
			};

			if(caHeader.flags & ~ECAFlags_NonFlagTypes) {

				Log_debugLnx("Flags: ");

				for(U64 i = 0; i < sizeof(caHeader.flags) << 3; ++i) {

					const C8 *flag = flags[U64_min(i, sizeof(flags) / sizeof(flags[0]) - 1)];

					if((caHeader.flags >> i) & 1 && flag)
						Log_debugLnx(flag);
				}
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
				Buffer_copy(
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
		Error_printx(err, ELogLevel_Error, ELogOptions_NewLine);

	CharString_freex(&tmp);
	Buffer_freex(&buf);
	return success;
}

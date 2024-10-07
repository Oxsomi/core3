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

#include "platforms/ext/listx.h"
#include "types/buffer.h"
#include "types/error.h"
#include "types/string.h"
#include "formats/oiCA.h"
#include "formats/oiDL.h"
#include "formats/oiSH.h"
#include "formats/oiSB.h"
#include "platforms/ext/formatx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/archivex.h"
#include "platforms/file.h"
#include "platforms/log.h"
#include "cli.h"
#include "types/math.h"

#ifdef CLI_SHADER_COMPILER
	#include "shader_compiler/compiler.h"
#endif

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

	if (!File_read(path, 1 * SECOND, &buf, &err)) {
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

				const U64 oldPtr = reqLen;
				reqLen += sizeof(CAExtraInfo);

				if (Buffer_length(buf) < reqLen) {
					Log_errorLnx("File wasn't the right size.");
					goto clean;
				}

				const CAExtraInfo extraInfo = *(const CAExtraInfo*)(buf.ptr + oldPtr);

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

				const U64 oldPtr = reqLen;
				reqLen += sizeof(DLExtraInfo);

				if (Buffer_length(buf) < reqLen) {
					Log_errorLnx("File wasn't the right size.");
					goto clean;
				}

				const DLExtraInfo extraInfo = *(const DLExtraInfo*)(buf.ptr + oldPtr);

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

//Printing an entry

Bool collectArchiveEntries(FileInfo info, ListCharString *arg, Error *e_rr) {

	Bool s_uccess = true;
	CharString tmp = CharString_createNull();

	gotoIfError2(clean, CharString_createCopyx(info.path, &tmp))
	gotoIfError2(clean, ListCharString_pushBackx(arg, tmp))

	tmp = CharString_createNull();		//Belongs to list now

clean:
	CharString_freex(&tmp);
	return s_uccess;
}

//Printing an entry

typedef struct OutputFolderToDisk {
	CharString base, output;
	Archive sourceArchive;
} OutputFolderToDisk;

Bool writeToDisk(FileInfo info, OutputFolderToDisk *output, Error *e_rr) {

	Bool s_uccess = true;
	CharString subDir = CharString_createNull();
	CharString tmp = CharString_createNull();

	const U64 start = CharString_length(output->base) == 1 && output->base.ptr[0] == '.' ? 0 : CharString_length(output->base);

	if(!CharString_cut(info.path, start, 0, &subDir))
		retError(clean, Error_invalidOperation(0, "writeToDisk()::info.path cut failed"))

	gotoIfError2(clean, CharString_createCopyx(output->output, &tmp))
	gotoIfError2(clean, CharString_appendStringx(&tmp, subDir))

	if (info.type == EFileType_File) {
		Buffer data = Buffer_createNull();
		gotoIfError3(clean, Archive_getFileDataConstx(output->sourceArchive, info.path, &data, e_rr))
		gotoIfError3(clean, File_write(data, tmp, 1 * SECOND, e_rr))
	}

	else gotoIfError3(clean, File_add(tmp, EFileType_Folder, 1 * SECOND, false, e_rr))

clean:
	CharString_freex(&tmp);
	return s_uccess;
}

//Showing the entire file or a part to disk or to log

Bool CLI_showFile(ParsedArgs args, Buffer b, U64 start, U64 length, Bool isAscii, Bool showEntireFile) {

	//Validate offset

	if (start + ((Bool)Buffer_length(b)) > Buffer_length(b)) {
		Log_debugLnx("Section out of bounds.");
		return false;
	}

	//Output it to a folder on disk was requested

	Error *e_rr = NULL;
	CharString tmp = CharString_createNull();
	CharString tmp1 = CharString_createNull();
	Bool s_uccess = false;

	if (args.parameters & EOperationHasParameter_Output) {

		if(!length)
			length = Buffer_length(b) - start;

		if (start + length > Buffer_length(b)) {
			Log_debugLnx("Section out of bounds.");
			goto clean;
		}

		CharString out = CharString_createNull();

		if (ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &out).genericError) {
			Log_errorLnx("Invalid argument -output <string>.");
			goto clean;
		}

		Buffer subBuffer = Buffer_createRefConst(b.ptr + start, length);
		gotoIfError3(clean, File_write(subBuffer, out, 1 * SECOND, e_rr))
	}

	//More info about a single entry

	else {

		if (!Buffer_length(b)) {
			Log_debugLnx("Section is empty.");
			goto clean;
		}

		Log_debugLnx("Section has %"PRIu64" bytes.", Buffer_length(b));

		//Get length

		U64 max = 32 * 32;

		if(!length)
			length = showEntireFile ? Buffer_length(b) - start : U64_min(max, Buffer_length(b) - start);

		else length = U64_min(max * 2, length);

		if (start + length > Buffer_length(b)) {
			Log_debugLnx("Section out of bounds.");
			goto clean;
		}

		//Show what offset is being displayed

		Log_debugLnx("Showing offset #%"PRIx64" with size %"PRIu64, start, length);
		Log_debugLnx(isAscii ? "File contents: (ascii)" : "File contents: (binary)");

		//Ascii can be directly output to log

		if (isAscii) {
			tmp = CharString_createRefSizedConst((const C8*)b.ptr + start, length, false);
			Log_debugLnx("%.*s", CharString_length(tmp), tmp.ptr);
			tmp = CharString_createNull();
		}

		//Binary needs to be formatted first

		else {

			for (U64 i = start, j = i + length, k = 0; i < j; ++i, ++k) {

				gotoIfError2(clean, CharString_createHexx(b.ptr[i], 2, &tmp1))
				gotoIfError2(clean, CharString_popFrontCount(&tmp1, 2))
				gotoIfError2(clean, CharString_appendStringx(&tmp, tmp1))
				gotoIfError2(clean, CharString_appendx(&tmp, ' '))

				if(!((k + 1) & 31))
					gotoIfError2(clean, CharString_appendStringx(&tmp, CharString_newLine()))

				CharString_freex(&tmp1);
			}

			Log_debugLnx("%.*s", CharString_length(tmp), tmp.ptr);
			CharString_freex(&tmp);
		}
	}

	s_uccess = true;

clean:
	CharString_freex(&tmp1);
	CharString_freex(&tmp);
	return s_uccess;
}

//Storing file or folder on disk

Bool CLI_storeFileOrFolder(ParsedArgs args, ArchiveEntry e, Archive a, Bool *madeFile, U64 start, U64 len) {

	CharString tmp = CharString_createNull();
	Bool s_uccess = false;
	Error *e_rr = NULL;

	//Save folder

	if (e.type == EFileType_Folder) {

		if(start || (len && len != a.entries.length))
			Log_warnLnx("Folder output to disk ignores offset and/or count.");

		CharString out = CharString_createNull();

		if (ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &out).genericError) {
			Log_errorLnx("Invalid argument -output <string>.");
			goto clean;
		}

		gotoIfError3(clean, File_add(out, EFileType_Folder, 1 * SECOND, true, e_rr))
		*madeFile = true;

		gotoIfError2(clean, CharString_createCopyx(out, &tmp))
		gotoIfError2(clean, CharString_appendx(&tmp, '/'))

		OutputFolderToDisk output = (OutputFolderToDisk) {
			.base = e.path,
			.output = tmp,
			.sourceArchive = a
		};

		gotoIfError3(clean, Archive_foreachx(
			a,
			e.path,
			(FileCallback) writeToDisk,
			&output,
			true,
			EFileType_Any,
			e_rr
		))

		CharString_freex(&tmp);
		madeFile = false;			//We successfully wrote, so keep it from deleting the folder
		s_uccess = true;

		goto clean;
	}

	//Save file

	else {
		CLI_showFile(args, e.data, start, len, false, false);
		s_uccess = true;
		goto clean;
	}

clean:
	CharString_freex(&tmp);
	return s_uccess;
}

//Handle inspection of individual data.
//Also handles info about the file in general.

Bool CLI_inspectData(ParsedArgs args) {

	Buffer buf = Buffer_createNull();
	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = false;

	CharString path = CharString_createNull();
	CharString tmp = CharString_createNull();
	CharString tmp1 = CharString_createNull();

	//Get file

	if ((err = ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &path)).genericError) {
		Log_errorLnx("Invalid argument -input <string>.");
		goto clean;
	}

	if (!File_read(path, 1 * SECOND, &buf, e_rr)) {
		Log_errorLnx("Invalid file path.");
		goto clean;
	}

	if (Buffer_length(buf) < 4) {
		Log_errorLnx("File has to start with magic number.");
		goto clean;
	}

	//Parse entry if available

	CharString entry = CharString_createNull();

	if (args.parameters & EOperationHasParameter_Entry)
		if ((err = ParsedArgs_getArg(args, EOperationHasParameter_EntryShift, &entry)).genericError) {
			Log_errorLnx("Invalid argument -entry <string or uint>.");
			goto clean;
		}

	//Parse start if available

	CharString starts = CharString_createNull();
	U64 start = 0;

	if (args.parameters & EOperationHasParameter_StartOffset)
		if (
			(err = ParsedArgs_getArg(args, EOperationHasParameter_StartOffsetShift, &starts)).genericError ||
			!CharString_parseU64(starts, &start) ||
			(start >> 32)
		) {
			Log_errorLnx("Invalid argument -start <uint>.");
			goto clean;
		}

	//Parse end if available

	CharString lengths = CharString_createNull();
	U64 length = 0;

	if (args.parameters & EOperationHasParameter_Length)
		if (
			(err = ParsedArgs_getArg(args, EOperationHasParameter_LengthShift, &lengths)).genericError ||
			!CharString_parseU64(lengths, &length) ||
			(length >> 32)
		) {
			Log_errorLnx("Invalid argument -length <uint>.");
			goto clean;
		}

	//Parse encryption key

	U32 encryptionKeyV[8] = { 0 };
	U32 *encryptionKey = NULL;			//Only if we have aes should encryption key be set.

	if (args.parameters & EOperationHasParameter_AES) {

		CharString key = CharString_createNull();

		if (
			(ParsedArgs_getArg(args, EOperationHasParameter_AESShift, &key)).genericError ||
			!CharString_isHex(key)
		) {
			Log_errorLnx("Invalid parameter sent to -aes. Expecting key in hex (32 bytes)");
			return false;
		}

		U64 off = CharString_startsWithStringInsensitive(key, CharString_createRefCStrConst("0x"), 0) ? 2 : 0;

		if (CharString_length(key) - off != 64) {
			Log_errorLnx("Invalid parameter sent to -aes. Expecting key in hex (32 bytes)");
			return false;
		}

		for (U64 i = off; i + 1 < CharString_length(key); ++i) {

			U8 v0 = C8_hex(key.ptr[i]);
			U8 v1 = C8_hex(key.ptr[++i]);

			v0 = (v0 << 4) | v1;
			*((U8*)encryptionKeyV + ((i - off) >> 1)) = v0;
		}

		encryptionKey = encryptionKeyV;
	}

	U32 magic = *(const U32*)buf.ptr;

	if(args.flags & (EOperationFlags_Bin | EOperationFlags_Includes) && magic != SHHeader_MAGIC) {
		Log_errorLnx("--bin and --includes flag can only be used with an oiSH file");
		return false;
	}

	ESHBinaryType binaryType = ESHBinaryType_Count;

	if (args.parameters & EOperationHasParameter_ShaderOutputMode) {

		if(magic != SHHeader_MAGIC) {
			Log_errorLnx("-compile-output argument can only be used with an oiSH file");
			return false;
		}

		CharString shaderOutputMode = CharString_createNull();
		if ((err = ParsedArgs_getArg(args, EOperationHasParameter_ShaderOutputModeShift, &shaderOutputMode)).genericError) {
			Log_errorLnx("Missing argument -compile-output");
			goto clean;
		}

		if(CharString_equalsStringInsensitive(shaderOutputMode, CharString_createRefCStrConst("DXIL")))
			binaryType = ESHBinaryType_DXIL;

		else if(CharString_equalsStringInsensitive(shaderOutputMode, CharString_createRefCStrConst("SPV")))
			binaryType = ESHBinaryType_SPIRV;

		else {
			Log_errorLnx("Invalid argument. Expected: -compile-output <spv/dxil>.");
			goto clean;
		}
	}

	switch (magic) {

		//oiCA header

		case CAHeader_MAGIC: {

			CAFile file = (CAFile) { 0 };
			ListCharString strings = { 0 };
			U64 baseCount = 0;

			Bool madeFile = false;
			CharString out = CharString_createNull();

			gotoIfError3(cleanCa, CAFile_readx(buf, encryptionKey, &file, e_rr))

			//Specific entry was requested

			if (args.parameters & EOperationHasParameter_Entry) {

				//Get index of entry

				U64 index = Archive_getIndexx(file.archive, entry);

				if (index == U64_MAX) {

					if (!CharString_parseU64(entry, &index)) {
						Log_errorLnx("Invalid argument -entry <uint> or <valid path> expected.");
						goto cleanCa;
					}

					if (index >= file.archive.entries.length) {
						Log_errorLnx("Index out of bounds, max is %"PRIu64".", file.archive.entries.length);
						goto cleanCa;
					}
				}

				ArchiveEntry e = file.archive.entries.ptr[index];

				//Output it to a folder on disk was requested

				if (args.parameters & EOperationHasParameter_Output) {
					CLI_storeFileOrFolder(args, e, file.archive, &madeFile, start, length);
					goto cleanCa;
				}

				//Want to output to log

				else {

					//Simply print the archive folder

					if (e.type == EFileType_Folder) {

						baseCount = CharString_countAllSensitive(e.path, '/', 0) + 1;

						gotoIfError3(cleanCa, Archive_foreachx(
							file.archive,
							e.path,
							(FileCallback) collectArchiveEntries,
							&strings,
							true,
							EFileType_Any,
							e_rr
						))
					}

					//Print the subsection of the file

					else {

						Bool isAscii = CharString_isValidAscii(
							CharString_createRefSizedConst((const C8*) e.data.ptr, Buffer_length(e.data), false)
						);

						Log_debugLnx("%.*s", CharString_length(e.path), e.path.ptr);
						CLI_showFile(args, e.data, start, length, isAscii, false);
						goto cleanCa;
					}

				}
			}

			//General info was requested

			else {

				if (args.parameters & EOperationHasParameter_Output) {

					ArchiveEntry e = (ArchiveEntry) {
						.type = EFileType_Folder,
						.path = CharString_createRefCStrConst(".")
					};

					CLI_storeFileOrFolder(args, e, file.archive, &madeFile, start, length);
					goto cleanCa;
				}

				gotoIfError3(cleanCa, Archive_foreachx(
					file.archive,
					CharString_createRefCStrConst("."),
					(FileCallback) collectArchiveEntries,
					&strings,
					true,
					EFileType_Any,
					e_rr
				))
			}

			//Sort to ensure the subdirectories are correct

			if(!ListCharString_sortInsensitive(strings))
				retError(cleanCa, Error_invalidOperation(0, "CLI_inspectData() sort strings (oiCA) failed"))

			//Process all and print

			if(!length && start < strings.length)
				length = U64_min(64, strings.length - start);

			U64 end = start + length;

			Log_debugLnx(
				"Showing offset #%"PRIx64" with count %"PRIu64" in selected folder (File contains %"PRIu64" entries)",
				start, length, file.archive.entries.length
			);

			for(U64 i = start; i < end && i < strings.length; ++i) {

				CharString pathi = strings.ptr[i];
				U64 parentCount = CharString_countAllSensitive(pathi, '/', 0);

				U64 v = Archive_getIndexx(file.archive, pathi);

				//000: self
				//001:   child (indented by 2)

				if(v == U64_MAX)
					retError(cleanCa, Error_notFound(0, 0, "CLI_inspectData() couldn't find archive entry (oiCA)"))

				gotoIfError2(cleanCa, CharString_createDecx(v, 3, &tmp))
 				gotoIfError2(cleanCa, CharString_createx(' ', 2 * (parentCount - baseCount), &tmp1))
				gotoIfError2(cleanCa, CharString_appendx(&tmp, ':'))
				gotoIfError2(cleanCa, CharString_appendx(&tmp, ' '))
				gotoIfError2(cleanCa, CharString_appendStringx(&tmp, tmp1))
				CharString_freex(&tmp1);

				CharString sub = CharString_createNull();
				if(!CharString_cutBeforeLastSensitive(pathi, '/', &sub))
					sub = CharString_createRefSizedConst(pathi.ptr, CharString_length(pathi), false);

				gotoIfError2(cleanCa, CharString_appendStringx(&tmp, sub))

				//Log and free temp

				Log_debugLnx("%.*s", CharString_length(tmp), tmp.ptr);
				CharString_freex(&tmp);
			}

			if(!strings.length)
				Log_debugLnx("Folder is empty.");

		cleanCa:

			if(madeFile)
				File_remove(out, 1 * SECOND, NULL);

			CAFile_freex(&file);
			ListCharString_freeUnderlyingx(&strings);
			CharString_freex(&tmp);

			if(err.genericError)
				goto clean;

			break;
		}

		//oiDL header

		case DLHeader_MAGIC: {

			DLFile file = (DLFile) { 0 };
			gotoIfError3(cleanDl, DLFile_readx(buf, encryptionKey, false, &file, e_rr))

			U64 end = 0;

			if (!(args.parameters & EOperationHasParameter_Entry)) {

				if(!length && start < DLFile_entryCount(file))
					length = U64_min(64, DLFile_entryCount(file) - start);

				end = start + length;
			}

			if (args.parameters & EOperationHasParameter_Entry) {

				//Grab entry

				U64 entryI = 0;

				if (!CharString_parseU64(entry, &entryI)) {
					Log_errorLnx("Invalid argument -entry <uint> expected.");
					goto cleanDl;
				}

				if (entryI >= DLFile_entryCount(file)) {
					Log_errorLnx("Index out of bounds, max is %"PRIu64, DLFile_entryCount(file));
					goto cleanDl;
				}

				Bool isAscii = file.settings.dataType == EDLDataType_Ascii;
				Buffer b =
					isAscii ? CharString_bufferConst(file.entryStrings.ptr[entryI]) :
					file.entryBuffers.ptr[entryI];

				if(!CLI_showFile(args, b, start, length, isAscii, false))
					goto cleanDl;
			}

			else {

				Log_debugLnx("oiDL Entries:");

				for (U64 i = start; i < end && i < DLFile_entryCount(file); ++i) {

					U64 entrySize =
						file.settings.dataType == EDLDataType_Ascii ? CharString_length(file.entryStrings.ptr[i]) :
						Buffer_length(file.entryBuffers.ptr[i]);

					gotoIfError2(cleanDl, CharString_createDecx(i, 3, &tmp))
					gotoIfError2(cleanDl, CharString_appendStringx(&tmp, CharString_createRefCStrConst(": length = ")))

					gotoIfError2(cleanDl, CharString_createDecx(entrySize, 0, &tmp1))
					gotoIfError2(cleanDl, CharString_appendStringx(&tmp, tmp1))

					Log_debugLnx("%s", tmp.ptr);
					CharString_freex(&tmp);
					CharString_freex(&tmp1);
				}
			}

		cleanDl:

			CharString_freex(&tmp);
			CharString_freex(&tmp1);
			DLFile_freex(&file);

			if(err.genericError)
				goto clean;

			break;
		}

		//oiSH header

		case SHHeader_MAGIC: {

			if(encryptionKey)
				retError(clean, Error_invalidState(0, "CLI_inspectData() oiSH doesn't have aes support!"))

			SHFile file = (SHFile) { 0 };
			Compiler comp = (Compiler) { 0 };
			gotoIfError3(cleanSh, SHFile_readx(buf, false, &file, e_rr))

			Bool binaryMode = args.flags & EOperationFlags_Bin;
			Bool includesMode = args.flags & EOperationFlags_Includes;

			if (binaryMode && includesMode) {
				Log_errorLnx("oiSH file data can't use --bin and --includes at the same time");
				goto cleanSh;
			}

			if((args.parameters & EOperationHasParameter_Output) && (
				binaryType == ESHBinaryType_Count ||
				!binaryMode ||
				!(args.parameters & EOperationHasParameter_Entry)
			)) {
				Log_errorLnx("oiSH file data can't use --output if -entry, --bin and -compile-output aren't defined");
				goto cleanSh;
			}

			U64 count = includesMode ? file.includes.length : (binaryMode ? file.binaries.length : file.entries.length);
			U64 end = 0;

			if (!(args.parameters & EOperationHasParameter_Entry)) {

				if(!length && start < count)
					length = U64_min(64, count - start);

				end = start + length;
			}

			if (args.parameters & EOperationHasParameter_Entry) {

				//Grab entry

				U64 entryI = 0;

				if (includesMode) {
					Log_errorLnx("oiSH file data includes mode doesn't support -entry, since all data is already displayed");
					goto cleanSh;
				}

				if (!CharString_parseU64(entry, &entryI)) {
					Log_errorLnx("Invalid argument -entry <uint> expected.");
					goto cleanSh;
				}

				if (entryI >= count) {
					Log_errorLnx("Index out of bounds, max is %"PRIu64, count);
					goto cleanSh;
				}

				if(!binaryMode)
					SHEntry_printx(file.entries.ptr[entryI]);

				else {

					//Compile mode was selected

					if (binaryType != ESHBinaryType_Count) {

						Buffer binary = file.binaries.ptr[entryI].binaries[binaryType];

						if (!Buffer_length(binary)) {
							Log_errorLnx("%s binary is missing at index %"PRIu64, ESHBinaryType_names[binaryType], entryI);
							goto cleanSh;
						}

						//Show as disassembly (DXIL or SPIRV disassembly) unless not available

						#ifdef CLI_SHADER_COMPILER

							gotoIfError3(cleanSh, Compiler_createx(&comp, e_rr))

							if (!Compiler_createDisassemblyx(comp, binaryType, binary, &tmp, e_rr)) {
								Log_errorLnx("%s disassembly failed at index %"PRIu64, ESHBinaryType_names[binaryType], entryI);
								goto cleanSh;
							}

							if(!CLI_showFile(args, CharString_bufferConst(tmp), start, length, true, true))
								goto cleanSh;

						#else
							if(!CLI_showFile(args, binary, start, length, false, false))
								goto cleanSh;
						#endif
					}

					else SHBinaryInfo_printx(file.binaries.ptr[entryI]);
				}
			}

			else {

				if (includesMode) {

					Log_debugLnx("oiSH includes:");

					for (U64 i = start; i < end && i < count; ++i) {

						SHInclude inc = file.includes.ptr[i];

						Log_debugLnx(
							"Include %"PRIu64" (%.*s) with CRC32C %"PRIX32,
							i,
							(int) CharString_length(inc.relativePath),
							inc.relativePath.ptr,
							inc.crc32c
						);
					}
				}

				else if (binaryMode) {

					Log_debugLnx("oiSH binaries:");

					for (U64 i = start; i < end && i < count; ++i) {

						SHBinaryInfo bin = file.binaries.ptr[i];

						if(bin.hasShaderAnnotation)
							Log_debugLnx(
								"Binary %"PRIu64" (lib_%"PRIu8"_%"PRIu8")",
								i, (U8)(bin.identifier.shaderVersion >> 8), (U8)bin.identifier.shaderVersion
							);

						else Log_debugLnx(
							"Binary %"PRIu64" (%s_%"PRIu8"_%"PRIu8": %.*s)",
							i, ESHPipelineStage_getStagePrefix(bin.identifier.stageType),
							(U8)(bin.identifier.shaderVersion >> 8), (U8)bin.identifier.shaderVersion,
							(int) CharString_length(bin.identifier.entrypoint),
							bin.identifier.entrypoint.ptr
						);
					}
				}

				else {

					Log_debugLnx("oiSH entries:");

					for (U64 i = start; i < end && i < count; ++i) {

						SHEntry shEntry = file.entries.ptr[i];
						const C8 *name = SHEntry_stageName(shEntry);

						Log_debugLnx(
							"Entry %"PRIu64" (%s): %.*s", i, name, (int) CharString_length(shEntry.name), shEntry.name.ptr
						);
					}
				}
			}

		cleanSh:

			SHFile_freex(&file);
			Compiler_freex(&comp);

			if(!s_uccess)
				goto clean;

			break;
		}

		//oiSB file

		case SBHeader_MAGIC: {

			if(encryptionKey)
				retError(clean, Error_invalidState(0, "CLI_inspectData() oiSH doesn't have aes support!"))

			SBFile file = (SBFile) { 0 };
			gotoIfError3(cleanSb, SBFile_readx(buf, false, &file, e_rr))

			SBFile_printx(file, 0, U16_MAX, true);		//TODO: parent

		cleanSb:

			SBFile_freex(&file);

			if(!s_uccess)
				goto clean;

			break;
		}

		//Invalid

		default:
			Log_errorLnx("File had unrecognized magic number.");
			goto clean;
	}

	s_uccess = true;

clean:

	if(err.genericError)
		Error_printx(err, ELogLevel_Error, ELogOptions_NewLine);

	CharString_freex(&tmp);
	Buffer_freex(&buf);
	return s_uccess;
}

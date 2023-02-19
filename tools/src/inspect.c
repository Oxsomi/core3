/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
*/

#include "types/buffer.h"
#include "types/error.h"
#include "types/string.h"
#include "formats/oiCA.h"
#include "formats/oiDL.h"
#include "platforms/ext/formatx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/errorx.h"
#include "platforms/file.h"
#include "platforms/log.h"
#include "cli.h"

typedef struct VersionString {
	C8 v[5];		//XX.Y
} VersionString;

VersionString VersionString_get(U8 version) {

	VersionString res = (VersionString) { 0 };

	U8 major = 1 + version / 10;
	U8 minor = version % 10;
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

		switch (type >> 4) {

			case EXXCompressionType_Brotli11:
				Log_debug(String_createConstRefUnsafe("Compression type: Brotli:11."), ELogOptions_NewLine);
				break;

			case EXXCompressionType_Brotli1:
				Log_debug(String_createConstRefUnsafe("Compression type: Brotli:1."), ELogOptions_NewLine);
				break;

			default:
				Log_debug(String_createConstRefUnsafe("Compression type: Unrecognized."), ELogOptions_NewLine);
		}
	}

	if (type & 0xF) {

		switch (type & 0xF) {

			case EXXEncryptionType_AES256GCM:
				Log_debug(String_createConstRefUnsafe("Encryption type: AES256GCM."), ELogOptions_NewLine);
				break;

			default:
				Log_debug(String_createConstRefUnsafe("Encryption type: Unrecognized."), ELogOptions_NewLine);
		}
	}
}

void XXFile_printVersion(U8 v) {

	//Version

	VersionString str = VersionString_get(v);

	Log_debug(String_createConstRefUnsafe("Version: "), ELogOptions_None);
	Log_debug(String_createConstRefAuto(str.v, sizeof(str.v)), ELogOptions_NewLine);
}

//Inspect header is a lightweight checker for the header.
//To get a more detailed view you can use OxC3 file data instead.
//Viewing the header doesn't require a key (if encrypted), but the data does.

Bool CLI_inspectHeader(ParsedArgs args) {

	Buffer buf = Buffer_createNull();
	Error err = Error_none();
	Bool success = false;

	String path = String_createNull();
	String tmp = String_createNull();

	if ((err = ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &path)).genericError) {
		Log_error(String_createConstRefUnsafe("Invalid argument -i <string>."), ELogOptions_NewLine);
		goto clean;
	}

	if ((err = File_read(path, 1 * SECOND, &buf)).genericError) {
		Log_error(String_createConstRefUnsafe("Invalid file path."), ELogOptions_NewLine);
		goto clean;
	}

	if (Buffer_length(buf) < 4) {
		Log_error(String_createConstRefUnsafe("File has to start with magic number."), ELogOptions_NewLine);
		goto clean;
	}

	U64 reqLen = 0;

	switch (*(const U32*)buf.ptr) {
		case CAHeader_MAGIC:	reqLen = sizeof(CAHeader);					break;
		case DLHeader_MAGIC:	reqLen = sizeof(DLHeader) + sizeof(U32);	break;
	}

	if (Buffer_length(buf) < reqLen) {
		Log_error(String_createConstRefUnsafe("File wasn't the right size."), ELogOptions_NewLine);
		goto clean;
	}

	switch (*(const U32*)buf.ptr) {

		//oiCA header

		case CAHeader_MAGIC: {
			
			CAHeader caHeader = *(const CAHeader*)buf.ptr;

			Log_debug(String_createConstRefUnsafe("Detected oiCA file with following info:"), ELogOptions_NewLine);

			XXFile_printVersion(caHeader.version);

			//File sizes

			static const C8 *dataTypes[] = {
				"U8 (8-bit number).",
				"U16 (16-bit number)",
				"U32 (32-bit number)",
				"U64 (64-bit number)"
			};

			Log_debug(String_createConstRefUnsafe("Data size type uses "), ELogOptions_None);

			Log_debug(
				String_createConstRefUnsafe(
					dataTypes[(caHeader.flags >> ECAFlags_FileSizeType_Shift) & ECAFlags_FileSizeType_Mask]
				), 
				ELogOptions_NewLine
			);

			if (caHeader.type >> 4) {

				Log_debug(String_createConstRefUnsafe("Compression size type uses "), ELogOptions_None);

				Log_debug(
					String_createConstRefUnsafe(
						dataTypes[(caHeader.flags >> ECAFlags_CompressedSizeType_Shift) & ECAFlags_CompressedSizeType_Mask]
					), 
					ELogOptions_NewLine
				);
			}

			//File and directory count

			U64 fileCountPtr = reqLen;
			reqLen += caHeader.flags & ECAFlags_FilesCountLong ? sizeof(U32) : sizeof(U16);

			U64 dirCountPtr = reqLen;
			reqLen += caHeader.flags & ECAFlags_DirectoriesCountLong ? sizeof(U16) : sizeof(U8);

			if (Buffer_length(buf) < reqLen) {
				Log_error(String_createConstRefUnsafe("File wasn't the right size."), ELogOptions_NewLine);
				goto clean;
			}

			//Entry count

			U64 fileCount = Buffer_forceReadSizeType(
				buf.ptr + fileCountPtr,
				caHeader.flags & ECAFlags_FilesCountLong ? EXXDataSizeType_U32 : EXXDataSizeType_U16
			);

			U64 dirCount = Buffer_forceReadSizeType(
				buf.ptr + dirCountPtr,
				caHeader.flags & ECAFlags_DirectoriesCountLong ? EXXDataSizeType_U16 : EXXDataSizeType_U8
			);

			_gotoIfError(clean, String_createDecx(dirCount, 0, &tmp));
			Log_debug(String_createConstRefUnsafe("Directory count: "), ELogOptions_None);
			Log_debug(tmp, ELogOptions_NewLine);
			String_freex(&tmp);

			_gotoIfError(clean, String_createDecx(fileCount, 0, &tmp));
			Log_debug(String_createConstRefUnsafe("File count: "), ELogOptions_None);
			Log_debug(tmp, ELogOptions_NewLine);
			String_freex(&tmp);

			//AES chunking

			U32 aesChunking = caHeader.flags & ECSFlags_AESChunkMask;

			if (aesChunking) {

				static const C8 *chunking[] = {
					"10 MiB",
					"100 MiB",
					"500 MiB"
				};

				String aesChunkStr = String_createConstRefUnsafe(chunking[(aesChunking >> ECSFlags_AESChunkShift) - 1]);

				Log_debug(String_createConstRefUnsafe("AES Chunking uses "), ELogOptions_None);
				Log_debug(aesChunkStr, ELogOptions_None);
				Log_debug(String_createConstRefUnsafe("per chunk."), ELogOptions_NewLine);
			}

			//Types

			XXFile_printType(caHeader.type);

			//Extended data

			if (caHeader.flags & ECAFlags_HasExtendedData) {

				U64 oldPtr = reqLen;
				reqLen += sizeof(CAExtraInfo);

				if (Buffer_length(buf) < reqLen) {
					Log_error(String_createConstRefUnsafe("File wasn't the right size."), ELogOptions_NewLine);
					goto clean;
				}

				CAExtraInfo extraInfo = *(const CAExtraInfo*)(buf.ptr + oldPtr);

				_gotoIfError(clean, String_createHexx(extraInfo.extendedMagicNumber, 8, &tmp));
				Log_debug(String_createConstRefUnsafe("Extended magic number: "), ELogOptions_None);
				Log_debug(tmp, ELogOptions_NewLine);
				String_freex(&tmp);

				_gotoIfError(clean, String_createDecx(extraInfo.headerExtensionSize, 0, &tmp));
				Log_debug(String_createConstRefUnsafe("Extended header size: "), ELogOptions_None);
				Log_debug(tmp, ELogOptions_NewLine);
				String_freex(&tmp);

				_gotoIfError(clean, String_createDecx(extraInfo.directoryExtensionSize, 0, &tmp));
				Log_debug(String_createConstRefUnsafe("Extended per directory size: "), ELogOptions_None);
				Log_debug(tmp, ELogOptions_NewLine);
				String_freex(&tmp);

				_gotoIfError(clean, String_createDecx(extraInfo.fileExtensionSize, 0, &tmp));
				Log_debug(String_createConstRefUnsafe("Extended per file size: "), ELogOptions_None);
				Log_debug(tmp, ELogOptions_NewLine);
				String_freex(&tmp);
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

			if(caHeader.flags) {

				Log_debug(String_createConstRefUnsafe("Flags: "), ELogOptions_NewLine);

				for(U64 i = 0; i < sizeof(caHeader.flags) << 3; ++i) {

					const C8 *flag = flags[U64_min(i, sizeof(flags) / sizeof(flags[0]) - 1)];

					if((caHeader.flags >> i) & 1 && flag)
						Log_debug(
							String_createConstRefUnsafe(flag), 
							ELogOptions_NewLine
						);
				}
			}

			break;
		}

		//oiDL header

		case DLHeader_MAGIC: {
		
			DLHeader dlHeader = *(const DLHeader*)(buf.ptr + sizeof(U32));

			Log_debug(String_createConstRefUnsafe("Detected oiDL file with following info:"), ELogOptions_NewLine);

			XXFile_printVersion(dlHeader.version);
			
			//AES chunking

			U32 aesChunking = dlHeader.flags & EDLFlags_AESChunkMask;

			if (aesChunking) {

				static const C8 *chunking[] = {
					"10 MiB",
					"50 MiB",
					"100 MiB"
				};

				String aesChunkStr = String_createConstRefUnsafe(chunking[(aesChunking >> EDLFlags_AESChunkShift) - 1]);

				Log_debug(String_createConstRefUnsafe("AES Chunking uses "), ELogOptions_None);
				Log_debug(aesChunkStr, ELogOptions_None);
				Log_debug(String_createConstRefUnsafe("per chunk."), ELogOptions_NewLine);
			}

			//Types

			XXFile_printType(dlHeader.type);

			//File sizes

			static const C8 *dataTypes[] = {
				"U8 (8-bit number).",
				"U16 (16-bit number)",
				"U32 (32-bit number)",
				"U64 (64-bit number)"
			};

			Log_debug(String_createConstRefUnsafe("Entry count size type uses "), ELogOptions_None);
			Log_debug(String_createConstRefUnsafe(dataTypes[dlHeader.sizeTypes & 3]), ELogOptions_NewLine);

			if (dlHeader.type >> 4) {
				Log_debug(String_createConstRefUnsafe("Compression size type uses "), ELogOptions_None);
				Log_debug(String_createConstRefUnsafe(dataTypes[(dlHeader.sizeTypes >> 2) & 3]), ELogOptions_NewLine);
			}

			Log_debug(String_createConstRefUnsafe("Buffer size type uses "), ELogOptions_None);
			Log_debug(String_createConstRefUnsafe(dataTypes[(dlHeader.sizeTypes >> 4) & 3]), ELogOptions_NewLine);

			reqLen += 1 << (dlHeader.sizeTypes & 3);

			if (Buffer_length(buf) < reqLen) {
				Log_error(String_createConstRefUnsafe("File wasn't the right size."), ELogOptions_NewLine);
				goto clean;
			}

			//Entry count

			U64 entryCount = Buffer_forceReadSizeType(
				buf.ptr + sizeof(U32) + sizeof(DLHeader),
				(EXXDataSizeType)(dlHeader.sizeTypes & 3)
			);

			_gotoIfError(clean, String_createDecx(entryCount, 0, &tmp));
			Log_debug(String_createConstRefUnsafe("Entry count: "), ELogOptions_None);
			Log_debug(tmp, ELogOptions_NewLine);
			String_freex(&tmp);

			//Extended data

			if (dlHeader.flags & EDLFlags_HasExtendedData) {

				U64 oldPtr = reqLen;
				reqLen += sizeof(DLExtraInfo);

				if (Buffer_length(buf) < reqLen) {
					Log_error(String_createConstRefUnsafe("File wasn't the right size."), ELogOptions_NewLine);
					goto clean;
				}

				DLExtraInfo extraInfo = *(const DLExtraInfo*)(buf.ptr + oldPtr);

				_gotoIfError(clean, String_createHexx(extraInfo.extendedMagicNumber, 8, &tmp));
				Log_debug(String_createConstRefUnsafe("Extended magic number: "), ELogOptions_None);
				Log_debug(tmp, ELogOptions_NewLine);
				String_freex(&tmp);

				_gotoIfError(clean, String_createDecx(extraInfo.extendedHeader, 0, &tmp));
				Log_debug(String_createConstRefUnsafe("Extended header size: "), ELogOptions_None);
				Log_debug(tmp, ELogOptions_NewLine);
				String_freex(&tmp);

				_gotoIfError(clean, String_createDecx(extraInfo.perEntryExtendedData, 0, &tmp));
				Log_debug(String_createConstRefUnsafe("Extended per entry size: "), ELogOptions_None);
				Log_debug(tmp, ELogOptions_NewLine);
				String_freex(&tmp);
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

			if(dlHeader.flags) {

				Log_debug(String_createConstRefUnsafe("Flags: "), ELogOptions_NewLine);

				for (U64 i = 0; i < sizeof(dlHeader.flags) << 3; ++i) {

					const C8 *flag = flags[U64_min(i, sizeof(flags) / sizeof(flags[0]) - 1)];

					if((dlHeader.flags >> i) & 1 && flag)
						Log_debug(
							String_createConstRefUnsafe(flag), 
							ELogOptions_NewLine
						);
				}
			}

			break;
		}

		default:
			Log_error(String_createConstRefUnsafe("File had unrecognized magic number"), ELogOptions_NewLine);
			goto clean;
	}

	success = true;

clean:

	if(err.genericError)
		Error_printx(err, ELogLevel_Error, ELogOptions_NewLine);

	String_freex(&tmp);
	Buffer_freex(&buf);
	return success;
}

Bool CLI_inspectData(ParsedArgs args) {
	return false;		//TODO:
}

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

//formats/oiCA/ca_write.c

#include "formats/oiCA/ca_file.h"
#include "formats/oiCA/ca_headers.h"
#include "formats/oiDL/dl_file.h"
#include "types/container/stream.h"
#include "types/container/container_types.h"
#include "types/container/buffer_encrypt.h"
#include "types/math/vec4i.h"
#include "types/base/time.h"
#include "types/base/mathi.h"

Bool CAFile_storeDate(Ns ns, U16 *time, U16 *date) {

	if (!time || !date)
		return false;

	Date d = (Date) { 0 };
	const Bool b = Time_getDate(ns, &d, false);

	if (!b || d.year < 1980 || d.year >(1980 + 0x7F))
		return false;

	*time = (d.second >> 1) | ((U16) d.minute << 5) | ((U16) d.hour << 11);
	*date = d.day | ((U16) d.month << 5) | ((U16) (d.year - 1980) << 9);
	return true;
}

Bool CAFile_write(
	const CAFile *caFile,
	const RefPtrType *encStreamType,
	StreamRef *result,
	U64 *startOffset,
	const Allocator *alloc,
	Error *e_rr
) {
	Bool s_uccess = true;
	StreamCursor cursor = (StreamCursor) { 0 };
	Buffer tmp = Buffer_createNull();
	I32x4 iv = I32x4_zero();
	U32 headerKey[8] = { 0 };
	U32 masterKey[8] = { 0 };
	Bool generatedKey = false;
	U64 offset = 0;
	DLFile names = { 0 }, content = { 0 };

	if (!caFile || !caFile->folders.ptr || !startOffset)
		retError(clean, Error_nullPointer(
			!result ? 2 : (!startOffset ? 3 : 0), "CAFile_write()::caFile and startOffset are required"
		));

	if (result && result->refPtrType->typeId != (TypeId)EContainerTypeId_Stream)
		retError(clean, Error_nullPointer(2, "CAFile_write()::result must be a valid StreamRef"));

	const CASettings *settings = &caFile->settings;

	names = caFile->names;
	content = caFile->content;

	if (settings->compressionType >= EXXCompressionType_Count)
		retError(clean, Error_invalidParameter(0, 0, "CAFile_write()::compressionType is invalid"));

	if (settings->compressionType != EXXCompressionType_None)
		retError(clean, Error_unsupportedOperation(0, "CAFile_write()::compressionType not supported yet"));    //TODO:

	if (settings->encryptionType >= EXXEncryptionType_Count)
		retError(clean, Error_invalidParameter(0, 1, "CAFile_write()::encryptionType is invalid"));

	if (*startOffset & 15)
		retError(clean, Error_unsupportedOperation(0, "CAFile_write() at misaligned startOffset is unsupported (16-byte)"));

	//Publish the final offset only after the complete archive has been written.

	offset = *startOffset;

	OxStream *stream = RefPtr_data(result, OxStream);

	U64 dirCount  = caFile->folders.length - 1;        //-1 for root
	U64 fileCount = caFile->files.length;

	if (dirCount >= U16_MAX)
		retError(clean, Error_outOfBounds(0, dirCount,  U16_MAX, "CAFile_write()::too many directories"));

	if (fileCount >= U32_MAX)
		retError(clean, Error_outOfBounds(0, fileCount, U32_MAX, "CAFile_write()::too many files"));

	Bool dirCountLong  = dirCount  > 254;
	Bool fileCountLong = fileCount > 65534;

	U64 dirRefSize  = dirCountLong  ? 2 : 1;
	U64 fileRefSize = fileCountLong ? 4 : 2;

	Bool hasDate         = settings->flags & ECASettingsFlags_IncludeDate;
	Bool hasExtendedDate = settings->flags & ECASettingsFlags_IncludeFullDate;

	if (hasExtendedDate)
		hasDate = true;

	U64 fileObjDateSize = !hasDate ? 0 : (hasExtendedDate ? sizeof(Ns) : sizeof(U16) * 2);
	U64 fileObjSize     = dirRefSize + fileObjDateSize;

	U16 flags =
		(hasDate         ? ECAFlags_FilesHaveDate         : ECAFlags_None) |
		(hasExtendedDate ? ECAFlags_FilesHaveExtendedDate : ECAFlags_None) |
		(dirCountLong    ? ECAFlags_DirectoriesCountLong  : ECAFlags_None) |
		(fileCountLong   ? ECAFlags_FilesCountLong        : ECAFlags_None);

	Bool isEncrypted = settings->encryptionType != EXXEncryptionType_None;

	//Fixed header region: CAHeader + fileCount + dirCount + directories[] + files[]

	U64 headerSize =
		sizeof(CAHeader) +
		fileRefSize +
		dirRefSize +
		dirCount  * dirRefSize +
		fileCount * fileObjSize;

	if (isEncrypted)
		headerSize += sizeof(I32x4) + 12;        //iv (12) + tag (16)

	//Align to 16 bytes before DLFiles

	headerSize = (headerSize + 15) & ~(U64) 15;

	//The cursor only has to hold the fixed header, the DLFiles that follow are written by DLFile_write.
	U64 cursorSize = U64_max(headerSize, 32 * KIBI);

	gotoIfError3(clean, DLFile_write(&names, alloc, NULL, encStreamType, I32x4_zero(), &headerSize, e_rr));

	headerSize = (headerSize + 15) & ~(U64) 15;
	gotoIfError3(clean, DLFile_write(&content, alloc, NULL, encStreamType, I32x4_zero(), &headerSize, e_rr));

	if (!result) {
		offset += headerSize;
		goto clean;
	}

	if (stream->reserve)
		gotoIfError3(clean, stream->reserve(stream, offset + headerSize, alloc, e_rr));

	gotoIfError3(clean, StreamCursor_create(result, cursorSize, true, alloc, &cursor, e_rr));

	U64 start = offset;

	//Header

	CAHeader header = (CAHeader) {
		.magicNumber = CAHeader_MAGIC,
		.version     = (U8) ECAVersion_V1_0,
		.type        = (U8) settings->encryptionType,
		.flags       = flags
	};

	gotoIfError3(clean, StreamCursor_append(&cursor, &offset, &header, sizeof(CAHeader), alloc, e_rr));

	//fileCount and dirCount

	if (fileCountLong) {
		gotoIfError3(clean, StreamCursor_appendU32(&cursor, &offset, (U32) fileCount, alloc, e_rr));
	}

	else gotoIfError3(clean, StreamCursor_appendU16(&cursor, &offset, (U16) fileCount, alloc, e_rr));

	if (dirCountLong) {
		gotoIfError3(clean, StreamCursor_appendU16(&cursor, &offset, (U16) dirCount, alloc, e_rr));
	}

	else gotoIfError3(clean, StreamCursor_appendU8 (&cursor, &offset, (U8) dirCount, alloc, e_rr));

	//directories[]
	//Folder index 0 is runtime root (no on-disk representation).
	//On disk: root-parented = 0xFF/0xFFFF sentinel, else (parent - 1).

	for (U64 i = 1; i < caFile->folders.length; ++i) {

		const CAFolderInfo *fi = &caFile->folders.ptr[i];

		U16 parentDisk = fi->parent == 0 ? (dirCountLong ? (U16) 0xFFFF : (U16) 0xFF) : (U16) (fi->parent - 1);

		if (dirCountLong) {
			gotoIfError3(clean, StreamCursor_appendU16(&cursor, &offset, parentDisk, alloc, e_rr));
		}

		else gotoIfError3(clean, StreamCursor_appendU8 (&cursor, &offset, (U8) parentDisk, alloc, e_rr));
	}

	//files[]

	for (U64 i = 0; i < caFile->files.length; ++i) {

		const CAFileInfo *fi = &caFile->files.ptr[i];
		U16 parent    = CAFileInfo_getParent(*fi);
		Ns  timestamp = CAFileInfo_getTimestamp(*fi);

		U16 parentDisk = parent == 0 ? (dirCountLong ? (U16) 0xFFFF : (U16) 0xFF) : (U16) (parent - 1);

		if (dirCountLong) {
			gotoIfError3(clean, StreamCursor_appendU16(&cursor, &offset, parentDisk, alloc, e_rr));
		}

		else gotoIfError3(clean, StreamCursor_appendU8 (&cursor, &offset, (U8) parentDisk, alloc, e_rr));

		if (hasDate) {

			if (hasExtendedDate) {
				gotoIfError3(clean, StreamCursor_appendU64(&cursor, &offset, timestamp, alloc, e_rr));
			}

			else {
				U16 date = 0, time = 0;

				if (!CAFile_storeDate(timestamp, &time, &date))
					retError(clean, Error_invalidState(0,
						"CAFile_write()::couldn't store file date, please use full date"
					));

				gotoIfError3(clean, StreamCursor_appendU16(&cursor, &offset, date, alloc, e_rr));
				gotoIfError3(clean, StreamCursor_appendU16(&cursor, &offset, time, alloc, e_rr));
			}
		}
	}

	//Encryption header (iv + tag over the fixed header region)

	if (isEncrypted) {

		//Read back the header region for AAD, same fast/slow path as DLFile_write

		if (StreamCursor_contains(&cursor, start, offset - start))
			tmp = Buffer_createRefConst(
				cursor.cacheData.ptr + (start - cursor.lastLocation),
				offset - start
			);

		else {

			gotoIfError3(clean, Buffer_createUninitializedBytes(offset - start, alloc, &tmp, e_rr));

			gotoIfError3(clean, StreamCursor_setReadOnly(&cursor, alloc, e_rr));

			U64 where = start;
			gotoIfError3(clean, StreamCursor_consume(
				&cursor, &where, tmp.ptrNonConst, offset - start, alloc, e_rr
			));

			gotoIfError3(clean, StreamCursor_setWritable(&cursor, e_rr));
		}

		const U32 zeroKey[8] = { 0 };
		const Bool hasKey = Buffer_neq(
			Buffer_createRefConst(settings->encryptionKey, sizeof(zeroKey)),
			Buffer_createRefConst(zeroKey, sizeof(zeroKey))
		);

		I32x4 tag = I32x4_zero();

		//A generated master key lives in masterKey until the whole write succeeded, only then is it handed to the caller.
		//A failed write therefore leaves the settings untouched and a retry generates a fresh key.

		if (hasKey)
			Buffer_memcpy(Buffer_createRef(masterKey, 32), Buffer_createRefConst(settings->encryptionKey, 32));

		else {

			if (!Buffer_csprng(Buffer_createRef(masterKey, sizeof(masterKey))))
				retError(clean, Error_invalidState(0, "CAFile_write()::key generation failed"));

			generatedKey = true;
		}

		Buffer_memcpy(Buffer_createRef(names.settings.encryptionKey, 32), Buffer_createRefConst(masterKey, 32));
		Buffer_memcpy(Buffer_createRef(content.settings.encryptionKey, 32), Buffer_createRefConst(masterKey, 32));

		if (!Buffer_csprng(Buffer_createRef(&iv, 12)))
			retError(clean, Error_invalidState(0, "CAFile_write()::root IV generation failed"));

		gotoIfError3(clean, Buffer_deriveEncryptionKey(
			masterKey, iv, EEncryptionKeyDomain_CAHeader, headerKey, e_rr
		));

		BufferEncrypt encrypt = (BufferEncrypt) {
			.target = NULL,
			.additionalData = &tmp,
			.type = EBufferEncryptionType_AES256GCM,
			.flags = EBufferEncryptionFlags_StopCreateIv,
			.nonConstEncrypt = { .key = headerKey, .tag = &tag, .iv = &iv }
		};
		gotoIfError3(clean, Buffer_encryptAdvanced(&encrypt, e_rr));

		Buffer_free(&tmp, alloc);

		gotoIfError3(clean, StreamCursor_append(&cursor, &offset, &iv,  3 * sizeof(U32),  alloc, e_rr));
		gotoIfError3(clean, StreamCursor_append(&cursor, &offset, &tag, sizeof(I32x4),    alloc, e_rr));

		tag = I32x4_zero();
	}

	//Pad to 16-byte alignment

	{
		U8 pad[16] = { 0 };
		U64 utilized = offset & 15;

		if (utilized)
			gotoIfError3(clean, StreamCursor_append(&cursor, &offset, pad, 16 - utilized, alloc, e_rr));
	}

	//Flush cursor before handing off to DLFile_write so it sees the header bytes

	gotoIfError3(clean, StreamCursor_closeAndKeepCache(&cursor, alloc, &tmp, e_rr));

	//DLFile names

	I32x4 nameIv = I32x4_xor(iv, I32x4_createFromU64x2(0, 1));

	gotoIfError3(clean, DLFile_write(&names, alloc, result, encStreamType, nameIv, &offset, e_rr));

	//Pad to 16-byte alignment

	{

		U64 utilized = offset & 15;

		if (utilized) {
			U8 pad[16] = { 0 };
			gotoIfError3(clean, StreamCursor_createWithCache(result, &tmp, true, &cursor, e_rr));
			gotoIfError3(clean, StreamCursor_append(&cursor, &offset, pad, 16 - utilized, alloc, e_rr));
			gotoIfError3(clean, StreamCursor_flush(&cursor, alloc, e_rr));
			StreamCursor_close(&cursor, alloc);
		}
	}

	//DLFile content

	I32x4 contentIv = I32x4_xor(iv, I32x4_createFromU64x2(0, 2));

	gotoIfError3(clean, DLFile_write(&content, alloc, result, encStreamType, contentIv, &offset, e_rr));

	if (generatedKey)
		Buffer_memcpy(
			Buffer_createRef((U32*) settings->encryptionKey, 32), Buffer_createRefConst(masterKey, sizeof(masterKey))
		);

clean:
	if (s_uccess)
		*startOffset = offset;

	Buffer_clearAllSecure(Buffer_createRef(headerKey, sizeof(headerKey)));
	Buffer_clearAllSecure(Buffer_createRef(masterKey, sizeof(masterKey)));
	Buffer_clearAllSecure(Buffer_createRef(names.settings.encryptionKey, 32));
	Buffer_clearAllSecure(Buffer_createRef(content.settings.encryptionKey, 32));
	iv = I32x4_zero();
	StreamCursor_close(&cursor, alloc);
	Buffer_free(&tmp, alloc);
	return s_uccess;
}

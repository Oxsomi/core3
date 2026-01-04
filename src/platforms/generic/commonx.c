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

#include "platforms/ext/listx_impl.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/formatx.h"
#include "platforms/ext/threadx.h"
#include "platforms/ext/ref_ptrx.h"
#include "formats/bmp/bmp.h"
#include "formats/dds/dds.h"
#include "formats/oiCA/ca_file.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiSH/sh_file.h"
#include "formats/oiSB/sb_file.h"
#include "types/container/buffer.h"
#include "types/container/allocation_buffer.h"
#include "types/base/error.h"
#include "types/container/archive.h"
#include "types/container/big_int.h"
#include "types/container/cdf_list.h"
#include "platforms/platform.h"

TListXImpl(CharString);
TListXBaseImpl(ListConstC8);

TListXImpl(U64);
TListXImpl(U8);		TListXImpl(U16);	TListXImpl(U32);
TListXImpl(I8);		TListXImpl(I16);	TListXImpl(I32); TListXImpl(I64);
TListXImpl(F32);	TListXImpl(F64);

TListXImpl(ListU8);
TListXImpl(ListU16);
TListXImpl(ListU32);
TListXImpl(ListU64);

TListXImpl(Buffer);

TListXImpl(CdfValue);
TListXImpl(ArchiveEntry);

TListXImpl(AllocationBufferBlock);

TListXBaseImpl(ListRefPtr);
TListXBaseImpl(ListWeakRefPtr);

//DDS

TListXImpl(SubResourceData);

//Contains small helper functions that don't require their own .c file

//RefPtr

Error RefPtr_createx(U32 objectLength, ObjectFreeFunc free, ETypeId type, RefPtr **result) {
	return RefPtr_create(objectLength, Platform_instance->alloc, free, type, result);
}

//Buffer

Error Buffer_createCopyx(Buffer buf, Buffer *output) {
	return Buffer_createCopy(buf, Platform_instance->alloc, output);
}

Error Buffer_createZeroBitsx(U64 length, Buffer *output) {
	return Buffer_createZeroBits(length, Platform_instance->alloc, output);
}

Error Buffer_createOneBitsx(U64 length, Buffer *output) {
	return Buffer_createOneBits(length, Platform_instance->alloc, output);
}

Error Buffer_createBitsx(U64 length, Bool value, Buffer *result) {
	return value ? Buffer_createOneBitsx(length, result) : Buffer_createZeroBitsx(length, result);
}

Bool Buffer_freex(Buffer *buf) { return Buffer_free(buf, Platform_instance->alloc); }

Error Buffer_createEmptyBytesx(U64 length, Buffer *output) {
	return Buffer_createEmptyBytes(length, Platform_instance->alloc, output);
}

Error Buffer_createUninitializedBytesx(U64 length, Buffer *output) {
	return Buffer_createUninitializedBytes(length, Platform_instance->alloc, output);
}

Bool Buffer_resizex(Buffer *buf, U64 newLen, Bool preserveContents, Bool clearUnsetContents, Error *e_rr) {
	return Buffer_resize(buf, newLen, preserveContents, clearUnsetContents, Platform_instance->alloc, e_rr);
}

Error AllocationBuffer_createx(U64 size, Bool isVirtual, U64 nonLinearAlignment, AllocationBuffer *allocationBuffer) {
	return AllocationBuffer_create(size, isVirtual, nonLinearAlignment, Platform_instance->alloc, allocationBuffer);
}

Bool AllocationBuffer_freex(AllocationBuffer *allocationBuffer) {
	return AllocationBuffer_free(allocationBuffer, Platform_instance->alloc);
}

Error AllocationBuffer_createRefFromRegionx(
	Buffer origin,
	U64 offset,
	U64 size,
	U64 nonLinearAlignment,
	AllocationBuffer *allocationBuffer
) {
	return AllocationBuffer_createRefFromRegion(
		origin,
		offset,
		size,
		nonLinearAlignment,
		Platform_instance->alloc,
		allocationBuffer
	);
}

Error AllocationBuffer_allocateBlockx(
	AllocationBuffer *allocationBuffer,
	U64 size,
	U64 alignment,
	Bool isNonLinearResource,
	const U8 **result
) {
	return AllocationBuffer_allocateBlock(
		allocationBuffer,
		size,
		alignment,
		isNonLinearResource,
		Platform_instance->alloc,
		result
	);
}

Error AllocationBuffer_allocateAndFillBlockx(
	AllocationBuffer *allocationBuffer,
	Buffer data,
	U64 alignment,
	Bool isNonLinearResource,
	U8 **result
) {
	return AllocationBuffer_allocateAndFillBlock(
		allocationBuffer,
		data,
		alignment,
		isNonLinearResource,
		Platform_instance->alloc,
		result
	);
}

//oiCA

Bool CAFile_freex(CAFile *caFile) { return CAFile_free(caFile, Platform_instance->alloc); }

Bool CAFile_writex(CAFile caFile, Buffer *result, Error *e_rr) {
	return CAFile_write(caFile, Platform_instance->alloc, result, e_rr);
}

Bool CAFile_readx(Buffer file, const U32 encryptionKey[8], CAFile *caFile, Error *e_rr) {
	return CAFile_read(file, encryptionKey, Platform_instance->alloc, caFile, e_rr);
}

Bool CAFile_combinex(CAFile a, CAFile b, CAFile *combined, Error *e_rr) {
	return CAFile_combine(a, b, Platform_instance->alloc, combined, e_rr);
}

//oiDL

Bool DLFile_createx(DLSettings settings, DLFile *dlFile, Error *e_rr) {
	return DLFile_create(settings, Platform_instance->alloc, dlFile, e_rr);
}

Bool DLFile_freex(DLFile *dlFile) { return DLFile_free(dlFile, Platform_instance->alloc); }

Bool DLFile_createListx(DLSettings settings, ListBuffer *buffers, DLFile *dlFile, Error *e_rr) {
	return DLFile_createList(settings, buffers, Platform_instance->alloc, dlFile, e_rr);
}

Bool DLFile_createUTF8Listx(DLSettings settings, ListBuffer buffers, DLFile *dlFile, Error *e_rr) {
	return DLFile_createUTF8List(settings, buffers, Platform_instance->alloc, dlFile, e_rr);
}

Bool DLFile_createBufferListx(DLSettings settings, ListBuffer buffers, DLFile *dlFile, Error *e_rr) {
	return DLFile_createBufferList(settings, buffers, Platform_instance->alloc, dlFile, e_rr);
}

Bool DLFile_createAsciiListx(DLSettings settings, ListCharString strings, DLFile *dlFile, Error *e_rr) {
	return DLFile_createAsciiList(settings, strings, Platform_instance->alloc, dlFile, e_rr);
}

Bool DLFile_addEntryx(DLFile *dlFile, Buffer entry, Error *e_rr) {
	return DLFile_addEntry(dlFile, entry, Platform_instance->alloc, e_rr);
}

Bool DLFile_addEntryAsciix(DLFile *dlFile, CharString entry, Error *e_rr) {
	return DLFile_addEntryAscii(dlFile, entry, Platform_instance->alloc, e_rr);
}

Bool DLFile_addEntryUTF8x(DLFile *dlFile, Buffer entry, Error *e_rr) {
	return DLFile_addEntryUTF8(dlFile, entry, Platform_instance->alloc, e_rr);
}

Bool DLFile_writex(DLFile dlFile, Buffer *result, Error *e_rr) {
	return DLFile_write(dlFile, Platform_instance->alloc, result, e_rr);
}

Bool DLFile_readx(Buffer file, const U32 encryptionKey[8], Bool allowLeftOverData, DLFile *dlFile, Error *e_rr) {
	return DLFile_read(file, encryptionKey, allowLeftOverData, Platform_instance->alloc, dlFile, e_rr);
}

Bool DLFile_combinex(DLFile a, DLFile b, DLFile *combined, Error *e_rr) {
	return DLFile_combine(a, b, Platform_instance->alloc, combined, e_rr);
}

//DDS

Error DDS_readx(Buffer buf, DDSInfo *info, ListSubResourceData *result) {
	return DDS_read(buf, info, Platform_instance->alloc, result);
}

Error DDS_writex(ListSubResourceData buf, DDSInfo info, Buffer *result) {
	return DDS_write(buf, info, Platform_instance->alloc, result);
}

Bool ListSubResourceData_freeAllx(ListSubResourceData *buf) {
	return ListSubResourceData_freeAll(buf, Platform_instance->alloc);
}

//BigInt

Error BigInt_createx(U16 bitCount, BigInt *big) { return BigInt_create(bitCount, Platform_instance->alloc, big); }
Error BigInt_createCopyx(BigInt *a, BigInt *b) { return BigInt_createCopy(a, Platform_instance->alloc, b); }

Bool BigInt_freex(BigInt *a) { return BigInt_free(a, Platform_instance->alloc); }

Error BigInt_createFromHexx(CharString text, U16 bitCount, BigInt *big) {
	return BigInt_createFromHex(text, bitCount, Platform_instance->alloc, big);
}

Error BigInt_createFromDecx(CharString text, U16 bitCount, BigInt *big) {
	return BigInt_createFromDec(text, bitCount, Platform_instance->alloc, big);
}

Error BigInt_createFromOctx(CharString text, U16 bitCount, BigInt *big) {
	return BigInt_createFromOct(text, bitCount, Platform_instance->alloc, big);
}

Error BigInt_createFromBinx(CharString text, U16 bitCount, BigInt *big) {
	return BigInt_createFromBin(text, bitCount, Platform_instance->alloc, big);
}

Error BigInt_createFromNytox(CharString text, U16 bitCount, BigInt *big) {
	return BigInt_createFromNyto(text, bitCount, Platform_instance->alloc, big);
}

Error BigInt_createFromStringx(CharString text, U16 bitCount, BigInt *big) {
	return BigInt_createFromString(text, bitCount, Platform_instance->alloc, big);
}

Bool BigInt_mulx(BigInt *a, BigInt b) { return BigInt_mul(a, b, Platform_instance->alloc); }

Error BigInt_resizex(BigInt *a, U8 newSize) { return BigInt_resize(a, newSize, Platform_instance->alloc); }
Bool BigInt_setx(BigInt *a, BigInt b, Bool allowResize) { return BigInt_set(a, b, allowResize, Platform_instance->alloc); }

Bool BigInt_truncx(BigInt *big) { return BigInt_trunc(big, Platform_instance->alloc); }

U128 U128_createFromDecx(CharString text, Error *failed) { return U128_createFromDec(text, failed, Platform_instance->alloc); }

U128 U128_createFromStringx(CharString text, Error *failed) {
	return U128_createFromString(text, failed, Platform_instance->alloc);
}

Error BigInt_hexx(BigInt b, CharString *result, Bool leadingZeros) {
	return BigInt_hex(b, Platform_instance->alloc, result, leadingZeros);
}

Error BigInt_octx(BigInt b, CharString *result, Bool leadingZeros) {
	return BigInt_oct(b, Platform_instance->alloc, result, leadingZeros);
}

Error BigInt_binx(BigInt b, CharString *result, Bool leadingZeros) {
	return BigInt_bin(b, Platform_instance->alloc, result, leadingZeros);
}

Error BigInt_nytox(BigInt b, CharString *result, Bool leadingZeros) {
	return BigInt_nyto(b, Platform_instance->alloc, result, leadingZeros);
}

Error BigInt_toStringx(BigInt b, CharString *result, EIntegerEncoding encoding, Bool leadingZeros) {
	return BigInt_toString(b, Platform_instance->alloc, result, encoding, leadingZeros);
}

//Error U128_decx(U128 b, CharString *result, Bool leadingZeros);

Error U128_hexx(U128 b, CharString *result, Bool leadingZeros) {
	return U128_hex(b, Platform_instance->alloc, result, leadingZeros);
}

Error U128_octx(U128 b, CharString *result, Bool leadingZeros) {
	return U128_oct(b, Platform_instance->alloc, result, leadingZeros);
}

Error U128_binx(U128 b, CharString *result, Bool leadingZeros) {
	return U128_bin(b, Platform_instance->alloc, result, leadingZeros);
}

Error U128_nytox(U128 b, CharString *result, Bool leadingZeros) {
	return U128_nyto(b, Platform_instance->alloc, result, leadingZeros);
}

Error U128_toStringx(U128 b, CharString *result, EIntegerEncoding encoding, Bool leadingZeros) {
	return U128_toString(b, Platform_instance->alloc, result, encoding, leadingZeros);
}

//Error BigInt_decx(BigInt b, CharString *result, Bool leadingZeros);

//Archive

Bool Archive_createx(Archive *archive, Error *e_rr) { return Archive_create(Platform_instance->alloc, archive, e_rr); }
Bool Archive_freex(Archive *archive) { return Archive_free(archive, Platform_instance->alloc); }

Bool Archive_createCopyx(Archive a, Archive *archive, Error *e_rr) {
	return Archive_createCopy(a, Platform_instance->alloc, archive, e_rr);
}

Bool Archive_combinex(Archive a, Archive b, ArchiveCombineSettings combineSettings, Archive *archive, Error *e_rr) {
	return Archive_combine(a, b, combineSettings, Platform_instance->alloc, archive, e_rr);
}

Bool Archive_hasFilex(Archive archive, CharString path) { return Archive_hasFile(archive, path, Platform_instance->alloc); }
Bool Archive_hasFolderx(Archive archive, CharString path) { return Archive_hasFolder(archive, path, Platform_instance->alloc); }
Bool Archive_hasx(Archive archive, CharString path) { return Archive_has(archive, path, Platform_instance->alloc); }

Bool Archive_addDirectoryx(Archive *archive, CharString path, Error *e_rr) {
	return Archive_addDirectory(archive, path, Platform_instance->alloc, e_rr);
}

Bool Archive_addFilex(Archive *archive, CharString path, Buffer *data, Ns timestamp, Error *e_rr) {
	return Archive_addFile(archive, path, data, timestamp, Platform_instance->alloc, e_rr);
}

Bool Archive_updateFileDatax(Archive *archive, CharString path, Buffer data, Error *e_rr) {
	return Archive_updateFileData(archive, path, data, Platform_instance->alloc, e_rr);
}

Bool Archive_getFileDatax(Archive archive, CharString path, Buffer *data, Error *e_rr) {
	return Archive_getFileData(archive, path, data, Platform_instance->alloc, e_rr);
}

Bool Archive_getFileDataConstx(Archive archive, CharString path, Buffer *data, Error *e_rr) {
	return Archive_getFileDataConst(archive, path, data, Platform_instance->alloc, e_rr);
}

Bool Archive_removeFilex(Archive *archive, CharString path, Error *e_rr) {
	return Archive_removeFile(archive, path, Platform_instance->alloc, e_rr);
}

Bool Archive_removeFolderx(Archive *archive, CharString path, Error *e_rr) {
	return Archive_removeFolder(archive, path, Platform_instance->alloc, e_rr);
}

Bool Archive_removex(Archive *archive, CharString path, Error *e_rr) {
	return Archive_remove(archive, path, Platform_instance->alloc, e_rr);
}

Bool Archive_renamex(Archive *archive, CharString loc, CharString newFileName, Error *e_rr) {
	return Archive_rename(archive, loc, newFileName, Platform_instance->alloc, e_rr);
}

U64 Archive_getIndexx(Archive archive, CharString loc) {
	return Archive_getIndex(archive, loc, Platform_instance->alloc);
}

Bool Archive_movex(Archive *archive, CharString loc, CharString directoryName, Error *e_rr) {
	return Archive_move(archive, loc, directoryName, Platform_instance->alloc, e_rr);
}

Bool Archive_getInfox(Archive archive, CharString loc, FileInfo *info, Error *e_rr) {
	return Archive_getInfo(archive, loc, info, Platform_instance->alloc, e_rr);
}

Bool Archive_queryFileEntryCountx(Archive archive, CharString loc, Bool isRecursive, U64 *res, Error *e_rr) {
	return Archive_queryFileEntryCount(archive, loc, isRecursive, res, Platform_instance->alloc, e_rr);
}

Bool Archive_queryFileCountx(Archive archive, CharString loc, Bool isRecursive, U64 *res, Error *e_rr) {
	return Archive_queryFileCount(archive, loc, isRecursive, res, Platform_instance->alloc, e_rr);
}

Bool Archive_queryFolderCountx(Archive archive, CharString loc, Bool isRecursive, U64 *res, Error *e_rr) {
	return Archive_queryFolderCount(archive, loc, isRecursive, res, Platform_instance->alloc, e_rr);
}

Bool Archive_foreachx(
	Archive archive,
	CharString loc,
	FileCallback callback,
	void *userData,
	Bool isRecursive,
	EFileType type,
	Error *e_rr
) {
	return Archive_foreach(archive, loc, callback, userData, isRecursive, type, Platform_instance->alloc, e_rr);
}

//Thread

Error Thread_createx(ThreadCallbackFunction callback, void *objectHandle, Thread **thread) {
	return Thread_create(&Platform_instance->alloc, callback, objectHandle, thread);
}

Bool Thread_freex(Thread **thread) {
	return Thread_free(&Platform_instance->alloc, thread);
}

Error Thread_waitAndCleanupx(Thread **thread) {
	return Thread_waitAndCleanup(&Platform_instance->alloc, thread);
}

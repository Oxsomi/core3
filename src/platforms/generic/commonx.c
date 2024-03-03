/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "platforms/ext/bmpx.h"
#include "platforms/ext/ddsx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/formatx.h"
#include "platforms/ext/threadx.h"
#include "formats/bmp.h"
#include "formats/dds.h"
#include "formats/oiCA.h"
#include "formats/oiDL.h"
#include "types/buffer.h"
#include "types/allocation_buffer.h"
#include "types/error.h"
#include "types/archive.h"
#include "types/big_int.h"
#include "types/cdf_list.h"
#include "types/buffer_layout.h"
#include "platforms/platform.h"

TListXImpl(CharString);
TListXBaseImpl(ListConstC8);

TListXImpl(U64);
TListXImpl(U8);		TListXImpl(U16);	TListXImpl(U32);
TListXImpl(I8);		TListXImpl(I16);	TListXImpl(I32); TListXImpl(I64);
TListXImpl(F32);	TListXImpl(F64);

TListXImpl(Buffer);

TListXImpl(CDFValue);
TListXImpl(ArchiveEntry);

TListXImpl(BufferLayoutMemberInfo);
TListXImpl(BufferLayoutStruct);
TListXImpl(AllocationBufferBlock);

TListXImpl(SubResourceData);

//Contains small helper functions that don't require their own .c file

//BMP

Error BMP_writex(Buffer buf, BMPInfo info, Buffer *result) {
	return BMP_write(buf, info, Platform_instance.alloc, result);
}

Error BMP_readx(Buffer buf, BMPInfo *info, Buffer *result) {
	return BMP_read(buf, info, Platform_instance.alloc, result);
}

//DDS

Error DDS_writex(ListSubResourceData buf, DDSInfo info, Buffer *result) {
	return DDS_write(buf, info, Platform_instance.alloc, result);
}

Error DDS_readx(Buffer buf, DDSInfo *info, ListSubResourceData *result) {
	return DDS_read(buf, info, Platform_instance.alloc, result);
}

Bool ListSubResourceData_freeAllx(ListSubResourceData *buf) {
	return ListSubResourceData_freeAll(buf, Platform_instance.alloc);
}

//Buffer

Error Buffer_createCopyx(Buffer buf, Buffer *output) {
	return Buffer_createCopy(buf, Platform_instance.alloc, output);
}

Error Buffer_createZeroBitsx(U64 length, Buffer *output) {
	return Buffer_createZeroBits(length, Platform_instance.alloc, output);
}

Error Buffer_createOneBitsx(U64 length, Buffer *output) {
	return Buffer_createOneBits(length, Platform_instance.alloc, output);
}

Error Buffer_createBitsx(U64 length, Bool value, Buffer *result) {
	return value ? Buffer_createOneBitsx(length, result) : Buffer_createZeroBitsx(length, result);
}

Bool Buffer_freex(Buffer *buf) { return Buffer_free(buf, Platform_instance.alloc); }

Error Buffer_createEmptyBytesx(U64 length, Buffer *output) {
	return Buffer_createEmptyBytes(length, Platform_instance.alloc, output);
}

Error Buffer_createUninitializedBytesx(U64 length, Buffer *output) {
	return Buffer_createUninitializedBytes(length, Platform_instance.alloc, output);
}

Error AllocationBuffer_createx(U64 size, Bool isVirtual, AllocationBuffer *allocationBuffer) {
	return AllocationBuffer_create(size, isVirtual, Platform_instance.alloc, allocationBuffer);
}

Bool AllocationBuffer_freex(AllocationBuffer *allocationBuffer) {
	return AllocationBuffer_free(allocationBuffer, Platform_instance.alloc);
}

Error AllocationBuffer_createRefFromRegionx(Buffer origin, U64 offset, U64 size, AllocationBuffer *allocationBuffer) {
	return AllocationBuffer_createRefFromRegion(origin, offset, size, Platform_instance.alloc, allocationBuffer);
}

Error AllocationBuffer_allocateBlockx(AllocationBuffer *allocationBuffer, U64 size, U64 alignment, const U8 **result) {
	return AllocationBuffer_allocateBlock(allocationBuffer, size, alignment, Platform_instance.alloc, result);
}

Error AllocationBuffer_allocateAndFillBlockx(AllocationBuffer *allocationBuffer, Buffer data, U64 alignment, U8 **result) {
	return AllocationBuffer_allocateAndFillBlock(allocationBuffer, data, alignment, Platform_instance.alloc, result);
}

//oiCA

Bool CAFile_freex(CAFile *caFile) { return CAFile_free(caFile, Platform_instance.alloc); }

Error CAFile_writex(CAFile caFile, Buffer *result) {
	return CAFile_write(caFile, Platform_instance.alloc, result);
}

Error CAFile_readx(Buffer file, const U32 encryptionKey[8], CAFile *caFile) {
	return CAFile_read(file, encryptionKey, Platform_instance.alloc, caFile);
}

//oiDL

Error DLFile_createx(DLSettings settings, DLFile *dlFile) {
	return DLFile_create(settings, Platform_instance.alloc, dlFile);
}

Bool DLFile_freex(DLFile *dlFile) { return DLFile_free(dlFile, Platform_instance.alloc); }

Error DLFile_createListx(DLSettings settings, ListBuffer *buffers, DLFile *dlFile) {
	return DLFile_createList(settings, buffers, Platform_instance.alloc, dlFile);
}

Error DLFile_createUTF8Listx(DLSettings settings, ListBuffer buffers, DLFile *dlFile) {
	return DLFile_createUTF8List(settings, buffers, Platform_instance.alloc, dlFile);
}

Error DLFile_createBufferListx(DLSettings settings, ListBuffer buffers, DLFile *dlFile) {
	return DLFile_createBufferList(settings, buffers, Platform_instance.alloc, dlFile);
}

Error DLFile_createAsciiListx(DLSettings settings, ListCharString strings, DLFile *dlFile) {
	return DLFile_createAsciiList(settings, strings, Platform_instance.alloc, dlFile);
}

Error DLFile_addEntryx(DLFile *dlFile, Buffer entry) { return DLFile_addEntry(dlFile, entry, Platform_instance.alloc); }

Error DLFile_addEntryAsciix(DLFile *dlFile, CharString entry) {
	return DLFile_addEntryAscii(dlFile, entry, Platform_instance.alloc);
}

Error DLFile_addEntryUTF8x(DLFile *dlFile, Buffer entry) {
	return DLFile_addEntryUTF8(dlFile, entry, Platform_instance.alloc);
}

Error DLFile_writex(DLFile dlFile, Buffer *result) { return DLFile_write(dlFile, Platform_instance.alloc, result); }

Error DLFile_readx(Buffer file, const U32 encryptionKey[8], Bool allowLeftOverData, DLFile *dlFile) {
	return DLFile_read(file, encryptionKey, allowLeftOverData, Platform_instance.alloc, dlFile);
}

//List

Error GenericList_createx(U64 length, U64 stride, GenericList *result) {
	return GenericList_create(length, stride, Platform_instance.alloc, result);
}

Error GenericList_createRepeatedx(U64 length, U64 stride, Buffer data, GenericList *result) {
	return GenericList_createRepeated(length, stride, data, Platform_instance.alloc, result);
}

Error GenericList_createCopyx(GenericList list, GenericList *result) {
	return GenericList_createCopy(list, Platform_instance.alloc, result);
}

Error GenericList_createSubsetReversex(GenericList list, U64 index, U64 length, GenericList *result) {
	return GenericList_createSubsetReverse(list, index, length, Platform_instance.alloc, result);
}

Error GenericList_createReversex(GenericList list, GenericList *result) {
	return GenericList_createSubsetReversex(list, 0, list.length, result);
}

Error GenericList_findx(GenericList list, Buffer buf, EqualsFunction eq, ListU64 *result) {
	return GenericList_find(list, buf, eq, Platform_instance.alloc, result);
}

Error GenericList_eraseAllx(GenericList *list, Buffer buf, EqualsFunction eq) {
	return GenericList_eraseAll(list, buf, Platform_instance.alloc, eq);
}

Error GenericList_insertx(GenericList *list, U64 index, Buffer buf) {
	return GenericList_insert(list, index, buf, Platform_instance.alloc);
}

Error GenericList_pushAllx(GenericList *list, GenericList other) {
	return GenericList_pushAll(list, other, Platform_instance.alloc);
}

Error GenericList_insertAllx(GenericList *list, GenericList other, U64 offset) {
	return GenericList_insertAll(list, other, offset, Platform_instance.alloc);
}

Error GenericList_reservex(GenericList *list, U64 capacity) {
	return GenericList_reserve(list, capacity, Platform_instance.alloc);
}

Error GenericList_resizex(GenericList *list, U64 size) { return GenericList_resize(list, size, Platform_instance.alloc); }
Error GenericList_shrinkToFitx(GenericList *list) { return GenericList_shrinkToFit(list, Platform_instance.alloc); }

Error GenericList_pushBackx(GenericList *l, Buffer buf) { return GenericList_pushBack(l, buf, Platform_instance.alloc); }
Error GenericList_pushFrontx(GenericList *l, Buffer buf) { return GenericList_pushFront(l, buf, Platform_instance.alloc); }
Bool GenericList_freex(GenericList *result) { return GenericList_free(result, Platform_instance.alloc); }

//BigInt

Error BigInt_createx(U16 bitCount, BigInt *big) { return BigInt_create(bitCount, Platform_instance.alloc, big); }
Error BigInt_createCopyx(BigInt *a, BigInt *b) { return BigInt_createCopy(a, Platform_instance.alloc, b); }

Bool BigInt_freex(BigInt *a) { return BigInt_free(a, Platform_instance.alloc); }

Error BigInt_createFromHexx(CharString text, U16 bitCount, BigInt *big) {
	return BigInt_createFromHex(text, bitCount, Platform_instance.alloc, big);
}

Error BigInt_createFromDecx(CharString text, U16 bitCount, BigInt *big) {
	return BigInt_createFromDec(text, bitCount, Platform_instance.alloc, big);
}

Error BigInt_createFromOctx(CharString text, U16 bitCount, BigInt *big) {
	return BigInt_createFromOct(text, bitCount, Platform_instance.alloc, big);
}

Error BigInt_createFromBinx(CharString text, U16 bitCount, BigInt *big) {
	return BigInt_createFromBin(text, bitCount, Platform_instance.alloc, big);
}

Error BigInt_createFromNytox(CharString text, U16 bitCount, BigInt *big) {
	return BigInt_createFromNyto(text, bitCount, Platform_instance.alloc, big);
}

Error BigInt_createFromStringx(CharString text, U16 bitCount, BigInt *big) {
	return BigInt_createFromString(text, bitCount, Platform_instance.alloc, big);
}

Bool BigInt_mulx(BigInt *a, BigInt b) { return BigInt_mul(a, b, Platform_instance.alloc); }

Error BigInt_resizex(BigInt *a, U8 newSize) { return BigInt_resize(a, newSize, Platform_instance.alloc); }
Bool BigInt_setx(BigInt *a, BigInt b, Bool allowResize) { return BigInt_set(a, b, allowResize, Platform_instance.alloc); }

Bool BigInt_truncx(BigInt *big) { return BigInt_trunc(big, Platform_instance.alloc); }

Error BigInt_isBase2x(BigInt a, Bool *isBase2) { return BigInt_isBase2(a, Platform_instance.alloc, isBase2); }
U128 U128_createFromDecx(CharString text, Error *failed) { return U128_createFromDec(text, failed, Platform_instance.alloc); }

U128 U128_createFromStringx(CharString text, Error *failed) {
	return U128_createFromString(text, failed, Platform_instance.alloc);
}

Error BigInt_hexx(BigInt b, CharString *result, Bool leadingZeros) {
	return BigInt_hex(b, Platform_instance.alloc, result, leadingZeros);
}

Error BigInt_octx(BigInt b, CharString *result, Bool leadingZeros) {
	return BigInt_oct(b, Platform_instance.alloc, result, leadingZeros);
}

Error BigInt_binx(BigInt b, CharString *result, Bool leadingZeros) {
	return BigInt_bin(b, Platform_instance.alloc, result, leadingZeros);
}

Error BigInt_nytox(BigInt b, CharString *result, Bool leadingZeros) {
	return BigInt_nyto(b, Platform_instance.alloc, result, leadingZeros);
}

Error BigInt_toStringx(BigInt b, CharString *result, EIntegerEncoding encoding, Bool leadingZeros) {
	return BigInt_toString(b, Platform_instance.alloc, result, encoding, leadingZeros);
}

//Error BigInt_decx(BigInt b, CharString *result, Bool leadingZeros);

//CharString

Bool CharString_freex(CharString *str) { return CharString_free(str, Platform_instance.alloc); }
Bool CharStringList_freex(CharStringList *arr) { return CharStringList_free(arr, Platform_instance.alloc); }

Error CharString_createx(C8 c, U64 size, CharString *result) {
	return CharString_create(c, size, Platform_instance.alloc, result);
}

Error CharString_createCopyx(CharString str, CharString *result) {
	return CharString_createCopy(str, Platform_instance.alloc, result);
}

Error CharString_createFromUTF16x(const U16 *ptr, U64 max, CharString *result) {
	return CharString_createFromUTF16(ptr, max, Platform_instance.alloc, result);
}

Error CharString_toUTF16x(CharString s, ListU16 *arr) {
	return CharString_toUTF16(s, Platform_instance.alloc, arr);
}

Error CharString_createNytox(U64 v, U8 leadingZeros, CharString *result) {
	return CharString_createNyto(v, leadingZeros, Platform_instance.alloc, result);
}

Error CharString_createHexx(U64 v, U8 leadingZeros, CharString *result) {
	return CharString_createHex(v, leadingZeros, Platform_instance.alloc, result);
}

Error CharString_createDecx(U64 v, U8 leadingZeros, CharString *result) {
	return CharString_createDec(v, leadingZeros, Platform_instance.alloc, result);
}

Error CharString_createOctx(U64 v, U8 leadingZeros, CharString *result) {
	return CharString_createOct(v, leadingZeros, Platform_instance.alloc, result);
}

Error CharString_createBinx(U64 v, U8 leadingZeros, CharString *result) {
	return CharString_createBin(v, leadingZeros, Platform_instance.alloc, result);
}

Error CharString_splitx(CharString s, C8 c,  EStringCase casing,  CharStringList *result) {
	return CharString_split(s, c, casing, Platform_instance.alloc, result);
}

Error CharString_splitStringx(CharString s, CharString other, EStringCase casing, CharStringList *result) {
	return CharString_splitString(s, other, casing, Platform_instance.alloc, result);
}

Error CharString_splitSensitivex(CharString s, C8 c, CharStringList *result) {
	return CharString_splitSensitive(s, c, Platform_instance.alloc, result);
}

Error CharString_splitInsensitivex(CharString s, C8 c, CharStringList *result) {
	return CharString_splitInsensitive(s, c, Platform_instance.alloc, result);
}

Error CharString_splitStringSensitivex(CharString s, CharString other, CharStringList *result) {
	return CharString_splitStringSensitive(s, other, Platform_instance.alloc, result);
}

Error CharString_splitStringInsensitivex(CharString s, CharString other, CharStringList *result) {
	return CharString_splitStringInsensitive(s, other, Platform_instance.alloc, result);
}

Error CharString_splitLinex(CharString s, CharStringList *result) {
	return CharString_splitLine(s, Platform_instance.alloc, result);
}

Error CharString_resizex(CharString *str, U64 length, C8 defaultChar) {
	return CharString_resize(str, length, defaultChar, Platform_instance.alloc);
}

Error CharString_reservex(CharString *str, U64 length) {
	return CharString_reserve(str, length, Platform_instance.alloc);
}

Error CharString_appendx(CharString *s, C8 c) {
	return CharString_append(s, c, Platform_instance.alloc);
}

Error CharString_appendStringx(CharString *s, CharString other) {
	return CharString_appendString(s, other, Platform_instance.alloc);
}

Error CharString_prependx(CharString *s, C8 c) {
	return CharString_prepend(s, c, Platform_instance.alloc);
}

Error CharString_prependStringx(CharString *s, CharString other) {
	return CharString_prependString(s, other, Platform_instance.alloc);
}

Error CharString_insertx(CharString *s, C8 c, U64 i) {
	return CharString_insert(s, c, i, Platform_instance.alloc);
}

Error CharString_insertStringx(CharString *s, CharString other, U64 i) {
	return CharString_insertString(s, other, i, Platform_instance.alloc);
}

Error CharString_replaceAllStringx(CharString *s, CharString search, CharString replace, EStringCase caseSensitive) {
	return CharString_replaceAllString(s, search, replace, caseSensitive, Platform_instance.alloc);
}

Error CharString_replaceStringx(
	CharString *s,
	CharString search,
	CharString replace,
	EStringCase caseSensitive,
	Bool isFirst
) {
	return CharString_replaceString(s, search, replace, caseSensitive, Platform_instance.alloc, isFirst);
}

Error CharString_replaceFirstStringx(CharString *s, CharString search, CharString replace, EStringCase caseSensitive) {
	return CharString_replaceStringx(s, search, replace, caseSensitive, true);
}

Error CharString_replaceLastStringx(CharString *s, CharString search, CharString replace, EStringCase caseSensitive) {
	return CharString_replaceStringx(s, search, replace, caseSensitive, false);
}

Error CharString_replaceAllStringSensitivex(CharString *s, CharString search, CharString replace) {
	return CharString_replaceAllStringSensitive(s, search, replace, Platform_instance.alloc);
}

Error CharString_replaceAllStringInsensitivex(CharString *s, CharString search, CharString replace) {
	return CharString_replaceAllStringInsensitive(s, search, replace, Platform_instance.alloc);
}

Error CharString_replaceStringSensitivex(CharString *s, CharString search, CharString replace, Bool isFirst) {
	return CharString_replaceStringSensitive(s, search, replace, Platform_instance.alloc, isFirst);
}

Error CharString_replaceStringInsensitivex(CharString *s, CharString search, CharString replace, Bool isFirst) {
	return CharString_replaceStringInsensitive(s, search, replace, Platform_instance.alloc, isFirst);
}

Error CharString_replaceFirstStringSensitivex(CharString *s, CharString search, CharString replace) {
	return CharString_replaceFirstStringSensitive(s, search, replace, Platform_instance.alloc);
}

Error CharString_replaceFirstStringInsensitivex(CharString *s, CharString search, CharString replace) {
	return CharString_replaceFirstStringInsensitive(s, search, replace, Platform_instance.alloc);
}

Error CharString_replaceLastStringSensitivex(CharString *s, CharString search, CharString replace) {
	return CharString_replaceLastStringSensitive(s, search, replace, Platform_instance.alloc);
}

Error CharString_replaceLastStringInsensitivex(CharString *s, CharString search, CharString replace) {
	return CharString_replaceLastStringInsensitive(s, search, replace, Platform_instance.alloc);
}

Error CharString_findAllx(CharString s, C8 c, EStringCase caseSensitive, ListU64 *result) {
	return CharString_findAll(s, c, Platform_instance.alloc, caseSensitive, result);
}

Error CharString_findAllStringx(CharString s, CharString other, EStringCase caseSensitive, ListU64 *result) {
	return CharString_findAllString(s, other, Platform_instance.alloc, caseSensitive, result);
}

Error CharString_findAllSensitivex(CharString s, C8 c, ListU64 *result) {
	return CharString_findAllSensitive(s, c, Platform_instance.alloc, result);
}

Error CharString_findAllInsensitivex(CharString s, C8 c, ListU64 *result) {
	return CharString_findAllInsensitive(s, c, Platform_instance.alloc, result);
}

Error CharString_findAllStringSensitivex(CharString s, CharString other, ListU64 *result) {
	return CharString_findAllStringSensitive(s, other, Platform_instance.alloc, result);
}

Error CharString_findAllStringInsensitivex(CharString s, CharString other, ListU64 *result) {
	return CharString_findAllStringInsensitive(s, other, Platform_instance.alloc, result);
}

Error CharStringList_createx(U64 length, CharStringList *result) {
	return CharStringList_create(length, Platform_instance.alloc, result);
}

Error CharStringList_createCopyx(CharStringList toCopy, CharStringList *arr) {
	return CharStringList_createCopy(toCopy, Platform_instance.alloc, arr);
}

Error CharStringList_setx(CharStringList arr, U64 i, CharString str) {
	return CharStringList_set(arr, i, str, Platform_instance.alloc);
}

Error CharStringList_unsetx(CharStringList arr, U64 i) {
	return CharStringList_unset(arr, i, Platform_instance.alloc);
}

Error CharStringList_combinex(CharStringList arr, CharString *result) {
	return CharStringList_combine(arr, Platform_instance.alloc, result);
}

Error CharStringList_concatx(CharStringList arr, C8 between, CharString *result) {
	return CharStringList_concat(arr, between, Platform_instance.alloc, result);
}

Error CharStringList_concatStringx(CharStringList arr, CharString between, CharString *result) {
	return CharStringList_concatString(arr, between, Platform_instance.alloc, result);
}

//Format string

Error CharString_formatVariadicx(CharString *result, const C8 *format, va_list args) {
	return CharString_formatVariadic(Platform_instance.alloc, result, format, args);
}

Error CharString_formatx(CharString *result, const C8 *format, ...) {

	if(!result || !format)
		return Error_nullPointer(!result ? 1 : 2, "CharString_formatx()::result and format are required");

	va_list arg1;
	va_start(arg1, format);
	Error err = CharString_formatVariadic(Platform_instance.alloc, result, format, arg1);
	va_end(arg1);

	return err;
}

//Archive

Error Archive_createx(Archive *archive) { return Archive_create(Platform_instance.alloc, archive); }
Bool Archive_freex(Archive *archive) { return Archive_free(archive, Platform_instance.alloc); }

Bool Archive_hasFilex(Archive archive, CharString path) { return Archive_hasFile(archive, path, Platform_instance.alloc); }
Bool Archive_hasFolderx(Archive archive, CharString path) { return Archive_hasFolder(archive, path, Platform_instance.alloc); }
Bool Archive_hasx(Archive archive, CharString path) { return Archive_has(archive, path, Platform_instance.alloc); }

Error Archive_addDirectoryx(Archive *archive, CharString path) {
	return Archive_addDirectory(archive, path, Platform_instance.alloc);
}

Error Archive_addFilex(Archive *archive, CharString path, Buffer data, Ns timestamp) {
	return Archive_addFile(archive, path, data, timestamp, Platform_instance.alloc);
}

Error Archive_updateFileDatax(Archive *archive, CharString path, Buffer data) {
	return Archive_updateFileData(archive, path, data, Platform_instance.alloc);
}

Error Archive_getFileDatax(Archive archive, CharString path, Buffer *data) {
	return Archive_getFileData(archive, path, data, Platform_instance.alloc);
}

Error Archive_getFileDataConstx(Archive archive, CharString path, Buffer *data) {
	return Archive_getFileDataConst(archive, path, data, Platform_instance.alloc);
}

Error Archive_removeFilex(Archive *archive, CharString path) {
	return Archive_removeFile(archive, path, Platform_instance.alloc);
}

Error Archive_removeFolderx(Archive *archive, CharString path) {
	return Archive_removeFolder(archive, path, Platform_instance.alloc);
}

Error Archive_removex(Archive *archive, CharString path) { return Archive_remove(archive, path, Platform_instance.alloc); }

Error Archive_renamex(Archive *archive, CharString loc, CharString newFileName) {
	return Archive_rename(archive, loc, newFileName, Platform_instance.alloc);
}

U64 Archive_getIndexx(Archive archive, CharString loc) {
	return Archive_getIndex(archive, loc, Platform_instance.alloc);
}

Error Archive_movex(Archive *archive, CharString loc, CharString directoryName) {
	return Archive_move(archive, loc, directoryName, Platform_instance.alloc);
}

Error Archive_getInfox(Archive archive, CharString loc, FileInfo *info) {
	return Archive_getInfo(archive, loc, info, Platform_instance.alloc);
}

Error Archive_queryFileEntryCountx(Archive archive, CharString loc, Bool isRecursive, U64 *res) {
	return Archive_queryFileEntryCount(archive, loc, isRecursive, res, Platform_instance.alloc);
}

Error Archive_queryFileCountx(Archive archive, CharString loc, Bool isRecursive, U64 *res) {
	return Archive_queryFileCount(archive, loc, isRecursive, res, Platform_instance.alloc);
}

Error Archive_queryFolderCountx(Archive archive, CharString loc, Bool isRecursive, U64 *res) {
	return Archive_queryFolderCount(archive, loc, isRecursive, res, Platform_instance.alloc);
}

Error Archive_foreachx(
	Archive archive,
	CharString loc,
	FileCallback callback,
	void *userData,
	Bool isRecursive,
	EFileType type
) {
	return Archive_foreach(archive, loc, callback, userData, isRecursive, type, Platform_instance.alloc);
}

//Thread

Error Thread_createx(ThreadCallbackFunction callback, void *objectHandle, Thread **thread) {
	return Thread_create(Platform_instance.alloc, callback, objectHandle, thread);
}

Bool Thread_freex(Thread **thread) {
	return Thread_free(Platform_instance.alloc, thread);
}

Error Thread_waitAndCleanupx(Thread **thread) {
	return Thread_waitAndCleanup(Platform_instance.alloc, thread);
}

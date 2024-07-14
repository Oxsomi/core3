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

#include "platforms/ext/listx_impl.h"
#include "platforms/ext/bmpx.h"
#include "platforms/ext/ddsx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/formatx.h"
#include "platforms/ext/threadx.h"
#include "platforms/ext/ref_ptrx.h"
#include "formats/bmp.h"
#include "formats/dds.h"
#include "formats/oiCA.h"
#include "formats/oiDL.h"
#include "formats/oiSH.h"
#include "types/buffer.h"
#include "types/allocation_buffer.h"
#include "types/error.h"
#include "types/archive.h"
#include "types/big_int.h"
#include "types/cdf_list.h"
#include "types/lexer.h"
#include "types/parser.h"
#include "types/buffer_layout.h"
#include "platforms/platform.h"

TListXImpl(CharString);
TListXBaseImpl(ListConstC8);

TListXImpl(U64);
TListXImpl(U8);		TListXImpl(U16);	TListXImpl(U32);
TListXImpl(I8);		TListXImpl(I16);	TListXImpl(I32); TListXImpl(I64);
TListXImpl(F32);	TListXImpl(F64);

TListXImpl(Buffer);

TListXImpl(CdfValue);
TListXImpl(ArchiveEntry);

TListXImpl(BufferLayoutMemberInfo);
TListXImpl(BufferLayoutStruct);
TListXImpl(AllocationBufferBlock);

TListXImpl(SubResourceData);

TListXBaseImpl(ListRefPtr);
TListXBaseImpl(ListWeakRefPtr);

TListXImpl(SHEntry);
TListXImpl(SHEntryRuntime);
TListXImpl(SHBinaryInfo);
TListXImpl(SHBinaryIdentifier);

TListXBaseImpl(ListThread);

//Lexer and parser

TListXImpl(Token);
TListXImpl(Symbol);
TListXImpl(LexerToken);
TListXImpl(LexerExpression);

//Contains small helper functions that don't require their own .c file

//BMP

Error BMP_writex(Buffer buf, BMPInfo info, Buffer *result) {
	return BMP_write(buf, info, Platform_instance.alloc, result);
}

Error BMP_readx(Buffer buf, BMPInfo *info, Buffer *result) {
	return BMP_read(buf, info, Platform_instance.alloc, result);
}

//RefPtr

Error RefPtr_createx(U32 objectLength, ObjectFreeFunc free, ETypeId type, RefPtr **result) {
	return RefPtr_create(objectLength, Platform_instance.alloc, free, type, result);
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

//Lexer and parser

Bool Lexer_createx(CharString str, Lexer *lexer, Error *e_rr) {
	return Lexer_create(str, Platform_instance.alloc, lexer, e_rr);
}

void Lexer_freex(Lexer *lexer) {
	Lexer_free(lexer, Platform_instance.alloc);
}

void Lexer_printx(Lexer lexer) {
	Lexer_print(lexer, Platform_instance.alloc);
}

Bool Parser_createx(const Lexer *lexer, Parser *parser, Error *e_rr) {
	return Parser_create(lexer, parser, Platform_instance.alloc, e_rr);
}

void Parser_freex(Parser *parser) {
	Parser_free(parser, Platform_instance.alloc);
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

Bool CAFile_writex(CAFile caFile, Buffer *result, Error *e_rr) {
	return CAFile_write(caFile, Platform_instance.alloc, result, e_rr);
}

Bool CAFile_readx(Buffer file, const U32 encryptionKey[8], CAFile *caFile, Error *e_rr) {
	return CAFile_read(file, encryptionKey, Platform_instance.alloc, caFile, e_rr);
}

Bool CAFile_combinex(CAFile a, CAFile b, CAFile *combined, Error *e_rr) {
	return CAFile_combine(a, b, Platform_instance.alloc, combined, e_rr);
}

//oiDL

Bool DLFile_createx(DLSettings settings, DLFile *dlFile, Error *e_rr) {
	return DLFile_create(settings, Platform_instance.alloc, dlFile, e_rr);
}

Bool DLFile_freex(DLFile *dlFile) { return DLFile_free(dlFile, Platform_instance.alloc); }

Bool DLFile_createListx(DLSettings settings, ListBuffer *buffers, DLFile *dlFile, Error *e_rr) {
	return DLFile_createList(settings, buffers, Platform_instance.alloc, dlFile, e_rr);
}

Bool DLFile_createUTF8Listx(DLSettings settings, ListBuffer buffers, DLFile *dlFile, Error *e_rr) {
	return DLFile_createUTF8List(settings, buffers, Platform_instance.alloc, dlFile, e_rr);
}

Bool DLFile_createBufferListx(DLSettings settings, ListBuffer buffers, DLFile *dlFile, Error *e_rr) {
	return DLFile_createBufferList(settings, buffers, Platform_instance.alloc, dlFile, e_rr);
}

Bool DLFile_createAsciiListx(DLSettings settings, ListCharString strings, DLFile *dlFile, Error *e_rr) {
	return DLFile_createAsciiList(settings, strings, Platform_instance.alloc, dlFile, e_rr);
}

Bool DLFile_addEntryx(DLFile *dlFile, Buffer entry, Error *e_rr) {
	return DLFile_addEntry(dlFile, entry, Platform_instance.alloc, e_rr);
}

Bool DLFile_addEntryAsciix(DLFile *dlFile, CharString entry, Error *e_rr) {
	return DLFile_addEntryAscii(dlFile, entry, Platform_instance.alloc, e_rr);
}

Bool DLFile_addEntryUTF8x(DLFile *dlFile, Buffer entry, Error *e_rr) {
	return DLFile_addEntryUTF8(dlFile, entry, Platform_instance.alloc, e_rr);
}

Bool DLFile_writex(DLFile dlFile, Buffer *result, Error *e_rr) {
	return DLFile_write(dlFile, Platform_instance.alloc, result, e_rr);
}

Bool DLFile_readx(Buffer file, const U32 encryptionKey[8], Bool allowLeftOverData, DLFile *dlFile, Error *e_rr) {
	return DLFile_read(file, encryptionKey, allowLeftOverData, Platform_instance.alloc, dlFile, e_rr);
}

Bool DLFile_combinex(DLFile a, DLFile b, DLFile *combined, Error *e_rr) {
	return DLFile_combine(a, b, Platform_instance.alloc, combined, e_rr);
}

//SHFile

Bool SHFile_createx(ESHSettingsFlags flags, U32 compilerVersion, U32 sourceHash, SHFile *shFile, Error *e_rr) {
	return SHFile_create(flags, compilerVersion, sourceHash, Platform_instance.alloc, shFile, e_rr);
}

void SHFile_freex(SHFile *shFile) {
	SHFile_free(shFile, Platform_instance.alloc);
}

Bool SHFile_addBinaryx(SHFile *shFile, SHBinaryInfo *binaries, Error *e_rr) {
	return SHFile_addBinaries(shFile, binaries, Platform_instance.alloc, e_rr);
}

Bool SHFile_addEntrypointx(SHFile *shFile, SHEntry *entry, Error *e_rr) {
	return SHFile_addEntrypoint(shFile, entry, Platform_instance.alloc, e_rr);
}

Bool SHFile_addIncludex(SHFile *shFile, SHInclude *include, Error *e_rr) {
	return SHFile_addInclude(shFile, include, Platform_instance.alloc, e_rr);
}

Bool SHFile_writex(SHFile shFile, Buffer *result, Error *e_rr) {
	return SHFile_write(shFile, Platform_instance.alloc, result, e_rr);
}

Bool SHFile_readx(Buffer file, Bool isSubFile, SHFile *shFile, Error *e_rr) {
	return SHFile_read(file, isSubFile, Platform_instance.alloc, shFile, e_rr);
}

Bool SHFile_combinex(SHFile a, SHFile b, SHFile *combined, Error *e_rr) {
	return SHFile_combine(a, b, Platform_instance.alloc, combined, e_rr);
}

void SHEntry_printx(SHEntry entry) { SHEntry_print(entry, Platform_instance.alloc); }
void SHEntryRuntime_printx(SHEntryRuntime entry) { SHEntryRuntime_print(entry, Platform_instance.alloc); }
void SHBinaryInfo_printx(SHBinaryInfo binary) { SHBinaryInfo_print(binary, Platform_instance.alloc); }

void SHBinaryIdentifier_freex(SHBinaryIdentifier *identifier) {
	SHBinaryIdentifier_free(identifier, Platform_instance.alloc);
}

void SHBinaryInfo_freex(SHBinaryInfo *info) {
	SHBinaryInfo_free(info, Platform_instance.alloc);
}

void SHEntry_freex(SHEntry *entry) {
	SHEntry_free(entry, Platform_instance.alloc);
}

void SHEntryRuntime_freex(SHEntryRuntime *entry) {
	SHEntryRuntime_free(entry, Platform_instance.alloc);
}

void ListSHEntryRuntime_freeUnderlyingx(ListSHEntryRuntime *entry) {
	ListSHEntryRuntime_freeUnderlying(entry, Platform_instance.alloc);
}

void ListSHInclude_freeUnderlyingx(ListSHInclude *includes) {
	ListSHInclude_freeUnderlying(includes, Platform_instance.alloc);
}

void SHInclude_freex(SHInclude *include) {
	SHInclude_free(include, Platform_instance.alloc);
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

Error GenericList_createCopySubsetx(GenericList list, U64 offset, U64 len, GenericList *result) {
	return GenericList_createCopySubset(list, offset, len, Platform_instance.alloc, result);
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

//Error U128_decx(U128 b, CharString *result, Bool leadingZeros);

Error U128_hexx(U128 b, CharString *result, Bool leadingZeros) {
	return U128_hex(b, Platform_instance.alloc, result, leadingZeros);
}

Error U128_octx(U128 b, CharString *result, Bool leadingZeros) {
	return U128_oct(b, Platform_instance.alloc, result, leadingZeros);
}

Error U128_binx(U128 b, CharString *result, Bool leadingZeros) {
	return U128_bin(b, Platform_instance.alloc, result, leadingZeros);
}

Error U128_nytox(U128 b, CharString *result, Bool leadingZeros) {
	return U128_nyto(b, Platform_instance.alloc, result, leadingZeros);
}

Error U128_toStringx(U128 b, CharString *result, EIntegerEncoding encoding, Bool leadingZeros) {
	return U128_toString(b, Platform_instance.alloc, result, encoding, leadingZeros);
}

//Error BigInt_decx(BigInt b, CharString *result, Bool leadingZeros);

//CharString

Bool CharString_freex(CharString *str) { return CharString_free(str, Platform_instance.alloc); }
Bool ListCharString_freeUnderlyingx(ListCharString *arr) { return ListCharString_freeUnderlying(arr, Platform_instance.alloc); }

Error CharString_createx(C8 c, U64 size, CharString *result) {
	return CharString_create(c, size, Platform_instance.alloc, result);
}

Error CharString_createCopyx(CharString str, CharString *result) {
	return CharString_createCopy(str, Platform_instance.alloc, result);
}

Error CharString_createFromUTF16x(const U16 *ptr, U64 max, CharString *result) {
	return CharString_createFromUTF16(ptr, max, Platform_instance.alloc, result);
}

Error CharString_createFromUTF32x(const U32 *ptr, U64 max, CharString *result) {
	return CharString_createFromUTF32(ptr, max, Platform_instance.alloc, result);
}

Error CharString_toUTF16x(CharString s, ListU16 *arr) {
	return CharString_toUTF16(s, Platform_instance.alloc, arr);
}

Error CharString_toUTF32x(CharString s, ListU32 *arr) {
	return CharString_toUTF32(s, Platform_instance.alloc, arr);
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

Error CharString_splitx(CharString s, C8 c,  EStringCase casing,  ListCharString *result) {
	return CharString_split(s, c, casing, Platform_instance.alloc, result);
}

Error CharString_splitStringx(CharString s, CharString other, EStringCase casing, ListCharString *result) {
	return CharString_splitString(s, other, casing, Platform_instance.alloc, result);
}

Error CharString_splitSensitivex(CharString s, C8 c, ListCharString *result) {
	return CharString_splitSensitive(s, c, Platform_instance.alloc, result);
}

Error CharString_splitInsensitivex(CharString s, C8 c, ListCharString *result) {
	return CharString_splitInsensitive(s, c, Platform_instance.alloc, result);
}

Error CharString_splitStringSensitivex(CharString s, CharString other, ListCharString *result) {
	return CharString_splitStringSensitive(s, other, Platform_instance.alloc, result);
}

Error CharString_splitStringInsensitivex(CharString s, CharString other, ListCharString *result) {
	return CharString_splitStringInsensitive(s, other, Platform_instance.alloc, result);
}

Error CharString_splitLinex(CharString s, ListCharString *result) {
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

Error CharString_replaceAllStringx(
	CharString *s,
	CharString search,
	CharString replace,
	EStringCase caseSensitive,
	U64 off,
	U64 len
) {
	return CharString_replaceAllString(s, search, replace, caseSensitive, Platform_instance.alloc, off, len);
}

Error CharString_replaceStringx(
	CharString *s,
	CharString search,
	CharString replace,
	EStringCase caseSensitive,
	Bool isFirst,
	U64 off,
	U64 len
) {
	return CharString_replaceString(s, search, replace, caseSensitive, Platform_instance.alloc, isFirst, off, len);
}

Error CharString_replaceFirstStringx(
	CharString *s, CharString search, CharString replace, EStringCase caseSensitive, U64 off, U64 len
) {
	return CharString_replaceStringx(s, search, replace, caseSensitive, true, off, len);
}

Error CharString_replaceLastStringx(
	CharString *s, CharString search, CharString replace, EStringCase caseSensitive, U64 off, U64 len
) {
	return CharString_replaceStringx(s, search, replace, caseSensitive, false, off, len);
}

Error CharString_replaceAllStringSensitivex(CharString *s, CharString search, CharString replace, U64 off, U64 len) {
	return CharString_replaceAllStringSensitive(s, search, replace, Platform_instance.alloc, off, len);
}

Error CharString_replaceAllStringInsensitivex(CharString *s, CharString search, CharString replace, U64 off, U64 len) {
	return CharString_replaceAllStringInsensitive(s, search, replace, Platform_instance.alloc, off, len);
}

Error CharString_replaceStringSensitivex(
	CharString *s,
	CharString search,
	CharString replace,
	Bool isFirst,
	U64 off,
	U64 len
) {
	return CharString_replaceStringSensitive(s, search, replace, Platform_instance.alloc, isFirst, off, len);
}

Error CharString_replaceStringInsensitivex(
	CharString *s,
	CharString search,
	CharString replace,
	Bool isFirst,
	U64 off,
	U64 len
) {
	return CharString_replaceStringInsensitive(s, search, replace, Platform_instance.alloc, isFirst, off, len);
}

Error CharString_replaceFirstStringSensitivex(CharString *s, CharString search, CharString replace, U64 off, U64 len) {
	return CharString_replaceFirstStringSensitive(s, search, replace, Platform_instance.alloc, off, len);
}

Error CharString_replaceFirstStringInsensitivex(CharString *s, CharString search, CharString replace, U64 off, U64 len) {
	return CharString_replaceFirstStringInsensitive(s, search, replace, Platform_instance.alloc, off, len);
}

Error CharString_replaceLastStringSensitivex(CharString *s, CharString search, CharString replace, U64 off, U64 len) {
	return CharString_replaceLastStringSensitive(s, search, replace, Platform_instance.alloc, off, len);
}

Error CharString_replaceLastStringInsensitivex(CharString *s, CharString search, CharString replace, U64 off, U64 len) {
	return CharString_replaceLastStringInsensitive(s, search, replace, Platform_instance.alloc, off, len);
}

Error CharString_findAllx(CharString s, C8 c, EStringCase caseSensitive, ListU64 *result, U64 off, U64 len) {
	return CharString_findAll(s, c, Platform_instance.alloc, caseSensitive, off, len, result);
}

Error CharString_findAllStringx(CharString s, CharString other, EStringCase caseSensitive, ListU64 *result, U64 off, U64 len) {
	return CharString_findAllString(s, other, Platform_instance.alloc, caseSensitive, off, len, result);
}

Error CharString_findAllSensitivex(CharString s, C8 c, U64 off, U64 len, ListU64 *result) {
	return CharString_findAllSensitive(s, c, off, len, Platform_instance.alloc, result);
}

Error CharString_findAllInsensitivex(CharString s, C8 c, U64 off, U64 len, ListU64 *result) {
	return CharString_findAllInsensitive(s, c, off, len, Platform_instance.alloc, result);
}

Error CharString_findAllStringSensitivex(CharString s, CharString other, U64 off, U64 len, ListU64 *result) {
	return CharString_findAllStringSensitive(s, other, off, len, Platform_instance.alloc, result);
}

Error CharString_findAllStringInsensitivex(CharString s, CharString other, U64 off, U64 len, ListU64 *result) {
	return CharString_findAllStringInsensitive(s, other, off, len, Platform_instance.alloc, result);
}

Error ListCharString_createCopyUnderlyingx(ListCharString toCopy, ListCharString *arr) {
	return ListCharString_createCopyUnderlying(toCopy, Platform_instance.alloc, arr);
}

Error ListCharString_combinex(ListCharString arr, CharString *result) {
	return ListCharString_combine(arr, Platform_instance.alloc, result);
}

Error ListCharString_concatx(ListCharString arr, C8 between, CharString *result) {
	return ListCharString_concat(arr, between, Platform_instance.alloc, result);
}

Error ListCharString_concatStringx(ListCharString arr, CharString between, CharString *result) {
	return ListCharString_concatString(arr, between, Platform_instance.alloc, result);
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
	const Error err = CharString_formatVariadic(Platform_instance.alloc, result, format, arg1);
	va_end(arg1);

	return err;
}

//Archive

Bool Archive_createx(Archive *archive, Error *e_rr) { return Archive_create(Platform_instance.alloc, archive, e_rr); }
Bool Archive_freex(Archive *archive) { return Archive_free(archive, Platform_instance.alloc); }

Bool Archive_hasFilex(Archive archive, CharString path) { return Archive_hasFile(archive, path, Platform_instance.alloc); }
Bool Archive_hasFolderx(Archive archive, CharString path) { return Archive_hasFolder(archive, path, Platform_instance.alloc); }
Bool Archive_hasx(Archive archive, CharString path) { return Archive_has(archive, path, Platform_instance.alloc); }

Bool Archive_addDirectoryx(Archive *archive, CharString path, Error *e_rr) {
	return Archive_addDirectory(archive, path, Platform_instance.alloc, e_rr);
}

Bool Archive_addFilex(Archive *archive, CharString path, Buffer *data, Ns timestamp, Error *e_rr) {
	return Archive_addFile(archive, path, data, timestamp, Platform_instance.alloc, e_rr);
}

Bool Archive_updateFileDatax(Archive *archive, CharString path, Buffer data, Error *e_rr) {
	return Archive_updateFileData(archive, path, data, Platform_instance.alloc, e_rr);
}

Bool Archive_getFileDatax(Archive archive, CharString path, Buffer *data, Error *e_rr) {
	return Archive_getFileData(archive, path, data, Platform_instance.alloc, e_rr);
}

Bool Archive_getFileDataConstx(Archive archive, CharString path, Buffer *data, Error *e_rr) {
	return Archive_getFileDataConst(archive, path, data, Platform_instance.alloc, e_rr);
}

Bool Archive_removeFilex(Archive *archive, CharString path, Error *e_rr) {
	return Archive_removeFile(archive, path, Platform_instance.alloc, e_rr);
}

Bool Archive_removeFolderx(Archive *archive, CharString path, Error *e_rr) {
	return Archive_removeFolder(archive, path, Platform_instance.alloc, e_rr);
}

Bool Archive_removex(Archive *archive, CharString path, Error *e_rr) {
	return Archive_remove(archive, path, Platform_instance.alloc, e_rr);
}

Bool Archive_renamex(Archive *archive, CharString loc, CharString newFileName, Error *e_rr) {
	return Archive_rename(archive, loc, newFileName, Platform_instance.alloc, e_rr);
}

U64 Archive_getIndexx(Archive archive, CharString loc) {
	return Archive_getIndex(archive, loc, Platform_instance.alloc);
}

Bool Archive_movex(Archive *archive, CharString loc, CharString directoryName, Error *e_rr) {
	return Archive_move(archive, loc, directoryName, Platform_instance.alloc, e_rr);
}

Bool Archive_getInfox(Archive archive, CharString loc, FileInfo *info, Error *e_rr) {
	return Archive_getInfo(archive, loc, info, Platform_instance.alloc, e_rr);
}

Bool Archive_queryFileEntryCountx(Archive archive, CharString loc, Bool isRecursive, U64 *res, Error *e_rr) {
	return Archive_queryFileEntryCount(archive, loc, isRecursive, res, Platform_instance.alloc, e_rr);
}

Bool Archive_queryFileCountx(Archive archive, CharString loc, Bool isRecursive, U64 *res, Error *e_rr) {
	return Archive_queryFileCount(archive, loc, isRecursive, res, Platform_instance.alloc, e_rr);
}

Bool Archive_queryFolderCountx(Archive archive, CharString loc, Bool isRecursive, U64 *res, Error *e_rr) {
	return Archive_queryFolderCount(archive, loc, isRecursive, res, Platform_instance.alloc, e_rr);
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
	return Archive_foreach(archive, loc, callback, userData, isRecursive, type, Platform_instance.alloc, e_rr);
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

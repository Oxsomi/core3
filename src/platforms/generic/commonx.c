#include "platforms/ext/bmpx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/formatx.h"
#include "formats/bmp.h"
#include "formats/oiCA.h"
#include "formats/oiDL.h"
#include "types/buffer.h"
#include "types/error.h"
#include "types/ref_ptr.h"
#include "platforms/platform.h"

//Contains small helper functions that don't require their own .c file

//BMP

Error BMP_writeRGBAx(
	Buffer buf,
	U16 w,
	U16 h,
	Bool isFlipped,
	Buffer *result
) {
	return BMP_writeRGBA(buf, w, h, isFlipped, Platform_instance.alloc, result);
}

//RefPtr

RefPtr RefPtr_createx(void *ptr, ObjectFreeFunc free) {
	return RefPtr_create(ptr, Platform_instance.alloc, free);
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

/* TODO:
//oiCA

Error CAFile_createx(CASettings settings, CAFile *caFile) {
	return CAFile_create(settings, Platform_instance.alloc, caFile);
}

Bool CAFile_freex(CAFile *caFile) { return CAFile_free(caFile, Platform_instance.alloc); }

Error CAFile_addEntryx(CAFile *caFile, CAEntry entry) { return CAFile_addEntry(caFile, entry, Platform_instance.alloc); }

Error CAFile_writex(CAFile caFile, Buffer *result) {
	return CAFile_write(caFile, Platform_instance.alloc, result);
}

//Error CAFile_readx(Buffer file, CAFile *caFile) {								TODO:
//	return CAFile_read(file, Platform_instance.alloc, caFile);
//}*/

//oiDL

Error DLFile_createx(DLSettings settings, DLFile *dlFile) {
	return DLFile_create(settings, Platform_instance.alloc, dlFile);
}

Bool DLFile_freex(DLFile *dlFile) { return DLFile_free(dlFile, Platform_instance.alloc); }

Error DLFile_addEntryx(DLFile *dlFile, Buffer entry) { return DLFile_addEntry(dlFile, entry, Platform_instance.alloc); }

Error DLFile_addEntryAsciix(DLFile *dlFile, String entry) {
	return DLFile_addEntryAscii(dlFile, entry, Platform_instance.alloc);
}

Error DLFile_addEntryUTF8x(DLFile *dlFile, Buffer entry) {
	return DLFile_addEntryUTF8(dlFile, entry, Platform_instance.alloc);
}

Error DLFile_writex(DLFile dlFile, Buffer *result) { return DLFile_write(dlFile, Platform_instance.alloc, result); }

//Error DLFile_readx(Buffer file, DLFile *dlFile) {							TODO:
//	return DLFile_read(file, Platform_instance.alloc, dlFile);
//}

//List

Error List_createx(U64 length, U64 stride, List *result) {
	return List_create(length, stride, Platform_instance.alloc, result);
}

Error List_createNullBytesx(U64 length, U64 stride, List *result) {
	return List_createNullBytes(length, stride, Platform_instance.alloc, result);
}

Error List_createRepeatedx(U64 length, U64 stride, Buffer data, List *result) {
	return List_createRepeated(length, stride, data, Platform_instance.alloc, result);
}

Error List_createCopyx(List list, List *result) {
	return List_createCopy(list, Platform_instance.alloc, result);
}

Error List_createSubsetReversex(List list, U64 index, U64 length, List *result) {
	return List_createSubsetReverse(list, index, length, Platform_instance.alloc, result);
}

Error List_createReversex(List list, List *result) { 
	return List_createSubsetReversex(list, 0, list.length, result); 
}

Error List_findx(List list, Buffer buf, List *result) {
	return List_find(list, buf, Platform_instance.alloc, result);
}

Error List_eraseAllx(List *list, Buffer buf) {
	return List_eraseAll(list, buf, Platform_instance.alloc);
}

Error List_insertx(List *list, U64 index, Buffer buf) {
	return List_insert(list, index, buf, Platform_instance.alloc);
}

Error List_pushAllx(List *list, List other) { return List_pushAll(list, other, Platform_instance.alloc); }

Error List_insertAllx(List *list, List other, U64 offset) {
	return List_insertAll(list, other, offset, Platform_instance.alloc);
}

Error List_reservex(List *list, U64 capacity) { return List_reserve(list, capacity, Platform_instance.alloc); }
Error List_resizex(List *list, U64 size) { return List_resize(list, size, Platform_instance.alloc); }
Error List_shrinkToFitx(List *list) { return List_shrinkToFit(list, Platform_instance.alloc); }

Error List_pushBackx(List *list, Buffer buf) { return List_pushBack(list, buf, Platform_instance.alloc); }
Bool List_freex(List *result) { return List_free(result, Platform_instance.alloc); }

//String

Bool String_freex(String *str) { return String_free(str, Platform_instance.alloc); }
Bool StringList_freex(StringList *arr) { return StringList_free(arr, Platform_instance.alloc); }

Error String_createx(C8 c, U64 size, String *result) { return String_create(c, size, Platform_instance.alloc, result); }
Error String_createCopyx(String str, String *result) { return String_createCopy(str, Platform_instance.alloc, result); }

Error String_createNytox(U64 v, Bool leadingZeros, String *result) {
	return String_createNyto(v, leadingZeros, Platform_instance.alloc, result);
}

Error String_createHexx(U64 v, Bool leadingZeros, String *result) {
	return String_createHex(v, leadingZeros, Platform_instance.alloc, result);
}

Error String_createDecx(U64 v, Bool leadingZeros, String *result) {
	return String_createDec(v, leadingZeros, Platform_instance.alloc, result);
}

Error String_createOctx(U64 v, Bool leadingZeros, String *result) {
	return String_createOct(v, leadingZeros, Platform_instance.alloc, result);
}

Error String_createBinx(U64 v, Bool leadingZeros, String *result) {
	return String_createBin(v, leadingZeros, Platform_instance.alloc, result);
}

Error String_splitx(String s, C8 c,  EStringCase casing,  StringList *result) {
	return String_split(s, c, casing, Platform_instance.alloc, result);
}

Error String_splitStringx(String s, String other, EStringCase casing, StringList *result) {
	return String_splitString(s, other, casing, Platform_instance.alloc, result);
}

Error String_splitLinex(String s, StringList *result) {
	return String_splitLine(s, Platform_instance.alloc, result);
}

Error String_resizex(String *str, U64 length, C8 defaultChar) {
	return String_resize(str, length, defaultChar, Platform_instance.alloc);
}

Error String_reservex(String *str, U64 length) { return String_reserve(str, length, Platform_instance.alloc); }

Error String_appendx(String *s, C8 c) { return String_append(s, c, Platform_instance.alloc); }
Error String_appendStringx(String *s, String other) { return String_appendString(s, other, Platform_instance.alloc); }

Error String_insertx(String *s, C8 c, U64 i) { return String_insert(s, c, i, Platform_instance.alloc); }

Error String_insertStringx(String *s, String other, U64 i) {
	return String_insertString(s, other, i, Platform_instance.alloc);
}

Error String_replaceAllStringx(String *s, String search, String replace, EStringCase caseSensitive) {
	return String_replaceAllString(s, search, replace, caseSensitive, Platform_instance.alloc);
}

Error String_replaceStringx(
	String *s, 
	String search, 
	String replace, 
	EStringCase caseSensitive,
	Bool isFirst
) {
	return String_replaceString(s, search, replace, caseSensitive, Platform_instance.alloc, isFirst);
}

Error String_replaceFirstStringx(String *s, String search, String replace, EStringCase caseSensitive) {
	return String_replaceStringx(s, search, replace, caseSensitive, true);
}

Error String_replaceLastStringx(String *s, String search, String replace, EStringCase caseSensitive) {
	return String_replaceStringx(s, search, replace, caseSensitive, false);
}

List String_findAllx(String s, C8 c, EStringCase caseSensitive) {
	return String_findAll(s, c, Platform_instance.alloc, caseSensitive);
}

List String_findAllStringx(String s, String other, EStringCase caseSensitive) {
	return String_findAllString(s, other, Platform_instance.alloc, caseSensitive);
}

Error StringList_createx(U64 length, StringList *result) {
	return StringList_create(length, Platform_instance.alloc, result);
}

Error StringList_createCopyx(const StringList *toCopy, StringList *arr) {
	return StringList_createCopy(toCopy, Platform_instance.alloc, arr);
}

Error StringList_setx(StringList arr, U64 i, String str) { return StringList_set(arr, i, str, Platform_instance.alloc); }
Error StringList_unsetx(StringList arr, U64 i) { return StringList_unset(arr, i, Platform_instance.alloc); }

Error StringList_combinex(StringList arr, String *result) {
	return StringList_combine(arr, Platform_instance.alloc, result);
}

Error StringList_concatx(StringList arr, C8 between, String *result) {
	return StringList_concat(arr, between, Platform_instance.alloc, result);
}

Error StringList_concatStringx(StringList arr, String between, String *result) {
	return StringList_concatString(arr, between, Platform_instance.alloc, result);
}

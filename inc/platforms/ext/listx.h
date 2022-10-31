#pragma once
#include "platforms/platform.h"
#include "types/list.h"

inline Error List_createx(U64 length, U64 stride, List *result) {
	return List_create(length, stride, Platform_instance.alloc, result);
}

inline Error List_createNullBytesx(U64 length, U64 stride, List *result) {
	return List_createNullBytes(length, stride, Platform_instance.alloc, result);
}

inline Error List_createRepeatedx(U64 length, U64 stride, Buffer data, List *result) {
	return List_createRepeated(length, stride, data, Platform_instance.alloc, result);
}

inline Error List_createCopyx(List list, List *result) {
	return List_createCopy(list, Platform_instance.alloc, result);
}

inline Error List_createSubsetReversex(
	List list,
	U64 index,
	U64 length,
	List *result
) {
	return List_createSubsetReverse(list, index, length, Platform_instance.alloc, result);
}

inline Error List_createReversex(List list, List *result) { 
	return List_createSubsetReversex(list, 0, list.length, result); 
}

inline Error List_findx(List list, Buffer buf, List *result) {
	return List_find(list, buf, Platform_instance.alloc, result);
}

inline Error List_eraseAllx(List *list, Buffer buf) {
	return List_eraseAll(list, buf, Platform_instance.alloc);
}

inline Error List_insertx(List *list, U64 index, Buffer buf) {
	return List_insert(list, index, buf, Platform_instance.alloc);
}

inline Error List_pushAllx(List *list, List other) {
	return List_pushAll(list, other, Platform_instance.alloc);
}

inline Error List_insertAllx(List *list, List other, U64 offset) {
	return List_insertAll(list, other, offset, Platform_instance.alloc);
}

inline Error List_reservex(List *list, U64 capacity) {
	return List_reserve(list, capacity, Platform_instance.alloc);
}

inline Error List_resizex(List *list, U64 size) {
	return List_resize(list, size, Platform_instance.alloc);
}

inline Error List_shrinkToFitx(List *list) {
	return List_shrinkToFit(list, Platform_instance.alloc);
}

inline Error List_pushBackx(List *list, Buffer buf) {
	return List_pushBack(list, buf, Platform_instance.alloc);
}

inline Error List_freex(List *result) {
	return List_free(result, Platform_instance.alloc);
}

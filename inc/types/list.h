#pragma once
#include "types.h"
#include "allocator.h"
#include "buffer.h"

//A POD list
//(Not applicable for types that resize)
//Allows both const and non const

typedef struct List {
	U8 *ptr;
	U64 stride, length, capacity;		//If capacity is 0 or U64_MAX, it indicates a ref
} List;

inline Bool List_isConstRef(List l) { return l.capacity == U64_MAX; }
inline Bool List_isRef(List l) { return !l.capacity || List_isConstRef(l); }

inline Bool List_empty(List l) { return !l.length; }
inline Bool List_any(List l) { return l.length; }
inline U64  List_bytes(List l) { return l.length * l.stride; }
inline U64  List_allocatedBytes(List l) { return List_isRef(l) ? 0 : l.capacity * l.stride; }

inline Buffer List_buffer(List l) { return Buffer_createRef(l.ptr, List_bytes(l)); }
inline Buffer List_allocatedBuffer(List l) { return Buffer_createRef(l.ptr, List_allocatedBytes(l)); }

inline U8 *List_begin(List list) { return List_isConstRef(list) ? NULL : list.ptr; }
inline U8 *List_end(List list) { return List_isConstRef(list) ? NULL : list.ptr + List_bytes(list); }

inline U8 *List_last(List list) { 
	return List_isConstRef(list) || !list.length ? NULL : list.ptr + List_bytes(list) - list.stride; 
}

inline const U8 *List_beginConst(List list) { return list.ptr; }
inline const U8 *List_endConst(List list) { return list.ptr + List_bytes(list); }
inline const U8 *List_lastConst(List list) { return !list.length ? NULL : list.ptr + List_bytes(list) - list.stride; }

inline const U8 *List_ptrConst(List list, U64 elementOffset) {
	return elementOffset < list.length ? list.ptr + elementOffset * list.stride : NULL;
}

inline U8 *List_ptr(List list, U64 elementOffset) {
	return !List_isConstRef(list) && elementOffset < list.length ? list.ptr + elementOffset * list.stride : NULL;
}

inline Buffer List_at(List list, U64 offset) {
	return offset < list.length ? Buffer_createRef(list.ptr + offset * list.stride, list.stride) : 
		Buffer_createNull();
}

Error List_eq(List a, List b, Bool *result);
Error List_neq(List a, List b, Bool *result);

inline List List_createEmpty(U64 stride) { return (List) { .stride = stride }; }
Error List_createFromBuffer(Buffer buf, U64 stride, Bool isConst, List *result);
Error List_createSubset(List list, U64 index, U64 length, List *result);

Error List_create(U64 length, U64 stride, Allocator allocator, List *result);
Error List_createNullBytes(U64 length, U64 stride, Allocator allocator, List *result);
Error List_createRepeated(U64 length, U64 stride, Buffer data, Allocator allocator, List *result);

Error List_createCopy(List list, Allocator allocator, List *result);
Error List_createSubsetReverse(
	List list, 
	U64 index,
	U64 length, 
	Allocator allocator, 
	List *result
);

inline Error List_createReverse(List list, Allocator allocator, List *result) { 
	return List_createSubsetReverse(list, 0, list.length, allocator, result); 
}

Error List_createRef(U8 *ptr, U64 length, U64 stride, List *result);
Error List_createConstRef(const U8 *ptr, U64 length, U64 stride, List *result);

Error List_set(List list, U64 index, Buffer buf);
Error List_get(List list, U64 index, Buffer *result);

Error List_copy(List src, U64 srcOffset, List dst, U64 dstOffset, U64 count);

Error List_swap(List l, U64 i, U64 j);
Bool List_reverse(List l);

//Find all occurrences in list
//Returns U64[]
//
Error List_find(List list, Buffer buf, Allocator allocator, List *result);

U64 List_findFirst(List list, Buffer buf, U64 index);
U64 List_findLast(List list, Buffer buf, U64 index);
U64 List_count(List list, Buffer buf);

inline Bool List_contains(List list, Buffer buf, U64 offset) { 
	return List_findFirst(list, buf, offset) != U64_MAX; 
}

Error List_eraseFirst(List *list, Buffer buf, U64 offset);
Error List_eraseLast(List *list, Buffer buf, U64 offset);
Error List_eraseAll(List *list, Buffer buf, Allocator allocator);
Error List_erase(List *list, U64 index);
Error List_eraseAllIndices(List *list, List indices);			//Sorts indices (U64[])
Error List_insert(List *list, U64 index, Buffer buf, Allocator allocator);
Error List_pushAll(List *list, List other, Allocator allocator);
Error List_insertAll(List *list, List other, U64 offset, Allocator allocator);

Error List_reserve(List *list, U64 capacity, Allocator allocator);
Error List_resize(List *list, U64 size, Allocator allocator);

Error List_shrinkToFit(List *list, Allocator allocator);

Bool List_sortU64(List list);
Bool List_sortU32(List list);
Bool List_sortU16(List list);
Bool List_sortU8(List list);

Bool List_sortI64(List list);
Bool List_sortI32(List list);
Bool List_sortI16(List list);
Bool List_sortI8(List list);

Bool List_sortF32(List list);

//Expects buf to be sized to stride (to allow copying to the stack)

Error List_popBack(List *list, Buffer output);
Error List_popLocation(List *list, U64 index, Buffer buf);

//

Error List_pushBack(List *list, Buffer buf, Allocator allocator);

Error List_clear(List *list);		//Doesn't remove data, only makes it unavailable

Error List_free(List *result, Allocator allocator);

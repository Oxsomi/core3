#pragma once
#include "types.h"
#include "allocator.h"
#include "buffer.h"

//A POD list
//(Not applicable for types that resize)
//Allows both const and non const

struct List {
	U8 *ptr;
	U64 stride, length, capacity;		//If capacity is 0 or U64_MAX, it indicates a ref
};

inline Bool List_isConstRef(struct List l) { return l.capacity == U64_MAX; }
inline Bool List_isRef(struct List l) { return !l.capacity || List_isConstRef(l); }

inline Bool List_empty(struct List l) { return !l.length; }
inline Bool List_any(struct List l) { return l.length; }
inline U64  List_bytes(struct List l) { return l.length * l.stride; }
inline U64  List_allocatedBytes(struct List l) { return List_isRef(l) ? 0 : l.capacity * l.stride; }

inline struct Buffer List_buffer(struct List l) { return Buffer_createRef(l.ptr, List_bytes(l)); }
inline struct Buffer List_allocatedBuffer(struct List l) { return Buffer_createRef(l.ptr, List_allocatedBytes(l)); }

inline U8 *List_begin(struct List list) { return List_isConstRef(list) ? NULL : list.ptr; }
inline U8 *List_end(struct List list) { return List_isConstRef(list) ? NULL : list.ptr + List_bytes(list); }
inline U8 *List_last(struct List list) { return List_isConstRef(list) || !list.length ? NULL : list.ptr + List_bytes(list) - list.stride; }

inline const U8 *List_beginConst(struct List list) { return list.ptr; }
inline const U8 *List_endConst(struct List list) { return list.ptr + List_bytes(list); }
inline const U8 *List_lastConst(struct List list) { return !list.length ? NULL : list.ptr + List_bytes(list) - list.stride; }

inline const U8 *List_ptrConst(struct List list, U64 elementOffset) {
	return elementOffset < list.length ? list.ptr + elementOffset * list.stride : NULL;
}

inline U8 *List_ptr(struct List list, U64 elementOffset) {
	return !List_isConstRef(list) && elementOffset < list.length ? list.ptr + elementOffset * list.stride : NULL;
}

inline struct Buffer List_at(struct List list, U64 offset) {
	return offset < list.length ? Buffer_createRef(list.ptr + offset * list.stride, list.stride) : Buffer_createNull();
}

struct Error List_eq(struct List a, struct List b, Bool *result);
struct Error List_neq(struct List a, struct List b, Bool *result);

struct Error List_create(U64 length, U64 stride, struct Allocator allocator, struct List *result);
struct Error List_createNullBytes(U64 length, U64 stride, struct Allocator allocator, struct List *result);
struct Error List_createRepeated(U64 length, U64 stride, struct Buffer data, struct Allocator allocator, struct List *result);
inline struct List List_createEmpty(U64 stride) { return (struct List) { .stride = stride }; }

struct Error List_createFromBuffer(struct Buffer buf, U64 stride, Bool isConst, struct List *result);

struct Error List_createCopy(struct List list, struct Allocator allocator, struct List *result);
struct Error List_createSubset(struct List list, U64 index, U64 length, struct List *result);
struct Error List_createSubsetReverse(
	struct List list, 
	U64 index, U64 length, 
	struct Allocator allocator, 
	struct List *result
);

inline struct Error List_createReverse(struct List list, struct Allocator allocator, struct List *result) { 
	return List_createSubsetReverse(list, 0, list.length, allocator, result); 
}

struct Error List_createRef(U8 *ptr, U64 length, U64 stride, struct List *result);
struct Error List_createConstRef(const U8 *ptr, U64 length, U64 stride, struct List *result);

struct Error List_set(struct List list, U64 index, struct Buffer buf);
struct Error List_get(struct List list, U64 index, struct Buffer *result);

struct Error List_copy(struct List src, U64 srcOffset, struct List dst, U64 dstOffset, U64 count);

struct Error List_swap(struct List l, U64 i, U64 j);
Bool List_reverse(struct List l);

//Find all occurrences in list
//Returns U64[]
//
struct Error List_find(struct List list, struct Buffer buf, struct Allocator allocator, struct List *result);

U64 List_findFirst(struct List list, struct Buffer buf, U64 index);
U64 List_findLast(struct List list, struct Buffer buf, U64 index);
U64 List_count(struct List list, struct Buffer buf);

inline Bool List_contains(struct List list, struct Buffer buf, U64 offset) { 
	return List_findFirst(list, buf, offset) != U64_MAX; 
}

struct Error List_eraseFirst(struct List *list, struct Buffer buf, U64 offset);
struct Error List_eraseLast(struct List *list, struct Buffer buf, U64 offset);
struct Error List_eraseAll(struct List *list, struct Buffer buf, struct Allocator allocator);
struct Error List_erase(struct List *list, U64 index);
struct Error List_eraseAllIndices(struct List *list, struct List indices);			//Sorts indices (U64[])
struct Error List_insert(struct List *list, U64 index, struct Buffer buf, struct Allocator allocator);
struct Error List_pushAll(struct List *list, struct List other, struct Allocator allocator);
struct Error List_insertAll(struct List *list, struct List other, U64 offset, struct Allocator allocator);

struct Error List_reserve(struct List *list, U64 capacity, struct Allocator allocator);
struct Error List_resize(struct List *list, U64 size, struct Allocator allocator);

struct Error List_shrinkToFit(struct List *list, struct Allocator allocator);

Bool List_sortU64(struct List list);
Bool List_sortU32(struct List list);
Bool List_sortU16(struct List list);
Bool List_sortU8(struct List list);

Bool List_sortI64(struct List list);
Bool List_sortI32(struct List list);
Bool List_sortI16(struct List list);
Bool List_sortI8(struct List list);

Bool List_sortF32(struct List list);

//Expects buf to be sized to stride (to allow copying to the stack)

struct Error List_popBack(struct List *list, struct Buffer output);
struct Error List_popLocation(struct List *list, U64 index, struct Buffer buf);

//

struct Error List_pushBack(struct List *list, struct Buffer buf, struct Allocator allocator);

struct Error List_clear(struct List *list);		//Doesn't remove data, only makes it unavailable

struct Error List_free(struct List *result, struct Allocator allocator);

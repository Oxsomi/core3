#include "types/list.h"
#include "types/error.h"
#include "types/buffer.h"

Error List_eq(List a, List b, Bool *result) {
	return Buffer_eq(List_buffer(a), List_buffer(b), result);
}

Error List_neq(List a, List b, Bool *result) {
	return Buffer_neq(List_buffer(a), List_buffer(b), result);
}

Error List_create(U64 length, U64 stride, Allocator allocator, List *result) {

	if(!result)
		return Error_nullPointer(3, 0);

	if(!length || !stride)
		return Error_invalidParameter(!length ? 0 : 1, 0, 0);

	if(length * stride / stride != length)
		return Error_overflow(0, 0, length * stride, U64_MAX);

	Buffer buf = Buffer_createNull();
	Error err = Buffer_createUninitializedBytes(length * stride, allocator, &buf);

	if(err.genericError)
		return err;

	*result = (List) { 
		.ptr = buf.ptr, 
		.length = length, 
		.stride = stride,
		.capacity = length
	};

	return Error_none();
}

Error List_createFromBuffer(Buffer buf, U64 stride, Bool isConst, List *result) {

	if(!result)
		return Error_nullPointer(3, 0);

	if(!buf.ptr || !stride)
		return Error_invalidParameter(!buf.ptr ? 0 : 1, 0, 0);

	if(buf.length % stride)
		return Error_invalidParameter(0, 0, 1);

	*result = (List) { 
		.ptr = buf.ptr, 
		.length = buf.length / stride, 
		.stride = stride,
		.capacity = isConst ? U64_MAX : 0
	};

	return Error_none();
}

Error List_createNullBytes(U64 length, U64 stride, Allocator allocator, List *result) {

	if(!result)
		return Error_nullPointer(3, 0);

	if(!length || !stride)
		return Error_invalidParameter(!length ? 0 : 1, 0, 0);

	if(length * stride / stride != length)
		return Error_overflow(0, 0, length * stride, U64_MAX);

	Buffer buf = Buffer_createNull();
	Error err = Buffer_createEmptyBytes(length * stride, allocator, &buf);

	if(err.genericError)
		return err;

	*result = (List) { 
		.ptr = buf.ptr, 
		.length = length, 
		.stride = stride,
		.capacity = length
	};

	return Error_none();
}

Error List_createRepeated(
	U64 length,
	U64 stride,
	Buffer data,
	Allocator allocator,
	List *result
) {

	if(!result)
		return Error_nullPointer(4, 0);

	if(!length || !stride)
		return Error_invalidParameter(!length ? 0 : 1, 0, 0);

	if(length * stride / stride != length)
		return Error_overflow(0, 0, length * stride, U64_MAX);

	Buffer buf = Buffer_createNull();
	Error err = Buffer_createUninitializedBytes(length * stride, allocator, &buf);

	if(err.genericError)
		return err;

	*result = (List) { 
		.ptr = buf.ptr, 
		.length = length, 
		.stride = stride,
		.capacity = length
	};

	for(U64 i = 0; i < length; ++i)
		if((err = List_set(*result, i, data)).genericError) {
			Buffer_free(&buf, allocator);
			return err;
		}

	return Error_none();
}

Error List_createCopy(List list, Allocator allocator, List *result) {

	if(List_empty(list)) {

		if(result) {
			*result = List_createEmpty(list.stride);
			return Error_none();
		}

		return Error_nullPointer(2, 0);
	}

	Error err = List_create(list.length, list.stride, allocator, result);

	if(err.genericError)
		return err;

	Buffer_copy(List_buffer(*result), List_buffer(list));
	return Error_none();
}

Error List_createSubset(List list, U64 index, U64 length, List *result) {

	if(!result)
		return Error_nullPointer(3, 0);

	if(index + length < index)
		return Error_overflow(1, 0, index + length, U64_MAX);

	if(index + length > list.length)
		return Error_outOfBounds(1, 0, index + length, list.length);

	if(List_isConstRef(list))
		return List_createConstRef(list.ptr + index * list.stride, length, list.stride, result);

	return List_createRef(list.ptr + index * list.stride, length, list.stride, result);
}

Error List_createSubsetReverse(
	List list,
	U64 index,
	U64 length,
	Allocator allocator,
	List *result
) {
	
	if(!result)
		return Error_nullPointer(4, 0);

	if(index + length < length)
		return Error_overflow(1, 0, index + length, U64_MAX);

	if(index + length > list.length)
		return Error_outOfBounds(1, 0, index + length, list.length);

	Error err = List_create(length, list.stride, allocator, result);

	if(err.genericError)
		return err;

	for(U64 last = index + length - 1, i = 0; last != index - 1; --last, ++i) {

		Buffer buf = Buffer_createNull();

		if((err = List_get(list, last, &buf)).genericError) {
			List_free(result, allocator);
			return err;
		}

		if((err = List_set(*result, i, buf)).genericError) {
			List_free(result, allocator);
			return err;
		}
	}

	return Error_none();
}

Error List_createRef(U8 *ptr, U64 length, U64 stride, List *result) {

	if(!ptr || !result)
		return Error_nullPointer(!ptr ? 0 : 3, 0);

	if(!length || !stride)
		return Error_invalidParameter(!length ? 1 : 2, 0, 0);

	if(length * stride / stride != length)
		return Error_overflow(1, 0, length * stride, U64_MAX);

	*result = (List) { 
		.ptr = ptr, 
		.length = length, 
		.stride = stride 
	};

	return Error_none();
}

Error List_createConstRef(const U8 *ptr, U64 length, U64 stride, List *result) {

	if(!ptr || !result)
		return Error_nullPointer(!ptr ? 0 : 3, 0);

	if(!length || !stride)
		return Error_invalidParameter(!length ? 1 : 2, 0, 0);

	if(length * stride / stride != length)
		return Error_overflow(1, 0, length * stride, U64_MAX);

	*result = (List) { 
		.ptr = (U8*) ptr, 
		.length = length, 
		.stride = stride, 
		.capacity = U64_MAX 
	};

	return Error_none();
}

Error List_set(List list, U64 index, Buffer buf) {

	if(List_isConstRef(list))
		return Error_constData(0, 0);

	if(index >= list.length)
		return Error_outOfBounds(1, 0, index, list.length);

	if(buf.length && buf.length != list.stride)
		return Error_invalidOperation(0);

	if(buf.length)
		Buffer_copy(Buffer_createRef(list.ptr + index * list.stride, list.stride), buf);

	else Buffer_unsetAllBits(Buffer_createRef(list.ptr + index * list.stride, list.stride));

	return Error_none();
}

Error List_get(List list, U64 index, Buffer *result) {

	if(!result)
		return Error_nullPointer(2, 0);

	if(index >= list.length)
		return Error_outOfBounds(1, 0, index, list.length);

	*result = Buffer_createRef(list.ptr + index * list.stride, list.stride);
	return Error_none();
}

Error List_find(List list, Buffer buf, Allocator allocator, List *result) {

	if(!buf.ptr || !buf.length)
		return Error_nullPointer(1, 0);

	if(!result)
		return Error_nullPointer(3, 0);

	*result = List_createEmpty(sizeof(U64));
	Error err = List_reserve(result, list.length / 100 + 16, allocator);

	if(err.genericError)
		return err;

	for(U64 i = 0; i < list.length; ++i) {

		Bool b = false;
		err = Buffer_eq(List_at(list, i), buf, &b);

		if(err.genericError) {
			List_free(result, allocator);
			return err;
		}

		if(b && (err = List_pushBack(result, Buffer_createRef(&i, sizeof(i)), allocator)).genericError) {
			List_free(result, allocator);
			return err;
		}
	}

	return Error_none();
}

U64 List_findFirst(List list, Buffer buf, U64 index) {

	if(buf.length != list.stride)
		return U64_MAX;

	Error err;

	for(U64 i = index; i < list.length; ++i) {

		Bool b = false;
		err = Buffer_eq(List_at(list, i), buf, &b);

		if(err.genericError)
			return U64_MAX;

		if(b)
			return i;
	}

	return U64_MAX;
}

U64 List_findLast(List list, Buffer buf, U64 index) {

	if(buf.length != list.stride)
		return U64_MAX;

	Error err;

	for(U64 i = list.length; i != U64_MAX && i >= index; --i) {

		Bool b = false;
		err = Buffer_eq(List_at(list, i), buf, &b);

		if(err.genericError)
			return U64_MAX;

		if(b)
			return i;
	}

	return U64_MAX;
}

U64 List_count(List list, Buffer buf) {

	if(buf.length != list.stride)
		return U64_MAX;

	U64 count = 0;
	Error err;

	for(U64 i = 0; i < list.length; ++i) {

		Bool b = false;
		err = Buffer_eq(List_at(list, i), buf, &b);

		if(err.genericError)
			return U64_MAX;

		if(b)
			++count;
	}

	return count;
}

Error List_copy(List src, U64 srcOffset, List dst, U64 dstOffset, U64 count) {

	Error err = List_createSubset(src, srcOffset, count, &src);

	if(err.genericError)
		return err;

	if(List_isConstRef(dst))
		return Error_constData(2, 0);

	if((err = List_createSubset(dst, dstOffset, count, &dst)).genericError)
		return err;

	Buffer_copy(List_buffer(dst), List_buffer(src));
	return Error_none();
}

Error List_shrinkToFit(List *list, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0, 0);

	if(List_isConstRef(*list))
		return Error_constData(0, 0);

	if(list->capacity == list->length)
		return Error_none();

	List copy = List_createEmpty(list->stride);
	Error err = List_createCopy(*list, allocator, &copy);

	if(err.genericError)
		return err;

	err = List_free(list, allocator);
	*list = copy;

	return err;
}

Error List_eraseAllIndices(List *list, List indices) {

	if(!list)
		return Error_nullPointer(0, 0);

	if(List_isConstRef(*list))
		return Error_constData(0, 0);

	if(!indices.length || !list->length)
		return Error_none();

	if(indices.stride != sizeof(U64))
		return Error_invalidOperation(0);

	if(!List_sortU64(indices))
		return Error_invalidParameter(1, 0, 0);

	//Ensure none of them reference out of bounds or are duplicate

	U64 last = U64_MAX;

	for(U64 *ptr = (U64*)indices.ptr, *end = (U64*)List_end(indices); ptr < end; ++ptr) {

		if(last == *ptr)
			return Error_alreadyDefined(0);

		if(*ptr >= list->length)
			return Error_outOfBounds(0, 0, ptr - (U64*)indices.ptr, list->length);

		last = *ptr;
	}

	//Since we're sorted from small to big, 
	//we can just easily fetch where our next block of memory should go and move it backwards

	U64 curr = 0;

	for (U64 *ptr = (U64*)indices.ptr, *end = (U64*)List_end(indices); ptr < end; ++ptr) {

		if(!curr)
			curr = *ptr;

		U64 me = *ptr + 1;
		U64 neighbor = ptr + 1 != end ? *(ptr + 1) : list->length;

		if(me == neighbor)
			continue;

		Buffer_revCopy(
			Buffer_createRef(list->ptr + curr * list->stride, (neighbor - me) * list->stride),
			Buffer_createRef(list->ptr + me * list->stride, (neighbor - me) * list->stride)
		);

		curr += neighbor - me;
	}

	list->length = curr;
	return Error_none();
}

//Easy insertion sort
//Uses about 8KB of cache on stack to hopefully sort quite quickly.
//Does mean that this can only be used for lists < 8KB.

#define TList_insertionSort(T) Bool List_##T##_insertionSort(List list) {						\
																								\
	U8 sorted[8192];		/* for U8[8192] -> U64[1024]. Fits neatly into cache */				\
																								\
	const T *tarr = (const T*) List_ptrConst(list, 0);								\
	T *tsorted = (T*) sorted;																	\
																								\
	*tsorted = *tarr;	/* Init first element */												\
																								\
	for(U64 j = 1; j < list.length; ++j) {														\
																								\
		T t = tarr[j];																			\
		U64 k = 0;																				\
																								\
		/* Check everything that came before us for order */									\
																								\
		for(; k < j; ++k)																		\
			if (t < tsorted[k]) {																\
																								\
				/* Move to the right */															\
																								\
				for (U64 l = j; l != k; --l)													\
					tsorted[l] = tsorted[l - 1];												\
																								\
				break;																			\
			}																					\
																								\
		/* Now that everything's moved, we can insert ourselves */								\
																								\
		tsorted[k] = t;																			\
	}																							\
																								\
	Buffer_copy(List_buffer(list), Buffer_createRef(sorted, sizeof(sorted)));				\
	return true;																				\
}

#define TList_sorts(f) f(U64); f(I64); f(U32); f(I32); f(U16); f(I16); f(U8); f(I8); f(F32);

TList_sorts(TList_insertionSort);

//https://stackoverflow.com/questions/33884057/quick-sort-stackoverflow-error-for-large-arrays
//qsort U64 should be taking about ~76ms / 1M elem and ~13ms / 1M (sorted)
//qsort U8 should be taking about half the time for unsorted because of cache speedups
//Expect F32 sorting to be at least 2x to 5x slower

#define TList_quickSort(T)																\
																						\
inline U64 List_##T##_qpartition(List list, U64 begin, U64 last) {						\
																						\
	T t = *((const T*)list.ptr + (begin + last) / 2);									\
	U64 i = begin - 1, j = last + 1;													\
																						\
	while (true) {																		\
																						\
		while(++i < last && *((const T*)list.ptr + i) < t);								\
		while(--j > begin && *((const T*)list.ptr + j) > t);							\
																						\
		if(i < j) {																		\
			T temp = *((const T*)list.ptr + i);											\
			*((T*)list.ptr + i) = *((const T*)list.ptr + j);							\
			*((T*)list.ptr + j) = temp;													\
		}																				\
																						\
		else return j;																	\
	}																					\
}																						\
																						\
inline Bool List_##T##_qsortRecurse(List list, U64 begin, U64 end) {					\
																						\
	if(begin >> 63 || end >> 63)														\
		return false;																	\
																						\
	while(begin < end && end != U64_MAX) {												\
																						\
		U64 pivot = List_##T##_qpartition(list, begin, end);							\
																						\
		if ((I64)pivot - begin <= (I64)end - (pivot + 1)) {								\
			List_##T##_qsortRecurse(list, begin, pivot);								\
			begin = pivot + 1;															\
			continue;																	\
		}																				\
																						\
		List_##T##_qsortRecurse(list, pivot + 1, end);									\
		end = pivot;																	\
	}																					\
																						\
	return true;																		\
}																						\
																						\
inline Bool List_##T##_qsort(List list) { return List_##T##_qsortRecurse(list, 0, list.length - 1); }

TList_sorts(TList_quickSort);

#define TList_sort(T) 																	\
Bool List_sort##T(List list) {															\
																						\
	if(list.length <= 1)																\
		return true;																	\
																						\
	if(List_bytes(list) <= 8192)														\
		return List_##T##_insertionSort(list);											\
																						\
	return List_##T##_qsort(list);														\
}

TList_sorts(TList_sort);

Error List_eraseFirst(List *list, Buffer buf, U64 offset) {

	if(!list)
		return Error_nullPointer(0, 0);

	U64 ind = List_findLast(*list, buf, offset);
	return ind == U64_MAX ? Error_none() : List_erase(list, ind);
}

Error List_eraseLast(List *list, Buffer buf, U64 offset) {

	if(!list)
		return Error_nullPointer(0, 0);

	U64 ind = List_findLast(*list, buf, offset);
	return ind == U64_MAX ? Error_none() : List_erase(list, ind);
}

Error List_eraseAll(List *list, Buffer buf, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0, 0);
	
	List indices = (List) { 0 };
	Error err = List_find(*list, buf, allocator, &indices);

	if(err.genericError)
		return err;

	err = List_eraseAllIndices(list, indices);

	Error err1 = List_free(&indices, allocator);
	return err.genericError ? err : err1;
}

Error List_erase(List *list, U64 index) {

	if(!list)
		return Error_nullPointer(0, 0);

	if(index >= list->length)
		return Error_outOfBounds(1, 0, index, list->length);

	Buffer_revCopy(
		Buffer_createRef(list->ptr + index * list->stride, (list->length - 1) * list->stride),
		Buffer_createRef(list->ptr + (index + 1) * list->stride, (list->length - 1) * list->stride)
	);

	--list->length;
	return Error_none();
}

Error List_insert(List *list, U64 index, Buffer buf, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0, 0);

	if(list->stride != buf.length)
		return Error_invalidOperation(0);

	if(index >= list->length)
		return Error_outOfBounds(1, 0, index, list->length);

	U64 prevSize = list->length;
	Error err = List_resize(list, list->length + 1, allocator);

	if(err.genericError)
		return err;

	//Copy our own data to the right

	if(prevSize)
		Buffer_revCopy(
			Buffer_createRef(list->ptr + (index + 1) * list->stride, (prevSize - index) * list->stride), 
			Buffer_createRef(list->ptr + index * list->stride, (prevSize - index) * list->stride)
		);

	//Copy the other data

	Buffer_copy(
		Buffer_createRef(list->ptr + index * list->stride, list->stride), 
		buf
	);

	return Error_none();
}

Error List_pushAll(List *list, List other, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0, 0);

	if(list->stride != other.stride)
		return Error_invalidOperation(0);

	if(!other.length)
		return Error_none();

	U64 oldSize = List_bytes(*list);
	Error err = List_resize(list, list->length + other.length, allocator);

	if(err.genericError)
		return err;

	Buffer_copy(
		Buffer_createRef(list->ptr + oldSize, List_bytes(other)),
		Buffer_createRef(other.ptr, List_bytes(other))
	);

	return Error_none();
}

Error List_swap(List l, U64 i, U64 j) {

	if(i >= l.length || j >= l.length)
		return Error_outOfBounds(i >= l.length ? 1 : 2, 0, i >= l.length ? i : j, l.length);

	if(List_isConstRef(l))
		return Error_constData(0, 0);

	U8 *iptr = List_ptr(l, i);
	U8 *jptr = List_ptr(l, j);

	U64 off = 0;

	for (; off + 7 < l.stride; off += 8) {
		U64 t = *(const U64*)(iptr + off);
		*(U64*)(iptr + off) = *(const U64*)(jptr + off);
		*(U64*)(jptr + off) = t;
	}

	for (; off < l.stride; ++off) {
		U8 t = iptr[off];
		iptr[off] = jptr[off];
		jptr[off] = t;
	}

	return Error_none();
}

Bool List_reverse(List l) {

	if(l.length <= 1)
		return true;

	if(List_isConstRef(l))
		return false;

	if (l.length == 2) {
		List_swap(l, 0, 1);
		return true;
	}

	for (U64 i = 0; i < l.length / 2; ++i) {
		U64 j = l.length - 1 - i;
		List_swap(l, i, j);
	}

	return true;
}

Error List_insertAll(List *list, List other, U64 offset, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0, 0);

	if(!other.length)
		return Error_none();

	if(list->stride != other.stride)
		return Error_invalidOperation(0);

	if(list->length + other.length < list->length)
		return Error_overflow(0, 0, list->length + other.length, U64_MAX);

	if(offset >= list->length)
		return Error_outOfBounds(2, 0, offset, list->length);

	U64 prevSize = list->length;
	Error err = List_resize(list, list->length + other.length, allocator);

	if(err.genericError)
		return err;

	//Copy our own data to the right

	if(prevSize)
		Buffer_revCopy(
			Buffer_createRef(list->ptr + (offset + other.length) * list->stride, (prevSize - offset) * list->stride), 
			Buffer_createRef(list->ptr + offset * list->stride, (prevSize - offset) * list->stride)
		);

	//Copy the other data

	Buffer_copy(
		Buffer_createRef(list->ptr + offset * list->stride, other.length * list->stride), 
		List_buffer(other)
	);

	return Error_none();
}

Error List_reserve(List *list, U64 capacity, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0, 0);

	if(List_isRef(*list) && list->length)
		return Error_invalidOperation(0);

	if(capacity * list->stride / list->stride != capacity)
		return Error_overflow(1, 0, capacity * list->stride, U64_MAX);

	if(capacity <= list->capacity)
		return Error_none();

	Buffer buffer = Buffer_createNull();
	Error err = Buffer_createUninitializedBytes(capacity * list->stride, allocator, &buffer);

	if(err.genericError)
		return err;

	Buffer_copy(buffer, List_buffer(*list));

	Buffer curr = List_allocatedBuffer(*list);

	if(curr.length)
		err = Buffer_free(&curr, allocator);

	list->ptr = buffer.ptr;
	list->capacity = capacity;

	return err;
}

Error List_resize(List *list, U64 size, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0, 0);

	if(List_isConstRef(*list))
		return Error_constData(0, 0);

	if (size <= list->capacity) {
		list->length = size;
		return Error_none();
	}

	if(size * 3 / 3 != size)
		return Error_overflow(1, 0, size * 3, U64_MAX);

	Error err = List_reserve(list, size * 3 / 2, allocator);

	if(err.genericError)
		return err;

	list->length = size;
	return Error_none();
}

Error List_pushBack(List *list, Buffer buf, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0, 0);

	if(List_isConstRef(*list))
		return Error_constData(0, 0);

	Error err = List_resize(list, list->length + 1, allocator);

	if(err.genericError)
		return err;
	
	return List_set(*list, list->length - 1, buf);
}

Error List_popLocation(List *list, U64 index, Buffer buf) {

	if(!list)
		return Error_nullPointer(0, 0);

	if(index >= list->length)
		return Error_outOfBounds(1, 0, index, list->length);

	if(List_isConstRef(*list))
		return Error_constData(0, 0);

	Buffer result = Buffer_createNull();
	Error err = List_get(*list, index, &result);

	if(err.genericError)
		return err;

	if(buf.length != result.length)
		return Error_invalidOperation(0);

	Buffer_copy(buf, result);
	return List_erase(list, index);
}

Error List_popBack(List *list, Buffer output) {

	if(!list)
		return Error_nullPointer(0, 0);

	if(!list->length)
		return Error_outOfBounds(0, 0, 0, 0);

	if(output.length && output.length != list->stride)
		return Error_invalidOperation(0);

	if(List_isConstRef(*list))
		return Error_constData(0, 0);

	if(output.length)
		Buffer_copy(output, Buffer_createRef(list->ptr + (list->length - 1) * list->stride, list->stride));

	--list->length;
	return Error_none();
}

Error List_clear(List *list) {

	if(!list)
		return Error_nullPointer(0, 0);

	list->length = 0;
	return Error_none();
}

Error List_free(List *result, Allocator allocator) {

	if(!result)
		return Error_none();

	Error err = Error_none();

	if (!List_isRef(*result)) {
		Buffer buf = List_allocatedBuffer(*result);
		err = Buffer_free(&buf, allocator);
	}

	*result = List_createEmpty(result->stride);
	return err;
}

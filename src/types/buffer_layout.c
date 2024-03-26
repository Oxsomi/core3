/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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

#include "types/buffer_layout.h"
#include "types/allocator.h"
#include "types/list_impl.h"
#include "types/math.h"

TListImpl(BufferLayoutMemberInfo);
TListImpl(BufferLayoutStruct);

//BufferLayoutMember

U64 BufferLayoutMember_getOffset(BufferLayoutMember member) {
	return ((U64)(member.offsetHiAndIsStruct & 0x7FFF) << 32) | member.offsetLo;
}

Bool BufferLayoutMember_isStruct(BufferLayoutMember member) {
	return member.offsetHiAndIsStruct >> 15;
}

ETypeId BufferLayoutMember_getTypeId(BufferLayoutMember member) {
	return BufferLayoutMember_isStruct(member) ? ETypeId_Undefined : member.typeIdUnion;
}

U32 BufferLayoutMember_getStructId(BufferLayoutMember member) {
	return BufferLayoutMember_isStruct(member) ? member.structIdUnion : U32_MAX;
}

//BufferLayoutMemberInfo

BufferLayoutMemberInfo BufferLayoutMemberInfo_create(ETypeId typeId, CharString name, U64 offset, U32 stride) {
	return (BufferLayoutMemberInfo) {
		.typeId = typeId,
		.structId = U32_MAX,
		.name = name,
		.offset = offset,
		.stride = stride
	};
}

BufferLayoutMemberInfo BufferLayoutMemberInfo_createStruct(U32 structId, CharString name, U64 offset, U32 stride) {
	return (BufferLayoutMemberInfo) {
		.typeId = ETypeId_Undefined,
		.structId = structId,
		.name = name,
		.offset = offset,
		.stride = stride
	};
}

BufferLayoutMemberInfo BufferLayoutMemberInfo_createArray(
	ETypeId typeId,
	CharString name,
	ListU32 arraySizes,
	U64 offset,
	U32 stride
) {
	return (BufferLayoutMemberInfo) {
		.typeId = typeId,
		.structId = U32_MAX,
		.name = name,
		.arraySizes = arraySizes,
		.offset = offset,
		.stride = stride
	};
}

BufferLayoutMemberInfo BufferLayoutMemberInfo_createStructArray(
	U32 structId,
	CharString name,
	ListU32 arraySizes,
	U64 offset,
	U32 stride
) {
	return (BufferLayoutMemberInfo) {
		.typeId = ETypeId_Undefined,
		.structId = structId,
		.name = name,
		.arraySizes = arraySizes,
		.offset = offset,
		.stride = stride
	};
}

//Manipulating data

U64 BufferLayoutStruct_allocatedData(BufferLayoutStructInfo info, U64 memberDataLen) {

	return
		sizeof(C8) * CharString_length(info.name) +
		sizeof(BufferLayoutMember) * info.members.length +
		memberDataLen;
}

Error BufferLayoutStruct_create(BufferLayoutStructInfo info, U32 id, Allocator alloc, BufferLayoutStruct *layoutStruct) {

	if(!layoutStruct)
		return Error_nullPointer(1, "BufferLayoutStruct_create()::layoutStruct is required");

	if(layoutStruct->data.ptr)
		return Error_invalidParameter(
			1, 0, "BufferLayoutStruct_create()::layoutStruct wasn't empty, indicating a possible memleak"
		);

	if(id == U32_MAX)
		return Error_invalidParameter(2, 0, "BufferLayoutStruct_create()::id is invalid");

	if(CharString_length(info.name) > U8_MAX)
		return Error_invalidParameter(0, 0, "BufferLayoutStruct_create()::info.name is limited to 255 C8s (chars)");

	if(info.members.length >= U16_MAX)
		return Error_invalidParameter(0, 1, "BufferLayoutStruct_create()::info.members is limited to 65534");

	U64 memberDataLen = 0;

	for(U16 i = 0; i < (U16) info.members.length; ++i) {

		const BufferLayoutMemberInfo m = info.members.ptr[i];

		if(!CharString_length(m.name) || CharString_length(m.name) > U8_MAX || !CharString_isValidAscii(m.name))
			return Error_invalidParameter(
				0, 0 + (((U32)i + 1) << 16),
				"BufferLayoutStruct_create()::info.members[i].name should have <0, 255] characters"
			);

		if(m.arraySizes.length > U8_MAX)
			return Error_invalidParameter(
				0, 1 + (((U32)i + 1) << 16), "BufferLayoutStruct_create()::info.members[i].arraySizes.length is limited to 255"
			);

		if(((U32)m.typeId == U32_MAX) && (m.structId == U32_MAX))
			return Error_invalidParameter(
				0, 2 + (((U32)i + 1) << 16), "BufferLayoutStruct_create()::info.members[i].typeId or structId is invalid"
			);

		if(((U32)m.typeId != U32_MAX) && (m.structId != U32_MAX))
			return Error_invalidParameter(
				0, 2 + (((U32)i + 1) << 16),
				"BufferLayoutStruct_create()::info.members[i] can either be POD or struct, not both"
			);

		if(m.structId != U32_MAX && m.structId >= id)
			return Error_invalidParameter(
				0, 3 + (((U32)i + 1) << 16), "BufferLayoutStruct_create()::info.members[i].structId is invalid"
			);

		if(m.offset >> 47)
			return Error_invalidParameter(
				0, 4 + (((U32)i + 1) << 16), "BufferLayoutStruct_create()::info.members[i].offset is out of bounds"
			);

		U64 totalLen = 1;

		for(U64 j = 0; j < m.arraySizes.length; ++j) {

			const U32 leni = m.arraySizes.ptr[j];

			//An array length of 0 is only allowed if there's one element

			if(!leni && !(!j && m.arraySizes.length == 1))
				return Error_invalidParameter(
					0, 1 + (((U32)j + 1) << 16), "BufferLayoutStruct_create()::info.members[i].arraySizes[j] is invalid"
				);

			const U64 prevLen = totalLen;
			totalLen *= leni;

			if(totalLen < prevLen)
				return Error_overflow((((U32)j + 1) << 16), totalLen, prevLen, "BufferLayoutStruct_create() overflow");
		}

		const U64 prevLen = totalLen;
		totalLen *= m.stride;

		if(totalLen < prevLen || totalLen >> 48)
			return Error_overflow(
				(((U32)i + 1) << 16), totalLen, ((U64) 1 << 48) - 1, "BufferLayoutStruct_create() overflow (2)"
			);

		memberDataLen += CharString_length(m.name) + ListU32_bytes(m.arraySizes);
	}

	const U64 len = BufferLayoutStruct_allocatedData(info, memberDataLen);
	Buffer allocated = Buffer_createNull();

	const Error err = Buffer_createUninitializedBytes(len, alloc, &allocated);

	if(err.genericError)
		return err;

	//Fill layout struct info

	*layoutStruct = (BufferLayoutStruct) {

		.id = id,

		.memberCount = (U16) info.members.length,
		.nameLength = (U8) CharString_length(info.name),

		.data = allocated
	};

	//Fill layout struct data

	allocated = Buffer_createRefFromBuffer(allocated, false);

	Buffer_copy(allocated, CharString_bufferConst(info.name));
	Buffer_offset(&allocated, CharString_bytes(info.name));

	//members: BufferLayoutMember[memberCount];
	//memberData: (U32[members[i].arrayIndices], C8[members[i].nameLength])[arrayIndices];

	BufferLayoutMember *members = (BufferLayoutMember*) allocated.ptr;
	Buffer_offset(&allocated, sizeof(BufferLayoutMember) * info.members.length);

	for(U16 i = 0; i < (U16) info.members.length; ++i) {

		const BufferLayoutMemberInfo m = info.members.ptr[i];

		members[i] = (BufferLayoutMember) {

			.structIdUnion = m.structId != U32_MAX ? m.structId : (U32) m.typeId,

			.offsetLo = (U32) m.offset,
			.offsetHiAndIsStruct = (U16) (m.offset >> 32) | (m.structId != U32_MAX ? 1 << 15 : 0),

			.arrayIndices = (U8) m.arraySizes.length,
			.nameLength = (U8) CharString_length(m.name),

			.stride = m.stride
		};

		Buffer_copy(allocated, ListU32_bufferConst(m.arraySizes));
		Buffer_offset(&allocated, ListU32_bytes(m.arraySizes));

		Buffer_copy(allocated, CharString_bufferConst(m.name));
		Buffer_offset(&allocated, CharString_bytes(m.name));
	}

	return Error_none();
}

Bool BufferLayoutStruct_free(Allocator alloc, BufferLayoutStruct *layoutStruct) {

	if(!layoutStruct || !layoutStruct->data.ptr)
		return true;

	const Bool b = Buffer_free(&layoutStruct->data, alloc);
	*layoutStruct = (BufferLayoutStruct) { 0 };

	return b;
}

//Getting data

CharString BufferLayoutStruct_getName(BufferLayoutStruct layoutStruct) {
	return CharString_createRefSizedConst((const C8*) layoutStruct.data.ptr, layoutStruct.nameLength, false);
}

BufferLayoutMember BufferLayoutStruct_getMember(BufferLayoutStruct layoutStruct, U16 memberId) {

	if(memberId >= layoutStruct.memberCount)
		return (BufferLayoutMember) { 0 };

	return ((const BufferLayoutMember*) (layoutStruct.data.ptr + layoutStruct.nameLength))[memberId];
}

BufferLayoutMemberInfo BufferLayoutStruct_getMemberInfo(BufferLayoutStruct layoutStruct, U16 memberId) {

	if(memberId >= layoutStruct.memberCount)
		return (BufferLayoutMemberInfo) { 0 };

	Buffer tmp = Buffer_createRefFromBuffer(layoutStruct.data, true);
	Buffer_offset(&tmp, layoutStruct.nameLength);

	const BufferLayoutMember *members = (const BufferLayoutMember*) tmp.ptr;
	Buffer_offset(&tmp, sizeof(BufferLayoutMember) * layoutStruct.memberCount);

	for(U16 i = 0; i <= memberId; ++i) {

		const BufferLayoutMember member = members[i];

		if (i == memberId) {

			ListU32 arraySizes = (ListU32) { 0 };

			if(member.arrayIndices)
				ListU32_createRefConst((const U32*)tmp.ptr, member.arrayIndices, &arraySizes);

			Buffer_offset(&tmp, member.arrayIndices * sizeof(U32));

			const CharString name = CharString_createRefSizedConst((const C8*)tmp.ptr, member.nameLength, false);
			Buffer_offset(&tmp, member.nameLength * sizeof(C8));

			return (BufferLayoutMemberInfo) {

				.name = name,
				.arraySizes = arraySizes,

				.typeId = BufferLayoutMember_getTypeId(member),
				.structId = BufferLayoutMember_getStructId(member),

				.offset = BufferLayoutMember_getOffset(member),

				.stride = member.stride
			};
		}

		Buffer_offset(&tmp, member.arrayIndices * sizeof(U32));
		Buffer_offset(&tmp, member.nameLength * sizeof(C8));
	}

	return (BufferLayoutMemberInfo) { 0 };
}

U16 BufferLayoutStruct_findMember(BufferLayoutStruct info, CharString copy) {

	for (U16 i = 0; i < (U16)info.memberCount; ++i)
		if (CharString_equalsStringSensitive(copy, BufferLayoutStruct_getMemberInfo(info, i).name))
			return i;

	return U16_MAX;
}

//BufferLayout

Error BufferLayout_create(Allocator alloc, BufferLayout *layout) {

	if(!layout)
		return Error_nullPointer(1, "BufferLayout_create()::layout is required");

	if(layout->structs.ptr)
		return Error_invalidParameter(1, 0, "BufferLayout_create()::layout wasn't empty, could indicate memleak");

	const Error err = ListBufferLayoutStruct_reserve(&layout->structs, 128, alloc);

	if (err.genericError)
		return err;

	//Root struct hasn't been selected yet, so set it to unset.

	layout->rootStructIndex = U32_MAX;
	return Error_none();
}

Bool BufferLayout_free(Allocator alloc, BufferLayout *layout) {

	if(!layout || !layout->structs.ptr)
		return true;

	for(U64 i = 0; i < layout->structs.length; ++i)
		BufferLayoutStruct_free(alloc, layout->structs.ptrNonConst + i);

	layout->rootStructIndex = 0;
	return ListBufferLayoutStruct_free(&layout->structs, alloc);
}

Error BufferLayout_createStruct(BufferLayout *layout, BufferLayoutStructInfo info, Allocator alloc, U32 *id) {

	if(!layout || !layout->structs.ptr)
		return Error_nullPointer(0, "BufferLayout_createStruct()::layout is required");

	if(!id)
		return Error_nullPointer(3, "BufferLayout_createStruct()::id is required");

	for (U16 i = 0; i < (U16) info.members.length; ++i) {

		const BufferLayoutMemberInfo member = info.members.ptr[i];

		if(member.structId != U32_MAX && member.structId >= layout->structs.length)
			return Error_outOfBounds(
				1, member.structId, layout->structs.length,
				"BufferLayout_createStruct()::info.members[i].structId out of bounds"
			);
	}

	if(layout->structs.length + 1 >= U32_MAX)
		return Error_outOfBounds(
			3, layout->structs.length, U32_MAX,
			"BufferLayout_createStruct()::layout->structs.length is limited to U32_MAX"
		);

	BufferLayoutStruct layoutStruct = (BufferLayoutStruct) { 0 };

	Error err;

	gotoIfError(clean, BufferLayoutStruct_create(info, (U32)layout->structs.length, alloc, &layoutStruct))
	gotoIfError(clean, ListBufferLayoutStruct_pushBack(&layout->structs, layoutStruct, alloc))

	*id = (U32)layout->structs.length - 1;

clean:

	if(err.genericError)
		BufferLayoutStruct_free(alloc, &layoutStruct);

	return err;
}

Error BufferLayout_assignRootStruct(BufferLayout *layout, U32 id) {

	if(!layout || !layout->structs.ptr)
		return Error_nullPointer(0, "BufferLayout_assignRootStruct()::layout is required");

	if(id >= layout->structs.length)
		return Error_outOfBounds(1, id, layout->structs.length, "BufferLayout_assignRootStruct()::id out of bounds");

	layout->rootStructIndex = id;
	return Error_none();
}

Error BufferLayout_createInstance(BufferLayout layout, U64 count, Allocator alloc, Buffer *result) {

	if(!result)
		return Error_nullPointer(3, "BufferLayout_createInstance()::result is required");

	if(result->ptr)
		return Error_invalidParameter(3, 0, "BufferLayout_createInstance()::result isn't empty, might indicate memleak");

	LayoutPathInfo info = (LayoutPathInfo) { 0 };
	const Error err = BufferLayout_resolveLayout(layout, CharString_createRefCStrConst("/"), &info, alloc);

	if(err.genericError)
		return err;

	const U64 bufLen = info.length * count;

	if(bufLen < info.length)
		return Error_overflow(0, bufLen, info.length, "BufferLayout_createInstance() overflow");

	return Buffer_createEmptyBytes(bufLen, alloc, result);
}

Error BufferLayout_resolveLayout(BufferLayout layout, CharString path, LayoutPathInfo *info, Allocator alloc) {

	if(!layout.structs.ptr)
		return Error_nullPointer(0, "BufferLayout_resolveLayout()::layout is empty");

	if(!info)
		return Error_nullPointer(2, "BufferLayout_resolveLayout()::info is NULL");

	if(CharString_equalsStringSensitive(path, CharString_createRefCStrConst("//")))
		return Error_invalidParameter(1, 0, "BufferLayout_resolveLayout()::path is invalid");

	U64 start = CharString_startsWithSensitive(path, '/');
	U64 end = CharString_length(path) - CharString_endsWithSensitive(path, '/');

	U32 currentStructId = layout.rootStructIndex;
	BufferLayoutStruct currentStruct = layout.structs.ptr[currentStructId];

	if (end <= start) {

		U64 totalLength = 0;

		for (U16 i = 0; i < currentStruct.memberCount; ++i) {

			BufferLayoutMemberInfo m = BufferLayoutStruct_getMemberInfo(currentStruct, i);

			U64 arrayStride = m.stride;

			for (U64 j = 0; j < m.arraySizes.length; ++j)
				arrayStride *= m.arraySizes.ptr[j];

			totalLength = U64_max(arrayStride + m.offset, totalLength);
		}

		*info = (LayoutPathInfo) {
			.length = totalLength,
			.structId = layout.rootStructIndex,
			.typeId = ETypeId_Undefined
		};

		return Error_none();
	}

	CharString copy = CharString_createNull();
	Error err = CharString_reserve(&copy, end - start, alloc);

	if(err.genericError)
		return err;

	Bool isInMember = false;
	BufferLayoutMemberInfo currentMember = (BufferLayoutMemberInfo) { 0 };

	U16 currentArrayDim = 0;

	U64 currentOffset = 0;

	for (U64 i = start; i < end; ++i) {

		C8 v = CharString_getAt(path, i);

		if(!C8_isValidAscii(v))
			gotoIfError(clean, Error_invalidParameter(1, 3, "BufferLayout_resolveLayout()::path contains non ascii char"))

		//For the final character we append first, so we can process the final access here.

		Bool isEnd = i == end - 1;

		if(isEnd)
			gotoIfError(clean, CharString_append(&copy, v, alloc))

		//Access member or array index

		if (v == '/' || isEnd) {

			if(!CharString_length(copy))
				gotoIfError(clean, Error_invalidParameter(1, 1, "BufferLayout_resolveLayout() had missing member (//)"))

			//Access struct member

			if (!isInMember) {

				U16 memberId = BufferLayoutStruct_findMember(currentStruct, copy);

				if(memberId == U16_MAX)
					gotoIfError(clean, Error_invalidParameter(1, 2, "BufferLayout_resolveLayout()::path member not found"))

				isInMember = true;
				currentMember = BufferLayoutStruct_getMemberInfo(currentStruct, memberId);

				currentArrayDim = 0;
				currentOffset += currentMember.offset;
			}

			//Access member of member or an array index

			else {

				//It's now safe to access a member of the member in the current array.

				if (currentArrayDim >= currentMember.arraySizes.length) {

					//Access struct

					currentStructId = currentMember.structId;
					currentStruct = layout.structs.ptr[currentStructId];

					//

					U16 memberId = BufferLayoutStruct_findMember(currentStruct, copy);

					if(memberId == U16_MAX)
						gotoIfError(clean, Error_invalidParameter(
							1, 2, "BufferLayout_resolveLayout()::path member not found"
						))

					isInMember = true;
					currentMember = BufferLayoutStruct_getMemberInfo(currentStruct, memberId);

					currentArrayDim = 0;
					currentOffset += currentMember.offset;
				}

				//Otherwise we're accessing a C-like array

				else {

					U64 arrayOffset;
					if(!CharString_parseU64(copy, &arrayOffset))
						gotoIfError(clean, Error_invalidParameter(
							1, 4, "BufferLayout_resolveLayout()::path expected U64 for array index"
						))

					U32 currentDim = currentMember.arraySizes.ptr[currentArrayDim];

					if(arrayOffset >= currentDim)
						gotoIfError(clean, Error_outOfBounds(
							0, arrayOffset, currentDim, "BufferLayout_resolveLayout()::path array index out of bounds"
						))

					U64 arrayStride = currentMember.stride;

					for(U64 j = (U64)currentArrayDim + 1; j < currentMember.arraySizes.length; ++j)
						arrayStride *= currentMember.arraySizes.ptr[j];

					currentOffset += arrayOffset * arrayStride;
					++currentArrayDim;
				}
			}

			CharString_clear(&copy);
			continue;
		}

		//Ensure \ is read correctly, which is why split wasn't used here.
		//We still want to allow / and \ in member names, because a JSON, FBX or whatever format might include it.

		if (v == '\\') {

			if (CharString_getAt(path, i + 1) == '/') {
				gotoIfError(clean, CharString_append(&copy, '/', alloc))
				++i;
				continue;
			}

			if (CharString_getAt(path, i + 1) == '\\') {
				gotoIfError(clean, CharString_append(&copy, '\\', alloc))
				++i;
				continue;
			}
		}

		//In any other case; including \n, \r, etc. we assume it was already correctly encoded.
		//If you want \n just insert the character beforehand.

		gotoIfError(clean, CharString_append(&copy, v, alloc))
	}

	U64 arrayStride = currentMember.stride;

	for (U64 i = (U64)currentArrayDim; i < currentMember.arraySizes.length; ++i)
		arrayStride *= currentMember.arraySizes.ptr[i];

	*info = (LayoutPathInfo) {
		.offset = currentOffset,
		.length = arrayStride,
		.typeId = currentMember.typeId,
		.structId = currentMember.structId
	};

clean:
	CharString_free(&copy, alloc);
	return err;
}

Error BufferLayout_resolve(
	Buffer buffer,
	BufferLayout layout,
	CharString path,
	Buffer *location,
	Allocator alloc
) {

	if(!location)
		return Error_nullPointer(3, "BufferLayout_resolve()::location is required");

	if(!buffer.ptr)
		return Error_nullPointer(0, "BufferLayout_resolve()::buffer is required");

	if(location->ptr)
		return Error_invalidParameter(3, 0, "BufferLayout_resolve()::location isn't empty, might indicate memleak");

	LayoutPathInfo info = (LayoutPathInfo) { 0 };
	const Error err = BufferLayout_resolveLayout(layout, path, &info, alloc);

	if(err.genericError)
		return err;

	if(info.offset + info.length > Buffer_length(buffer))
		return Error_outOfBounds(
			0, info.offset + info.length, Buffer_length(buffer), "BufferLayout_resolve()::offset + length out of bounds"
		);

	*location =
		Buffer_isConstRef(buffer) ? Buffer_createRefConst(buffer.ptr + info.offset, info.length) :
		Buffer_createRef((U8*)buffer.ptr + info.offset, info.length);

	return Error_none();
}

Error BufferLayout_setData(
	Buffer buffer,
	BufferLayout layout,
	CharString path,
	Buffer newData,
	Allocator alloc
) {

	if(Buffer_isConstRef(buffer))
		return Error_constData(0, 0, "BufferLayout_setData()::buffer must be writable");

	Buffer buf = Buffer_createNull();
	const Error err = BufferLayout_resolve(buffer, layout, path, &buf, alloc);

	if(err.genericError)
		return err;

	if(Buffer_length(newData) != Buffer_length(buf))
		return Error_invalidParameter(3, 0, "BufferLayout_setData()::buffer size mismatches");

	Buffer_copy(buf, newData);
	return Error_none();
}

Error BufferLayout_getData(
	Buffer buffer,
	BufferLayout layout,
	CharString path,
	Buffer *currentData,
	Allocator alloc
) {

	if(!currentData)
		return Error_nullPointer(3, "BufferLayout_getData()::currentData is required");

	if(currentData->ptr)
		return Error_invalidParameter(3, 0, "BufferLayout_getData()::currentData isn't empty, might indicate memleak");

	Buffer buf = Buffer_createNull();
	const Error err = BufferLayout_resolve(buffer, layout, path, &buf, alloc);

	if(err.genericError)
		return err;

	*currentData = Buffer_createRefConst(buf.ptr, Buffer_length(buf));
	return Error_none();
}

#define BUFFER_LAYOUT_SGET_IMPL(T)																		\
																										\
Error BufferLayout_set##T(																				\
	Buffer buffer, 																						\
	BufferLayout layout, 																				\
	CharString path, 																					\
	T t, 																								\
	Allocator alloc																						\
) {																										\
	return BufferLayout_setData(buffer, layout, path, Buffer_createRefConst(&t, sizeof(T)), alloc);		\
}																										\
																										\
Error BufferLayout_get##T(																				\
	Buffer buffer, 																						\
	BufferLayout layout, 																				\
	CharString path, 																					\
	T *t, 																								\
	Allocator alloc																						\
) {																										\
																										\
	if(!t)																								\
		return Error_nullPointer(3, "BufferLayout_get" #T " t is required");							\
																										\
	Buffer buf = Buffer_createRef(t, sizeof(T));														\
	Buffer currentData = Buffer_createNull();															\
	Error err = BufferLayout_getData(buffer, layout, path, &currentData, alloc);						\
																										\
	if(err.genericError)																				\
		return err;																						\
																										\
	Buffer_copy(buf, currentData);																		\
	return Error_none();																				\
}

#define BUFFER_LAYOUT_XINT_SGET_IMPL(prefix, suffix)													\
BUFFER_LAYOUT_SGET_IMPL(prefix##8##suffix);																\
BUFFER_LAYOUT_SGET_IMPL(prefix##16##suffix);															\
BUFFER_LAYOUT_SGET_IMPL(prefix##32##suffix);															\
BUFFER_LAYOUT_SGET_IMPL(prefix##64##suffix);

#define BUFFER_LAYOUT_VEC_IMPL(prefix)																	\
BUFFER_LAYOUT_SGET_IMPL(prefix##32##x2);																\
BUFFER_LAYOUT_SGET_IMPL(prefix##32##x4)

//Setters for C8, Bool and ints/uints

BUFFER_LAYOUT_SGET_IMPL(C8);
BUFFER_LAYOUT_SGET_IMPL(Bool);

BUFFER_LAYOUT_XINT_SGET_IMPL(U, );
BUFFER_LAYOUT_XINT_SGET_IMPL(I, );

BUFFER_LAYOUT_SGET_IMPL(F32);
BUFFER_LAYOUT_SGET_IMPL(F64);

BUFFER_LAYOUT_VEC_IMPL(I);
BUFFER_LAYOUT_VEC_IMPL(F);

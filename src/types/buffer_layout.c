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

#include "types/buffer_layout.h"
#include "types/allocator.h"
#include "types/error.h"
#include "types/buffer.h"

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
	List arraySizes, 
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
	List arraySizes, 
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
		return Error_nullPointer(1);

	if(layoutStruct->data.ptr)
		return Error_invalidParameter(1, 0);

	if(id == U32_MAX)
		return Error_invalidParameter(2, 0);

	if(CharString_length(info.name) > U8_MAX)
		return Error_invalidParameter(0, 0);

	if(info.members.length >= U16_MAX)
		return Error_invalidParameter(0, 1);

	U64 memberDataLen = 0;
		
	for(U16 i = 0; i < (U16) info.members.length; ++i) {

		BufferLayoutMemberInfo m = ((const BufferLayoutMemberInfo*)info.members.ptr)[i];

		if(!CharString_length(m.name) || CharString_length(m.name) > U8_MAX || !CharString_isValidAscii(m.name))
			return Error_invalidParameter(0, 0 + (((U32)i + 1) << 16));

		if(m.arraySizes.length && m.arraySizes.stride != sizeof(U32))
			return Error_invalidParameter(0, 1 + (((U32)i + 1) << 16));

		if(m.arraySizes.length > U8_MAX)
			return Error_invalidParameter(0, 1 + (((U32)i + 1) << 16));

		if(((U32)m.typeId == U32_MAX) && (m.structId == U32_MAX))
			return Error_invalidParameter(0, 2 + (((U32)i + 1) << 16));

		if(((U32)m.typeId != U32_MAX) && (m.structId != U32_MAX))
			return Error_invalidParameter(0, 2 + (((U32)i + 1) << 16));

		if(m.structId != U32_MAX && m.structId >= id)
			return Error_invalidParameter(0, 3 + (((U32)i + 1) << 16));

		if(m.offset >> 47)
			return Error_invalidParameter(0, 4 + (((U32)i + 1) << 16));

		U64 totalLen = 1;

		for(U64 j = 0; j < m.arraySizes.length; ++j) {

			U32 leni = ((const U32*)m.arraySizes.ptr)[j];

			//An array length of 0 is only allowed if there's one element

			if(!leni && !(!j && m.arraySizes.length == 1))
				return Error_invalidParameter(0, 1 + (((U32)j + 1) << 16));

			U64 prevLen = totalLen;
			totalLen *= leni;

			if(totalLen < prevLen)
				return Error_overflow((((U32)j + 1) << 16), totalLen, prevLen);
		}

		U64 prevLen = totalLen;
		totalLen *= m.stride;

		if(totalLen < prevLen || totalLen >> 48)
			return Error_overflow((((U32)i + 1) << 16), totalLen, ((U64) 1 << 48) - 1);

		memberDataLen += CharString_length(m.name) + List_bytes(m.arraySizes);
	}

	U64 len = BufferLayoutStruct_allocatedData(info, memberDataLen);
	Buffer allocated = Buffer_createNull();
	
	Error err = Buffer_createUninitializedBytes(len, alloc, &allocated);

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

		BufferLayoutMemberInfo m = ((const BufferLayoutMemberInfo*)info.members.ptr)[i];

		members[i] = (BufferLayoutMember) {

			.structIdUnion = m.structId != U32_MAX ? m.structId : (U32) m.typeId,

			.offsetLo = (U32) m.offset,
			.offsetHiAndIsStruct = (U16) (m.offset >> 32) | (m.structId != U32_MAX ? 1 << 15 : 0),

			.arrayIndices = (U8) m.arraySizes.length,
			.nameLength = (U8) CharString_length(m.name),

			.stride = m.stride
		};

		Buffer_copy(allocated, List_bufferConst(m.arraySizes));
		Buffer_offset(&allocated, List_bytes(m.arraySizes));

		Buffer_copy(allocated, CharString_bufferConst(m.name));
		Buffer_offset(&allocated, CharString_bytes(m.name));
	}

	return Error_none();
}

Bool BufferLayoutStruct_free(Allocator alloc, BufferLayoutStruct *layoutStruct) {

	if(!layoutStruct || !layoutStruct->data.ptr)
		return true;

	Bool b = Buffer_free(&layoutStruct->data, alloc);
	*layoutStruct = (BufferLayoutStruct) { 0 };

	return b;
}

//Getting data

CharString BufferLayoutStruct_getName(BufferLayoutStruct layoutStruct) {
	return CharString_createConstRefSized((const C8*) layoutStruct.data.ptr, layoutStruct.nameLength, false);
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

		BufferLayoutMember member = members[i];

		if (i == memberId) {
			
			List arraySizes = (List) { 0 };

			if(member.arrayIndices)
				List_createConstRef(tmp.ptr, member.arrayIndices, sizeof(U32), &arraySizes);

			Buffer_offset(&tmp, member.arrayIndices * sizeof(U32));

			CharString name = CharString_createConstRefSized((const C8*)tmp.ptr, member.nameLength, false);
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
		if (CharString_equalsString(
			copy, 
			BufferLayoutStruct_getMemberInfo(info, i).name, 
			EStringCase_Sensitive
		))
			return i;

	return U16_MAX;
}

//BufferLayout

Error BufferLayout_create(Allocator alloc, BufferLayout *layout) {
	
	if(!layout)
		return Error_nullPointer(1);

	if(layout->structs.ptr)
		return Error_invalidParameter(1, 0);

	layout->structs = List_createEmpty(sizeof(BufferLayoutStruct));
	Error err = List_reserve(&layout->structs, 128, alloc);

	if (err.genericError) {
		layout->structs = (List) { 0 };
		return err;
	}

	//Root struct hasn't been selected yet, so set it to unset.

	layout->rootStructIndex = U32_MAX;

	return Error_none();
}

Bool BufferLayout_free(Allocator alloc, BufferLayout *layout) {

	if(!layout || !layout->structs.ptr)
		return true;

	for(U64 i = 0; i < layout->structs.length; ++i)
		BufferLayoutStruct_free(alloc, (BufferLayoutStruct*) layout->structs.ptr + i);

	layout->rootStructIndex = 0;
	return List_free(&layout->structs, alloc);
}

Error BufferLayout_createStruct(BufferLayout *layout, BufferLayoutStructInfo info, Allocator alloc, U32 *id) {

	if(!layout || !layout->structs.ptr)
		return Error_nullPointer(0);

	if(!id)
		return Error_nullPointer(3);

	for (U16 i = 0; i < (U16) info.members.length; ++i) {

		BufferLayoutMemberInfo member = ((const BufferLayoutMemberInfo*) info.members.ptr)[i];

		if(member.structId != U32_MAX && member.structId >= layout->structs.length)
			return Error_outOfBounds(1, member.structId, layout->structs.length);
	}

	if(layout->structs.length + 1 >= U32_MAX)
		return Error_outOfBounds(3, layout->structs.length, U32_MAX);

	BufferLayoutStruct layoutStruct = (BufferLayoutStruct) { 0 };

	Error err = Error_none();

	_gotoIfError(clean, BufferLayoutStruct_create(info, (U32)layout->structs.length, alloc, &layoutStruct));
	_gotoIfError(clean, List_pushBack(&layout->structs, Buffer_createConstRef(&layoutStruct, sizeof(layoutStruct)), alloc));

	*id = (U32)layout->structs.length - 1;

clean:
	
	if(err.genericError)
		BufferLayoutStruct_free(alloc, &layoutStruct);

	return err;
}

Error BufferLayout_assignRootStruct(BufferLayout *layout, U32 id) {

	if(!layout || !layout->structs.ptr)
		return Error_nullPointer(0);

	if(id >= layout->structs.length)
		return Error_outOfBounds(1, id, layout->structs.length);

	layout->rootStructIndex = id;
	return Error_none();
}

Error BufferLayout_createInstance(BufferLayout layout, U64 count, Allocator alloc, Buffer *result) {

	if(!result)
		return Error_nullPointer(3);

	if(result->ptr)
		return Error_invalidParameter(3, 0);

	LayoutPathInfo info = (LayoutPathInfo) { 0 };
	Error err = BufferLayout_resolveLayout(layout, CharString_createConstRefUnsafe("/"), &info, alloc);

	if(err.genericError)
		return err;

	U64 bufLen = info.length * count;

	if(bufLen < info.length)
		return Error_overflow(0, bufLen, info.length);

	return Buffer_createEmptyBytes(bufLen, alloc, result);
}

Error BufferLayout_resolveLayout(BufferLayout layout, CharString path, LayoutPathInfo *info, Allocator alloc) {

	if(!layout.structs.ptr)
		return Error_nullPointer(0);

	if(!info)
		return Error_nullPointer(2);

	if(layout.rootStructIndex >= layout.structs.length)
		return Error_unsupportedOperation(0);

	if(CharString_equalsString(path, CharString_createConstRefUnsafe("//"), EStringCase_Sensitive))
		return Error_invalidParameter(1, 0);

	if(CharString_length(path) > U16_MAX)
		return Error_invalidParameter(1, 1);

	U64 start = CharString_startsWith(path, '/', EStringCase_Sensitive);
	Bool suffix = CharString_endsWith(path, '/', EStringCase_Sensitive);

	if (suffix) {

		//Exception to the rule; the character might be escaped by a backslash.
		//We have to count all backslashes before it. If it's uneven then the slash was escaped.
		//Otherwise either there's no leading backslash or the leading backslash was itself escaped.

		U64 leadingBackslashes = 0;
		U64 endStart = CharString_length(path) - 2;

		while (CharString_getAt(path, endStart) == '\\') {		//If it runs out of bounds getAt will return C8_MAX
			++leadingBackslashes;
			--endStart;
		}

		if(leadingBackslashes & 1)
			suffix = false;
	}

	if(CharString_length(path) == 1 && start)
		suffix = false;

	U64 end = CharString_length(path) - suffix;

	U32 currentStructId = layout.rootStructIndex;
	BufferLayoutStruct currentStruct = ((const BufferLayoutStruct*)layout.structs.ptr)[currentStructId];

	if (end <= start) {

		U64 totalLength = 0;

		for (U16 i = 0; i < currentStruct.memberCount; ++i) {

			BufferLayoutMemberInfo m = BufferLayoutStruct_getMemberInfo(currentStruct, i);

			U64 arrayStride = m.stride;

			for (U64 j = 0; j < m.arraySizes.length; ++j)
				arrayStride *= ((const U32*) m.arraySizes.ptr)[j];

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
			_gotoIfError(clean, Error_invalidParameter(1, 3));

		//For the final character we append first so we can process the final access here.

		Bool isEnd = i == end - 1;

		if(isEnd)
			_gotoIfError(clean, CharString_append(&copy, v, alloc));

		//Access member or array index

		if (v == '/' || isEnd) {

			if(!CharString_length(copy))
				_gotoIfError(clean, Error_invalidParameter(1, 1));

			//Access struct member
		
			if (!isInMember) {
				
				U16 memberId = BufferLayoutStruct_findMember(currentStruct, copy);

				if(memberId == U16_MAX)
					_gotoIfError(clean, Error_invalidParameter(1, 2));

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
					currentStruct = ((const BufferLayoutStruct*)layout.structs.ptr)[currentStructId];

					//

					U16 memberId = BufferLayoutStruct_findMember(currentStruct, copy);

					if(memberId == U16_MAX)
						_gotoIfError(clean, Error_invalidParameter(1, 2));

					isInMember = true;
					currentMember = BufferLayoutStruct_getMemberInfo(currentStruct, memberId);

					currentArrayDim = 0;
					currentOffset += currentMember.offset;
				}

				//Otherwise we're accessing a C-like array

				else {

					U64 arrayOffset;
					if(!CharString_parseU64(copy, &arrayOffset))
						_gotoIfError(clean, Error_invalidParameter(1, 4));
					
					U32 currentDim = ((const U32*) currentMember.arraySizes.ptr)[currentArrayDim];

					if(arrayOffset >= currentDim)
						_gotoIfError(clean, Error_outOfBounds(0, arrayOffset, currentDim));

					U64 arrayStride = currentMember.stride;

					for(U64 j = (U64)currentArrayDim + 1; j < currentMember.arraySizes.length; ++j)
						arrayStride *= ((const U32*) currentMember.arraySizes.ptr)[j];

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
				_gotoIfError(clean, CharString_append(&copy, '/', alloc));
				++i;
				continue;
			}

			if (CharString_getAt(path, i + 1) == '\\') {
				_gotoIfError(clean, CharString_append(&copy, '\\', alloc));
				++i;
				continue;
			}
		}

		//In any other case; including \n, \r, etc. we assume it was already correctly encoded.
		//If you want \n just insert the character beforehand.

		_gotoIfError(clean, CharString_append(&copy, v, alloc));
	}

	U64 arrayStride = currentMember.stride;

	for (U64 i = (U64)currentArrayDim; i < currentMember.arraySizes.length; ++i)
		arrayStride *= ((const U32*) currentMember.arraySizes.ptr)[i];

	*info = (LayoutPathInfo) {

		.memberName = CharString_createConstRefSized(
			currentMember.name.ptr, 
			CharString_length(currentMember.name),
			CharString_isNullTerminated(currentMember.name)
		),

		.offset = currentOffset,
		.length = arrayStride,
		.typeId = currentMember.typeId,
		.structId = currentMember.structId
	};

	if(currentArrayDim < currentMember.arraySizes.length)
		_gotoIfError(clean, List_createConstRef(
			currentMember.arraySizes.ptr + sizeof(U32) * currentArrayDim, 
			currentMember.arraySizes.length - currentArrayDim, 
			sizeof(U32),
			&info->leftoverArray
		));

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
		return Error_nullPointer(3);

	if(!buffer.ptr)
		return Error_nullPointer(0);

	if(location->ptr)
		return Error_invalidParameter(3, 0);

	LayoutPathInfo info = (LayoutPathInfo) { 0 };
	Error err = BufferLayout_resolveLayout(layout, path, &info, alloc);

	if(err.genericError)
		return err;

	if(info.offset + info.length > Buffer_length(buffer))
		return Error_outOfBounds(0, info.offset + info.length, Buffer_length(buffer));

	*location = 
		Buffer_isConstRef(buffer) ? Buffer_createConstRef(buffer.ptr + info.offset, info.length) : 
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
		return Error_constData(0, 0);

	Buffer buf = Buffer_createNull();

	Error err = BufferLayout_resolve(
		buffer, 
		layout, 
		path, 
		&buf, 
		alloc
	);

	if(err.genericError)
		return err;

	if(Buffer_length(newData) != Buffer_length(buf))
		return Error_invalidParameter(3, 0);

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
		return Error_nullPointer(3);

	if(currentData->ptr)
		return Error_invalidParameter(3, 0);

	Buffer buf = Buffer_createNull();

	Error err = BufferLayout_resolve(
		buffer, 
		layout, 
		path, 
		&buf, 
		alloc
	);

	if(err.genericError)
		return err;

	*currentData = Buffer_createConstRef(buf.ptr, Buffer_length(buf));
	return Error_none();
}

#define _BUFFER_LAYOUT_SGET_IMPL(T)																	\
																									\
Error BufferLayout_set##T(																			\
	Buffer buffer, 																					\
	BufferLayout layout, 																			\
	CharString path, 																				\
	T t, 																							\
	Allocator alloc																					\
) {																									\
	return BufferLayout_setData(																	\
		buffer,																						\
		layout,																						\
		path,																						\
		Buffer_createConstRef(&t, sizeof(T)),														\
		alloc																						\
	);																								\
}																									\
																									\
Error BufferLayout_get##T(																			\
	Buffer buffer, 																					\
	BufferLayout layout, 																			\
	CharString path, 																				\
	T *t, 																							\
	Allocator alloc																					\
) {																									\
																									\
	if(!t)																							\
		return Error_nullPointer(3);																\
																									\
	Buffer buf = Buffer_createRef(t, sizeof(T));													\
	Buffer currentData = Buffer_createNull();														\
																									\
	Error err = BufferLayout_getData(																\
		buffer, 																					\
		layout, 																					\
		path, 																						\
		&currentData, 																				\
		alloc																						\
	);																								\
																									\
	if(err.genericError)																			\
		return err;																					\
																									\
	Buffer_copy(buf, currentData);																	\
	return Error_none();																			\
}

#define _BUFFER_LAYOUT_XINT_SGET_IMPL(prefix, suffix)												\
_BUFFER_LAYOUT_SGET_IMPL(prefix##8##suffix);														\
_BUFFER_LAYOUT_SGET_IMPL(prefix##16##suffix);														\
_BUFFER_LAYOUT_SGET_IMPL(prefix##32##suffix);														\
_BUFFER_LAYOUT_SGET_IMPL(prefix##64##suffix);

#define __BUFFER_LAYOUT_VEC_IMPL(prefix)															\
_BUFFER_LAYOUT_SGET_IMPL(prefix##32##x2);															\
_BUFFER_LAYOUT_SGET_IMPL(prefix##32##x4)

//Setters for C8, Bool and ints/uints

_BUFFER_LAYOUT_SGET_IMPL(C8);
_BUFFER_LAYOUT_SGET_IMPL(Bool);

_BUFFER_LAYOUT_XINT_SGET_IMPL(U, );
_BUFFER_LAYOUT_XINT_SGET_IMPL(I, );

_BUFFER_LAYOUT_SGET_IMPL(F32);
_BUFFER_LAYOUT_SGET_IMPL(F64);

__BUFFER_LAYOUT_VEC_IMPL(I);
__BUFFER_LAYOUT_VEC_IMPL(F);

//Looping through members and parent

Error BufferLayout_foreach(
	BufferLayout layout,
	CharString path,
	BufferLayoutForeachFunc func,
	void *userData,
	Bool isRecursive,
	Allocator alloc
) {

	if(!func)
		return Error_nullPointer(2);

	LayoutPathInfo info = (LayoutPathInfo) { 0 };
	Error err = Error_none();
	CharString tmp = CharString_createNull();

	_gotoIfError(clean, BufferLayout_resolveLayout(layout, path, &info, alloc));

	Bool prefix = CharString_startsWith(path, '/', EStringCase_Sensitive);
	Bool suffix = CharString_endsWith(path, '/', EStringCase_Sensitive);

	if (suffix) {

		//Exception to the rule; the character might be escaped by a backslash.
		//We have to count all backslashes before it. If it's uneven then the slash was escaped.
		//Otherwise either there's no leading backslash or the leading backslash was itself escaped.

		U64 leadingBackslashes = 0;
		U64 start = CharString_length(path) - 2;

		while (CharString_getAt(path, start) == '\\') {		//If it runs out of bounds getAt will return C8_MAX
			++leadingBackslashes;
			--start;
		}

		if(leadingBackslashes & 1)
			suffix = false;
	}

	if(CharString_length(path) == 1 && prefix)
		suffix = false;

	//Go through members of array

	if (info.leftoverArray.length) {

		for(U32 i = 0, j = ((const U32*)info.leftoverArray.ptr)[0]; i < j; ++i) {

			U64 length = info.length / j;

			LayoutPathInfo layoutPathInfo = (LayoutPathInfo) {
				.memberName = info.memberName,
				.offset = info.offset + length * i,
				.length = length,
				.typeId = info.typeId,
				.structId = info.structId
			};

			if(info.leftoverArray.length > 1)
				_gotoIfError(clean, List_createConstRef(
					info.leftoverArray.ptr + sizeof(U32), info.leftoverArray.length - 1, 
					info.leftoverArray.stride,
					&layoutPathInfo.leftoverArray
				));

			_gotoIfError(clean, CharString_format(
				alloc,
				&tmp,
				"%.*s/%"PRIu32,
				(int) (CharString_length(path) - prefix - suffix),
				path.ptr + prefix,
				i
			));

			_gotoIfError(clean, func(layout, layoutPathInfo, tmp, userData));

			if (isRecursive && (info.leftoverArray.length > 1 || info.structId != U32_MAX))
				_gotoIfError(clean, BufferLayout_foreach(
					layout,
					tmp,
					func,
					userData,
					true,
					alloc
				));

			CharString_free(&tmp, alloc);
		}
	}

	//Go through members of struct

	else if(info.typeId == ETypeId_Undefined) {

		BufferLayoutStruct s = ((const BufferLayoutStruct*)layout.structs.ptr)[info.structId];

		for (U16 i = 0; i < s.memberCount; ++i) {

			BufferLayoutMemberInfo infoi = BufferLayoutStruct_getMemberInfo(s, i);

			U64 length = infoi.stride;

			for(U64 j = 0; j < infoi.arraySizes.length; ++j)
				length *= ((const U32*)infoi.arraySizes.ptr)[j];

			LayoutPathInfo layoutPathInfo = (LayoutPathInfo) {

				.memberName = CharString_createConstRefSized(
					infoi.name.ptr,
					CharString_length(infoi.name),
					CharString_isNullTerminated(infoi.name)
				),

				.offset = info.offset + infoi.offset,
				.length = length,
				.typeId = infoi.typeId,
				.structId = infoi.structId
			};

			if(infoi.arraySizes.length)
				_gotoIfError(clean, List_createConstRef(
					infoi.arraySizes.ptr, infoi.arraySizes.length, infoi.arraySizes.stride, 
					&layoutPathInfo.leftoverArray
				));

			_gotoIfError(clean, CharString_format(
				alloc,
				&tmp,
				"%.*s/%.*s",
				(int) (CharString_length(path) - prefix - suffix),
				path.ptr + prefix,
				(int) CharString_length(infoi.name),
				infoi.name.ptr
			));

			_gotoIfError(clean, func(layout, layoutPathInfo, tmp, userData));

			if(isRecursive && infoi.structId != U32_MAX)
				_gotoIfError(clean, BufferLayout_foreach(
					layout,
					tmp,
					func,
					userData,
					true,
					alloc
				));

			CharString_free(&tmp, alloc);
		}
	}

	//We can't be subdivided further, because we're already a plain type.

	else _gotoIfError(clean, Error_invalidOperation(0));

clean:
	CharString_free(&tmp, alloc);
	return err;
}

typedef struct WrappedBufferLayoutForeach {

	const U8 *begin;
	void *userData;
	bool isConst;
	BufferLayoutForeachDataFunc dataFunc;

} WrappedBufferLayoutForeach;

Error BufferLayout_foreachDataInternal(
	BufferLayout layout, 
	LayoutPathInfo info, 
	CharString path, 
	WrappedBufferLayoutForeach *data
) {

	Buffer buf = 
		data->isConst ? Buffer_createConstRef(data->begin + info.offset, info.length) :
		Buffer_createRef((U8*)data->begin + info.offset, info.length);

	return data->dataFunc(layout, info, path, buf, data->userData);
}

Error BufferLayout_foreachData(
	Buffer buffer,
	BufferLayout layout,
	CharString path,
	BufferLayoutForeachDataFunc func,
	void *userData,
	Bool isRecursive,
	Allocator alloc
) {

	if(!func)
		return Error_nullPointer(3);

	LayoutPathInfo info = (LayoutPathInfo) { 0 };
	Error err = Error_none();

	_gotoIfError(clean, BufferLayout_resolveLayout(layout, CharString_createConstRefUnsafe("/"), &info, alloc));
	
	if (Buffer_length(buffer) != info.length)
		return Error_invalidOperation(0);

	WrappedBufferLayoutForeach wrapped = (WrappedBufferLayoutForeach) {
		.begin = buffer.ptr,
		.isConst = Buffer_isConstRef(buffer),
		.userData = userData,
		.dataFunc = func
	};

	_gotoIfError(clean, BufferLayout_foreach(
		layout,
		path,
		(BufferLayoutForeachFunc) BufferLayout_foreachDataInternal,
		&wrapped,
		isRecursive,
		alloc
	));

clean:
	return err;
}

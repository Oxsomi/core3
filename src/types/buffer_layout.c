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

Error BufferLayoutMemberInfo_validate(CharString name, U64 offset, List arraySizes, BufferLayoutMemberInfo *result) {

	if(!result)
		return Error_nullPointer(3);

	if(result->name.ptr)
		return Error_invalidParameter(3, 0);

	if(!CharString_length(name) || CharString_length(name) > U8_MAX || !CharString_isValidAscii(name))
		return Error_invalidParameter(0, 0);

	if(arraySizes.length && arraySizes.stride != sizeof(U32))
		return Error_invalidParameter(2, 0);

	if(arraySizes.length > U8_MAX)
		return Error_invalidParameter(2, 1);

	if(offset >> 47)
		return Error_invalidParameter(1, 0);

	return Error_none();
}

Error BufferLayoutMemberInfo_create(
	ETypeId typeId, 
	CharString name, 
	U64 offset, 
	U32 stride, 
	BufferLayoutMemberInfo *result
) {

	Error err = BufferLayoutMemberInfo_validate(name, offset, (List) { 0 }, result);

	if(err.genericError)
		return err;

	*result = (BufferLayoutMemberInfo) {
		.typeId = typeId,
		.structId = U32_MAX,
		.name = name,
		.offset = offset,
		.stride = stride
	};

	return Error_none();
}

Error BufferLayoutMemberInfo_createStruct(
	U32 structId, 
	CharString name, 
	U64 offset, 
	U32 stride,
	BufferLayoutMemberInfo *result
) {

	Error err = BufferLayoutMemberInfo_validate(name, offset, (List) { 0 }, result);

	if(err.genericError)
		return err;

	*result = (BufferLayoutMemberInfo) {
		.typeId = ETypeId_Undefined,
		.structId = structId,
		.name = name,
		.offset = offset,
		.stride = stride
	};

	return Error_none();
}

Error BufferLayoutMemberInfo_createArray(
	ETypeId typeId, 
	CharString name, 
	List arraySizes, 
	U64 offset, 
	U32 stride,
	BufferLayoutMemberInfo *result
) {
	Error err = BufferLayoutMemberInfo_validate(name, offset, arraySizes, result);

	if(err.genericError)
		return err;

	*result = (BufferLayoutMemberInfo) {
		.typeId = typeId,
		.structId = U32_MAX,
		.name = name,
		.arraySizes = arraySizes,
		.offset = offset,
		.stride = stride
	};

	return Error_none();
}

Error BufferLayoutMemberInfo_createStructArray(
	U32 structId, 
	CharString name, 
	List arraySizes, 
	U64 offset,
	U32 stride,
	BufferLayoutMemberInfo *result
) {
	Error err = BufferLayoutMemberInfo_validate(name, offset, arraySizes, result);

	if(err.genericError)
		return err;

	*result = (BufferLayoutMemberInfo) {
		.typeId = ETypeId_Undefined,
		.structId = structId,
		.name = name,
		.arraySizes = arraySizes,
		.offset = offset,
		.stride = stride
	};

	return Error_none();
}

Bool BufferLayoutMemberInfo_isDynamic(BufferLayoutMemberInfo info) {

	for(U64 i = 0; i < info.arraySizes.length; ++i)
		if(!((const U32*)info.arraySizes.ptr)[i])
			return true;

	return false;
}

//Manipulating data

U64 BufferLayoutStruct_allocatedData(BufferLayoutStructInfo info, U64 memberDataLen) {

	return 
		sizeof(C8) * CharString_length(info.name) +
		sizeof(BufferLayoutMember) * info.members.length +
		memberDataLen;
}

Error BufferLayoutStruct_create(
	List/*<BufferLayoutStruct>*/ allStructs,
	BufferLayoutStructInfo info, 
	U32 id, 
	Allocator alloc, 
	BufferLayoutStruct *layoutStruct
) {

	if(!allStructs.length)
		return Error_nullPointer(0);

	if(allStructs.stride != sizeof(BufferLayoutStruct))
		return Error_invalidParameter(0, 0);

	if(!layoutStruct)
		return Error_nullPointer(4);

	if(layoutStruct->data.ptr)
		return Error_invalidParameter(4, 0);

	if(id == U32_MAX)
		return Error_invalidParameter(2, 0);

	if(CharString_length(info.name) >= U8_MAX)
		return Error_invalidParameter(1, 0);

	if(info.members.length >= U16_MAX)
		return Error_invalidParameter(1, 1);

	U64 memberDataLen = 0;
	Bool isContiguous = true;
		
	for(U16 i = 0; i < (U16) info.members.length; ++i) {

		BufferLayoutMemberInfo m = ((const BufferLayoutMemberInfo*)info.members.ptr)[i];

		if(m.structId != U32_MAX && m.structId >= id)
			return Error_invalidParameter(0, 3 + (((U32)i + 1) << 16));

		if(m.structId != U32_MAX)
			isContiguous &= ((const BufferLayoutStruct*)allStructs.ptr)[m.structId].isContiguous;

		U64 totalLen = m.stride;

		//It's possible that any of the arraySizes is defined as 0.
		//In this case, the root buffer won't store it (hence it being length 0).
		//It will instead have a property that defines the undefined array size.
		//prop: F32[1][][3] would need each property to define the size:
		//	e.g. BufferLayout_setSize("prop/0", mySize);
		//	This will then allocate the underlying BufferLayout and properties.
		//	So in this case it'll allocate F32[3] per property.
		//	If it's not defined it'll be assumed as an empty array in json.
		//So names: C8[][] is a CharString[]. First you define the size (psuedocode):
		//	BufferLayout_setSize("names", 2);
		//Then you set the properties:
		//	BufferLayout_setSize("names/0", 5);
		//	BufferLayout_setData("names/0", "hello");
		//	BufferLayout_setSize("names/1", 3);
		//	BufferLayout_setData("names/1", "hey");
		//This can be shortened by using setString since that does both setSize and setData.

		for(U64 j = 0; j < m.arraySizes.length; ++j) {

			U32 leni = ((const U32*)m.arraySizes.ptr)[j];

			if(leni == U32_MAX)
				return Error_outOfBounds(0, U32_MAX, U32_MAX);

			U64 prevLen = totalLen;
			totalLen *= leni;

			if(!leni)
				isContiguous = false;

			else if(totalLen < prevLen)
				return Error_overflow((((U32)j + 1) << 16), totalLen, prevLen);
		}

		if(totalLen >> 47)
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
		.isContiguous = isContiguous,

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

	for (U16 i = 0; i < info.memberCount; ++i)
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
	Error err = List_reserve(&layout->structs, 48, alloc);

	if (err.genericError) {
		layout->structs = (List) { 0 };
		return err;
	}

	//Root struct hasn't been selected yet, so set it to unset.

	layout->rootStructIndex = U32_MAX;
	return Error_none();
}

Bool BufferLayout_free(BufferLayout *layout, Allocator alloc) {

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

	if(info.members.length >= U16_MAX)
		return Error_outOfBounds(1, info.members.length, U16_MAX);

	if(info.members.stride == sizeof(BufferLayoutMemberInfo) || !info.members.length)
		return Error_invalidParameter(1, 0);

	for (U16 i = 0; i < (U16) info.members.length; ++i) {

		BufferLayoutMemberInfo member = ((const BufferLayoutMemberInfo*) info.members.ptr)[i];

		if(member.structId != U32_MAX && member.structId >= layout->structs.length)
			return Error_outOfBounds(1, member.structId, layout->structs.length);
	}

	if(layout->structs.length + 1 >= U32_MAX)
		return Error_outOfBounds(3, layout->structs.length, U32_MAX);

	BufferLayoutStruct layoutStruct = (BufferLayoutStruct) { 0 };

	Error err = Error_none();

	_gotoIfError(clean, BufferLayoutStruct_create(layout->structs, info, (U32)layout->structs.length, alloc, &layoutStruct));
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

Error BufferLayoutInstance_create(BufferLayout layout, Allocator alloc, BufferLayoutInstance *result) {

	if(!result)
		return Error_nullPointer(3);

	if(result->layout.structs.ptr)
		return Error_invalidParameter(3, 0);

	LayoutPathInfo info = (LayoutPathInfo) { 0 };
	Error err = BufferLayout_resolveLayout(layout, CharString_createConstRefUnsafe("/"), &info, NULL, alloc);

	if(err.genericError)
		return err;

	U64 bufLen = info.length;

	if(bufLen)
		_gotoIfError(clean, Buffer_createEmptyBytes(bufLen, alloc, &result->data));

	result->allocations = List_createEmpty(sizeof(BufferLayoutDynamicData));
	_gotoIfError(clean, List_reserve(&result->allocations, 48, alloc));

	result->layout = layout;

	List constVer = (List) { 0 };

	_gotoIfError(clean, List_createConstRef(
		result->layout.structs.ptr, sizeof(BufferLayoutDynamicData), 
		result->layout.structs.length,
		&constVer
	));

	result->layout.structs = constVer;
		
clean:

	if (err.genericError) {
		Buffer_free(&result->data, alloc);
		List_free(&result->allocations, alloc);
	}
	
	return err;
}

Bool LayoutPathInfo_free(LayoutPathInfo *info, Allocator alloc) {

	if(!info || !info->memberName.ptr)
		return true;

	Bool b = CharString_free(&info->memberName, alloc);
	b &= List_free(&info->leftoverArray, alloc);
	*info = (LayoutPathInfo) { 0 };

	return b;
}

Bool BufferLayoutInstance_free(BufferLayoutInstance *instance, Allocator alloc) {

	if(!instance || !instance->layout.structs.ptr)
		return true;

	Bool b = true;

	for(U64 i = 0; i < instance->allocations.length; ++i)
		b &= Buffer_free(&((const BufferLayoutDynamicData*)instance->allocations.ptr)[i].data, alloc);

	b &= Buffer_free(&instance->data, alloc);
	b &= List_free(&instance->allocations, alloc);
	*instance = (BufferLayoutInstance) { 0 };
	return b;
}

void fixPath(CharString *path) {

	*path = CharString_createConstRefSized(path->ptr, CharString_length(*path), CharString_isNullTerminated(*path));

	Bool prefix = CharString_startsWith(*path, '/', EStringCase_Sensitive);
	Bool suffix = CharString_endsWith(*path, '/', EStringCase_Sensitive);

	if (suffix) {

		//Exception to the rule; the character might be escaped by a backslash.
		//We have to count all backslashes before it. If it's uneven then the slash was escaped.
		//Otherwise either there's no leading backslash or the leading backslash was itself escaped.

		U64 leadingBackslashes = 0;
		U64 start = CharString_length(*path) - 2;

		while (CharString_getAt(*path, start) == '\\') {		//If it runs out of bounds getAt will return C8_MAX
			++leadingBackslashes;
			--start;
		}

		if(leadingBackslashes & 1)
			suffix = false;
	}

	if(CharString_length(*path) == 1 && prefix)
		suffix = false;

	if(prefix) {
		++path->ptr;
		--path->lenAndNullTerminated;
	}

	if(suffix)
		--path->lenAndNullTerminated;
}

U64 BufferLayoutInstance_getIndex(BufferLayoutInstance *instance, CharString path, Bool fixPathb) {

	if(!instance || !instance->layout.structs.ptr)
		return U64_MAX;

	if(fixPathb)
		fixPath(&path);

	if(CharString_isEmpty(path))
		return U64_MAX;

	//TODO: HashMap!

	for (U64 i = 0; i < instance->allocations.length; ++i) {

		BufferLayoutDynamicData di = ((const BufferLayoutDynamicData*)instance->allocations.ptr)[i];
		CharString temp = CharString_createConstRefSized(di.data.ptr, di.nameLength, false);

		if(!CharString_equalsString(temp, path, EStringCase_Sensitive))
			continue;

		return i;
	}

	return U64_MAX;
}

U32 BufferLayoutInstance_getSize(BufferLayoutInstance *instance, CharString path) {

	U64 id = BufferLayoutInstance_getIndex(instance, path, true);

	if(id == U64_MAX)
		return U32_MAX;

	return ((const BufferLayoutDynamicData*)instance->allocations.ptr)[id].arraySize;
}

Error BufferLayoutInstance_setSizeInternal(
	BufferLayoutInstance *instance,
	CharString path,
	U32 size,
	U32 stride,
	Allocator alloc
) {

	//Cut /myTest/ (outer /)

	fixPath(&path);

	if(CharString_isEmpty(path))
		return Error_invalidParameter(1, 0);

	if(CharString_length(path) >= U32_MAX)
		return Error_outOfBounds(1, CharString_length(path), U32_MAX);

	//

	U64 id = BufferLayoutInstance_getIndex(instance, path, false);

	//Missing from array; Create new

	Bool wasNew = id == U64_MAX;

	if(wasNew) {

		Error err = List_pushBack(&instance->allocations, Buffer_createNull(), alloc);

		if(err.genericError)
			return err;

		id = instance->allocations.length - 1;
	}

	BufferLayoutDynamicData *data = (BufferLayoutDynamicData*)instance->allocations.ptr + id;
	Buffer *prev = NULL;

	if(!wasNew) {

		if(data->arraySize == size)		//Same size
			return Error_none();

		prev = &data->data;

		if(CharString_length(path) != data->nameLength)
			return Error_invalidOperation(0);
	}

	//Move data to new buffer of the same size.

	Buffer neo = Buffer_createNull();
	Error err = Buffer_createEmptyBytes(CharString_length(path) + (U64)size * stride, alloc, &neo);

	if(err.genericError)
		return err;

	//Copy name and contents to new buffer and preserve old content (truncated)

	if(prev) {
		Buffer_copy(neo, *prev);
		Buffer_free(prev, alloc);
	}

	//Save name only; the rest is empty

	else Buffer_copy(neo, CharString_bufferConst(path));

	//Save

	data->data = neo;
	data->arraySize = size;
	data->nameLength = CharString_length(path);

	return Error_none();
}

Error BufferLayoutInstance_setSize(
	BufferLayoutInstance *instance,
	CharString path,
	U32 size,
	Allocator alloc
) {

	if(!instance || !instance->layout.structs.ptr)
		return Error_nullPointer(0);

	LayoutPathInfo info = (LayoutPathInfo) { 0 };
	Error err = BufferLayoutInstance_resolveLayout(
		*instance,
		path,
		&info,
		NULL,
		false, true,
		alloc
	);

	if(err.genericError)
		return err;

	//If it's not an array or it's not dynamic, it's an invalid operation.

	if (info.leftoverArray.length == 0 || *(const U32*)info.leftoverArray.ptr)
		return Error_invalidOperation(0);

	//Get type stride.
	//Since info.length is 0.

	U64 stride = info.stride * size;

	//Dynamic arrays (as sub array) mean nothing gets allocated for this.
	//Example: C8[][8][] if you setSize on the root it won't allocate anything besides name.
	//Then setSize on arr/0/0 will actually allocate.
	//C8[][4] setSize will allocate 4 C8s per element.

	for(U64 i = 1; i < info.leftoverArray.length; ++i) {

		U64 arri = ((const U32*)info.leftoverArray.ptr)[i];

		if(arri && stride * arri < stride)
			return Error_overflow(2, stride * arri, U64_MAX);

		stride *= arri;
	}

	return BufferLayoutInstance_setSizeInternal(
		instance, 
		path,
		size,
		stride,
		alloc
	);
}

Error BufferLayoutInstance_setString(
	BufferLayoutInstance *instance, 
	CharString path, 
	CharString value, 
	Allocator alloc
) {

	if(!instance || !instance->layout.structs.ptr)
		return Error_nullPointer(0);

	LayoutPathInfo info = (LayoutPathInfo) { 0 };
	Error err = BufferLayoutInstance_resolveLayout(
		*instance,
		path,
		&info,
		NULL,
		false, true,
		alloc
	);

	if(err.genericError)
		return err;

	//Only allow C8[] to set it

	if (info.leftoverArray.length == 1 && info.typeId == ETypeId_C8) {

		U32 len = *(const U32*)info.leftoverArray.ptr;
		U64 newLen = CharString_length(value);

		//Limit of 4GiB strings in serialization.
		//If you really want to, use paging (e.g. C8[][])
		//and then allocate it per 4GiB chunks.

		if(newLen >= U32_MAX)
			return Error_outOfBounds(2, newLen, U32_MAX);

		//If it's a static array, we'll only allow setString if the string is the same size.

		if(len && (U32)newLen != len)
			return Error_invalidOperation(1);

		//Dynamic array, so it has to be resized.

		if(
			!len && 
			(err = BufferLayoutInstance_setSizeInternal(instance, path, (U32) newLen, sizeof(C8), alloc)).genericError
		)
			return err;

		//Set the data

		return BufferLayoutInstance_setData(*instance, path, CharString_bufferConst(value), alloc);
	}

	return Error_invalidOperation(0);
}

Error BufferLayout_resolveLayout(
	BufferLayout layout,
	CharString path,
	LayoutPathInfo *info,
	CharString *parent,					//If not null, will return a StringRef into CharString (empty if root)
	Allocator alloc
) {
	return BufferLayoutInstance_resolveLayout(
		(BufferLayoutInstance) { .layout = layout },
		path,
		info,
		parent,
		false,
		false,
		alloc
	);
}

Error BufferLayoutInstance_resolveLayout(
	BufferLayoutInstance layoutInstance,
	CharString path,
	LayoutPathInfo *info,
	CharString *parent,
	Bool resolveDynamicSizes,
	Bool checkDynamicSizes,	
	Allocator alloc
) {

	if(!layoutInstance.layout.structs.ptr)
		return Error_nullPointer(0);

	if(!info)
		return Error_nullPointer(2);

	if(info->memberName.ptr)
		return Error_invalidParameter(2, 0);

	if(parent && parent->ptr)
		return Error_invalidParameter(3, 0);

	if(layoutInstance.layout.rootStructIndex >= layoutInstance.layout.structs.length)
		return Error_unsupportedOperation(0);

	if(CharString_length(path) >= U16_MAX)
		return Error_invalidParameter(1, 1);

	if(
		CharString_equalsString(path, CharString_createConstRefUnsafe("//"), EStringCase_Sensitive) ||
		CharString_equalsString(path, CharString_createConstRefUnsafe("/\\/"), EStringCase_Sensitive)
	)
		return Error_invalidParameter(1, 0);

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

	U32 currentStructId = layoutInstance.layout.rootStructIndex;
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
			.stride = totalLength,
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
	U64 prev = 0;

	for (U64 i = start; i < end; ++i) {

		C8 v = CharString_getAt(path, i);

		if(!C8_isValidAscii(v))
			_gotoIfError(clean, Error_invalidParameter(1, 3));

		//For the final character we append first so we can process the final access here.

		Bool isEnd = i == end - 1;
		
		//Handle special ending; \\ or \/

		if (i == end - 2 && v == '\\') {

			//Escaped slash

			if (CharString_getAt(path, end - 1) == '/') {
				isEnd = true;
				++i;
				_gotoIfError(clean, CharString_append(&copy, '/', alloc));
			}

			//Escaped backslash

			else if (CharString_getAt(path, end - 1) == '\\') {
				isEnd = true;
				++i;
				_gotoIfError(clean, CharString_append(&copy, '\\', alloc));
			}

			//It's a normal backslash and it not the end of the string yet.
			//It'll add it at the end of the loop
		}

		//Handle end

		else if(isEnd)
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

			//Exclude last child to get parent

			if(isEnd && parent)
				*parent = CharString_createConstRefSized(path.ptr, prev, false);

			//Remember last parent and clear member name buff

			prev = i;
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
		.structId = currentMember.structId,
		.stride = currentMember.stride
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

Error BufferLayoutInstance_resolve(
	BufferLayoutInstance layoutInstance, 
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
	Error err = BufferLayout_resolveLayout(layout, path, &info, NULL, alloc);

	if(err.genericError)
		return err;

	if(info.offset + info.length > Buffer_length(buffer))
		return Error_outOfBounds(0, info.offset + info.length, Buffer_length(buffer));

	*location = 
		Buffer_isConstRef(buffer) ? Buffer_createConstRef(buffer.ptr + info.offset, info.length) : 
		Buffer_createRef((U8*)buffer.ptr + info.offset, info.length);

	return Error_none();
}

Error BufferLayoutInstance_setData(
	BufferLayoutInstance layoutInstance, 
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

BufferLayoutInstance BufferLayoutInstance_createConstRef(BufferLayoutInstance nonConst) {

	List tmp = (List) { 0 };

	if(
		tmp.length &&
		List_createConstRef(
			nonConst.allocations.ptr, sizeof(BufferLayoutDynamicData), nonConst.allocations.length,
			&tmp
		).genericError
	)
		return (BufferLayoutInstance) { 0 };

	nonConst.allocations = tmp;
	nonConst.data = Buffer_createConstRef(nonConst.data.ptr, Buffer_length(nonConst.data));
	return nonConst;
}

Error BufferLayoutInstance_getData(
	BufferLayoutInstance layoutInstance, 
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
Error BufferLayoutInstance_set##T(																	\
	BufferLayoutInstance layoutInstance, 															\
	CharString path, 																				\
	T t, 																							\
	Allocator alloc																					\
) {																									\
	return BufferLayoutInstance_setData(															\
		layoutInstance,																				\
		path,																						\
		Buffer_createConstRef(&t, sizeof(T)),														\
		alloc																						\
	);																								\
}																									\
																									\
Error BufferLayoutInstance_get##T(																	\
	BufferLayoutInstance layoutInstance, 															\
	CharString path, 																				\
	T *t, 																							\
	Allocator alloc																					\
) {																									\
																									\
	if(!t)																							\
		return Error_nullPointer(2);																\
																									\
	Buffer buf = Buffer_createRef(t, sizeof(T));													\
	Buffer currentData = Buffer_createNull();														\
																									\
	Error err = BufferLayoutInstance_getData(														\
		layoutInstance, 																			\
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

Error BufferLayoutInstance_foreach(
	BufferLayoutInstance layoutInstance,
	CharString path,
	BufferLayoutForeachDataFunc func,
	void *userData,
	Bool isRecursive,
	Allocator alloc
) {

	if(!func)
		return Error_nullPointer(2);

	LayoutPathInfo info = (LayoutPathInfo) { 0 };
	Error err = Error_none();
	CharString tmp = CharString_createNull();
	CharString tmp1 = CharString_createNull();

	_gotoIfError(clean, BufferLayoutInstance_resolveLayout(layoutInstance, path, &info, NULL, true, true, alloc));

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
				.structId = info.structId,
				.stride = info.stride
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
				.structId = infoi.structId,
				.stride = infoi.stride
			};

			if(infoi.arraySizes.length)
				_gotoIfError(clean, List_createConstRef(
					infoi.arraySizes.ptr, infoi.arraySizes.length, infoi.arraySizes.stride, 
					&layoutPathInfo.leftoverArray
				));

			//TODO: Fix: Path can contain \. This would turn members into wrong path
			//		Escape infoi.name

			if(!CharString_length(path) || CharString_equals(path, '/', EStringCase_Sensitive))
				_gotoIfError(clean, CharString_format(
					alloc,
					&tmp,
					"%.*s",
					(int) CharString_length(infoi.name),
					infoi.name.ptr
				))

			else { 

				_gotoIfError(clean, CharString_createCopy(infoi.name, alloc, &tmp1));

				//Replace \ with \\ to ensure it's escaped
				//And after that we replace / with \/.

				_gotoIfError(clean, CharString_replaceAllString(
					&tmp1, 
					CharString_createConstRefUnsafe("\\"),
					CharString_createConstRefUnsafe("\\\\"),
					EStringCase_Sensitive,
					alloc
				));

				_gotoIfError(clean, CharString_replaceAllString(
					&tmp1, 
					CharString_createConstRefUnsafe("/"),
					CharString_createConstRefUnsafe("\\/"),
					EStringCase_Sensitive,
					alloc
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

				CharString_free(&tmp1, alloc);
			}

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
	LayoutPathInfo_free(&info, alloc);
	CharString_free(&tmp1, alloc);
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

	_gotoIfError(clean, BufferLayout_resolveLayout(layout, CharString_createConstRefUnsafe("/"), &info, NULL, alloc));
	
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

Error BufferLayout_getStructName(BufferLayout layout, U32 structId, CharString *typeName) {

	if(!typeName)
		return Error_nullPointer(2);

	if(typeName->ptr)
		return Error_invalidParameter(2, 0);

	if(structId >= layout.structs.length)
		return Error_outOfBounds(1, structId, layout.structs.length);

	BufferLayoutStruct str = ((const BufferLayoutStruct*)layout.structs.ptr)[structId];

	*typeName = CharString_createConstRefSized((const C8*) str.data.ptr, str.nameLength, false);
	return Error_none();
}

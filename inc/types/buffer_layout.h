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

#pragma once
#include "types/type_id.h"
#include "types/string.h"
#include "math/vec.h"

//Buffer layout member

typedef struct BufferLayoutMember {

	union {
		ETypeId typeIdUnion;
		U32 structIdUnion;
	};

	U32 offsetLo;

	U16 offsetHiAndIsStruct;
	U8 arrayIndices;			//0 = no array, 1 = 1D, 2 = 2D, etc.
	U8 nameLength;

	U32 stride;

} BufferLayoutMember;

U64 BufferLayoutMember_getOffset(BufferLayoutMember member);
Bool BufferLayoutMember_isStruct(BufferLayoutMember member);

ETypeId BufferLayoutMember_getTypeId(BufferLayoutMember member);
U32 BufferLayoutMember_getStructId(BufferLayoutMember member);

//Runtime info struct.
//To save RAM this is not used instead of BufferLayoutMember.
//This is only to read a single member and to create a single struct.
//After compaction it reduces in size by 6x.
//JSON is more flexible with notation and so it could end up making lots of structs.
//Or in other languages the same struct may be recreated multiple times but with a different padding.

typedef struct BufferLayoutMemberInfo {

	CharString name;
	List arraySizes;		//List<U32>

	ETypeId typeId;			//Set to ETypeId_Undefined to use structId
	U32 structId;

	U64 offset;

	U32 stride;
	U32 padding;

} BufferLayoutMemberInfo;

Error BufferLayoutMemberInfo_create(
	ETypeId typeId, 
	CharString name, 
	U64 offset, 
	U32 stride, 
	BufferLayoutMemberInfo *result
);

Error BufferLayoutMemberInfo_createStruct(
	U32 structId, 
	CharString name, 
	U64 offset, 
	U32 stride, 
	BufferLayoutMemberInfo *result
);

Error BufferLayoutMemberInfo_createArray(
	ETypeId typeId, 
	CharString name, 
	List arraySizes, 
	U64 offset, 
	U32 stride, 
	BufferLayoutMemberInfo *result
);

Error BufferLayoutMemberInfo_createStructArray(
	U32 structId, 
	CharString name, 
	List arraySizes, 
	U64 offset, 
	U32 stride, 
	BufferLayoutMemberInfo *result
);

Bool BufferLayoutMemberInfo_isDynamic(BufferLayoutMemberInfo info);

//Buffer layout struct

typedef struct BufferLayoutStruct {

	U32 id;

	U16 memberCount;
	U8 nameLength;
	Bool isContiguous;		//If any of the members (or their members) aren't contigious then this won't be.

	//structName: C8[nameLength];
	//members: BufferLayoutMember[memberCount];
	//memberData: (U32[members[i].arrayIndices], C8[members[i].nameLength])[memberCount];

	Buffer data;

} BufferLayoutStruct;

typedef struct BufferLayoutStructInfo {

	CharString name;
	List members;		//List<BufferLayoutMemberInfo>

} BufferLayoutStructInfo;

CharString BufferLayoutStruct_getName(BufferLayoutStruct layoutStruct);

BufferLayoutMember BufferLayoutStruct_getMember(BufferLayoutStruct layoutStruct, U16 memberId);
BufferLayoutMemberInfo BufferLayoutStruct_getMemberInfo(BufferLayoutStruct layoutStruct, U16 memberId);

//Buffer layout

typedef struct BufferLayout {

	List structs;				//List<BufferLayoutStruct>

	U32 rootStructIndex;

} BufferLayout;

Error BufferLayout_create(Allocator alloc, BufferLayout *layout);
Bool BufferLayout_free(BufferLayout *layout, Allocator alloc);

//Struct management

Error BufferLayout_createStruct(BufferLayout *layout, BufferLayoutStructInfo info, Allocator alloc, U32 *id);

Error BufferLayout_assignRootStruct(BufferLayout *layout, U32 id);

//Instantiating a buffer

typedef struct BufferLayoutDynamicData {

	U32 arraySize;

	U32 nameLength;

	Buffer data;			//C8[nameLength], dynamic data (/ arraySize = stride)

} BufferLayoutDynamicData;

typedef struct BufferLayoutInstance {

	//These allocations are for dynamically sized arrays.
	//For example:
	// 
	//"arr": "F32[][]"
	//"arr" => (123, void* (null))
	//"arr/0" => (23, F32[23])
	//"arr/1" => (24, F32[24])
	// 
	//"arr1": "F32[3][]"
	//"arr1/0" => (123, F32[123])
	//
	//"arr2": "F32[][3]"
	//"arr2" => (123, (F32[3])[123])
	// 
	//2D array is not being allocated as 123x23 because for example C8[][] is a CharString[].
	//And each element there is of course a different size.

	List allocations;			//<BufferLayoutDynamicData>

	Buffer data;				//Root data

	BufferLayout layout;		//Reference to the buffer layout

} BufferLayoutInstance;

Error BufferLayoutInstance_create(BufferLayout layout, Allocator alloc, BufferLayoutInstance *result);
Bool BufferLayoutInstance_free(BufferLayoutInstance *instance, Allocator alloc);

//Returns U32_MAX if not found
U32 BufferLayoutInstance_getSize(BufferLayoutInstance *instance, CharString path);

Error BufferLayoutInstance_setSize(BufferLayoutInstance *instance, CharString path, U32 size, Allocator alloc);
Error BufferLayoutInstance_setString(BufferLayoutInstance *instance, CharString path, CharString value, Allocator alloc);

//Resolving a path to a layout id
//A BufferLayout path uses the following rules:
//No backwards paths unlike file paths (So x/./ would be referring to x's member called . not x!).
//Case sensitive.
//Backslash is treated as a normal character unless a slash or backslash is next to it. In that case:
//\/ is treated as a normal slash as part of the member name.
//\\ is treated as a normal backslash; e.g. \\/ would evaluate to \/ in the member name.
//If the object is an array, it will treat it the same as a member:
//a/0 or b/0 could be either a pointing to the 0th index and b pointing to a member called 0.
//a/0/0/0 could be pointing to a[0][0][0] or a.0.0.0 depending on context.
//A layout path can start with a slash, so /a/ and a/ would resolve to the same (a).
//Leading slashes are ignored.
//Empty members are invalid, so that makes // and /a// invalid.
//Array indices can be given by either hex (0x[0-9A-Fa-f]+), octal (0o[0-7]+), 
//binary 0b[0-1]+, decimal ([0-9]+) or Nytodecimal (0n[0-9A-Za-z_$]+).

typedef struct LayoutPathInfo {

	CharString memberName;		//Can be empty for root struct
	U64 offset, length, stride;
	ETypeId typeId;
	U32 structId;
	List leftoverArray;			//How long the remainder of array is

} LayoutPathInfo;

Bool LayoutPathInfo_free(LayoutPathInfo *info, Allocator alloc);

Error BufferLayoutInstance_resolveLayout(
	BufferLayoutInstance layoutInstance,
	CharString path,
	LayoutPathInfo *info,
	CharString *parent,					//If not null, will return a StringRef into CharString (empty if root).
	Bool resolveDynamicSizes,			//If true, LayoutPathInfo has to be freed!
	Bool checkDynamicSizes,				//If true, will require valid instance (not only layout) to check sizes.
	Allocator alloc
);

Error BufferLayout_resolveLayout(
	BufferLayout layout,
	CharString path,
	LayoutPathInfo *info,
	CharString *parent,					//If not null, will return a StringRef into CharString (empty if root)
	Allocator alloc
);

Error BufferLayoutInstance_resolve(
	BufferLayoutInstance layoutInstance, 
	CharString path, 
	Buffer *location, 
	Allocator alloc
);

//Setting data in the buffer
//Only works on a contiguous block of memory.
//E.g. setting C8[][] doesn't work. But setting C8[] does work.
//Any normal array of a static block of memory works too 
//(e.g. MyStruct[10] or MyStruct[] if MyStruct is pod).
//Dynamic variables are seen as size 0, 
//So technically if MyStruct contains C8[] and F32 then it'll become a F32[].
//But this isn't accessible by the API directly. 

Error BufferLayoutInstance_setData(
	BufferLayoutInstance layoutInstance, 
	CharString path, 
	Buffer newData, 
	Allocator alloc
);

Error BufferLayoutInstance_getData(
	BufferLayoutInstance layoutInstance, 
	CharString path, 
	Buffer *currentData, 
	Allocator alloc
);

BufferLayoutInstance BufferLayoutInstance_createConstRef(BufferLayoutInstance nonConst);

//Auto generated getters and setters

#define _BUFFER_LAYOUT_SGET(T)																		\
																									\
Error BufferLayout_set##T(																			\
	BufferLayoutInstance layoutInstance, 															\
	CharString path, 																				\
	T t, 																							\
	Allocator alloc																					\
);																									\
																									\
Error BufferLayout_get##T(																			\
	BufferLayoutInstance layoutInstance, 															\
	CharString path, 																				\
	T *t, 																							\
	Allocator alloc																					\
);

#define _BUFFER_LAYOUT_XINT_SGET(prefix, suffix)													\
_BUFFER_LAYOUT_SGET(prefix##8##suffix);																\
_BUFFER_LAYOUT_SGET(prefix##16##suffix);															\
_BUFFER_LAYOUT_SGET(prefix##32##suffix);															\
_BUFFER_LAYOUT_SGET(prefix##64##suffix);

#define _BUFFER_LAYOUT_VEC_SGET(prefix)																\
_BUFFER_LAYOUT_SGET(prefix##32##x2);																\
_BUFFER_LAYOUT_SGET(prefix##32##x4)

//Setters for C8, Bool and ints/uints

_BUFFER_LAYOUT_SGET(C8);
_BUFFER_LAYOUT_SGET(Bool);

_BUFFER_LAYOUT_XINT_SGET(U, );
_BUFFER_LAYOUT_XINT_SGET(I, );

_BUFFER_LAYOUT_SGET(F32);
_BUFFER_LAYOUT_SGET(F64);

_BUFFER_LAYOUT_VEC_SGET(I);
_BUFFER_LAYOUT_VEC_SGET(F);

//Looping through parent and child members

typedef Error (*BufferLayoutForeachDataFunc)(BufferLayout, LayoutPathInfo, CharString, Buffer, void*);

Error BufferLayoutInstance_foreachData(
	BufferLayoutInstance layoutInstance, 
	CharString path, 
	BufferLayoutForeachDataFunc func, 
	void *userData,
	Bool isRecursive,
	Allocator alloc
);

Error BufferLayout_getStructName(BufferLayout layout, U32 structId, CharString *typeName);

//TODO: Helper functions such as isStruct, isArray, isValue, isType

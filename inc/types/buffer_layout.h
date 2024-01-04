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
#include "types/vec.h"

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
	ListU32 arraySizes;

	ETypeId typeId;			//Set to ETypeId_Undefined to use structId
	U32 structId;

	U64 offset;

	U32 stride;
	U32 padding;

} BufferLayoutMemberInfo;

TList(BufferLayoutMemberInfo);

BufferLayoutMemberInfo BufferLayoutMemberInfo_create(ETypeId typeId, CharString name, U64 offset, U32 stride);
BufferLayoutMemberInfo BufferLayoutMemberInfo_createStruct(U32 structId, CharString name, U64 offset, U32 stride);

BufferLayoutMemberInfo BufferLayoutMemberInfo_createArray(
	ETypeId typeId, 
	CharString name, 
	ListU32 arraySizes, 
	U64 offset, 
	U32 stride
);

BufferLayoutMemberInfo BufferLayoutMemberInfo_createStructArray(
	U32 structId, 
	CharString name, 
	ListU32 arraySizes, 
	U64 offset, 
	U32 stride
);

//Buffer layout struct

typedef struct BufferLayoutStruct {

	U32 id;

	U16 memberCount;
	U8 nameLength;
	U8 padding0;

	//structName: C8[nameLength];
	//members: BufferLayoutMember[memberCount];
	//memberData: (U32[members[i].arrayIndices], C8[members[i].nameLength])[memberCount];

	Buffer data;

} BufferLayoutStruct;

typedef struct BufferLayoutStructInfo {

	CharString name;
	ListBufferLayoutMemberInfo members;

} BufferLayoutStructInfo;

TList(BufferLayoutStruct);

CharString BufferLayoutStruct_getName(BufferLayoutStruct layoutStruct);

BufferLayoutMember BufferLayoutStruct_getMember(BufferLayoutStruct layoutStruct, U16 memberId);
BufferLayoutMemberInfo BufferLayoutStruct_getMemberInfo(BufferLayoutStruct layoutStruct, U16 memberId);

//Buffer layout

typedef struct BufferLayout {

	ListBufferLayoutStruct structs;

	U32 rootStructIndex;

} BufferLayout;

Error BufferLayout_create(Allocator alloc, BufferLayout *layout);
Bool BufferLayout_free(Allocator alloc, BufferLayout *layout);

//Struct management

Error BufferLayout_createStruct(BufferLayout *layout, BufferLayoutStructInfo info, Allocator alloc, U32 *id);

Error BufferLayout_assignRootStruct(BufferLayout *layout, U32 id);

//Instantiating a buffer

Error BufferLayout_createInstance(BufferLayout layout, U64 count, Allocator alloc, Buffer *result);

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
//A layout path can start with a slash, so /a/ and a/ would resolve to the same.
//Leading slashes are ignored.
//Empty members are invalid, so that makes // and /a// invalid.
//Array indices can be given by either hex (0x[0-9A-Fa-f]+), octal (0o[0-7]+), 
//binary 0b[0-1]+, decimal ([0-9]+) or Nytodecimal (0n[0-9A-Za-z_$]+).

typedef struct LayoutPathInfo {

	U64 offset, length;
	ETypeId typeId;
	U32 structId;
	ListU32 leftoverArray;		//How long the remainder of array is

} LayoutPathInfo;

Error BufferLayout_resolveLayout(BufferLayout layout, CharString path, LayoutPathInfo *info, Allocator alloc);

Error BufferLayout_resolve(
	Buffer buffer, 
	BufferLayout layout, 
	CharString path, 
	Buffer *location, 
	Allocator alloc
);

//Setting data in the buffer

Error BufferLayout_setData(
	Buffer buffer, 
	BufferLayout layout, 
	CharString path, 
	Buffer newData, 
	Allocator alloc
);

Error BufferLayout_getData(
	Buffer buffer, 
	BufferLayout layout, 
	CharString path, 
	Buffer *currentData, 
	Allocator alloc
);

//Auto generated getters and setters

#define _BUFFER_LAYOUT_SGET(T)																		\
																									\
Error BufferLayout_set##T(																			\
	Buffer buffer, 																					\
	BufferLayout layout, 																			\
	CharString path, 																				\
	T t, 																							\
	Allocator alloc																					\
);																									\
																									\
Error BufferLayout_get##T(																			\
	Buffer buffer, 																					\
	BufferLayout layout, 																			\
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

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

#include "types/type_id.h"
#include "types/string.h"
#include "types/allocator.h"
#include "types/error.h"
#include "types/buffer.h"

Bool EDataType_isSigned(EDataType type) { return type & EDataType_IsSigned; }

EDataType ETypeId_getDataType(ETypeId id) { return (EDataType)(id & 7); }
U8 ETypeId_getDataTypeBytes(ETypeId id) { return 1 << ((id >> 3) & 3); }
U8 ETypeId_getHeight(ETypeId id) { return (id >> 5) & 3; }
U8 ETypeId_getWidth(ETypeId id) { return (id >> 7) & 3; }
U8 ETypeId_getElements(ETypeId id) { return ETypeId_getWidth(id) * ETypeId_getHeight(id); }
U64 ETypeId_getBytes(ETypeId id) { return (U64)ETypeId_getDataTypeBytes(id) * ETypeId_getElements(id); }

U8 ETypeId_getLibraryId(ETypeId id) { return (U8)(id >> 24); }
U8 ETypeId_getLibraryTypeId(ETypeId id) { return (U8)(id >> 16); }

//Lookups

List ETypeId_lookup;
Bool ETypeId_initialized = false;

typedef struct ETypeIdLookup {
	CharString name;
	ETypeId id;
} ETypeIdLookup;

#define _Expand(T) {													\
																		\
	ETypeIdLookup type = (ETypeIdLookup) {								\
		.name = CharString_createConstRefUnsafe(#T),					\
		.id = T															\
	};																	\
																		\
	_gotoIfError(clean, List_pushBack(									\
		&ETypeId_lookup, 												\
		Buffer_createConstRef((const U8*)&type, sizeof(type)), 			\
		alloc															\
	));																	\
}

#define _ExpandXInt(prefix, suffix) 									\
_Expand(ETypeId_##prefix##8##suffix);									\
_Expand(ETypeId_##prefix##16##suffix);									\
_Expand(ETypeId_##prefix##32##suffix);									\
_Expand(ETypeId_##prefix##64##suffix);

#define _ExpandXIntN(prefix, suffix) 									\
_ExpandXInt(prefix, x2##suffix);										\
_ExpandXInt(prefix, x3##suffix);										\
_ExpandXInt(prefix, x4##suffix);

#define _ExpandXIntNxN(prefix) 											\
_ExpandXIntN(prefix, x2);												\
_ExpandXIntN(prefix, x3);												\
_ExpandXIntN(prefix, x4);

#define _ExpandFloat(suffix) 											\
_Expand(ETypeId_F##16##suffix);											\
_Expand(ETypeId_F##32##suffix);											\
_Expand(ETypeId_F##64##suffix);

#define _ExpandFloatN(suffix) 											\
_ExpandFloat(x2##suffix);												\
_ExpandFloat(x3##suffix);												\
_ExpandFloat(x4##suffix);

#define _ExpandFloatNxN() 												\
_ExpandFloatN(x2);														\
_ExpandFloatN(x3);														\
_ExpandFloatN(x4);

Error ETypeId_create(Allocator alloc) {

	if(ETypeId_initialized)
		return Error_invalidOperation(0);

	ETypeId_lookup = List_createEmpty(sizeof(ETypeIdLookup));
	Error err = List_reserve(&ETypeId_lookup, 256, alloc);

	if(err.genericError)
		return err;

	_Expand(ETypeId_C8);
	_Expand(ETypeId_Bool);

	_ExpandXInt(I, );
	_ExpandXInt(U, );
	_ExpandFloat();

	_ExpandXIntN(I, );
	_ExpandXIntN(U, );
	_ExpandFloatN();

	_ExpandXIntNxN(I);
	_ExpandXIntNxN(U);
	_ExpandFloatNxN();

	ETypeId_initialized = true;

clean:

	if(err.genericError)
		List_free(&ETypeId_lookup, alloc);

	return err;
}

Bool ETypeId_free(Allocator alloc) {

	if(!ETypeId_initialized)
		return true;

	//Even though builtin types don't need to free the strings, user defined types do

	for(U64 i = ETypeId_Max; i < ETypeId_lookup.length; ++i)
		CharString_free(&((ETypeIdLookup*)ETypeId_lookup.ptr)->name, alloc);

	List_free(&ETypeId_lookup, alloc);
	ETypeId_initialized = false;
	return true;
}

Error ETypeId_registerTypeId(ETypeId id, CharString name, Allocator alloc) {

	//Validate library id and if not undefined

	U8 libId = ETypeId_getLibraryId(id);

	if((libId >> 4) == 0xC)
		return Error_invalidParameter(0, 0);

	if(id == ETypeId_Undefined)
		return Error_invalidParameter(0, 1);

	//Check if type exists first

	ETypeId t = ETypeId_Undefined;
	Error err = ETypeId_fromString(name, &t);

	if(err.genericError && err.genericError != EGenericError_NotFound)
		return err;

	if(!err.genericError)
		return Error_alreadyDefined(0);
	
	//Even though it could be safe to keep the name by reference,
	//it could also not be.
	//So we have to copy here.

	CharString copy = CharString_createNull();
	err = Error_none();

	_gotoIfError(clean, CharString_createCopy(name, alloc, &copy));

	ETypeIdLookup type = (ETypeIdLookup) {
		.name = name,
		.id = id
	};

	_gotoIfError(clean, List_pushBack(
		&ETypeId_lookup,
		Buffer_createConstRef((const U8*)&type, sizeof(type)),
		alloc
	));

clean:

	if(err.genericError)
		CharString_free(&copy, alloc);

	return err;
}

Error ETypeId_registerType(
	U8 libraryId,
	U8 typeId,
	U8 width,
	U8 height,
	EDataTypeStride dataTypeStride,
	EDataType dataType,
	CharString name,
	ETypeId *result,
	Allocator alloc
) {
	
	if(width > 4 || height > 4)
		return Error_outOfBounds(width > 4 ? 2 : 3, width > 4 ? width : height, 4);

	if(dataTypeStride > EDataTypeStride_64)
		return Error_invalidEnum(4, (U64) dataTypeStride, (U64) EDataTypeStride_64);

	if(!result)
		return Error_nullPointer(7);

	switch (dataType) {

		case EDataType_Bool:
		case EDataType_Char:

			if(dataTypeStride != EDataTypeStride_8 || width != 1 || height != 1)
				return Error_invalidOperation(0);
			
			break;

		case EDataType_Float:

			if(dataTypeStride == EDataTypeStride_8)
				return Error_invalidOperation(1);

			break;

		case EDataType_UInt:
		case EDataType_Int:
			break;

		default:
			return Error_invalidEnum(5, (U64) dataType, 0);
	}

	return ETypeId_registerTypeId(
		*result = _makeTypeId(libraryId, typeId, width, height, dataTypeStride, dataType),
		name,
		alloc
	);
}

//TODO: Hashmap

Error ETypeId_asString(ETypeId id, CharString *result) {

	if(!ETypeId_initialized)
		return Error_invalidOperation(0);

	if(id == ETypeId_Undefined)
		return Error_invalidParameter(0, 0);

	if(!result)
		return Error_nullPointer(1);

	if(CharString_length(*result))
		return Error_invalidParameter(1, 0);

	for (U64 i = 0; i < ETypeId_lookup.length; ++i) {

		ETypeIdLookup lookup = ((ETypeIdLookup*)ETypeId_lookup.ptr)[i];

		if (lookup.id == id) {

			*result = CharString_createConstRefSized(
				lookup.name.ptr,
				CharString_length(lookup.name),
				CharString_isNullTerminated(lookup.name)
			);

			return Error_none();
		}
	}

	return Error_notFound(0, 1);
}

Error ETypeId_fromString(CharString str, ETypeId *id) {

	if(!ETypeId_initialized)
		return Error_invalidOperation(0);

	if(!CharString_length(str))
		return Error_invalidParameter(0, 0);

	if(!id)
		return Error_nullPointer(1);

	for (U64 i = 0; i < ETypeId_lookup.length; ++i) {

		ETypeIdLookup lookup = ((ETypeIdLookup*)ETypeId_lookup.ptr)[i];

		if (CharString_equalsString(lookup.name, str, EStringCase_Sensitive)) {
			*id = lookup.id;
			return Error_none();
		}
	}

	return Error_notFound(0, 1);
}

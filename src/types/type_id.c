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
U8 ETypeId_getHeight(ETypeId id) { return ((id >> 5) & 3) + 1; }
U8 ETypeId_getWidth(ETypeId id) { return ((id >> 7) & 3) + 1; }
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
	ETypeIdStringifyFunc func;
} ETypeIdLookup;

#define _Expand(T) {																\
																					\
	ETypeIdLookup type = (ETypeIdLookup) {											\
		.name = CharString_createConstRefUnsafe(#T + sizeof("ETypeId_")  - 1),		\
		.id = T																		\
	};																				\
																					\
	_gotoIfError(clean, List_pushBack(												\
		&ETypeId_lookup, 															\
		Buffer_createConstRef((const U8*)&type, sizeof(type)), 						\
		alloc																		\
	));																				\
}

#define _ExpandXInt(prefix, suffix) 												\
_Expand(ETypeId_##prefix##8##suffix);												\
_Expand(ETypeId_##prefix##16##suffix);												\
_Expand(ETypeId_##prefix##32##suffix);												\
_Expand(ETypeId_##prefix##64##suffix);

#define _ExpandXIntN(prefix, suffix) 												\
_ExpandXInt(prefix, x2##suffix);													\
_ExpandXInt(prefix, x3##suffix);													\
_ExpandXInt(prefix, x4##suffix);

#define _ExpandXIntNxN(prefix) 														\
_ExpandXIntN(prefix, x2);															\
_ExpandXIntN(prefix, x3);															\
_ExpandXIntN(prefix, x4);

#define _ExpandFloat(suffix) 														\
_Expand(ETypeId_F##16##suffix);														\
_Expand(ETypeId_F##32##suffix);														\
_Expand(ETypeId_F##64##suffix);

#define _ExpandFloatN(suffix) 														\
_ExpandFloat(x2##suffix);															\
_ExpandFloat(x3##suffix);															\
_ExpandFloat(x4##suffix);

#define _ExpandFloatNxN() 															\
_ExpandFloatN(x2);																	\
_ExpandFloatN(x3);																	\
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

Error ETypeId_registerTypeId(ETypeId id, CharString name, ETypeIdStringifyFunc stringifyFunc, Allocator alloc) {

	if(!stringifyFunc)
		return Error_nullPointer(2);

	//Validate library id and if not undefined

	U8 libId = ETypeId_getLibraryId(id);

	if((libId >> 4) == 0xC)
		return Error_invalidParameter(0, 0);

	if(id == ETypeId_Undefined)
		return Error_invalidParameter(0, 1);

	//Check if type exists first

	//Name

	ETypeId t = ETypeId_Undefined;
	Error err = ETypeId_fromString(name, &t);

	if(err.genericError && err.genericError != EGenericError_NotFound)
		return err;

	if(!err.genericError)
		return Error_alreadyDefined(0);

	//Then check description to be unique

	t = ETypeId_Undefined;
	err = ETypeId_getTypeFromDesc((U16)id, ETypeId_getLibraryId(id), &t);

	if(!err.genericError)
		return Error_alreadyDefined(1);

	//Then check short type to be unique to be unique

	t = ETypeId_Undefined;
	err = ETypeId_getTypeFromShortType(ETypeId_getLibraryId(id), ETypeId_getLibraryTypeId(id), &t);

	if(!err.genericError)
		return Error_alreadyDefined(1);
	
	//Even though it could be safe to keep the name by reference,
	//it could also not be.
	//So we have to copy here.

	CharString copy = CharString_createNull();
	err = Error_none();

	_gotoIfError(clean, CharString_createCopy(name, alloc, &copy));

	ETypeIdLookup type = (ETypeIdLookup) {
		.name = name,
		.id = id,
		.func = stringifyFunc
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
	ETypeIdStringifyFunc stringifyFunc,
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
		stringifyFunc,
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

Error ETypeId_getTypeFromDesc(U16 desc, U8 libraryId, ETypeId *res) {

	if(!res)
		return Error_nullPointer(1);

	for (U64 i = 0; i < ETypeId_lookup.length; ++i) {

		ETypeIdLookup lookup = ((ETypeIdLookup*)ETypeId_lookup.ptr)[i];

		if (ETypeId_getLibraryId(lookup.id) == libraryId && (U16)lookup.id == desc) {
			*res = lookup.id;
			return Error_none();
		}
	}

	return Error_notFound(0, 0);
}

Error ETypeId_getTypeFromShortType(U8 libraryId, U8 libraryTypeId, ETypeId *res) {

	if(!res)
		return Error_nullPointer(1);

	for (U64 i = 0; i < ETypeId_lookup.length; ++i) {

		ETypeIdLookup lookup = ((ETypeIdLookup*)ETypeId_lookup.ptr)[i];

		if (ETypeId_getLibraryId(lookup.id) == libraryId && ETypeId_getLibraryTypeId(lookup.id) == libraryTypeId) {
			*res = lookup.id;
			return Error_none();
		}

	}

	return Error_notFound(0, 0);
}

Error ETypeId_getBaseType(ETypeId id, ETypeId *res) {

	if(!res)
		return Error_nullPointer(1);

	//Masking out width or height.

	if(ETypeId_getHeight(id) > 1)
		return ETypeId_getTypeFromDesc(id & (((1 << 5) - 1) | (3 << 7)), ETypeId_getLibraryId(id), res);

	return ETypeId_getTypeFromDesc(id & ((1 << 5) - 1), ETypeId_getLibraryId(id), res);
}

Error ETypeId_getPureType(ETypeId id, ETypeId *res) {

	if(!res)
		return Error_nullPointer(1);

	//Masking out width and height.

	return ETypeId_getTypeFromDesc(id & ((1 << 5) - 1), ETypeId_getLibraryId(id), res);
}

Error ETypeId_stringifySingle(ETypeId typeId, const U8 *data, Allocator alloc, CharString *stringified) {

	Error err = Error_none();

	CharString typeStr = CharString_createNull();
	_gotoIfError(clean, ETypeId_asString(typeId, &typeStr));

	switch (ETypeId_getDataType(typeId)) {

		case EDataType_Float:

			if (ETypeId_getDataTypeBytes(typeId) == 2)
				_gotoIfError(clean, CharString_format(
					alloc, stringified, "0x%04X_%s",
					*(const U16*)data, typeStr.ptr
				))

			else _gotoIfError(clean, CharString_format(
				alloc, stringified, "%.2f_%s",
				ETypeId_getDataTypeBytes(typeId) == 4 ? *(const F32*)data : *(const F64*)data, 
				typeStr.ptr
			));

			break;

		case EDataType_UInt: {

			U8 stride = ETypeId_getDataTypeBytes(typeId);

			switch(stride) {

				case 1: case 2: case 4: case 8:

					_gotoIfError(clean, CharString_format(

						alloc, stringified, "%"PRIu64"_%s", 

						stride == 8 ? *(const U64*)data : (
							stride == 4 ? *(const U32*)data : (
								stride == 2 ? *(const U16*)data : *data
							)
						),

						typeStr.ptr
					));

					break;

				default:
					_gotoIfError(clean, Error_unimplemented(0));
			}

			break;
		}

		case EDataType_Int: {

			U8 stride = ETypeId_getDataTypeBytes(typeId);

			switch(stride) {

				case 1: case 2: case 4: case 8:

					_gotoIfError(clean, CharString_format(

						alloc, stringified, "%"PRIi64"_%s", 

						stride == 8 ? *(const I64*)data : (
							stride == 4 ? *(const I32*)data : (
								stride == 2 ? *(const I16*)data : 
								*(const I8*)data
							)
						),

						typeStr.ptr
					));

					break;

				default:
					_gotoIfError(clean, Error_unimplemented(0));
			}

			break;
		}

		case EDataType_Char:
		
			_gotoIfError(clean, CharString_format(
				alloc, stringified, "%c_%s",
				*(const C8*)data,
				typeStr.ptr
			));

			break;

		case EDataType_Bool:
		
			_gotoIfError(clean, CharString_format(
				alloc, stringified, "%s_%s",
				*data ? "true" : "false",
				typeStr.ptr
			));

			break;

		default:
			_gotoIfError(clean, Error_unimplemented(1));
	}

clean:
	return err;
}

Error ETypeId_stringifyVector(ETypeId typeId, const U8 *data, Allocator alloc, CharString *stringified) {

	Error err = Error_none();
	CharString tmp[4] =  { 0 };

	CharString typeStr = CharString_createNull();
	_gotoIfError(clean, ETypeId_asString(typeId, &typeStr));

	ETypeId baseType = ETypeId_Undefined;
	_gotoIfError(clean, ETypeId_getBaseType(typeId, &baseType));

	U64 stride = ETypeId_getBytes(baseType);

	for(U64 i = 0; i < ETypeId_getWidth(typeId); ++i)
		_gotoIfError(clean, ETypeId_stringifySingle(baseType, data + i * stride, alloc, tmp + i));

	switch (ETypeId_getWidth(typeId)) {

		case 2:

			_gotoIfError(clean, CharString_format(
				alloc, stringified, "%s{ %s, %s }",
				typeStr.ptr, tmp[0].ptr, tmp[1].ptr
			));

			break;

		case 3:

			_gotoIfError(clean, CharString_format(
				alloc, stringified, "%s{ %s, %s, %s }",
				typeStr.ptr, tmp[0].ptr, tmp[1].ptr, tmp[2].ptr
			));

			break;

		case 4:

			_gotoIfError(clean, CharString_format(
				alloc, stringified, "%s{ %s, %s, %s, %s }",
				typeStr.ptr, tmp[0].ptr, tmp[1].ptr, tmp[2].ptr, tmp[3].ptr
			));

			break;

		default:
			_gotoIfError(clean, Error_invalidParameter(0, 0));
	}

clean:

	for(U64 i = 0; i < sizeof(tmp) / sizeof(tmp[0]); ++i)
		CharString_free(tmp + i, alloc);

	return err;
}

Error ETypeId_stringifyMatrix(ETypeId typeId, const U8 *data, Allocator alloc, CharString *stringified) {

	Error err = Error_none();
	CharString tmp[4] =  { 0 };

	CharString typeStr = CharString_createNull();
	_gotoIfError(clean, ETypeId_asString(typeId, &typeStr));

	ETypeId baseType = ETypeId_Undefined;
	_gotoIfError(clean, ETypeId_getBaseType(typeId, &baseType));

	U64 stride = ETypeId_getBytes(baseType);

	for(U64 i = 0; i < ETypeId_getHeight(typeId); ++i)
		_gotoIfError(clean, ETypeId_stringifyVector(baseType, data + i * stride, alloc, tmp + i));

	switch (ETypeId_getHeight(typeId)) {

		case 2:

			_gotoIfError(clean, CharString_format(
				alloc, stringified, "%s{ %s, %s }",
				typeStr.ptr, tmp[0].ptr, tmp[1].ptr
			));

			break;

		case 3:

			_gotoIfError(clean, CharString_format(
				alloc, stringified, "%s{ %s, %s, %s }",
				typeStr.ptr, tmp[0].ptr, tmp[1].ptr, tmp[2].ptr
			));

			break;

		case 4:

			_gotoIfError(clean, CharString_format(
				alloc, stringified, "%s{ %s, %s, %s, %s }",
				typeStr.ptr, tmp[0].ptr, tmp[1].ptr, tmp[2].ptr, tmp[3].ptr
			));

			break;

		default:
			_gotoIfError(clean, Error_invalidParameter(0, 0));
	}

clean:

	for(U64 i = 0; i < sizeof(tmp) / sizeof(tmp[0]); ++i)
		CharString_free(tmp + i, alloc);

	return err;
}

Error ETypeId_stringify(ETypeId typeId, Buffer data, Allocator alloc, CharString *stringified) {

	if(!stringified)
		return Error_nullPointer(3);

	if(stringified->ptr)
		return Error_invalidParameter(3, 0);

	if (typeId == ETypeId_Undefined)
		return Error_invalidParameter(0, 0);

	for (U64 i = 0; i < ETypeId_lookup.length; ++i) {

		ETypeIdLookup lookup = ((ETypeIdLookup*)ETypeId_lookup.ptr)[i];

		if (lookup.id == typeId) {

			U64 len = ETypeId_getBytes(typeId);

			if(Buffer_length(data) != len)
				return Error_invalidParameter(1, 0);

			//Builtin conversions

			if (ETypeId_getLibraryId(typeId) == _LIBRARYID_DEFAULT && ETypeId_getLibraryTypeId(typeId) < ETypeId_Max) {

				if(ETypeId_getWidth(typeId) == 1 && ETypeId_getHeight(typeId) == 1)
					return ETypeId_stringifySingle(typeId, data.ptr, alloc, stringified);

				if (ETypeId_getHeight(typeId) == 1)
					return ETypeId_stringifyVector(typeId, data.ptr, alloc, stringified);

				return ETypeId_stringifyMatrix(typeId, data.ptr, alloc, stringified);
			}

			//Function pointer call; quite slow

			if(!lookup.func)
				return Error_invalidState(0);

			return lookup.func(typeId, data, alloc, stringified);
		}
	}

	return Error_notFound(0, 0);
}

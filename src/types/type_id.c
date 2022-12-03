#include "types/type_id.h"

EDataTypePrimitive EDataType_getPrimitive(EDataType type) { return (EDataTypePrimitive)(type >> 1); }
Bool EDataType_isSigned(EDataType type) { return type & EDataType_IsSigned; }

EDataType TypeId_getDataType(TypeId id) { return (EDataType)(id & 0xF); }
U8 TypeId_getDataTypeBytes(TypeId id) { return ((id >> 4) & 7) + 1; }
U8 TypeId_getElements(TypeId id) { return ((id >> 7) & 63) + 1; }

Bool TypeId_hasValidSize(TypeId id) { return TypeId_getDataTypeBytes(id) != _TYPESIZE_UNDEF; }

U64 TypeId_getBytes(TypeId id) { 
	return TypeId_hasValidSize(id) ? (U64)TypeId_getDataTypeBytes(id) * TypeId_getElements(id) : U64_MAX;
}

U8 TypeId_getLibrarySubId(TypeId id) { return (U8)((id >> 22) & 3); }
U8 TypeId_getLibraryId(TypeId id) { return (U8)((id >> 24) & 0xF); }

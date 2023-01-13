#include "types/type_id.h"

EDataTypePrimitive EDataType_getPrimitive(EDataType type) { return (EDataTypePrimitive)(type >> 1); }
Bool EDataType_isSigned(EDataType type) { return type & EDataType_IsSigned; }

EDataType ETypeId_getDataType(ETypeId id) { return (EDataType)(id & 0xF); }
U8 ETypeId_getDataTypeBytes(ETypeId id) { return ((id >> 4) & 7) + 1; }
U8 ETypeId_getElements(ETypeId id) { return ((id >> 7) & 63) + 1; }

Bool ETypeId_hasValidSize(ETypeId id) { return ETypeId_getDataTypeBytes(id) != _TYPESIZE_UNDEF; }

U64 ETypeId_getBytes(ETypeId id) { 
	return ETypeId_hasValidSize(id) ? (U64)ETypeId_getDataTypeBytes(id) * ETypeId_getElements(id) : U64_MAX;
}

U8 ETypeId_getLibrarySubId(ETypeId id) { return (U8)((id >> 22) & 3); }
U8 ETypeId_getLibraryId(ETypeId id) { return (U8)((id >> 24) & 0xF); }

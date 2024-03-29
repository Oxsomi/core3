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

Bool EDataType_isSigned(EDataType type) { return type & EDataType_IsSigned; }

EDataType ETypeId_getDataType(ETypeId id) { return (EDataType)(id & 7); }
U8 ETypeId_getDataTypeBytes(ETypeId id) { return 1 << ((id >> 3) & 3); }
U8 ETypeId_getHeight(ETypeId id) { return (id >> 5) & 3; }
U8 ETypeId_getWidth(ETypeId id) { return (id >> 7) & 3; }
U8 ETypeId_getElements(ETypeId id) { return ETypeId_getWidth(id) * ETypeId_getHeight(id); }
U64 ETypeId_getBytes(ETypeId id) { return (U64)ETypeId_getDataTypeBytes(id) * ETypeId_getElements(id); }

U8 ETypeId_getLibraryId(ETypeId id) { return (U8)(id >> 24); }
U8 ETypeId_getLibraryTypeId(ETypeId id) { return (U8)(id >> 16); }

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

#include "graphics/generic/pipeline.h"
#include "platforms/ext/listx.h"
#include "types/error.h"

Error PipelineRef_dec(PipelineRef **pipeline) {
	return !RefPtr_dec(pipeline) ? Error_invalidOperation(0) : Error_none();
}

Error PipelineRef_inc(PipelineRef *pipeline) {
	return !RefPtr_inc(pipeline) ? Error_invalidOperation(0) : Error_none();
}

Bool PipelineRef_decAll(List *list) {

	if(!list)
		return true;

	if(list->stride != sizeof(PipelineRef*))
		return false;

	Bool success = true;

	for(U64 i = 0; i < list->length; ++i)
		success &= !PipelineRef_dec(&((PipelineRef**) list->ptr)[i]).genericError;

	List_freex(list);
	return success;
}

PipelineRef *PipelineRef_at(List list, U64 index) {

	Buffer buf = List_at(list, index);

	if(Buffer_length(buf) != sizeof(PipelineRef*))
		return NULL;

	return *(PipelineRef* const*) buf.ptr;
}

/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "audio/openal_soft/openal_soft.h"
#include "types/base/error.h"

Bool alProcessError(ALenum error, Error *e_rr) {

	const C8 *err;

	switch(error) {
		case AL_INVALID_NAME:		err = "OpenAL: Invalid name parameter.";		break;
		case AL_INVALID_ENUM:		err = "OpenAL: Invalid parameter.";				break;
		case AL_INVALID_VALUE:		err = "OpenAL: Invalid enum parameter value.";	break;
		case AL_INVALID_OPERATION:	err = "OpenAL: Illegal call.";					break;
		case AL_OUT_OF_MEMORY:		err = "OpenAL: Unable to allocate memory.";		break;
		default:					err = NULL;								break;
	}

	if(err && e_rr)
		*e_rr = Error_invalidState(0, err);

	return !err;
}

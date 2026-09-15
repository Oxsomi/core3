/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
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

//graphics/generic/sampler.h

#pragma once
#include "types/base/types.h"
#include "types/math/flp.h"
#include "formats/oiPL/pl_file.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct CharString CharString;
typedef struct RefPtr RefPtr;
typedef struct Error Error;

typedef PLSamplerInfo SamplerInfo;

typedef RefPtr GraphicsDeviceRef;
typedef RefPtr SamplerRef;
typedef RefPtr DescriptorTableRef;

typedef struct Sampler {

	GraphicsDeviceRef *device;

	DescriptorTableRef *bindlessDescriptorTable;

	SamplerInfo info;
	U16 padding[7];

	U32 samplerLocation;

} Sampler;

#define Sampler_ext(ptr, T) (!ptr ? NULL : (T##Sampler*)(ptr + 1))        //impl
#define SamplerRef_ptr(ptr) RefPtr_data(ptr, Sampler)

Bool GraphicsDeviceRef_createSampler(
	GraphicsDeviceRef *dev,
	SamplerInfo info,
	Bool disallowBindlessDescriptor,                //Won't try to allocate into bindlessDescriptorTable or device's default
	DescriptorTableRef *bindlessDescriptorTable,
	const CharString *name,
	SamplerRef **sampler,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif

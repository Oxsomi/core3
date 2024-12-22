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

#pragma once
#include "types/base/types.h"
#include "types/math/flp.h"
#include "types/container/ref_ptr.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum ESamplerFilterMode {

	ESamplerFilterMode_Nearest,
	ESamplerFilterMode_LinearMagNearestMinMip,
	ESamplerFilterMode_LinearMinNearestMagMip,
	ESamplerFilterMode_LinearMagMinNearestMip,
	ESamplerFilterMode_LinearMipNearestMagMin,
	ESamplerFilterMode_LinearMagMipNearestMin,
	ESamplerFilterMode_LinearMinMipNearestMag,
	ESamplerFilterMode_Linear,

	ESamplerFilterMode_None				= 0,

	ESamplerFilterMode_LinearMag		= 1 << 0,
	ESamplerFilterMode_LinearMin		= 1 << 1,
	ESamplerFilterMode_LinearMip		= 1 << 2,

	ESamplerFilterMode_PropertyCount	= 3,
	ESamplerFilterMode_All				= (1 << ESamplerFilterMode_PropertyCount) - 1

} ESamplerFilterMode;

typedef enum ESamplerAddressMode {
	ESamplerAddressMode_Repeat,
	ESamplerAddressMode_MirrorRepeat,
	ESamplerAddressMode_ClampToEdge,
	ESamplerAddressMode_ClampToBorder,
	ESamplerAddressMode_Count
} ESamplerAddressMode;

typedef enum ESamplerBorderColor {
	ESamplerBorderColor_TransparentBlack,		//0.xxxx
	ESamplerBorderColor_OpaqueBlackFloat,		//0.xxx, 1.f
	ESamplerBorderColor_OpaqueBlackInt,			//0.xxx, 1
	ESamplerBorderColor_OpaqueWhiteFloat,		//1.f.xxxx
	ESamplerBorderColor_OpaqueWhiteInt,			//1.xxxx
	ESamplerBorderColor_Count
} ESamplerBorderColor;

typedef struct SamplerInfo {

	U8 filter;							//ESamplerFilterMode
	U8 addressU, addressV, addressW;	//ESamplerAddressMode[3] (3 bits each)

	U8 aniso;							//0-16
	U8 borderColor;						//ESamplerBorderColor
	U8 comparisonFunction;				//ECompareOp
	Bool enableComparison;

	F16 mipBias, minLod, maxLod;

} SamplerInfo;

typedef RefPtr GraphicsDeviceRef;
typedef RefPtr SamplerRef;

typedef struct Sampler {

	GraphicsDeviceRef *device;

	U32 samplerLocation;
	U32 pad0;

	SamplerInfo info;
	U16 pad1;

} Sampler;

#define Sampler_ext(ptr, T) (!ptr ? NULL : (T##Sampler*)(ptr + 1))		//impl
#define SamplerRef_ptr(ptr) RefPtr_data(ptr, Sampler)

Error SamplerRef_dec(SamplerRef **buffer);
Error SamplerRef_inc(SamplerRef *buffer);

Error GraphicsDeviceRef_createSampler(GraphicsDeviceRef *dev, SamplerInfo info, CharString name, SamplerRef **sampler);

#ifdef __cplusplus
	}
#endif

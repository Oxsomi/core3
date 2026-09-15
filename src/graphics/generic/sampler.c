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

//graphics/generic/sampler.c

#include "graphics/generic/interface.h"
#include "graphics/generic/sampler.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/device.h"
#include "graphics/generic/pipeline_structs.h"
#include "graphics/generic/descriptor_table.h"
#include "graphics/generic/bindless_descriptor.h"
#include "formats/oiSH/sh_registers.h"
#include "types/container/ref_ptr.h"

void Sampler_free(void *samplerGeneric, const Allocator *alloc) {

	Sampler *sampler = (Sampler*) samplerGeneric;

	(void)alloc;

	if(sampler->bindlessDescriptorTable) {

		GraphicsDeviceRef_freeDescriptorBindless(
			sampler->device, sampler->bindlessDescriptorTable, sampler->samplerLocation, NULL
		);

		RefPtr_dec(&sampler->bindlessDescriptorTable);
	}

	Sampler_freeExt(sampler);
	RefPtr_dec(&sampler->device);
}

Bool GraphicsDeviceRef_createSampler(
	GraphicsDeviceRef *dev,
	SamplerInfo info,
	Bool disallowBindlessDescriptor,
	DescriptorTableRef *bindlessDescriptorTable,
	const CharString *name,
	SamplerRef **sampler,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!dev || dev->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_createSampler()::dev is required"));

	if(bindlessDescriptorTable && disallowBindlessDescriptor)
		retError(clean, Error_invalidState(
			0,
			"GraphicsDeviceRef_createSampler() bindlessDescriptorTable is set, but disallowed"
		));

	if(bindlessDescriptorTable && bindlessDescriptorTable->refPtrType->typeId != (TypeId) EGraphicsTypeId_DescriptorTable)
		retError(clean, Error_nullPointer(
			0,
			"GraphicsDeviceRef_createSampler()::bindlessDescriptorTable should be valid if non NULL"
		));

	//A device whose layout declares no sampler array has nowhere to put a bindless sampler, which is the
	// default now that EGraphicsDeviceFlags_EnableDynamicSamplers is opt in.
	//Taking the table anyway would fail every plain createSampler on such a device, for a slot the caller
	// never asked for; the sampler still works bound, or better, as an immutable one.
	//Same shape as a device without bindless at all, where defaultDescriptorTable is simply absent.

	if (!disallowBindlessDescriptor && !bindlessDescriptorTable) {

		const GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);

		const DescriptorLayout *defaultLayout =
			device->defaultDescLayout ? DescriptorLayoutRef_ptr(device->defaultDescLayout) : NULL;

		if(defaultLayout && defaultLayout->anySampler)
			bindlessDescriptorTable = device->defaultDescriptorTable;
	}

	if(info.filter &~ ESamplerFilterMode_All)
		retError(clean, Error_invalidParameter(
			1, 0, "GraphicsDeviceRef_createSampler()::info.filter contains invalid bits"
		));

	if(
		info.addressU >= ESamplerAddressMode_Count ||
		info.addressV >= ESamplerAddressMode_Count ||
		info.addressW >= ESamplerAddressMode_Count
	)
		retError(clean, Error_invalidParameter(
			1, 1, "GraphicsDeviceRef_createSampler()::info.addressU, addressV or addressW is invalid"
		));

	if(info.aniso > 16)
		retError(clean, Error_invalidParameter(
			1, 4, "GraphicsDeviceRef_createSampler()::info.aniso needs to be <=16 if anisotropy is used"
		));

	if(info.borderColor >= ESamplerBorderColor_Count)
		retError(clean, Error_invalidParameter(
			1, 5, "GraphicsDeviceRef_createSampler()::info.borderColor is out of bounds"
		));

	if(info.comparisonFunction >= ECompareOp_Count)
		retError(clean, Error_invalidParameter(
			1, 6, "GraphicsDeviceRef_createSampler()::info.comparisonFunction is out of bounds"
		));

	if(
		!EFloatType_isFinite(EFloatType_F16, info.mipBias) ||
		!EFloatType_isFinite(EFloatType_F16, info.minLod) ||
		!EFloatType_isFinite(EFloatType_F16, info.maxLod)
	)
		retError(clean, Error_invalidParameter(
			1, 8, "GraphicsDeviceRef_createSampler()::info.mipBias, minLod or maxLod is invalid"
		));

	if(!info.maxLod)
		info.maxLod = F32_castF16(65504.f);        //Set to F16 max

	gotoIfError3(clean, RefPtr_create(&GraphicsDeviceRef_getTypes(dev)->sampler, sampler, e_rr));

	gotoIfError3(clean, RefPtr_inc(dev));

	Sampler *samp = SamplerRef_ptr(*sampler);

	*samp = (Sampler) { .device = dev, .info = info };

	if(bindlessDescriptorTable) {
		gotoIfError3(clean, RefPtr_inc(bindlessDescriptorTable));
		samp->bindlessDescriptorTable = bindlessDescriptorTable;
	}

	gotoIfError3(clean, GraphicsDeviceRef_createSamplerExt(dev, samp, name, e_rr));

	Descriptor sampDesc = Descriptor_sampler(*sampler);

	if(bindlessDescriptorTable && !GraphicsDeviceRef_allocateDescriptorBindless(
		dev,
		bindlessDescriptorTable,
		info.enableComparison ? EGfxRegisterType_SamplerComparisonState : EGfxRegisterType_Sampler,
		0,
		false,
		&sampDesc,
		&samp->samplerLocation,
		e_rr
	)) {
		s_uccess = false;
		goto clean;
	}

clean:

	if(!s_uccess)
		RefPtr_dec(sampler);

	return s_uccess;
}

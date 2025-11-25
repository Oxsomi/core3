/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "platforms/ext/listx_impl.h"
#include "graphics/generic/interface.h"
#include "graphics/generic/sampler.h"
#include "graphics/generic/device.h"
#include "graphics/generic/pipeline_structs.h"
#include "graphics/generic/descriptor_table.h"
#include "graphics/generic/bindless_descriptor.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/ref_ptrx.h"
#include "platforms/log.h"
#include "types/container/string.h"
#include "formats/oiSH/registers.h"

void SamplerRef_dec(SamplerRef **sampler) { RefPtr_dec(sampler); }

Error SamplerRef_inc(SamplerRef *sampler) {
	return !RefPtr_inc(sampler) ?
		Error_invalidOperation(0, "SamplerRef_inc()::sampler is required") : Error_none();
}

void Sampler_free(Sampler *sampler, Allocator allocator) {

	(void)allocator;

	//Log_debugLnx("Destroy: %p", sampler);

	if(sampler->bindlessDescriptorTable) {

		GraphicsDeviceRef_freeDescriptorBindless(
			sampler->device, sampler->bindlessDescriptorTable, sampler->samplerLocation, NULL
		);

		DescriptorTableRef_dec(&sampler->bindlessDescriptorTable);
	}

	Sampler_freeExt(sampler);
	GraphicsDeviceRef_dec(&sampler->device);
}

Error GraphicsDeviceRef_createSampler(
	GraphicsDeviceRef *dev,
	SamplerInfo info,
	Bool disallowBindlessDescriptor,
	DescriptorTableRef *bindlessDescriptorTable,
	CharString name,
	SamplerRef **sampler
) {

	if(!dev || dev->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_createSampler()::dev is required");

	if(bindlessDescriptorTable && disallowBindlessDescriptor)
		return Error_invalidState(0, "GraphicsDeviceRef_createSampler() bindlessDescriptorTable is set, but disallowed");

	if(bindlessDescriptorTable && bindlessDescriptorTable->typeId != (ETypeId) EGraphicsTypeId_DescriptorTable)
		return Error_nullPointer(0, "GraphicsDeviceRef_createSampler()::bindlessDescriptorTable should be valid if non NULL");

	if (!disallowBindlessDescriptor && !bindlessDescriptorTable)
		bindlessDescriptorTable = GraphicsDeviceRef_ptr(dev)->defaultDescriptorTable;

	if(info.filter &~ ESamplerFilterMode_All)
		return Error_invalidParameter(
			1, 0, "GraphicsDeviceRef_createSampler()::info.filter contains invalid bits"
		);

	if(
		info.addressU >= ESamplerAddressMode_Count ||
		info.addressV >= ESamplerAddressMode_Count ||
		info.addressW >= ESamplerAddressMode_Count
	)
		return Error_invalidParameter(
			1, 1, "GraphicsDeviceRef_createSampler()::info.addressU, addressV or addressW is invalid"
		);

	if(info.aniso > 16)
		return Error_invalidParameter(
			1, 4, "GraphicsDeviceRef_createSampler()::info.aniso needs to be <=16 if anisotropy is used"
		);

	if(info.borderColor >= ESamplerBorderColor_Count)
		return Error_invalidParameter(
			1, 5, "GraphicsDeviceRef_createSampler()::info.borderColor is out of bounds"
		);

	if(info.comparisonFunction >= ECompareOp_Count)
		return Error_invalidParameter(
			1, 6, "GraphicsDeviceRef_createSampler()::info.comparisonFunction is out of bounds"
		);

	if(
		!EFloatType_isFinite(EFloatType_F16, info.mipBias) ||
		!EFloatType_isFinite(EFloatType_F16, info.minLod) ||
		!EFloatType_isFinite(EFloatType_F16, info.maxLod)
	)
		return Error_invalidParameter(
			1, 8, "GraphicsDeviceRef_createSampler()::info.mipBias, minLod or maxLod is invalid"
		);

	if(!info.maxLod)
		info.maxLod = F32_castF16(65504.f);		//Set to F16 max

	Error err = RefPtr_createx(
		(U32)(sizeof(Sampler) + GraphicsDeviceRef_getObjectSizes(dev)->sampler),
		(ObjectFreeFunc) Sampler_free,
		(ETypeId) EGraphicsTypeId_Sampler,
		sampler
	);

	if(err.genericError)
		return err;

	gotoIfError(clean, GraphicsDeviceRef_inc(dev))

	Sampler *samp = SamplerRef_ptr(*sampler);

	//Log_debugLnx("Create: Sampler %.*s (%p)", (int) CharString_length(name), name.ptr, samp);

	*samp = (Sampler) { .device = dev, .info = info };

	if(bindlessDescriptorTable) {
		gotoIfError(clean, DescriptorTableRef_inc(bindlessDescriptorTable))
		samp->bindlessDescriptorTable = bindlessDescriptorTable;
	}

	gotoIfError(clean, GraphicsDeviceRef_createSamplerExt(dev, samp, name))

	if(bindlessDescriptorTable && !GraphicsDeviceRef_allocateDescriptorBindless(
		dev,
		bindlessDescriptorTable,
		info.enableComparison ? ESHRegisterType_SamplerComparisonState : ESHRegisterType_Sampler,
		0,
		false,
		Descriptor_sampler(*sampler),
		&samp->samplerLocation,
		&err
	))
		goto clean;

clean:

	if(err.genericError)
		SamplerRef_dec(sampler);

	return err;
}

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

#include "platforms/ext/listx_impl.h"
#include "graphics/generic/sampler.h"
#include "graphics/generic/device.h"
#include "graphics/generic/pipeline_structs.h"
#include "platforms/ext/bufferx.h"
#include "types/string.h"

Error SamplerRef_dec(SamplerRef **sampler) {
	return !RefPtr_dec(sampler) ? Error_invalidOperation(0, "SamplerRef_dec()::sampler is required") : Error_none();
}

Error SamplerRef_inc(SamplerRef *sampler) {
	return !RefPtr_inc(sampler) ? Error_invalidOperation(0, "SamplerRef_inc()::sampler is required") : Error_none();
}

impl extern const U64 SamplerExt_size;
impl Bool Sampler_freeExt(Sampler *sampler);
impl Error GraphicsDeviceRef_createSamplerExt(GraphicsDeviceRef *dev, Sampler *sampler, CharString name);

Bool Sampler_free(Sampler *sampler, Allocator allocator) {

	(void)allocator;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(sampler->device);

	if(sampler->samplerLocation) {

		ELockAcquire acq = Lock_lock(&device->descriptorLock, U64_MAX);

		if(acq >= ELockAcquire_Success) {
			ListU32 allocationList = (ListU32) { 0 };
			ListU32_createRefConst(&sampler->samplerLocation, 1, &allocationList);
			GraphicsDeviceRef_freeDescriptors(sampler->device, &allocationList);
			sampler->samplerLocation = 0;
		}

		if(acq == ELockAcquire_Acquired)
			Lock_unlock(&device->descriptorLock);
	}

	Bool success = Sampler_freeExt(sampler);
	success &= !GraphicsDeviceRef_dec(&sampler->device).genericError;
	return success;
}
Error GraphicsDeviceRef_createSampler(GraphicsDeviceRef *dev, SamplerInfo info, CharString name, SamplerRef **sampler) {

	if(!dev || dev->typeId != EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_createSampler()::dev is required");

	if(info.filter &~ ESamplerFilterMode_All)
		return Error_invalidParameter(1, 0, "GraphicsDeviceRef_createSampler()::info.filter contains invalid bits");

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
		return Error_invalidParameter(1, 5, "GraphicsDeviceRef_createSampler()::info.borderColor is out of bounds");

	if(info.comparisonFunction >= ECompareOp_Count)
		return Error_invalidParameter(1, 6, "GraphicsDeviceRef_createSampler()::info.comparisonFunction is out of bounds");

	if(
		!EFloatType_isFinite(EFloatType_F16, info.mipBias) ||
		!EFloatType_isFinite(EFloatType_F16, info.minLod) ||
		!EFloatType_isFinite(EFloatType_F16, info.maxLod)
	)
		return Error_invalidParameter(1, 8, "GraphicsDeviceRef_createSampler()::info.mipBias, minLod or maxLod is invalid");

	if(!info.maxLod)
		info.maxLod = F32_castF16(65504.f);		//Set to F16 max

	Error err = RefPtr_createx(
		(U32)(sizeof(Sampler) + SamplerExt_size),
		(ObjectFreeFunc) Sampler_free,
		(ETypeId) EGraphicsTypeId_Sampler,
		sampler
	);

	if(err.genericError)
		return err;

	ELockAcquire acq = ELockAcquire_Invalid;
	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);
	_gotoIfError(clean, GraphicsDeviceRef_inc(dev));

	Sampler *samp = SamplerRef_ptr(*sampler);

	*samp = (Sampler) { .device = dev, .info = info };

	acq = Lock_lock(&device->descriptorLock, U64_MAX);

	if(acq < ELockAcquire_Success)
		_gotoIfError(clean, Error_invalidState(
			0, "GraphicsDeviceRef_createSampler() couldn't acquire descriptor lock"
		));

	samp->samplerLocation = GraphicsDeviceRef_allocateDescriptor(dev, EDescriptorType_Sampler);

	if(samp->samplerLocation == U32_MAX)
		_gotoIfError(clean, Error_outOfMemory(0, "GraphicsDeviceRef_createSampler() couldn't allocate Sampler descriptor"));

	_gotoIfError(clean, GraphicsDeviceRef_createSamplerExt(dev, samp, name));

clean:

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&device->descriptorLock);

	if(err.genericError)
		SamplerRef_dec(sampler);

	return err;
}

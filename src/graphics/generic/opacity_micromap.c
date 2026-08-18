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

//graphics/generic/opacity_micromap.c

#include "graphics/generic/opacity_micromap.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device.h"
#include "graphics/generic/interface.h"
#include "types/container/string.h"
#include "types/container/ref_ptr.h"
#include "types/container/list_impl.h"
#include "types/base/constants.h"

TListImpl(OpacityMicromapUsage);

void OpacityMicromap_free(void *micromapGeneric, const Allocator *alloc) {

	OpacityMicromap *micromap = (OpacityMicromap*) micromapGeneric;

	SpinLock_lock(&micromap->base.lock, U64_MAX);

	OpacityMicromap_freeExt(micromap);
	CharString_free(&micromap->base.name, alloc);

	ListOpacityMicromapUsage_free(&micromap->usages, alloc);

	RefPtr_dec(&micromap->base.asBuffer);
	RefPtr_dec(&micromap->base.tempScratchBuffer);

	RefPtr_dec(&micromap->inputBuffer.buffer);
	RefPtr_dec(&micromap->entryBuffer.buffer);

	RefPtr_dec(&micromap->base.device);
}

OpacityMicromapCreateInfo OpacityMicromapCreateInfo_create(
	ERTASBuildFlags buildFlags,
	const DeviceData *inputBuffer,
	const DeviceData *entryBuffer,
	U32 entryStride,
	const ListOpacityMicromapUsage *usages
) {

	//Each field is only filled in when its input is both present and usable, so a bad argument produces an
	// info the create call rejects rather than one that looks valid.
	//A NULL buffer and a buffer with no DeviceBufferRef are the same mistake, and both leave the field zeroed.

	OpacityMicromapCreateInfo info = (OpacityMicromapCreateInfo) { .buildFlags = buildFlags };

	if(inputBuffer && inputBuffer->buffer)
		info.inputBuffer = *inputBuffer;

	if(entryBuffer && entryBuffer->buffer)
		info.entryBuffer = *entryBuffer;

	//An entry stride that can't hold one record is never salvageable, and letting it through would make the
	// capacity check downstream compute a nonsense entry count.

	if(entryStride >= sizeof(OpacityMicromapEntry) && !(entryStride & 3))
		info.entryStride = entryStride;

	//An empty list is a caller error rather than a default, so it stays empty and the create call names it.

	if(usages && usages->length)
		info.usages = *usages;

	return info;
}

OpacityMicromapCreateInfo OpacityMicromapCreateInfo_uniform(
	ERTASBuildFlags buildFlags,
	const DeviceData *inputBuffer,
	const DeviceData *entryBuffer,
	U32 entryStride,
	const OpacityMicromapUsage *usage
) {

	ListOpacityMicromapUsage usages = (ListOpacityMicromapUsage) { 0 };

	//The error is deliberately dropped: a non NULL single element ref cannot fail, and the create call
	// rejects an empty usage list anyway, so there is nothing a caller could do with it here.

	if(usage)
		ListOpacityMicromapUsage_createRefConst(usage, 1, &usages, NULL);

	return OpacityMicromapCreateInfo_create(buildFlags, inputBuffer, entryBuffer, entryStride, &usages);
}

Bool GraphicsDeviceRef_createOpacityMicromapExt(
	GraphicsDeviceRef *dev,
	const OpacityMicromapCreateInfo *info,
	const CharString *name,
	OpacityMicromapRef **micromapRef,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(dev);

	DeviceData inputBuffer = (DeviceData) { 0 };
	DeviceData entryBuffer = (DeviceData) { 0 };

	if(!dev || dev->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(
			0, "GraphicsDeviceRef_createOpacityMicromapExt()::dev is required"
		));

	if(!info)
		retError(clean, Error_nullPointer(
			1, "GraphicsDeviceRef_createOpacityMicromapExt()::info is required"
		));

	if(!micromapRef || *micromapRef)
		retError(clean, Error_invalidParameter(
			3, 0, "GraphicsDeviceRef_createOpacityMicromapExt()::micromapRef must be an empty pointer"
		));

	const EGraphicsFeatures feat = GraphicsDeviceRef_ptr(dev)->info.capabilities.features;

	if(!(feat & EGraphicsFeatures_RayMicromapOpacity))
		retError(clean, Error_unsupportedOperation(
			0, "GraphicsDeviceRef_createOpacityMicromapExt() is unsupported without opacity micromap support"
		));

	//Rejected rather than ignored, so a caller can't believe an update happened.
	//Micromaps have no update mode at all: VkBuildMicromapModeEXT only has BUILD.

	if(info->buildFlags & ~(ERTASBuildFlags) ERTASBuildFlags_SupportedOpacityMicromapExt)
		retError(clean, Error_unsupportedOperation(
			1,
			"GraphicsDeviceRef_createOpacityMicromapExt()::buildFlags only allows FastTrace, FastBuild and "
			"AllowCompaction"
		));

	if(info->entryStride < sizeof(OpacityMicromapEntry) || (info->entryStride & 3))
		retError(clean, Error_unsupportedOperation(
			1,
			"GraphicsDeviceRef_createOpacityMicromapExt()::entryStride must be >= sizeof(OpacityMicromapEntry) "
			"and 4 byte aligned"
		));

	if(!info->usages.length)
		retError(clean, Error_unsupportedOperation(
			1, "GraphicsDeviceRef_createOpacityMicromapExt()::usages needs at least one entry"
		));

	//The entry count is the sum of every usage, and it has to match what the entry buffer can hold.
	//Summed as a U64 so an overflowing set of usages is caught rather than wrapping into a plausible count.

	U64 entryCount = 0;

	for (U64 i = 0; i < info->usages.length; ++i) {

		const OpacityMicromapUsage usage = info->usages.ptr[i];

		if(usage.format != EOpacityMicromapFormat_Opacity2State && usage.format != EOpacityMicromapFormat_Opacity4State)
			retError(clean, Error_unsupportedOperation(
				1, "GraphicsDeviceRef_createOpacityMicromapExt()::usages[i].format must be Opacity2State or Opacity4State"
			));

		if(!usage.count)
			retError(clean, Error_unsupportedOperation(
				1, "GraphicsDeviceRef_createOpacityMicromapExt()::usages[i].count can't be 0"
			));

		entryCount += usage.count;
	}

	if(entryCount >> 32)
		retError(clean, Error_outOfBounds(
			1, entryCount, U32_MAX, "GraphicsDeviceRef_createOpacityMicromapExt() entry count out of bounds"
		));

	//Validated before the size check, since a len of 0 is normalized to "rest of the buffer" in place.

	inputBuffer = info->inputBuffer;
	entryBuffer = info->entryBuffer;

	gotoIfError3(clean, RTAS_validateDeviceBuffer(&inputBuffer, e_rr));
	gotoIfError3(clean, RTAS_validateDeviceBuffer(&entryBuffer, e_rr));

	if(entryBuffer.len / info->entryStride < entryCount)
		retError(clean, Error_unsupportedOperation(
			1, "GraphicsDeviceRef_createOpacityMicromapExt()::entryBuffer can't hold every entry the usages describe"
		));

	//Allocate refPtr

	gotoIfError3(clean, RefPtr_create(&GraphicsDeviceRef_getTypes(dev)->opacityMicromap, micromapRef, e_rr));

	OpacityMicromap *micromap = OpacityMicromapRef_ptr(*micromapRef);

	*micromap = (OpacityMicromap) {
		.base = (RTAS) { .flags = (U8) info->buildFlags },
		.entryStride = info->entryStride,
		.entryCount = (U32) entryCount
	};

	//Set as soon as the object exists rather than once it's fully built, because a failure below frees the half
	// built micromap and OpacityMicromap_freeExt reaches the backend through base.device.

	gotoIfError3(clean, RefPtr_inc(dev));
	micromap->base.device = dev;

	gotoIfError3(clean, RefPtr_inc(inputBuffer.buffer));
	micromap->inputBuffer = inputBuffer;

	gotoIfError3(clean, RefPtr_inc(entryBuffer.buffer));
	micromap->entryBuffer = entryBuffer;

	//Copied rather than referenced: Vulkan needs the usages again in every BLAS that links this micromap, so
	// they have to survive as long as the object does and not just as long as the create call.

	gotoIfError3(clean, ListOpacityMicromapUsage_createCopy(info->usages, alloc, &micromap->usages, e_rr));

	if(name)
		gotoIfError3(clean, CharString_createCopy(*name, alloc, &micromap->base.name, e_rr));

	gotoIfError3(clean, OpacityMicromap_initExt(micromap, e_rr));

clean:

	if(!s_uccess)
		RefPtr_dec(micromapRef);

	return s_uccess;
}

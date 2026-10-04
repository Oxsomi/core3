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

//graphics/generic/device_loss.c

#include "types/container/list_impl.h"
#include "graphics/generic/device.h"
#include "graphics/generic/device_internal.h"
#include "graphics/generic/interface.h"
#include "graphics/generic/resource.h"
#include "graphics/generic/texture.h"
#include "graphics/generic/device_buffer.h"
#include "types/base/time.h"
#include "platforms/logx.h"
#include "types/base/constants.h"

TListImpl(LiveResource);

static void GraphicsDevice_logBreadcrumbs(GraphicsDevice *device);

//TODO: a linear scan under the device lock on every free makes N frees O(N^2), and the registry is always on.
//Should become a hash map keyed by the resource once core3 has one (GitHub #40).

const GraphicsResource *LiveResource_res(const LiveResource *live) {
	const UnifiedTexture *tex = TextureRef_getUnifiedTextureFast(live->resource);
	return tex ? &tex->resource : &DeviceBufferRef_ptr(live->resource)->resource;
}

static LiveResource *GraphicsDevice_findLive(GraphicsDevice *device, const RefPtr *resource, U64 *index) {

	for(U64 i = 0; i < device->liveResources.length; ++i)
		if(device->liveResources.ptr[i].resource == resource) {

			if(index)
				*index = i;

			return device->liveResources.ptrNonConst + i;
		}

	return NULL;
}

Bool GraphicsDevice_registerResource(
	GraphicsDevice *device,
	RefPtr *resource,
	U64 apiObject,
	const CharString *name,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDevice_getAlloc(device);
	ELockAcquire acq = ELockAcquire_Invalid;

	LiveResource live = (LiveResource) { .resource = resource, .apiObject = apiObject };

	if(name && CharString_length(*name))
		gotoIfError3(clean, CharString_createCopy(*name, alloc, &live.name, e_rr));

	acq = SpinLock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "GraphicsDevice_registerResource() couldn't acquire the device lock"));

	gotoIfError3(clean, ListLiveResource_pushBack(&device->liveResources, live, alloc, e_rr));
	live.name = CharString_createNull();

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	CharString_free(&live.name, alloc);
	return s_uccess;
}

void GraphicsDevice_unregisterResource(GraphicsDevice *device, const RefPtr *resource) {

	const ELockAcquire acq = SpinLock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		return;

	//Every entry of the resource (a texture has one per image) moves to the freed ring, where the oldest makes room;
	// an entry keeps its name there for a fault in its old memory to name.

	//TODO: popLocation shifts the rest of the list, another O(N) per free on top of the scan.
	//Goes away with the hash map above (GitHub #40).

	U64 i = 0;
	LiveResource *live = NULL;

	while((live = GraphicsDevice_findLive(device, resource, &i)) != NULL) {

		FreedResource *freed = &device->freedResources[device->freedResourceNext];
		device->freedResourceNext = (device->freedResourceNext + 1) % GRAPHICS_FREED_RESOURCES;

		CharString_free(&freed->name, GraphicsDevice_getAlloc(device));

		const GraphicsResource *res = LiveResource_res(live);

		*freed = (FreedResource) {
			.apiObject = live->apiObject,
			.address = res->deviceAddress,
			.size = res->deviceAddress ? res->size : 0,
			.blockOffset = res->blockOffset,
			.freedAt = Time_now(),
			.name = live->name,
			.blockId = res->allocated ? res->blockId : U32_MAX,
			.type = res->type
		};

		ListLiveResource_popLocation(&device->liveResources, i, NULL, NULL);
	}

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);
}

//What a live entry reads from its resource and a freed one copied, so both describe the same way.

static FreedResource LiveResource_info(const LiveResource *live) {
	const GraphicsResource *res = LiveResource_res(live);
	return (FreedResource) {
		.apiObject = live->apiObject,
		.address = res->deviceAddress,
		.size = res->deviceAddress ? res->size : 0,
		.blockOffset = res->blockOffset,
		.name = live->name,
		.blockId = res->allocated ? res->blockId : U32_MAX,
		.type = res->type
	};
}

static Bool LiveResource_matches(const FreedResource *live, U64 address, U64 apiObject) {
	return
		(apiObject && live->apiObject == apiObject) ||
		(address && live->size && address >= live->address && address - live->address < live->size);
}

static Bool LiveResource_describe(
	const FreedResource *live, U64 address, CharString *str, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;
	CharString tmp = CharString_createNull();

	const C8 *kind = live->type < EResourceType_Count ? EResourceType_names[live->type] : "resource";
	const C8 *name = CharString_length(live->name) ? live->name.ptr : "(unnamed)";

	if(live->freedAt) {
		gotoIfError3(clean, CharString_format(
			alloc, &tmp, e_rr, "freed %s '%s', %.1f s ago", kind, name,
			(F64) (Time_now() - live->freedAt) / SECOND
		));
	}

	else gotoIfError3(clean, CharString_format(alloc, &tmp, e_rr, "%s '%s'", kind, name));

	gotoIfError3(clean, CharString_appendString(str, &tmp, alloc, e_rr));
	CharString_free(&tmp, alloc);

	if(address && live->size && address >= live->address) {
		gotoIfError3(clean, CharString_format(
			alloc, &tmp, e_rr, " at +0x%" PRIx64 " of 0x%" PRIx64, address - live->address, live->size
		));
	}

	else if(live->blockId != U32_MAX)
		gotoIfError3(clean, CharString_format(
			alloc, &tmp, e_rr, " in memory block %" PRIu32 " at 0x%" PRIx64, live->blockId, live->blockOffset
		));

	gotoIfError3(clean, CharString_appendString(str, &tmp, alloc, e_rr));

clean:
	CharString_free(&tmp, alloc);
	return s_uccess;
}

Bool GraphicsDevice_describeResource(
	GraphicsDevice *device, U64 address, U64 apiObject, CharString *str, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;
	Bool found = false;

	const ELockAcquire acq = SpinLock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		return false;

	for(U64 i = 0; i < device->liveResources.length; ++i) {

		const FreedResource live = LiveResource_info(&device->liveResources.ptr[i]);

		if(LiveResource_matches(&live, address, apiObject)) {
			gotoIfError3(clean, LiveResource_describe(&live, address, str, alloc, e_rr));
			found = true;
			goto clean;
		}
	}

	//Newest first, since a recent free is the likelier culprit when an address was reused.

	for(U32 j = 0; j < GRAPHICS_FREED_RESOURCES; ++j) {

		const U32 k = (device->freedResourceNext + GRAPHICS_FREED_RESOURCES - 1 - j) % GRAPHICS_FREED_RESOURCES;
		const FreedResource *freed = &device->freedResources[k];

		if(freed->freedAt && LiveResource_matches(freed, address, apiObject)) {
			gotoIfError3(clean, LiveResource_describe(freed, address, str, alloc, e_rr));
			found = true;
			goto clean;
		}
	}

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	return s_uccess && found;
}

Bool GraphicsDevice_describeNearest(
	GraphicsDevice *device, U64 address, CharString *str, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;
	CharString tmp = CharString_createNull();

	const ELockAcquire acq = SpinLock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		return false;

	FreedResource below = { 0 }, above = { 0 };
	Bool hasBelow = false, hasAbove = false;

	for(U64 i = 0; i < device->liveResources.length + GRAPHICS_FREED_RESOURCES; ++i) {

		const Bool isLive = i < device->liveResources.length;

		const FreedResource r =
			isLive ? LiveResource_info(&device->liveResources.ptr[i]) :
			device->freedResources[i - device->liveResources.length];

		if(!r.size || (!isLive && !r.freedAt))
			continue;

		if(r.address + r.size <= address && (!hasBelow || r.address + r.size > below.address + below.size)) {
			below = r;
			hasBelow = true;
		}

		else if(r.address > address && (!hasAbove || r.address < above.address)) {
			above = r;
			hasAbove = true;
		}
	}

	if(hasBelow) {
		gotoIfError3(clean, CharString_format(
			alloc, &tmp, e_rr, "0x%" PRIx64 " past the end of ", address - (below.address + below.size)
		));

		gotoIfError3(clean, CharString_appendString(str, &tmp, alloc, e_rr));
		gotoIfError3(clean, LiveResource_describe(&below, 0, str, alloc, e_rr));
		CharString_free(&tmp, alloc);
	}

	if(hasAbove) {
		gotoIfError3(clean, CharString_format(
			alloc, &tmp, e_rr, "%s0x%" PRIx64 " before ", hasBelow ? "; " : "", above.address - address
		));

		gotoIfError3(clean, CharString_appendString(str, &tmp, alloc, e_rr));
		gotoIfError3(clean, LiveResource_describe(&above, 0, str, alloc, e_rr));
	}

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	CharString_free(&tmp, alloc);
	return s_uccess && (hasBelow || hasAbove);
}

Bool GraphicsDeviceRef_findResourceByAddress(GraphicsDeviceRef *deviceRef, U64 address, WeakRefPtr **resource, U64 *offset) {

	if(!deviceRef || !address || !resource)
		return false;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	const ELockAcquire acq = SpinLock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		return false;

	Bool found = false;

	for(U64 i = 0; i < device->liveResources.length; ++i) {

		const FreedResource live = LiveResource_info(&device->liveResources.ptr[i]);

		if(LiveResource_matches(&live, address, 0)) {

			*resource = device->liveResources.ptr[i].resource;

			if(offset)
				*offset = address - live.address;

			found = true;
			break;
		}
	}

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	return found;
}

void GraphicsDeviceRef_reportLoss(GraphicsDeviceRef *deviceRef) {

	if(!deviceRef)
		return;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	const ELockAcquire acq = SpinLock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		return;

	//The backend says whether the device is lost at all; a submit or wait fails for other reasons too. Checked and
	// set under the lock, so two threads seeing the failure report it once.

	if(!AtomicI64_load(&device->lossReported) && GraphicsDeviceRef_reportLossExt(deviceRef)) {
		AtomicI64_store(&device->lossReported, 1);
		GraphicsDevice_logBreadcrumbs(device);
	}

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);
}

U32 GraphicsDevice_claimBreadcrumb(GraphicsDevice *device, U32 scopeId) {

	ListU32 *scopes = &device->breadcrumbScopes[device->fifId];

	if(!device->breadcrumbs || scopes->length >= GRAPHICS_BREADCRUMBS)
		return U32_MAX;

	if(!ListU32_pushBack(scopes, scopeId, GraphicsDevice_getAlloc(device), NULL))
		return U32_MAX;

	return (U32) scopes->length - 1;
}

//Newest frame first: the scopes the GPU began and never ended are where it was when it was lost.

void GraphicsDevice_startBreadcrumbs(GraphicsDevice *device) {

	if(!device->breadcrumbs)
		return;

	U32 *slots = device->breadcrumbs + (U64) device->fifId * GRAPHICS_BREADCRUMBS;
	const ListU32 scopes = device->breadcrumbScopes[device->fifId];

	//A finished frame has every slot at 2. One with a slot at 1 was entered and never left, which only a lost device
	// does, so it is kept for the report; one with slots still at 0 and none at 1 never reached the GPU at all (its
	// submit failed), which says nothing about a loss.
	//Nor does a frame whose submit failed without a loss (breadcrumbFailedFrames), even with a slot at 1:
	// a flush can have run its begins while their ends sat in the part that never executed, so it is never kept.
	//Keeping only frames reused after a reported loss would fail the other way:
	// a driver can signal a lost device's fence, so the loss only shows submits after the frame it happened in.

	const U64 failedBit = (U64) 1 << device->fifId;
	const Bool failed = !!(device->breadcrumbFailedFrames & failedBit);
	device->breadcrumbFailedFrames &= ~failedBit;

	Bool entered = false;

	for(U64 i = 0; i < scopes.length && !entered && !failed; ++i)
		entered = slots[i] == 1;

	if(entered && !device->breadcrumbUnfinishedScopes.length) {

		const Allocator *alloc = GraphicsDevice_getAlloc(device);

		if(
			ListU32_resize(&device->breadcrumbUnfinishedSlots, scopes.length, alloc, NULL) &&
			ListU32_createCopy(scopes, alloc, &device->breadcrumbUnfinishedScopes, NULL)
		) {
			for(U64 i = 0; i < scopes.length; ++i)
				device->breadcrumbUnfinishedSlots.ptrNonConst[i] = slots[i];

			device->breadcrumbUnfinishedSubmit = device->breadcrumbSubmit[device->fifId];
		}
	}

	for(U32 i = 0; i < scopes.length; ++i)
		slots[i] = 0;

	ListU32_clear(&device->breadcrumbScopes[device->fifId], NULL);
	device->breadcrumbSubmit[device->fifId] = device->submitId;
}

//A breadcrumb's scope as the report names it: the id an app recorded it with, or the phase of the submit it covers.

static const C8 *GraphicsDevice_breadcrumbName(U32 id, C8 *buf, U64 len) {

	if(id == GRAPHICS_BREADCRUMB_UPLOADS)
		return "uploads";

	if(id == GRAPHICS_BREADCRUMB_READBACKS)
		return "readbacks";

	//"scope " and the id's digits, written back to front

	const C8 prefix[] = "scope ";
	C8 digits[10];
	U64 n = 0, at = 0;

	do {
		digits[n++] = (C8) ('0' + id % 10);
		id /= 10;
	} while(id && n < sizeof(digits));

	for(U64 i = 0; i + 1 < sizeof(prefix) && at + 1 < len; ++i)
		buf[at++] = prefix[i];

	while(n && at + 1 < len)
		buf[at++] = digits[--n];

	buf[at] = '\0';
	return buf;
}

//One submit's scopes: how many ended, the ones that began without ending, and the last that ended.

static void GraphicsDevice_logBreadcrumbSubmit(U64 submit, const ListU32 scopes, const U32 *slots) {

	U32 ended = 0, open = 0, lastEnded = U32_MAX;

	for(U32 j = 0; j < scopes.length; ++j) {

		const U32 v = slots[j];

		if(v == 2) {
			++ended;
			lastEnded = j;
		}

		else if(v == 1)
			++open;
	}

	Log_errorLnx(
		"\tsubmit %" PRIu64 ": %" PRIu32 " of %" PRIu64 " scopes ended, %" PRIu32 " began without ending",
		submit, ended, scopes.length, open
	);

	C8 name[32];

	for(U32 j = 0, logged = 0; j < scopes.length && logged < 8; ++j)
		if(slots[j] == 1) {
			Log_errorLnx(
				"\t\t%s began and never ended (%" PRIu32 " of the submit)",
				GraphicsDevice_breadcrumbName(scopes.ptr[j], name, sizeof(name)), j
			);

			++logged;
		}

	if(lastEnded != U32_MAX)
		Log_errorLnx(
			"\t\tlast that ended: %s (%" PRIu32 " of the submit)",
			GraphicsDevice_breadcrumbName(scopes.ptr[lastEnded], name, sizeof(name)), lastEnded
		);
}

static void GraphicsDevice_logBreadcrumbs(GraphicsDevice *device) {

	if(!device->breadcrumbs) {
		Log_errorLnx("\tno breadcrumbs to say which scope it was in (EGraphicsDeviceFlags_Breadcrumbs)");
		return;
	}

	//The submit the device was lost in, when a later one reused its frame before the loss showed

	if(device->breadcrumbUnfinishedScopes.length) {
		Log_errorLnx("\tunfinished when its frame was reused, so likely where it was lost:");
		GraphicsDevice_logBreadcrumbSubmit(
			device->breadcrumbUnfinishedSubmit, device->breadcrumbUnfinishedScopes,
			device->breadcrumbUnfinishedSlots.ptr
		);

		ListU32_clear(&device->breadcrumbUnfinishedScopes, NULL);
		ListU32_clear(&device->breadcrumbUnfinishedSlots, NULL);
	}

	U32 order[MAX_FRAMES_IN_FLIGHT];

	for(U32 i = 0; i < device->framesInFlight; ++i)
		order[i] = i;

	for(U32 i = 0; i < device->framesInFlight; ++i)
		for(U32 j = i + 1; j < device->framesInFlight; ++j)
			if(device->breadcrumbSubmit[order[j]] > device->breadcrumbSubmit[order[i]]) {
				const U32 tmp = order[i];
				order[i] = order[j];
				order[j] = tmp;
			}

	for(U32 i = 0; i < device->framesInFlight; ++i) {

		const U32 fif = order[i];
		const ListU32 scopes = device->breadcrumbScopes[fif];

		if(scopes.length && (device->breadcrumbFailedFrames & ((U64) 1 << fif))) {
			Log_errorLnx(
				"\tsubmit %" PRIu64 ": failed before it all reached the GPU, so its scopes say nothing about the loss",
				device->breadcrumbSubmit[fif]
			);

			continue;
		}

		if(scopes.length)
			GraphicsDevice_logBreadcrumbSubmit(
				device->breadcrumbSubmit[fif], scopes,
				device->breadcrumbs + (U64) fif * GRAPHICS_BREADCRUMBS
			);
	}
}

Bool GraphicsDeviceRef_isLost(GraphicsDeviceRef *deviceRef) {
	return deviceRef && AtomicI64_load(&GraphicsDeviceRef_ptr(deviceRef)->lossReported);
}

void GraphicsDevice_freeResourceRegistry(GraphicsDevice *device, const Allocator *alloc) {

	for(U64 i = 0; i < device->liveResources.length; ++i)
		CharString_free(&device->liveResources.ptrNonConst[i].name, alloc);

	ListLiveResource_free(&device->liveResources, alloc);

	for(U32 i = 0; i < GRAPHICS_FREED_RESOURCES; ++i)
		CharString_free(&device->freedResources[i].name, alloc);

	for(U32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		ListU32_free(&device->breadcrumbScopes[i], alloc);

	ListU32_free(&device->breadcrumbUnfinishedScopes, alloc);
	ListU32_free(&device->breadcrumbUnfinishedSlots, alloc);
}

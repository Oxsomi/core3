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

//graphics/generic/device_buffer.h

#pragma once
#include "graphics/generic/resource.h"
#include "types/container/stream.h"
#include "types/base/lock.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct CharString CharString;

typedef RefPtr GraphicsDeviceRef;

typedef enum EDeviceBufferUsage {

	EDeviceBufferUsage_None                 = 0,

	EDeviceBufferUsage_Vertex               = 1 << 0,        //Allow for use as vertex buffer
	EDeviceBufferUsage_Index                = 1 << 1,        //Allow for use as index buffer
	EDeviceBufferUsage_Indirect             = 1 << 2,        //Allow for use in indirect draw/dispatch calls
	EDeviceBufferUsage_Uniform              = 1 << 3,        //Allow use as a UBO/constant buffer

	//Raytracing types (internal)

	EDeviceBufferUsage_ScratchExt           = 1 << 4,        //Allow for internal use as scratch buffer
	EDeviceBufferUsage_ASExt                = 1 << 5,        //Allow for internal use as acceleration structure
	EDeviceBufferUsage_ASReadExt            = 1 << 6,        //Allow buffer to be read by AS creation
	EDeviceBufferUsage_SBTExt               = 1 << 7,        //Allow for internal use as shader binding table
	EDeviceBufferUsage_Predicate            = 1 << 8         //Allow use as a scope predicate (U64 per read)

} EDeviceBufferUsage;

typedef RefPtr DeviceBufferRef;
typedef RefPtr DescriptorTableRef;

TList(DevicePendingRange);

typedef struct DeviceData {
	DeviceBufferRef *buffer;
	U64 offset, len;
} DeviceData;

typedef struct DeviceBuffer {

	GraphicsResource resource;

	EDeviceBufferUsage usage;
	Bool isPendingFullCopy, isPending, isFirstFrame;

	//Whether cpuStream survives the upload that consumed it. Off, it is dropped the way cpuData is.

	Bool keepStream;

	DescriptorTableRef *bindlessDescriptorTable;

	Buffer cpuData;                           //Null if not cpu backed & uploaded. If not cpu backed this will free post upload

	//The upload SOURCE, when it is not cpuData. A dirty range is read straight out of this into mapped memory
	//or into staging, so the bytes never exist on the host as a whole: a file backed buffer is uploaded
	// without the file being resident, and an archive or encryption stream decodes into the destination
	// rather than into a buffer that is then copied.
	//NULL leaves cpuData as the source, which is what every existing caller gets.
	//Owned: a reference is taken at create and dropped with the buffer.

	StreamRef *cpuStream;

	ListDevicePendingRange pendingChanges;

	U32 readHandle, writeHandle;

	SpinLock lock;

} DeviceBuffer;

//TODO: Ability to query allocation size (inc alignment)
//TODO: Ability to say resource won't need upload / or only clear.

#define DeviceBuffer_ext(ptr, T) (!ptr ? NULL : (T##DeviceBuffer*)(ptr + 1))        //impl
#define DeviceBufferRef_ptr(ptr) RefPtr_data(ptr, DeviceBuffer)

//How many pieces a source read of this length would be split into, which is 1 for everything that is not worth
//splitting or not safe to split. Exposed so the RULE can be asserted rather than inferred from a timing:
//a stream that does not declare EStreamType_ConcurrentRead has to come back as 1 however large the range is.

U8 DeviceBuffer_readPieces(const DeviceBuffer *buffer, const OxStream *stream, U64 length);

//What an upload consumed, released once it has: cpuData unless CPUBacked, and the stream unless keepSource.
//Both backends call this rather than freeing cpuData themselves, so the rule lives in one place.

void DeviceBuffer_releaseUploadSource(DeviceBuffer *buffer, const Allocator *alloc);

//What a pending range reads from, which is the stream where there is one and cpuData where there is not.
//Both backends go through this rather than indexing cpuData, so a source that is not resident works the same
// on either of them.

Bool DeviceBuffer_readUploadSource(
	DeviceBuffer *buffer, U64 offset, U64 length, Buffer dst, const Allocator *alloc, Error *e_rr
);

//A buffer whose upload SOURCE is a stream rather than bytes the caller holds, so the bytes never exist on the
//host as a whole. size is what the resource will be; this does not read the stream to find out.
//A reference is taken on the stream and dropped with the buffer. CPUBacked is refused, since that flag is a
// host copy to read BACK into, which is the opposite of what a stream source is for.

Bool GraphicsDeviceRef_createBufferStream(
	GraphicsDeviceRef *dev,
	EDeviceBufferUsage usage,
	EGraphicsResourceFlag flags,
	DescriptorTableRef *bindlessDescriptorTable,
	const CharString *name,
	StreamRef *stream,
	U64 size,
	Bool keepSource,              //Hold the stream past the upload, for a later partial update of a range
	DeviceBufferRef **buf,
	Error *e_rr
);

//Create empty buffer or initialized with data.
//Initializing to non-zero isn't free due to copies.
//    Initializing to non-zero will move the buffer to created DeviceBuffer, unless it's a ref (then it will create a new one)

Bool GraphicsDeviceRef_createBuffer(
	GraphicsDeviceRef *dev,
	EDeviceBufferUsage usage,
	EGraphicsResourceFlag resourceFlags,
	DescriptorTableRef *bindlessDescriptorTable,
	const CharString *name,
	U64 len,
	DeviceBufferRef **buf,
	Error *e_rr
);

Bool GraphicsDeviceRef_createBufferData(
	GraphicsDeviceRef *dev,
	EDeviceBufferUsage usage,
	EGraphicsResourceFlag resourceFlags,
	DescriptorTableRef *bindlessDescriptorTable,
	const CharString *name,
	Buffer *dat,
	DeviceBufferRef **buf,
	Error *e_rr
);

//A raw buffer is WORD addressed: a D3D12 raw view takes its first element and its count in 4 byte units, so a
//resource whose size stops inside a word would leave that word unreachable by any shader. The BACKEND
//allocation is padded up to a whole word for that reason, and a view reaching the end of the resource rounds
// up to match, which is what keeps the last bytes of a file or a record readable.
//
//resource.size stays exactly what the caller asked for: it is the length of the upload and of every region
// derived from the resource, and rounding it would change how many elements those describe.

static inline U64 DeviceBuffer_allocSize(U64 size) { return (size + 3) &~ (U64)3; }

//Mark the underlying data for the DeviceBuffer as dirty.
//Count 0 indicates rest of the buffer starting at offset.
//Each region that doesn't intersect will be considered as 1 copy (otherwise it will be merged).
//Call this as little as possible while still not copying too much data.
//A range needs a source that can still be read: a CPU backed buffer, a stream backed one, or a CPUAllocated
// one whose memory the caller wrote into directly. Anything else can only be marked for its first upload.

Bool DeviceBufferRef_markDirty(DeviceBufferRef *buffer, U64 offset, U64 count, Error *e_rr);

//Read a region back from the device into cpuData (requires CPUBacked).
//The copy is recorded after the next submit's command lists, so it sees that frame's results, and completes
// once the frame finished on the device (the callback fires then; stall with wait() if it's needed now).
//len 0 means the whole buffer starting at offset.
Bool DeviceBufferRef_pullRegion(
	DeviceBufferRef *buf, U64 offset, U64 len, DevicePullCallback callback, void *context, Error *e_rr
);

#ifdef __cplusplus
	}
#endif

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

//formats/oiPL/pl_file.h

#pragma once
#include "types/math/flp.h"
#include "formats/gfx_util/gfx_util.h"
#include "formats/oiDL/dl_file.h"

#ifdef __cplusplus
	extern "C" {
#endif

//oiPL describes a pipeline layout without a device: the descriptor bindings, the push constant range and the
// sampler values a layout bakes in, which is what a pipeline's shaders compile against and therefore part of
// what decides their ISA.
//It stands alone (a replay stream records layouts as they are created, a cache keys on the serialized bytes,
// a serialized layout can be instantiated directly like a D3D12 serialized root signature) and embeds as a
// subfile wherever a format carries pipelines, the way oiSB embeds in oiSH.

//Where a value came from, per record, so a report can say what was proven and what was chosen.
//Sampler values can never come from reflection, so every sampler a file carries counts as supplied.

typedef enum EPLSource {
	EPLSource_Derived,                    //Proven by shader reflection
	EPLSource_Supplied,                   //The caller chose it
	EPLSource_Assumed,                    //Nobody chose it, so a value was picked
	EPLSource_Count
} EPLSource;

//A sampler's state, stored by value: a file has to describe a baked sampler without a device, so it only
// becomes a real sampler once something instantiates the layout.

typedef enum ESamplerFilterMode {

	ESamplerFilterMode_Nearest,
	ESamplerFilterMode_LinearMagNearestMinMip,
	ESamplerFilterMode_LinearMinNearestMagMip,
	ESamplerFilterMode_LinearMagMinNearestMip,
	ESamplerFilterMode_LinearMipNearestMagMin,
	ESamplerFilterMode_LinearMagMipNearestMin,
	ESamplerFilterMode_LinearMinMipNearestMag,
	ESamplerFilterMode_Linear,

	ESamplerFilterMode_None             = 0,

	ESamplerFilterMode_LinearMag        = 1 << 0,
	ESamplerFilterMode_LinearMin        = 1 << 1,
	ESamplerFilterMode_LinearMip        = 1 << 2,

	ESamplerFilterMode_PropertyCount    = 3,
	ESamplerFilterMode_All              = (1 << ESamplerFilterMode_PropertyCount) - 1

} ESamplerFilterMode;

typedef enum ESamplerAddressMode {
	ESamplerAddressMode_Repeat,
	ESamplerAddressMode_MirrorRepeat,
	ESamplerAddressMode_ClampToEdge,
	ESamplerAddressMode_ClampToBorder,
	ESamplerAddressMode_Count
} ESamplerAddressMode;

typedef enum ESamplerBorderColor {
	ESamplerBorderColor_TransparentBlack,        //0.xxxx
	ESamplerBorderColor_OpaqueBlackFloat,        //0.xxx, 1.f
	ESamplerBorderColor_OpaqueBlackInt,          //0.xxx, 1
	ESamplerBorderColor_OpaqueWhiteFloat,        //1.f.xxxx
	ESamplerBorderColor_OpaqueWhiteInt,          //1.xxxx
	ESamplerBorderColor_Count
} ESamplerBorderColor;

typedef struct PLSamplerInfo {

	U8 filter;                            //ESamplerFilterMode
	U8 addressU, addressV, addressW;      //ESamplerAddressMode[3]

	U8 aniso;                             //0-16
	U8 borderColor;                       //ESamplerBorderColor
	U8 comparisonFunction;                //ECompareOp
	Bool enableComparison;

	F16 mipBias, minLod, maxLod;
	U16 padding;

} PLSamplerInfo;

//One binding of the layout.

typedef struct PLDescriptorBinding {

	U32 registerType;            //EGfxRegisterType, including its mask bits
	U32 count;                   //Descriptor count; 0 is an unbounded array whose capacity has to be supplied

	//A layout depends on which api consumes it, so both numberings travel and the device picks its own.

	GfxBindings bindings;

	U32 visibility;              //Bit mask of EGfxPipelineStage

	//The active member follows the register class

	union {
		U32 strideOrLength;       //Buffer classes: structured stride or constant buffer size
		GfxTextureFormat texture; //Writable texture classes
		U32 samplerId;            //Sampler classes: 1 + an index into samplers[], 0 keeps the sampler dynamic
	};

	U32 name24_source8;          //Low 24 bits string id (0xFFFFFF = unnamed), high 8 bits EPLSource
	U32 padding;                 //Zeroes; the U64 view of bindings aligns the row to 8 bytes

} PLDescriptorBinding;

#define PLDescriptorBinding_NAME_NONE 0xFFFFFF

static inline U32 PLDescriptorBinding_name(PLDescriptorBinding b) { return b.name24_source8 & 0xFFFFFF; }
static inline EPLSource PLDescriptorBinding_source(PLDescriptorBinding b) { return (EPLSource)(b.name24_source8 >> 24); }

static inline U32 PLDescriptorBinding_pack(U32 name, EPLSource source) {
	return (name & 0xFFFFFF) | ((U32)source << 24);
}

TList(PLSamplerInfo);
TList(PLDescriptorBinding);

typedef enum EPLSettingsFlags {
	EPLSettingsFlags_None            = 0,
	EPLSettingsFlags_HideMagicNumber = 1 << 0,        //Only valid if the oiPL is a subfile
	EPLSettingsFlags_Invalid         = 0xFFFFFFFF << 1
} EPLSettingsFlags;

typedef struct PLFile {

	DLFile names;                //String pool: binding and push constant names

	ListPLDescriptorBinding bindings;
	ListPLSamplerInfo samplers;

	//The push constant range sits beside the bindings rather than among them, since it takes no descriptor.
	//Only its SPIRV pair is unused (a SPIRV push constant binds through no pair); the DXIL pair is the b
	// register a root signature binds and strideOrLength is the byte size.

	PLDescriptorBinding pushConstant;
	Bool hasPushConstant;
	U8 padding[3];

	EPLSettingsFlags flags;
	U64 hash;                    //Refreshed by PLFile_finalize

} PLFile;

TList(PLFile);

Bool PLFile_create(EPLSettingsFlags flags, const Allocator *alloc, PLFile *plFile, Error *e_rr);
void PLFile_free(PLFile *plFile, const Allocator *alloc);
void ListPLFile_freeUnderlying(ListPLFile *files, const Allocator *alloc);

//Add a string to the pool (deduplicated, copied), returning its id.
//U32_MAX means "no string"; passing an empty/null CharString returns U32_MAX without adding.

Bool PLFile_addString(PLFile *plFile, CharString *str, const Allocator *alloc, U32 *id, Error *e_rr);

//Deep copy, so a stored layout can replace another file's without sharing memory.

Bool PLFile_copy(const PLFile *src, const Allocator *alloc, PLFile *dst, Error *e_rr);

//The device free rules one binding row has to satisfy; every consumer judges a row through this one function,
// so no two of them can disagree about what a valid row is.
//nameCount/samplerCount of U64_MAX skip those bounds (a caller without the file's pools); device bound rules
// (feature support, heap capacity, backend space limits) stay with the device.
//allowReservedSpace is for the runtime's own internal layouts, which are the only ones allowed to bind there.

Bool PLDescriptorBinding_validate(
	const PLDescriptorBinding *b, U64 nameCount, U64 samplerCount, Bool allowReservedSpace, Error *e_rr
);

//Appends every device free problem the layout has to issues, one line each, like SPFile_validate does for a
// pipeline: a bad layout names its mismatches before a driver gets to fail on them opaquely.

Bool PLFile_validate(const PLFile *plFile, const Allocator *alloc, ListCharString *issues, Error *e_rr);

//The device free rules a sampler's stored values have to satisfy; the reader runs it on every pooled sampler.

Bool PLSamplerInfo_validate(const PLSamplerInfo *s, Error *e_rr);

Bool PLFile_finalize(PLFile *plFile, const Allocator *alloc, Error *e_rr);

//Serialization. A NULL stream accumulates the size into *offset without writing, so a parent format can
// reserve before it commits.

Bool PLFile_write(const PLFile *plFile, const Allocator *alloc, StreamRef *streamRef, U64 *offset, Error *e_rr);
Bool PLFile_read(StreamRef *streamRef, U64 *offset, Bool isSubFile, const Allocator *alloc, PLFile *plFile, Error *e_rr);

typedef enum EPLVersion {
	EPLVersion_Undefined,
	EPLVersion_V1_1            //Current (on-disk version byte 1, displayed as major.minor 1.1)
} EPLVersion;

typedef enum EPLFlag {
	EPLFlag_None            = 0,
	EPLFlag_HasPushConstant = 1 << 0,        //The push constant row follows bindings[]
	EPLFlag_Unsupported     = 0xFE           //Every bit above HasPushConstant
} EPLFlag;

typedef struct PLHeader {

	U8 version;                     //EPLVersion
	U8 flags;                       //EPLFlag
	U8 bindingCount;
	U8 samplerCount;

} PLHeader;

static_assert(sizeof(PLHeader) == 4, "PLHeader size is part of oiPL");
static_assert(EGfxBinaryType_Count == 2, "PLDescriptorBinding serializes one GfxBinding pair per binary type");
static_assert(sizeof(PLDescriptorBinding) == 40, "PLDescriptorBinding size is part of oiPL");
static_assert(sizeof(PLSamplerInfo) == 16, "PLSamplerInfo size is part of oiPL");

#define PLHeader_MAGIC 0x4C50696F

#ifdef __cplusplus
	}
#endif

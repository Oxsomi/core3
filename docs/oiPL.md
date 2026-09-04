# oiPL (Oxsomi Pipeline Layout)

*The oiPL format is an [oiXX format](oiXX.md), as such it inherits the properties from that such as compression, encryption and endianness (though enc/comp is not supported, since oiPL is most often embedded in an oiSP or packaged inside of an oiCA/oiDL file).*

oiPL is a single pipeline layout which specifies the descriptor bindings, the push constant range and the sampler values a layout bakes in. The layout is part of what a pipeline's shaders compile against and therefore part of what decides their ISA: descriptor set pointers and push constants arrive in user SGPRs and consume the root signature's budget, so two layouts that both accept a shader can still produce different code. It stands alone (a replay stream records layouts as they are created, a cache keys on the serialized bytes, a serialized layout can be instantiated directly like a D3D12 serialized root signature) and embeds as a subfile wherever a format carries pipelines, the way oiSB embeds in oiSH.

Just like any oiXX file it's made with the following things in mind:

- Ease of read + write.
- An easy spec.
- Good security for parsing + writing.

## File format spec

```c
typedef struct PLHeader {		//Should be aligned to 4-byte

	U32 magicNumber;			//oiPL (0x4C50696F); optional if it's embedded in a parent format.

	U8 version;					//EPLVersion; on-disk byte 1, displayed as 1.1
	U8 flags;					//EPLFlag; unknown flags refuse the file
	U8 bindingCount;
	U8 samplerCount;

} PLHeader;

typedef enum EPLFlag {
	EPLFlag_None            = 0,
	EPLFlag_HasPushConstant = 1 << 0,		//The push constant row follows bindings[]
	EPLFlag_Unsupported     = 0xFE			//Every bit above HasPushConstant
} EPLFlag;

//Where a value came from, per record.
//Sampler values can never come from reflection, so every sampler a file carries counts as supplied.

typedef enum EPLSource {
	EPLSource_Derived,			//Proven by shader reflection
	EPLSource_Supplied,			//The caller chose it
	EPLSource_Assumed,			//Nobody chose it, so a value was picked
	EPLSource_Count
} EPLSource;

//One binding of the layout.

typedef struct PLDescriptorBinding {

	U32 registerType;			//EGfxRegisterType, including its mask bits
	U32 count;					//Descriptor count; 0 is an unbounded array whose capacity has to be supplied

	//A layout depends on which api consumes it, so both numberings travel and the device picks its own.

	GfxBindings bindings;

	U32 visibility;				//Bit mask of EGfxPipelineStage

	//The active member follows the register class

	union {
		U32 strideOrLength;		//Buffer classes: structured stride or constant buffer size
		GfxTextureFormat texture;	//Writable texture classes
		U32 samplerId;			//Sampler classes: 1 + an index into samplers[], 0 keeps the sampler dynamic
	};

	U32 name24_source8;			//Low 24 bits string id (0xFFFFFF = unnamed), high 8 bits EPLSource
	U32 padding;				//Zeroes; the U64 view of bindings aligns the row to 8 bytes

} PLDescriptorBinding;

typedef struct PLSamplerInfo {

	U8 filter;					//ESamplerFilterMode
	U8 addressU, addressV, addressW;	//ESamplerAddressMode[3]

	U8 aniso;					//0-16
	U8 borderColor;				//ESamplerBorderColor
	U8 comparisonFunction;		//ECompareOp
	Bool enableComparison;

	F16 mipBias, minLod, maxLod;
	U16 padding;

} PLSamplerInfo;

typedef enum ESamplerFilterMode {	//Bit per linear property; all off is nearest

	ESamplerFilterMode_Nearest,
	ESamplerFilterMode_Linear			= 7,

	ESamplerFilterMode_LinearMag		= 1 << 0,
	ESamplerFilterMode_LinearMin		= 1 << 1,
	ESamplerFilterMode_LinearMip		= 1 << 2

} ESamplerFilterMode;

typedef enum ESamplerAddressMode {
	ESamplerAddressMode_Repeat,
	ESamplerAddressMode_MirrorRepeat,
	ESamplerAddressMode_ClampToEdge,
	ESamplerAddressMode_ClampToBorder
} ESamplerAddressMode;

typedef enum ESamplerBorderColor {
	ESamplerBorderColor_TransparentBlack,		//0.xxxx
	ESamplerBorderColor_OpaqueBlackFloat,		//0.xxx, 1.f
	ESamplerBorderColor_OpaqueBlackInt,			//0.xxx, 1
	ESamplerBorderColor_OpaqueWhiteFloat,		//1.f.xxxx
	ESamplerBorderColor_OpaqueWhiteInt			//1.xxxx
} ESamplerBorderColor;

//Final file format; please manually parse the members.
//Verify if everything's in bounds.
//Verify if PLFile includes any invalid data.

PLFile {

	PLHeader header;

	PLDescriptorBinding bindings[header.bindingCount];
	PLDescriptorBinding pushConstant;		//Only present if header.flags & HasPushConstant
	PLSamplerInfo samplers[header.samplerCount];

	U8 padding[..];							//Zeroes until the next 16-byte boundary

	DLFile names;							//String pool: binding and push constant names (16-byte aligned)
}
```

The push constant row sits beside the bindings rather than among them, since it takes no descriptor. Its SPIRV pair is `U32_MAX` (a SPIRV push constant binds through no pair), its DXIL pair carries the b register the constants occupy in a root signature, and `strideOrLength` is the byte size.

The sampler description and its enums are owned by oiPL, since a file has to describe a baked sampler without a device.

## Shared graphics vocabulary

The register, stage and binding vocabulary is owned by `formats/gfx_util` (gfx_util.h), the shared library for values more than one format serializes (oiSH, oiSP and oiPL all store them), and is normative here:

```c
typedef enum EGfxRegisterType {

	EGfxRegisterType_Sampler,
	EGfxRegisterType_SamplerComparisonState,

	EGfxRegisterType_ConstantBuffer,				//UBO or CBuffer
	EGfxRegisterType_PushConstants,					//Push constants or CBuffer (DXIL)
	EGfxRegisterType_ByteAddressBuffer,
	EGfxRegisterType_StructuredBuffer,
	EGfxRegisterType_StructuredBufferAtomic,		//SBuffer + atomic counter
	EGfxRegisterType_StorageBuffer,
	EGfxRegisterType_StorageBufferAtomic,
	EGfxRegisterType_AccelerationStructure,

	EGfxRegisterType_Texture1D,
	EGfxRegisterType_Texture2D,
	EGfxRegisterType_Texture3D,
	EGfxRegisterType_TextureCube,
	EGfxRegisterType_Texture2DMS,
	EGfxRegisterType_SubpassInput,

	EGfxRegisterType_TypeMask			= 0xF,
	EGfxRegisterType_IsArray			= 1 << 4,	//Only valid on textures
	EGfxRegisterType_IsCombinedSampler	= 1 << 5,	//Only valid on textures
	EGfxRegisterType_IsWrite			= 1 << 6	//Invalid on samplers, AS and CBuffer classes

} EGfxRegisterType;

typedef enum EGfxPipelineStage {	//visibility is a bit mask of these (1 << stage)

	EGfxPipelineStage_Vertex,
	EGfxPipelineStage_Pixel,
	EGfxPipelineStage_Compute,
	EGfxPipelineStage_GeometryExt,
	EGfxPipelineStage_Hull,
	EGfxPipelineStage_Domain,

	EGfxPipelineStage_RaygenExt,
	EGfxPipelineStage_CallableExt,
	EGfxPipelineStage_MissExt,
	EGfxPipelineStage_ClosestHitExt,
	EGfxPipelineStage_AnyHitExt,
	EGfxPipelineStage_IntersectionExt,

	EGfxPipelineStage_MeshExt,
	EGfxPipelineStage_TaskExt,

	EGfxPipelineStage_Reserved

} EGfxPipelineStage;

typedef enum EGfxBinaryType {
	EGfxBinaryType_SPIRV,
	EGfxBinaryType_DXIL,
	EGfxBinaryType_Count
} EGfxBinaryType;

typedef struct GfxBinding {
	U32 space;					//Space or set, depending on binary type
	U32 binding;
} GfxBinding;

//U32_MAX for both space and binding indicates 'not present'
typedef union GfxBindings {
	U64 arrU64[EGfxBinaryType_Count];
	GfxBinding arr[EGfxBinaryType_Count];
} GfxBindings;

typedef struct GfxTextureFormat {
	U8 primitive;					//EGfxTexturePrimitive (gfx_util.h)
	U8 formatId;					//ETextureFormatId; must match primitive and be uncompressed
} GfxTextureFormat;

typedef enum ECompareOp {
	ECompareOp_Gt,
	ECompareOp_Geq,
	ECompareOp_Eq,
	ECompareOp_Neq,
	ECompareOp_Leq,
	ECompareOp_Lt,
	ECompareOp_Always,
	ECompareOp_Never
} ECompareOp;
```

## Sentinels & invariants

- The low 24 bits of `name24_source8` at `0xFFFFFF` mean unnamed; anything else has to index the names oiDL. The high 8 bits have to be a valid `EPLSource`.
- A `U32_MAX` binding pair means the register doesn't exist for that binary type; at least one pair has to be present on a stored binding row.
- `count = 0` stores an unbounded array. It is readable and reportable but not instantiable: the real capacity is a heap decision the file can't make, so instantiating it is refused until the count is supplied.
- A sampler row's `samplerId` is layout relative (1 + index into this file's `samplers[]`), so a transplant into another file needs no rebasing.
- Register space 0xC3 (`OXC3_RESERVED_SPACE`, gfx_util.h) is the runtime's own bindless space and can never appear in a stored layout, on either binary type.

## Validation

The device free rules live in `PLDescriptorBinding_validate`; the reader, `PLFile_validate` and every consumer judge a row through that one function, so no two of them can disagree about what a valid row is:

- The register type and source have to be known values.
- The write flag is invalid on samplers, acceleration structures and constant buffer classes.
- A constant buffer's size has to be `0 < size <= 64KiB`; a structured buffer's stride can't be 0.
- A baked sampler reference has to land inside `samplers[]`; a name id has to land inside the names oiDL.
- A binding pair is present or absent as a whole; a row outside the push constant has to bind on at least one binary type.
- The reserved space is refused (only the runtime's own internal layouts are exempt).

`PLFile_validate` additionally names overlapping rows (a SPIRV pair binds one (set, binding) slot; a DXIL register occupies its whole `[binding, binding + count)` range in its space and b/t/u/s namespace, and an unbounded array reaches every register above its start) and unbounded arrays, one issue line each, so a bad layout reports its mismatches before a driver gets to fail on them opaquely. Device bound rules (feature support, heap capacity, backend space limits) stay with the device, which consumes its own numbering: the SPIRV pairs on Vulkan and the DXIL pairs on D3D12.

## Hashing & comparing

Hashes are generated like following:

- FNV-1a64 is used (64-bit FNV-1a).
- The seed is `hasPushConstant` FNVed as a single U64.
- The whole `bindings[]` byte buffer is FNVed, then `samplers[]`.
- The push constant row is FNVed by its bytes when present.
- Every string in the pool (in order) is FNVed as its length (one U64) followed by its bytes, so two pools sharing one concatenation can't collide.

This hash is refreshed by `PLFile_finalize` (and on read), doubles as a cache and dedup key for instantiated layouts, and folds into the hash of any oiSP that embeds the layout, so a standalone oiPL and an embedded one can never disagree. It is only available at runtime.

## Changelog

1.1: Initial format specification (no shipped file predates it, so it evolves in place rather than versioning). Bindings with per binary type numbering and per row provenance, an optional push constant range, samplers by value and the names oiDL.

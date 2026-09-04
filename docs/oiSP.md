# oiSP (Oxsomi Shader Pipeline)

*The oiSP format is an [oiXX format](oiXX.md), as such it inherits the properties from that such as compression, encryption and endianness (though enc/comp is not supported, since oiSP is most often packaged alongside an oiSH, or inside an oiCA/oiDL file).*

oiSP stores pipelines: the compute, graphics and ray tracing state that sits around a shader, together with which shader stages the pipeline is built from and which descriptor layout it was created against. Pipeline state changes the compiled ISA, so the same oiSP plus the same [oiSH](oiSH.md) reproduces the same pipeline, on a device or offline.

Just like any oiXX file it's made with the following things in mind:

- Ease of read + write.
- An easy spec.
- Good security for parsing + writing.

## File format spec

```c
typedef struct SPHeader {       //Should be aligned to 4-byte

	U32 magicNumber;            //oiSP (0x5053696F); optional if it's embedded in a parent format.

	U8 version;                 //ESPVersion; on-disk byte 1 (ESPVersion_V1_1), displayed as 1.1
	U8 flags;                   //ESPFlag; none defined yet
	U16 padding;

	U32 pipelineCount;

	U32 stageCount;             //Shared pool, pipelines index ranges into it

	U32 specializationCount;    //Shared pool, pipelines index ranges into it

	U32 graphicsStateCount;     //Kind specific pool, referenced by SPPipelineBase.stateIndex

	U32 raytracingStateCount;   //Kind specific pool, referenced by SPPipelineBase.stateIndex

	U32 blendAttachmentCount;   //Only the attachments a blend state can reach are stored

	U32 vertexBufferCount;      //Only the buffers that carry a stride or instance rate are stored

	U32 vertexAttributeCount;   //Only the input locations that carry a format are stored

	U32 layoutCount;            //oiPL subfiles, referenced by SPPipelineBase.layoutIndex

} SPHeader;

typedef struct SPPipelineBase { //What every pipeline has, whatever kind it is

	U32 name;                   //String id, U32_MAX if unnamed

	U8 type;                    //ESPPipelineType: Compute, Graphics, Raytracing
	U8 flags;                   //ESPPipelineFlag
	U8 stageStart, stageCount;  //Range into stages[]

	U32 specializationStart;    //Range into specializations[]

	U32 specializationCount;

	U32 stateIndex;             //Into graphicsStates[] or raytracingStates[]; unused by compute

	U32 layoutIndex;            //Into layouts[], U32_MAX when the pipeline takes the device's default layout

} SPPipelineBase;

typedef struct SPStage {        //One stage of a pipeline

	U32 shaderFile;             //String id of the oiSH this stage came from, U32_MAX if unnamed

	U32 entrypoint;             //String id of the entry name

	U32 sourceHash;             //SHFile.sourceHash when the pipeline was made, 0 if unknown

	U8 stage;                   //EGfxPipelineStage
	U8 padding[3];

} SPStage;

typedef struct SPSpecialization {   //One field that reflection couldn't prove

	U8 field;                       //ESPField
	U8 index;                       //Sub index for indexed fields (render target, vertex buffer, layout row)
	U8 source;                      //ESPFieldSource: Derived, Supplied, Assumed
	U8 padding;

	U32 value;                      //The value that would be used, in the field's own units

} SPSpecialization;

//These are identical between the file and a live pipeline, so they're stored exactly as a pipeline binds them.

typedef struct SPDepthState {

	U8 flags;                   //EDepthStencilFlags
	U8 depthCompare;            //ECompareOp
	U8 stencilCompare;          //ECompareOp
	U8 stencilFail;             //EStencilOp

	U8 stencilPass;             //EStencilOp
	U8 stencilDepthFail;        //EStencilOp
	U8 stencilWriteMask;
	U8 stencilReadMask;

} SPDepthState;

typedef struct SPRasterizerState {

	U16 cullMode;               //ECullMode
	U16 flags;                  //ERasterizerFlags

	F32 depthBiasClamp;

	I32 depthBiasConstantFactor;

	F32 depthBiasSlopeFactor;

} SPRasterizerState;

typedef struct SPBlendAttachment {      //How one render target combines with what's already there

	U8 srcBlend, dstBlend;              //EBlend

	U8 srcBlendAlpha, dstBlendAlpha;    //EBlend

	U8 blendOp, blendOpAlpha;           //EBlendOp

} SPBlendAttachment;

typedef struct SPVertexAttribute {  //One vertex input location

	U16 offset11;                   //Byte offset within its buffer, 11 bits
	U8 bufferId4;                   //Which buffer it fetches from, 4 bits
	U8 format;                      //ETextureFormatId; must not be compressed

} SPVertexAttribute;

//A pipeline binds more state than a file has reason to keep, so blend state and the vertex layout exist twice: as the
// runtime form a pipeline is created from (SPBlendStateRuntime, SPVertexLayoutRuntime in sp_state.h) and as the
// stored form below. Nothing is lost between them; what the stored form leaves out is state no pipeline can reach.

typedef struct SPBlendStateStored {

	Bool enable;
	Bool allowIndependentBlend;     //0 = every target uses attachments[0]
	U8 renderTargetMask;            //Bit per render target
	U8 logicOpExt;                  //ELogicOpExt; replaces blending when set

	U8 writeMask[8];                //EWriteMask

	//The attachments go to blendAttachments[], and only the ones the state can reach:
	//N = enable ? (allowIndependentBlend ? popcount(renderTargetMask) : 1) : 0

} SPBlendStateStored;

typedef struct SPVertexLayoutStored {

	//A vertex layout is sparse by index rather than by count (location 3 can be filled while 0 to 2 aren't), so which
	// entries were stored travels as a mask. They go to vertexBuffers[] and vertexAttributes[].

	U16 bufferMask;                 //Bit per vertex buffer that carries a stride or an instance rate
	U16 attributeMask;              //Bit per input location that carries a format

} SPVertexLayoutStored;

typedef struct SPInputAssemblerStored {

	U8 topologyMode;            //ETopologyMode
	U8 padding[3];

	U32 patchControlPoints;     //1..32 with tessellation, else 0

	SPVertexLayoutStored vertexLayout;

} SPInputAssemblerStored;

typedef struct SPGraphicsStateStored {  //One record of graphicsStates[]; 64 bytes against the runtime form's 204

	U8 msaa;                            //EMSAASamples
	U8 renderTargetCount;               //0..8
	U8 depthFormat;                     //EDepthStencilFormat; 0 = no depth attachment
	U8 padding;

	F32 msaaMinSampleShading;           //0 = off, else the 0..1 sample fraction

	U8 renderTargetFormats[8];          //ETextureFormatId

	SPBlendStateStored blend;
	SPDepthState depth;
	SPRasterizerState rasterizer;
	SPInputAssemblerStored inputAssembler;

} SPGraphicsStateStored;

typedef struct SPRaytracingState {  //Compute has no extra state, so a compute pipeline is just its base record

	U8 maxRecursionDepth;
	U8 raytracingFlags;             //EPipelineRaytracingFlags
	U16 padding;

} SPRaytracingState;

typedef enum ESPPipelineType {
	ESPPipelineType_Compute,
	ESPPipelineType_Graphics,
	ESPPipelineType_Raytracing,
	ESPPipelineType_Count
} ESPPipelineType;

//Where a field's value came from.
//The values mirror EPLSource (see oiPL.md), so a layout row's tag and a field's tag mean the same thing everywhere.

typedef enum ESPFieldSource {
	ESPFieldSource_Derived,     //Proven by the shader's reflection
	ESPFieldSource_Supplied,    //The caller chose it
	ESPFieldSource_Assumed,     //Nobody chose it, a value was picked
	ESPFieldSource_Count
} ESPFieldSource;

typedef enum ESPPipelineFlag {
	ESPPipelineFlag_None                 = 0,
	ESPPipelineFlag_GeneratedVertexStage = 1 << 0,  //A vertex stage was generated, not declared
	ESPPipelineFlag_GeneratedPixelStage  = 1 << 1,  //A pixel stage was generated, not declared
	ESPPipelineFlag_AssumedHitGrouping   = 1 << 2,  //Hit groups were paired by order, not stated
	ESPPipelineFlag_Unsupported          = 0xFF << 3
} ESPPipelineFlag;

typedef enum ESPVersion {
	ESPVersion_Undefined,
	ESPVersion_V1_1             //Current; on-disk byte 1, displayed as 1.1
} ESPVersion;

typedef enum ESPFlag {
	ESPFlag_None        = 0,
	ESPFlag_Unsupported = 0xFF
} ESPFlag;

//Every field of a pipeline that reflection can't prove.
//This mirrors PipelineGraphicsInfo and PipelineRaytracingInfo field for field, so a pipeline that supplies every
// field reported for it is complete, never partial.
//Indexed fields address an array element through SPSpecialization.index; the rest must have index 0.

typedef enum ESPField {

	//Render targets and blending

	ESPField_RenderTargetFormat,    //Indexed by render target; ETextureFormatId
	ESPField_RenderTargetCount,
	ESPField_BlendEnable,
	ESPField_BlendIndependent,      //allowIndependentBlend
	ESPField_BlendTargetMask,       //renderTargetMask
	ESPField_BlendLogicOp,          //ELogicOpExt
	ESPField_BlendWriteMask,        //Indexed by render target; EWriteMask
	ESPField_BlendSrc,              //Indexed by render target; EBlend
	ESPField_BlendDst,              //Indexed; EBlend
	ESPField_BlendSrcAlpha,         //Indexed; EBlend
	ESPField_BlendDstAlpha,         //Indexed; EBlend
	ESPField_BlendOp,               //Indexed; EBlendOp
	ESPField_BlendOpAlpha,          //Indexed; EBlendOp

	//Depth and stencil

	ESPField_DepthFormat,           //EDepthStencilFormat
	ESPField_DepthStencilFlags,     //EDepthStencilFlags
	ESPField_DepthCompare,          //ECompareOp
	ESPField_StencilCompare,        //ECompareOp
	ESPField_StencilFail,           //EStencilOp
	ESPField_StencilPass,           //EStencilOp
	ESPField_StencilDepthFail,      //EStencilOp
	ESPField_StencilWriteMask,
	ESPField_StencilReadMask,

	//Rasterizer

	ESPField_CullMode,              //ECullMode
	ESPField_RasterizerFlags,       //ERasterizerFlags
	ESPField_DepthBiasConstant,     //I32 stored in the U32
	ESPField_DepthBiasClamp,        //F32 bits stored in the U32
	ESPField_DepthBiasSlope,        //F32 bits stored in the U32

	//Multisampling, topology, tessellation

	ESPField_Msaa,                  //EMSAASamples
	ESPField_MsaaMinSampleShading,  //F32 bits stored in the U32
	ESPField_TopologyMode,          //ETopologyMode
	ESPField_PatchControlPoints,

	//Vertex input

	ESPField_VertexBufferStride,    //Indexed by vertex buffer
	ESPField_VertexBufferRate,      //Indexed by vertex buffer

	//Ray tracing

	ESPField_MaxRecursionDepth,
	ESPField_RaytracingFlags,       //EPipelineRaytracingFlags

	//Descriptor layout (any pipeline kind); indexed fields index the pipeline layout's own binding/sampler rows.
	//These only exist when the pipeline carries a layout (layoutIndex != U32_MAX): the device's default layout
	// isn't described by the file, so there is nothing there to address.

	ESPField_LayoutBindingType,         //EGfxRegisterType plus its mask bits
	ESPField_LayoutBindingSpace,
	ESPField_LayoutBindingRegister,
	ESPField_LayoutBindingCount,
	ESPField_LayoutBindingVisibility,   //Bit mask of EGfxPipelineStage
	ESPField_LayoutBindingData,         //Stride, cbuffer size, texture format or 1 + sampler id, by the type

	ESPField_LayoutPushConstantSize,
	ESPField_LayoutPushConstantVisibility,

	ESPField_LayoutSamplerFilter,       //ESamplerFilterMode
	ESPField_LayoutSamplerAddressU,     //ESamplerAddressMode
	ESPField_LayoutSamplerAddressV,
	ESPField_LayoutSamplerAddressW,
	ESPField_LayoutSamplerAniso,
	ESPField_LayoutSamplerBorder,       //ESamplerBorderColor
	ESPField_LayoutSamplerCompareOp,    //ECompareOp
	ESPField_LayoutSamplerCompareEnable,
	ESPField_LayoutSamplerMipBias,      //F16 bits in the low half
	ESPField_LayoutSamplerMinLod,       //F16 bits in the low half
	ESPField_LayoutSamplerMaxLod,       //F16 bits in the low half

	ESPField_Count

} ESPField;
```

Sections are laid out in the order the header declares them:

```c
//Final file format; please manually parse the members.
//Verify if everything's in bounds.
//Verify if SPFile includes any invalid data.

SPFile {                    //Has to be 16-byte aligned

	SPHeader header;

	SPPipelineBase pipelines[header.pipelineCount];
	SPStage stages[header.stageCount];
	SPSpecialization specializations[header.specializationCount];

	SPGraphicsStateStored graphicsStates[header.graphicsStateCount];

	SPRaytracingState raytracingStates[header.raytracingStateCount];

	//Then the entries those states select, each in its own section, per graphics state and in index order.
	//The three counts in the header are the totals; a reader derives them itself and rejects the file if they differ.

	blendAttachments[header.blendAttachmentCount]:
		for i < header.graphicsStateCount:
			for j < 8 where blend stores target j:
				SPBlendAttachment attachment

	vertexBuffers[header.vertexBufferCount]:
		for i < header.graphicsStateCount:
			for j < 16 where bufferMask has bit j:
				U16 bufferStride12_isInstance1      //Stride in bits 0-11, per instance in bit 12

	vertexAttributes[header.vertexAttributeCount]:
		for i < header.graphicsStateCount:
			for j < 16 where attributeMask has bit j:
				SPVertexAttribute attribute

	U8[N] pad;              //Padding to align to 16-byte

	//No encryption/compression; string list (see oiDL.md).
	//Pipeline names, shader file names and entrypoint names, referenced by string id.
	//Each string must be below 32Ki characters.
	DLFile names;           //16-byte aligned

	PLFile layouts[header.layoutCount];     //oiPL subfiles, one per stored descriptor layout, see oiPL.md
}
```

The types are Oxsomi types; `U<X>`: x-bit unsigned integer, `I<X>` x-bit signed integer, `F<X>` x-bit float.

The magic number in the header can only be absent if embedded in another file.

On read a stored graphics state is expanded into the runtime form (`SPGraphicsStateStored_expand`) and on write folded back (`SPGraphicsState_store`); the runtime forms (`SPBlendStateRuntime`, `SPVertexLayoutRuntime`, `SPGraphicsState`, 204 bytes) are what a pipeline is created from and never appear in a file. An entry goes back to the index it was written for rather than the slot it was stored in, and every index that stores nothing reads as zero; `SPFile_finalize` clears those same entries in memory, so the content hash describes bytes that survive a write.

`SPRasterizerState`, `SPDepthState`, `SPBlendAttachment`, `SPBlendStateRuntime`, `SPVertexAttribute` and `SPVertexLayoutRuntime` are declared in `oiSP/sp_state.h` and aliased by `graphics/generic/pipeline_structs.h` as `Rasterizer`, `DepthStencilState`, `BlendStateAttachment`, `BlendState`, `VertexAttribute` and `VertexBindingLayout`, so there is one definition rather than two that could drift and lowering a stored pipeline is a struct copy. Because those structs are shared and written to disk verbatim, every size is pinned with a `static_assert` in `sp_file.h`; adding a field is a format change and needs a version bump. `SPFile_toGraphicsInfo` and `SPFile_toRaytracingInfo` lower a pipeline into a create info; compute has no state to lower, so `SPFile_toComputeStage` yields the single stage a compute pipeline binds.

Every pipeline is an `SPPipelineBase` regardless of kind, with the kind specific state hanging off `stateIndex`: anything that walks stages, names or provenance takes the base and never learns which kind it got, and a compute pipeline is 24 bytes rather than carrying a payload it never uses.

Stages are stored by name rather than by an index into a list that only existed at build time: the oiSH the stage came from, the entrypoint within it and that oiSH's `sourceHash` at the time, so a loader resolves the name and can tell when the shader has moved on. Stages of one pipeline may come from several oiSH files, the same shape a pipeline is created from at runtime; mixing compute, graphics and ray tracing stages in one pipeline is rejected.

### State enums

A pipeline's integer fields carry values from the enums below. They're declared in `oiSP/sp_state.h` alongside the structs that store them, because a field kept as a raw integer means nothing without the set it came from, and a reader has to make sense of a file with no device present; the graphics layer includes them from here rather than declaring its own. `EGfxPipelineStage` is the one exception: it belongs to [oiSH](oiSH.md) and is specified there.

```c
typedef enum EMSAASamples {
	EMSAASamples_Off,           //No MSAA (x1)
	EMSAASamples_x2Ext,         //Requires querying the device
	EMSAASamples_x4,            //Always supported
	EMSAASamples_x8Ext,         //Requires querying the device
	EMSAASamples_Count
} EMSAASamples;

typedef enum ETopologyMode {
	ETopologyMode_TriangleList,
	ETopologyMode_TriangleStrip,
	ETopologyMode_LineList,
	ETopologyMode_LineStrip,
	ETopologyMode_PointList,
	ETopologyMode_TriangleListAdj,
	ETopologyMode_TriangleStripAdj,
	ETopologyMode_LineListAdj,
	ETopologyMode_LineStripAdj,
	EToplogyMode_Count
} ETopologyMode;

typedef enum EPipelineRaytracingFlags { //rt.flags; a derived pipeline assumes EPipelineRaytracingFlags_Default
	EPipelineRaytracingFlags_SkipTriangles      = 1 << 0,
	EPipelineRaytracingFlags_SkipAABBs          = 1 << 1,
	EPipelineRaytracingFlags_Reserved2          = 1 << 2, //Reserved, free to reuse
	EPipelineRaytracingFlags_NoNullAnyHit       = 1 << 3, //Null shaders disallowed per stage (extra validation)
	EPipelineRaytracingFlags_NoNullClosestHit   = 1 << 4,
	EPipelineRaytracingFlags_NoNullMiss         = 1 << 5,
	EPipelineRaytracingFlags_NoNullIntersection = 1 << 6,
	EPipelineRaytracingFlags_AllowOpacityMicromapExt = 1 << 7, //Requires feature RayMicromapOpacity; opt in at creation
	EPipelineRaytracingFlags_Count              = 8,
	EPipelineRaytracingFlags_Default            = EPipelineRaytracingFlags_SkipAABBs,
	EPipelineRaytracingFlags_DefaultStrict      = EPipelineRaytracingFlags_SkipAABBs | EPipelineRaytracingFlags_NoNullClosestHit | EPipelineRaytracingFlags_NoNullMiss
} EPipelineRaytracingFlags;

typedef enum ECullMode {
	ECullMode_Back,
	ECullMode_None,
	ECullMode_Front,
	ECullMode_Count
} ECullMode;

typedef enum ERasterizerFlags {
	ERasterizerFlags_IsClockWise      = 1 << 0, //Winding order
	ERasterizerFlags_IsWireframeExt   = 1 << 1, //Fill mode; needs the wireframe extension
	ERasterizerFlags_EnableDepthClamp = 1 << 2,
	ERasterizerFlags_EnableDepthBias  = 1 << 3
} ERasterizerFlags;

typedef enum EDepthStencilFlags {
	EDepthStencilFlags_DepthTest     = 1 << 0,
	EDepthStencilFlags_DepthWriteBit = 1 << 1,  //Use DepthWrite instead
	EDepthStencilFlags_StencilTest   = 1 << 2,
	EDepthStencilFlags_DepthWrite    = EDepthStencilFlags_DepthTest | EDepthStencilFlags_DepthWriteBit
} EDepthStencilFlags;

//Owned by the shared graphics format vocabulary (formats/gfx_util/gfx_util.h), normative here:
typedef enum ECompareOp {
	ECompareOp_Gt, ECompareOp_Geq,
	ECompareOp_Eq, ECompareOp_Neq,
	ECompareOp_Leq, ECompareOp_Lt,
	ECompareOp_Always, ECompareOp_Never,
	ECompareOp_Count
} ECompareOp;

typedef enum EStencilOp {
	EStencilOp_Keep, EStencilOp_Zero,
	EStencilOp_Replace, EStencilOp_IncClamp,
	EStencilOp_DecClamp, EStencilOp_Invert,
	EStencilOp_IncWrap, EStencilOp_DecWrap,
	EStencilOp_Count
} EStencilOp;

typedef enum EBlend {
	EBlend_Zero, EBlend_One,
	EBlend_SrcColor, EBlend_InvSrcColor,
	EBlend_DstColor, EBlend_InvDstColor,
	EBlend_SrcAlpha, EBlend_InvSrcAlpha,
	EBlend_DstAlpha, EBlend_InvDstAlpha,
	EBlend_BlendFactor, EBlend_InvBlendFactor,
	EBlend_AlphaFactor, EBlend_InvAlphaFactor,
	EBlend_SrcAlphaSat,
	EBlend_Src1ColorExt, EBlend_Src1AlphaExt,   //Dual source; needs the dualSrcBlend feature
	EBlend_InvSrc1ColorExt, EBlend_InvSrc1AlphaExt,
	EBlend_Count
} EBlend;

typedef enum EBlendOp {
	EBlendOp_Add, EBlendOp_Subtract,
	EBlendOp_ReverseSubtract,
	EBlendOp_Min, EBlendOp_Max,
	EBlendOp_Count
} EBlendOp;

typedef enum EWriteMask {
	EWriteMask_R    = 1 << 0,
	EWriteMask_G    = 1 << 1,
	EWriteMask_B    = 1 << 2,
	EWriteMask_A    = 1 << 3,
	EWriteMask_All  = 0xF,
	EWriteMask_RGBA = 0xF,
	EWriteMask_RGB  = 0x7,
	EWriteMask_RG   = 0x3
} EWriteMask;

typedef enum ELogicOpExt {
	ELogicOpExt_Off, ELogicOpExt_Clear,
	ELogicOpExt_Set, ELogicOpExt_Copy,
	ELogicOpExt_CopyInvert, ELogicOpExt_None,
	ELogicOpExt_Invert, ELogicOpExt_And,
	ELogicOpExt_Nand, ELogicOpExt_Or,
	ELogicOpExt_Nor, ELogicOpExt_Xor,
	ELogicOpExt_Equiv, ELogicOpExt_AndReverse,
	ELogicOpExt_AndInvert, ELogicOpExt_OrReverse,
	ELogicOpExt_OrInvert,
	ELogicOpExt_Count
} ELogicOpExt;
```

`ETextureFormatId` (render target and vertex input formats) and `EDepthStencilFormat` (`None`, `D16`, `D32`, `D24S8Ext`, `D32S8X24Ext`) are the shared texture format enums from `types/container/texture_format.h`, used unchanged.

### Provenance

Every field a pipeline needs but a shader can't prove is recorded in `specializations[]` with where its value came from:

- **Derived**: proven by the shader's reflection (a vertex input's format, how many render targets the pixel stage writes).
- **Supplied**: the caller chose it.
- **Assumed**: nobody chose it, so a value was picked.

A pipeline is exact only when nothing was assumed, so a disassembly taken from a guessed pipeline is never mistaken for an exact one, and a tool can ask for exactly the fields that are missing. Fields are addressed by a stable path name (`rtv.format`, `blend.src[2]`, `msaa.minSampleShading`, `rt.maxRecursionDepth`, `layout.sampler.filter[0]`, ...) with a sub index for indexed ones; that single vocabulary serves the specialization report, a state template, sweep axes and a web form, so they can never disagree about what a field is called.

### Descriptor layout

`SPPipelineBase.layoutIndex` names one of the file's embedded [oiPL](oiPL.md) subfiles, or `U32_MAX` for the device's default layout, which the file never describes. The layout is part of what a shader compiles into, not just a validation gate: descriptor set pointers and push constants arrive in user SGPRs and consume the root signature's budget, so two layouts that both accept a shader can still produce different ISA. A layout shared by several pipelines is stored once and referenced by index, and one can be lifted out or dropped in whole, which is how a stored layout overrides a derived one: structure (new rows, sampler values) can't travel as per field supplies. Derivation fills the rows from what the stages' binaries reflect, skipping the registers the runtime owns, preferring the SPIR-V binding pair and falling back to DXIL; the row format itself, and its per row provenance tags, are oiPL's and specified in [oiPL.md](oiPL.md). The three F16 sampler fields print as the float value that overrides parse, not as their bit pattern.

### Sentinels & invariants

- `name == U32_MAX` means unnamed, and so does `shaderFile`/`entrypoint == U32_MAX` on a stage. All other string ids must be `< names.length`.
- `type < ESPPipelineType_Count`; `flags` may not contain `ESPPipelineFlag_Unsupported` bits; `stage < EGfxPipelineStage_Count`.
- `specializations[k].field < ESPField_Count` and `source < ESPFieldSource_Count`; `index` must be below the field's index count (1 for a field without indices).
- `stageStart + stageCount` and `specializationStart + specializationCount` must sit inside their pools; every pipeline has at least one stage.
- A graphics pipeline's `stateIndex < graphicsStateCount`, a ray tracing pipeline's `stateIndex < raytracingStateCount`. Compute ignores `stateIndex`.
- `layoutIndex` is `U32_MAX` or `< layoutCount`.
- `graphicsStates[k].renderTargetCount <= 8`.

### Validation

`SPFile_read` refuses:

- A misaligned (non 16-byte) offset, a wrong magic number, a version other than 1.1 or any `ESPFlag_Unsupported` bit.
- Header counts that don't match what the stored blend states and masks imply for `blendAttachmentCount`, `vertexBufferCount` and `vertexAttributeCount`.
- Any sentinel or invariant above that doesn't hold.
- A names oiDL that isn't a plain string list (compressed or encrypted), a string that isn't fully loaded, or a string of 32Ki characters or more.

`SPFile_validate` performs the structural checks that need no device, against the shader signatures a pipeline is built from: the pixel stage writing more render targets than the pipeline declares, a declared target with no format, depth state with no depth attachment, a vertex input the pipeline has no format for, half a tessellation pair or a control point count outside 1..32 (or set without tessellation), a render target count above 8, a blend target mask enabling targets that don't exist, blending enabled with nothing masked, sample shading outside 0..1, and a ray tracing pipeline that can't trace a single ray. Device validation (format support, sample counts, feature bits) stays with the backend, and so does enum bounds checking: the reader keeps every stored integer as is and the backend rejects a value it can't map, so a newer file's value never turns into a silently different one.

## Hashing & comparing

Pipelines, stages and specializations are appended in producer order. This deterministic ordering allows simple comparison and hashing and means threading SPFile generation is off limits.

Hashes are generated like following:

- FNV-1a64 is used (64-bit FNV-1a).
- First, every blend attachment no target reaches and every vertex attribute without a format is cleared in memory, so the hash describes bytes that survive a write.
- The seed is `flags & ~HideMagicNumber` FNVed as a single U64 (HideMagicNumber is a serialization detail and never influences the hash).
- The whole `pipelines[]` byte buffer is FNVed, then `stages[]`, then `specializations[]`, then `graphicsStates[]` (the runtime form), then `raytracingStates[]`.
- Every embedded layout is finalized and its own oiPL hash is FNVed as a single U64, in order, so a standalone oiPL and an embedded one can never disagree.
- Every string in the pool (in order) is FNVed by its bytes.

This hash is refreshed by `SPFile_finalize` (and on read). It can be used for quick comparison, for example to tell whether two stored pipelines describe the same state, and is only available at runtime.

## Changelog

1.1: Initial format specification (no shipped file predates it, so it evolves in place rather than versioning). Pipeline records with their common base (name, kind, stage range, specialization range, kind specific state index, layout index), the shared stage pool naming each stage's oiSH + entrypoint + source hash, the specialization pool recording every field a shader can't prove along with whether it was derived, supplied or assumed, and the kind specific graphics and ray tracing state pools. The graphics state covers `PipelineGraphicsInfo` completely, so a pipeline that supplies every reported field is whole rather than partial; the state structs and enums are declared here and aliased by the graphics layer. Blend state and the vertex layout exist as a `Stored` and a `Runtime` form, so a graphics state costs 64 bytes on disk against the 204 a pipeline binds. A derived ray tracing pipeline assumes `rt.flags = EPipelineRaytracingFlags_Default` (skip AABBs), the same default the graphics layer creates with. Carries each pipeline's descriptor layout as an embedded [oiPL](oiPL.md) subfile (`SPPipelineBase.layoutIndex` into `layouts[]`), whose own hash folds into the file hash.

# oiSP (Oxsomi Shader Pipeline)

*The oiSP format is an [oiXX format](oiXX.md), as such it inherits the properties from that such as compression, encryption and endianness (though enc/comp is not supported, since oiSP is most often packaged alongside an oiSH, or inside an oiCA/oiDL file).*

oiSP stores **pipelines**: the compute, graphics and ray tracing state that sits *around* a shader, together with which shader stages the pipeline is built from. It completes the shader family: [oiSH](oiSH.md) holds the compiled binaries and their backend reflection, [oiSR](oiSR.md) the frontend symbol AST, [oiSB](oiSB.md) the buffer layouts, and oiSP what a shader is actually *compiled into*.

It exists because a shader alone doesn't describe a pipeline. A pixel shader's signature proves it writes a `float4` to `SV_Target0`, but nothing in it says whether that target is `RGBA8` or `RGBA16f`, whether blending is on, how many samples the pipeline runs at, or how vertex locations pack into buffers. Those are pipeline decisions and they change the compiled ISA, so a disassembly is only reproducible if they're recorded. Storing them means the same oiSP plus the same oiSH gives the same pipeline, on a device or offline.

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

} SPHeader;

typedef struct SPPipelineBase { //What every pipeline has, whatever kind it is

	U32 name;                   //String id, U32_MAX if unnamed

	U8 type;                    //ESPPipelineType: Compute, Graphics, Raytracing
	U8 flags;                   //ESPPipelineFlag
	U8 stageStart, stageCount;  //Range into stages[]

	U32 specializationStart;    //Range into specializations[]

	U32 specializationCount;

	U32 stateIndex;             //Into graphicsStates[] or raytracingStates[]; unused by compute

} SPPipelineBase;

typedef struct SPStage {        //One stage of a pipeline

	U32 shaderFile;             //String id of the oiSH this stage came from, U32_MAX if unnamed

	U32 entrypoint;             //String id of the entry name

	U32 sourceHash;             //SHFile.sourceHash when the pipeline was made, 0 if unknown

	U8 stage;                   //ESHPipelineStage
	U8 padding[3];

} SPStage;

typedef struct SPSpecialization {   //One field that reflection couldn't prove

	U8 field;                       //ESPField
	U8 index;                       //Sub index for indexed fields (render target, vertex buffer)
	U8 source;                      //ESPFieldSource: Derived, Supplied, Assumed
	U8 padding;

	U32 value;                      //The value that would be used, in the field's own units

} SPSpecialization;

//Two of these have a runtime form and a stored form, because a pipeline binds more state than a file has reason to
//keep. Nothing is lost between them: what the stored form leaves out is state no pipeline can reach.
//The runtime forms are further down; they're what the graphics layer aliases and what a pipeline is created from.

typedef struct SPBlendStateStored { //What the file holds; SPBlendStateRuntime is what a pipeline binds

	Bool enable;
	Bool allowIndependentBlend;     //0 = every target uses attachments[0]
	U8 renderTargetMask;            //Bit per render target
	U8 logicOpExt;                  //ELogicOpExt; replaces blending when set

	U8 writeMask[8];                //EWriteMask

	//The attachments go to blendAttachments[], and only the ones a pipeline can reach:
	//N = enable ? (allowIndependentBlend ? popcount(renderTargetMask) : 1) : 0

} SPBlendStateStored;

typedef struct SPVertexLayoutStored {   //What the file holds; SPVertexLayoutRuntime is what a pipeline binds

	//A vertex layout is sparse by index rather than by count (location 3 can be filled while 0 to 2 aren't), so which
	//entries were stored travels as a mask. They go to vertexBuffers[] and vertexAttributes[].

	U16 bufferMask;                     //Bit per vertex buffer that carries a stride or an instance rate
	U16 attributeMask;                  //Bit per input location that carries a format

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

//These are identical on both sides, so they're stored exactly as a pipeline binds them.

typedef struct SPBlendAttachment {      //How one render target combines with what's already there

	U8 srcBlend, dstBlend;              //EBlend

	U8 srcBlendAlpha, dstBlendAlpha;    //EBlend

	U8 blendOp, blendOpAlpha;           //EBlendOp

} SPBlendAttachment;

//And these are the runtime forms: what a pipeline is created from, and what the graphics layer aliases.
//A file never holds them; the reader expands the stored forms into these and the writer folds them back.

typedef struct SPBlendStateRuntime {    //What a pipeline binds; the file stores SPBlendStateStored instead

	Bool enable;
	Bool allowIndependentBlend;         //0 = every target uses attachments[0]
	U8 renderTargetMask;                //Bit per render target
	U8 logicOpExt;                      //ELogicOpExt; replaces blending when set

	U8 writeMask[8];                    //EWriteMask

	SPBlendAttachment attachments[8];

} SPBlendStateRuntime;

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

typedef struct SPVertexAttribute {  //One vertex input location

	U16 offset11;                   //Byte offset within its buffer, 11 bits
	U8 bufferId4;                   //Which buffer it fetches from, 4 bits
	U8 format;                      //ETextureFormatId; must not be compressed

} SPVertexAttribute;

typedef struct SPVertexLayoutRuntime {      //What a pipeline binds; the file stores SPVertexLayoutStored instead

	U16 bufferStrides12_isInstance1[16];    //Per buffer: stride in bits 0-11, per instance in bit 12

	SPVertexAttribute attributes[16];       //Per shader input location

} SPVertexLayoutRuntime;

typedef struct SPInputAssembler {

	U8 topologyMode;            //ETopologyMode
	U8 padding[3];

	U32 patchControlPoints;     //1..32 with tessellation, else 0

	SPVertexLayoutRuntime vertexLayout;

} SPInputAssembler;

typedef struct SPGraphicsState {        //What a pipeline is created from; the file stores SPGraphicsStateStored

	U8 msaa;                            //EMSAASamples
	U8 renderTargetCount;               //0..8
	U8 depthFormat;                     //EDepthStencilFormat; 0 = no depth attachment
	U8 padding;

	F32 msaaMinSampleShading;           //0 = off, else the 0..1 sample fraction

	U8 renderTargetFormats[8];          //ETextureFormatId

	SPBlendStateRuntime blend;          //Its attachments trail the file, not this struct
	SPDepthState depth;
	SPRasterizerState rasterizer;
	SPInputAssembler inputAssembler;    //Its vertex layout trails the file too

} SPGraphicsState;

typedef struct SPRaytracingState {

	U8 maxRecursionDepth;
	U8 raytracingFlags;         //EPipelineRaytracingFlags
	U16 padding;

} SPRaytracingState;

typedef enum ESPPipelineType {
	ESPPipelineType_Compute,
	ESPPipelineType_Graphics,
	ESPPipelineType_Raytracing,
	ESPPipelineType_Count
} ESPPipelineType;

typedef enum ESPFieldSource {   //Where a field's value came from
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
//Indexed fields address an array element through SPSpecialization.index; the rest must have index 0.

typedef enum ESPField {

	ESPField_RenderTargetFormat,    //Indexed by render target; ETextureFormatId
	ESPField_RenderTargetCount,
	ESPField_BlendEnable,
	ESPField_BlendIndependent,
	ESPField_BlendTargetMask,
	ESPField_BlendLogicOp,          //ELogicOpExt
	ESPField_BlendWriteMask,        //Indexed; EWriteMask
	ESPField_BlendSrc,              //Indexed; EBlend
	ESPField_BlendDst,              //Indexed; EBlend
	ESPField_BlendSrcAlpha,         //Indexed; EBlend
	ESPField_BlendDstAlpha,         //Indexed; EBlend
	ESPField_BlendOp,               //Indexed; EBlendOp
	ESPField_BlendOpAlpha,          //Indexed; EBlendOp

	ESPField_DepthFormat,           //EDepthStencilFormat
	ESPField_DepthStencilFlags,     //EDepthStencilFlags
	ESPField_DepthCompare,          //ECompareOp
	ESPField_StencilCompare,        //ECompareOp
	ESPField_StencilFail,           //EStencilOp
	ESPField_StencilPass,           //EStencilOp
	ESPField_StencilDepthFail,      //EStencilOp
	ESPField_StencilWriteMask,
	ESPField_StencilReadMask,

	ESPField_CullMode,              //ECullMode
	ESPField_RasterizerFlags,       //ERasterizerFlags
	ESPField_DepthBiasConstant,     //I32 stored in the U32
	ESPField_DepthBiasClamp,        //F32 bits stored in the U32
	ESPField_DepthBiasSlope,        //F32 bits stored in the U32

	ESPField_Msaa,                  //EMSAASamples
	ESPField_MsaaMinSampleShading,  //F32 bits stored in the U32
	ESPField_TopologyMode,          //ETopologyMode
	ESPField_PatchControlPoints,

	ESPField_VertexBufferStride,    //Indexed by vertex buffer
	ESPField_VertexBufferRate,      //Indexed by vertex buffer

	ESPField_MaxRecursionDepth,
	ESPField_RaytracingFlags,       //EPipelineRaytracingFlags

	ESPField_Count
} ESPField;
```

Enum-valued fields are stored as integers naming the enum they mirror in a comment, the same way oiSH stores an `ESBType` as `U8`. The state enums (`EBlend`, `ECompareOp`, `EPipelineRaytracingFlags`, ...) are oiSP's own, declared in `sp_state.h` below; only `ETextureFormatId` and `EDepthStencilFormat` come from the texture format tables, which carry no graphics dependency either, so the format stays readable without a device.

`SPRasterizerState`, `SPDepthState`, `SPBlendAttachment`, `SPBlendStateRuntime`, `SPVertexAttribute` and
`SPVertexLayoutRuntime` are not merely byte compatible with the state a graphics pipeline binds, they are that state:
they're declared in `oiSP/sp_state.h` and the graphics layer aliases them, so there is one definition rather than two
that could drift. Lowering a stored pipeline is therefore a struct copy, and a graphics pipeline's whole state is
204 bytes.

Because those structs are shared, a field added on the graphics side would silently change the on disk layout, so every
size above is pinned with a `static_assert` in `sp_file.h`. Adding a field is a format change and needs a version bump.

Blend state and the vertex layout are the two that exist twice, as a `Stored` struct and a `Runtime` one, because most
of what a pipeline binds is state no pipeline can read back. Blending reaches an attachment only when it's on, and then
only through `attachments[0]` (independent blending off) or the targets `renderTargetMask` selects (independent
blending on), so `SPBlendStateStored` keeps the switches and sends the attachments to their own section.

A vertex layout is sparse the same way, but by index rather than by count: a buffer is real once its packed `U16` is
non-zero and an input location once it has a format, and since location 3 can be filled while 0 to 2 aren't,
`SPVertexLayoutStored` is a pair of masks rather than a count.

So `SPGraphicsStateStored` is 64 bytes against `SPGraphicsState`'s 204, and a trivial vertex shader with one buffer and
one attribute costs 64 + 2 + 4. `SPGraphicsState_store` and `SPGraphicsStateStored_expand` are the whole of the
conversion; a reader never sees a runtime struct with holes in it.

None of the per-state counts are stored, since the blend state and the two masks already imply them. The header's
`blendAttachmentCount`, `vertexBufferCount` and `vertexAttributeCount` are the totals, and a reader that derives
different ones rejects the file. On read an entry goes back to the index it was written for rather than the slot it was
stored in, and every index that stores nothing reads as zero. `SPFile_finalize` clears those same entries in memory, so
the content hash describes bytes that survive a write.

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

	//Then the entries those states actually use, each in its own section, per graphics state and in index order.
	//The three counts in the header are the totals; a reader derives them itself and rejects the file if they differ.

	blendAttachments[header.blendAttachmentCount]:
		for i < header.graphicsStateCount:
			for j < 8 where blend stores target j:
				SPBlendAttachment attachment

	vertexBuffers[header.vertexBufferCount]:
		for i < header.graphicsStateCount:
			for j < 16 where bufferMask has bit j:
				U16 bufferStride12_isInstance1

	vertexAttributes[header.vertexAttributeCount]:
		for i < header.graphicsStateCount:
			for j < 16 where attributeMask has bit j:
				SPVertexAttribute attribute

	U8[N] pad;              //Padding to align to 16-byte

	DLFile names;           //Pipeline names, shader file names and entrypoint names (see oiDL.md)
}
```

The types are Oxsomi types; `U<X>`: x-bit unsigned integer, `I<X>` x-bit signed integer, `F<X>` x-bit float.

The magic number can only be absent if embedded in another file.

### State enums

A pipeline's integer fields carry values from the enums below. They're declared in `oiSP/sp_state.h` alongside the
structs that store them, because a field kept as a raw integer means nothing without the set it came from, and a reader
has to make sense of a file with no device present. The graphics layer includes them from here rather than declaring
its own. `ESHPipelineStage` is the one exception: it belongs to [oiSH](oiSH.md) and is specified there.

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
	EPipelineRaytracingFlags_AllowMotionBlurExt = 1 << 2, //Requires feature RayMotionBlur
	EPipelineRaytracingFlags_NoNullAnyHit       = 1 << 3, //Null shaders disallowed per stage (extra validation)
	EPipelineRaytracingFlags_NoNullClosestHit   = 1 << 4,
	EPipelineRaytracingFlags_NoNullMiss         = 1 << 5,
	EPipelineRaytracingFlags_NoNullIntersection = 1 << 6,
	EPipelineRaytracingFlags_Count              = 7,
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
	EDepthStencilFlags_DepthWrite    = DepthTest | DepthWriteBit
} EDepthStencilFlags;

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
	EBlend_Src1ColorExt, EBlend_Src1AlphaExt,   //Dual source; needs the extension
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

typedef enum EPipelineRaytracingFlags {
	EPipelineRaytracingFlags_SkipTriangles      = 1 << 0,
	EPipelineRaytracingFlags_SkipAABBs          = 1 << 1,
	EPipelineRaytracingFlags_AllowMotionBlurExt = 1 << 2,   //Requires the RayMotionBlur feature
	EPipelineRaytracingFlags_NoNullAnyHit       = 1 << 3,
	EPipelineRaytracingFlags_NoNullClosestHit   = 1 << 4,
	EPipelineRaytracingFlags_NoNullMiss         = 1 << 5,
	EPipelineRaytracingFlags_NoNullIntersection = 1 << 6,
	EPipelineRaytracingFlags_Count              = 7,
	EPipelineRaytracingFlags_Default            = SkipAABBs
} EPipelineRaytracingFlags;
```

`ETextureFormatId` (render target and vertex input formats) and `EDepthStencilFormat` (`None`, `D16`, `D32`, `D24S8Ext`,
`D32S8X24Ext`) are the shared texture format enums from `types/container/texture_format.h`, used unchanged.

### Common ancestor

Every pipeline is an `SPPipelineBase` regardless of kind, and the kind specific state hangs off `stateIndex`. Anything that walks stages, names or provenance (a state template, a sweep, a form, an inspector) takes the base and never learns which kind it got. It also keeps the packing honest: a compute pipeline is 20 bytes rather than carrying a graphics payload it never uses, and validation shared by all three kinds is written once against the base.

### Provenance

Every field a pipeline needs but a shader can't prove is recorded in `specializations[]` with where its value came from:

- **Derived**: proven by the shader's reflection (a vertex input's format, how many render targets the pixel stage writes).
- **Supplied**: the caller chose it.
- **Assumed**: nobody chose it, so a value was picked.

Serializing this is the point of keeping it. A pipeline assembled from guesses still says which of its fields nobody chose, so a disassembly taken from it is never mistaken for an exact one, and a tool can ask for exactly the fields that are missing. Because the field set mirrors `PipelineGraphicsInfo` and `PipelineRaytracingInfo` completely, supplying everything a pipeline reports yields a whole pipeline rather than a partially specified one.

Fields are addressed by a stable path name (`rtv.format`, `blend.op`, `msaa.minSampleShading`, `rt.maxRecursionDepth`, ...) with a sub-index for indexed ones. That single vocabulary serves the specialization report, a state template, sweep axes and a web form, so they can never disagree about what a field is called or which values are legal.

### Stages

Stages are stored by **name**, not by an index into a list that only existed at build time: the oiSH the stage came from, the entrypoint within it, and that oiSH's `sourceHash` at the time. A loader resolves the name and can tell from the hash when the shader has moved on. Stages of one pipeline may come from several oiSH files, which is the same shape a pipeline is created from at runtime.

Compute, graphics and ray tracing stages sitting in one oiSH are *separate* pipelines; a pipeline names exactly the stages it uses, and mixing kinds in one pipeline is rejected.

### Sentinels & invariants

- `name == U32_MAX` means unnamed, and so does `shaderFile`/`entrypoint == U32_MAX` on a stage. All other string ids must be `< strings.length`.
- `type < ESPPipelineType_Count`; `flags` may not contain `ESPPipelineFlag_Unsupported` bits; `stage < ESHPipelineStage_Count`.
- `specializations[k].field < ESPField_Count` and `source < ESPFieldSource_Count`. A non-zero `index` is only legal on a field `ESPField_isIndexed` reports as indexed.
- `stageStart + stageCount <= stageCount` of the file, and `specializationStart + specializationCount <= specializationCount`; every pipeline has at least one stage.
- A graphics pipeline's `stateIndex < graphicsStateCount`, a ray tracing pipeline's `stateIndex < raytracingStateCount`. Compute ignores `stateIndex`.
- `graphicsStates[k].renderTargetCount <= 8`.

### Validation

`SPFile_validate` performs the checks that need **no device**, so a mismatch names itself long before a driver returns an opaque error: the pixel stage writing more render targets than the pipeline declares, a declared target with no format, depth state with no depth attachment, a vertex input the pipeline has no format for, half a tessellation pair or a control point count outside 1..32 (or set without tessellation), a render target count above 8, a blend target mask enabling targets that don't exist, blending enabled with nothing masked, sample shading outside 0..1, and a ray tracing pipeline that can't trace a single ray.

Device-dependent validation stays with the backend, since it needs capabilities rather than the pipeline alone: whether a format is supported as a vertex input or color attachment, which MSAA counts the device offers, and whether features like dual-source blending, wireframe, depth bias or geometry shaders exist. Enum *bounds* checking stays there too: the reader keeps every stored integer as is and the backend rejects a value it can't map, so a newer file's value never turns into a silently different one here.

## Hashing & comparing

Pipelines, stages and specializations are appended in producer order. This deterministic ordering allows simple comparison and hashing and means threading SPFile generation is off limits.

Hashes are generated like following:

- FNV-1a64 is used (64-bit FNV-1a).
- The seed is `flags & ~HideMagicNumber` FNVed (HideMagicNumber is a serialization detail and never influences the hash).
- The whole `pipelines[]` byte buffer is FNVed, then `stages[]`, then `specializations[]`, then `graphicsStates[]`, then `raytracingStates[]`.
- Every string in the pool (in order) is FNVed by its bytes.

This hash is refreshed by `SPFile_finalize` (and on read). It can be used for quick comparison, for example to tell whether two stored pipelines describe the same state, and is only available at runtime.

## Relationship to the graphics layer

The state goes the other way round from the rest of the format: rather than oiSP describing what the graphics layer
holds, `oiSP/sp_state.h` declares it and `graphics/generic/pipeline_structs.h` aliases it, so `Rasterizer`,
`DepthStencilState`, `BlendState`, `BlendStateAttachment`, `VertexAttribute` and `VertexBindingLayout` are
`SPRasterizerState`, `SPDepthState`, `SPBlendStateRuntime`, `SPBlendAttachment`, `SPVertexAttribute` and
`SPVertexLayoutRuntime` under their graphics names, and the enums above are the same declarations both sides read. None of it needs a
device, which is what lets the format keep its side of the bargain.

`BlendState` and `VertexBindingLayout` alias the `Runtime` forms specifically, since those are what a pipeline binds.
The `Stored` forms never leave the format: the graphics layer has no reason to know a file drops what it can't reach.

Lowering has no compute counterpart to `SPFile_toGraphicsInfo` and `SPFile_toRaytracingInfo` because compute has no
state to lower. `SPFile_toComputeStage` stands in its place, yielding the single stage a compute pipeline is allowed to
bind; there's no `fromComputeInfo` either, since deriving a compute pipeline is already exact.

Everything above that boundary is still the graphics layer's: it owns `PipelineGraphicsInfo`, so it provides the
interop, lowering an oiSP pipeline into a create-info and dumping a live pipeline back into an oiSP. Because the state
is shared, lowering copies the structs whole and only the fields oiSP stores differently (render target formats, the
topology and patch count the input assembler carries) are moved across by hand.

The result is that a file loads in tooling that has no device (a web inspector, an offline ISA run) and a renderer can
round-trip its own pipelines through it, without two definitions of the same state drifting apart.

## Changelog

1.1: Initial format specification (no shipped file predates it, so it evolves in place rather than versioning). Carries the pipeline records with their common base (name, kind, stage range, specialization range, kind specific state index), the shared stage pool naming each stage's oiSH + entrypoint + source hash, the specialization pool recording every field a shader can't prove along with whether it was derived, supplied or assumed, and the kind specific graphics and ray tracing state pools. The graphics state covers `PipelineGraphicsInfo` completely, including the rasterizer, full depth/stencil, per-target blend factors and write masks, sample-rate shading and tessellation control points, so a pipeline that supplies every reported field is whole rather than partial; the state structs and the enums they store are declared here and aliased by the graphics layer rather than duplicated. Blend state and the vertex layout exist as a `Stored` and a `Runtime` form, since a blend state can only reach the attachments blending selects and a vertex layout only the buffers and input locations it fills in, so a graphics state costs 64 bytes on disk against the 204 a pipeline binds. A derived ray tracing pipeline assumes `rt.flags = EPipelineRaytracingFlags_Default` (skip AABBs), the same default the graphics layer creates with.

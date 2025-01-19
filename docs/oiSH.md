# oiSH (Oxsomi SHader)

*The oiSH format is an [oiXX format](oiXX.md), as such it inherits the properties from that such as compression, encryption and endianness (though enc/comp is not supported, since oiSH is most often packaged inside of an oiCA/oiDL file).*

**NOTE: oiSH (1.2) is the successor of the original oiSH (0.1) from OxC1. This isn't the same format anymore. oiSH (0.1) lacked a lot of important features and is deprecated.**

oSH is a single shader represented in a single or multiple binary/text format(s). The only definition is that the shader type must match one (or multiple) of the following:

- SPIRV
- DXIL
- (**unsupported for now**): MetalIR (Metal IR)
- (**unsupported for now**): WGSL

It includes the shader binary/text along with the extensions it uses, so it can easily be validated if the shader binary can be loaded by the runtime.

Just like any oiXX file it's made with the following things in mind:

- Ease of read + write.
- An easy spec.
- Good security for parsing + writing.
- Support for various graphics APIs without transpiling.
  - Though it can also target 1 single graphics API to avoid extra storage (at OxC3 package time).

Finally, this would become a part of an oiSC file (Oxsomi Shader Cache). Which includes a list of all binaries and the pipeline info to create it. A pipeline can also be marked as dynamic, which means that certain properties are possible to determine at runtime (e.g. the pipeline will have multiple instances). This can be handy if the same pipeline might be used with different types of render texture formats, depth formats and other things that might only be possible to determine at runtime. The base pipeline can be made with the defaults that were filled in and new instances can be created more efficiently from this base pipeline in some cases.

## File format spec

```c
typedef struct SHHeader {		//4-byte aligned

	U32 magicNumber;			//oiSH (0x4853696F); optional if it's part of an oiSC.

    U32 compilerVersion;

	U32 hash;					//CRC32C of contents starting at uniqueUniforms

    U32 sourceHash;				//CRC32C of source(s), for determining if it's dirty. See CRC32C section.

	U16 uniqueUniforms;
	U8 version;					//major.minor (%10 = minor, /10 = major (+1 to get real major)) at least 1
    U8 sizeTypes;				//Every 2 bits size type of spirv, dxil

    U16 binaryCount;			//How many unique binaries are stored
    U16 stageCount;				//How many stages reference into the binaries

    U16 includeFileCount;
    U16 semanticCount;

    U16 arrayDimCount;
    U16 registerNameCount;

} SHHeader;

//Loosely maps to EPipelineStage in OxC3 graphics

typedef enum ESHPipelineStage {

	ESHPipelineStage_Vertex,
	ESHPipelineStage_Pixel,
	ESHPipelineStage_Compute,
	ESHPipelineStage_GeometryExt,		//GeometryShader extension is required
	ESHPipelineStage_Hull,
	EPSHipelineStage_Domain,

	//RayPipeline extension is required

	ESHPipelineStage_RaygenExt,
	ESHPipelineStage_CallableExt,
	ESHPipelineStage_MissExt,
	ESHPipelineStage_ClosestHitExt,
	ESHPipelineStage_AnyHitExt,
	ESHPipelineStage_IntersectionExt,

    //MeshShader extension is required

	ESHPipelineStage_MeshExt,
	ESHPipelineStage_TaskExt,

	//WorkGraph extension is required

	ESHPipelineStage_WorkgraphExt

} ESHPipelineStage;

typedef struct EntryInfoFixedSize {
    U8 pipelineStage;			//ESHPipelineStage
	U8 binaryCount;				//How many binaries this entrypoint references
} EntryInfoFixedSize;

typedef enum ESHBinaryFlags {

	ESHBinaryFlags_None 					= 0,

	//What type of binaries it includes
	//Must be one at least

	ESHBinaryFlags_HasSPIRV					= 1 << 0,
	ESHBinaryFlags_HasDXIL					= 1 << 1,

	//Reserved
	//ESHBinaryFlags_HasMetalIR				= 1 << 2,
	//ESHBinaryFlags_HasWGSL				= 1 << 3

	ESHBinaryFlags_HasShaderAnnotation		= 1 << 4,

	ESHBinaryFlags_HasBinary				= ESHBinaryFlags_HasSPIRV | ESHBinaryFlags_HasDXIL,
	//ESHBinaryFlags_HasText				= ESHBinaryFlags_HasMSL | ESHBinaryFlags_HasWGSL
	ESHBinaryFlags_HasSource				= ESHBinaryFlags_HasBinary // | ESHBinaryFlags_HasText

} ESHBinaryFlags;

typedef enum ESHExtension {

    ESHExtension_None						= 0,

	//These losely map to EGraphicsDataTypes in OxC3 graphics

	ESHExtension_F64						= 1 << 0,
	ESHExtension_I64						= 1 << 1,
	ESHExtension_16BitTypes					= 1 << 2,		//I16, F16

	ESHExtension_AtomicI64					= 1 << 3,
	ESHExtension_AtomicF32					= 1 << 4,
	ESHExtension_AtomicF64					= 1 << 5,

	//Some of them are present in EGraphicsFeatures in OxC3 graphics

	ESHExtension_SubgroupArithmetic			= 1 << 6,
	ESHExtension_SubgroupShuffle			= 1 << 7,

	ESHExtension_RayQuery					= 1 << 8,
	ESHExtension_RayMicromapOpacity			= 1 << 9,
	ESHExtension_RayMicromapDisplacement	= 1 << 10,
	ESHExtension_RayMotionBlur				= 1 << 11,
	ESHExtension_RayReorder					= 1 << 12,

	ESHExtension_Multiview					= 1 << 13,
	ESHExtension_ComputeDeriv				= 1 << 14,

	ESHExtension_PAQ						= 1 << 15,		//Payload access qualifiers

    ESHExtension_MeshTaskTexDeriv			= 1 << 16,

	ESHExtension_WriteMSTexture				= 1 << 17,

	ESHExtension_Bindless					= 1 << 18,
	ESHExtension_UnboundArraySize			= 1 << 19

} ESHExtension;

//Maps to EGraphicsVendorId
typedef enum ESHVendor {
	ESHVendor_NV,
	ESHVendor_AMD,
	ESHVendor_ARM,
	ESHVendor_QCOM,
	ESHVendor_INTC,
	ESHVendor_IMGT,
	ESHVendor_MSFT,
	ESHVendor_APPL,
	ESHVendor_SMSG,
	ESHVendor_HWEI,
	ESHVendor_Count
} ESHVendor;

typedef struct BinaryInfoFixedSize {

	U8 shaderModel;				//U4 major, minor
	U8 entrypointType;			//See entrypointType section
	U16 entrypoint;				//U16_MAX if library, otherwise index into stageNames

	U16 vendorMask;				//Bitset of ESHVendor
	U8 uniformCount;
	U8 binaryFlags;				//ESHBinaryFlags

	ESHExtension extensions;	//&~ dormantExt = used extensions, this is what the shader was compiled with

	ESHExtension dormantExt;	//Dormant extensions (not detected in final executable)

	U16 registerCount;
	U16 padding;

} BinaryInfoFixedSize;

typedef enum ESHRegisterType {

	ESHRegisterType_Sampler,

	ESHRegisterType_ConstantBuffer,					//UBO or CBuffer
	ESHRegisterType_ByteAddressBuffer,
	ESHRegisterType_StructuredBuffer,
	ESHRegisterType_StructuredBufferAtomic,			//SBuffer + atomic counter
	ESHRegisterType_StorageBuffer,
	ESHRegisterType_StorageBufferAtomic,
	ESHRegisterType_AccelerationStructure,

	ESHRegisterType_Texture1D,
	ESHRegisterType_Texture2D,
	ESHRegisterType_Texture3D,
	ESHRegisterType_TextureCube,
	ESHRegisterType_Texture2DMS,
	ESHRegisterType_SubpassInput,

	ESHRegisterType_Count,

	ESHRegisterType_TypeMask			= 0xF,
	ESHRegisterType_IsArray				= 1 << 4,	//Only valid on textures
	ESHRegisterType_IsCombinedSampler	= 1 << 5,	//^

	//Invalid on samplers, AS and CBuffer
	//Required on append/consume buffer
	//Valid on everything else (textures and various buffers)
	ESHRegisterType_IsWrite				= 1 << 6

} ESHRegisterType;

//Consists of primitive type (lower 4-bit) and component count (higher 2-bit)
typedef enum ESHTexturePrimitive {

	ESHTexturePrimitive_UInt,
	ESHTexturePrimitive_SInt,
	ESHTexturePrimitive_UNorm,
	ESHTexturePrimitive_SNorm,
	ESHTexturePrimitive_Float,
	ESHTexturePrimitive_Double,
	ESHTexturePrimitive_Count,

	ESHTexturePrimitive_TypeMask	= 0x0F,

	ESHTexturePrimitive_Component1	= 0x00,		//R
	ESHTexturePrimitive_Component2	= 0x10,		//RG
	ESHTexturePrimitive_Component3	= 0x20,		//RGB
	ESHTexturePrimitive_Component4	= 0x30,		//RGBA

	ESHTexturePrimitive_Unused		= 0xC0

} ESHTexturePrimitive;

typedef struct SHBinding {
	U32 space, binding;
} SHBinding;

//U32_MAX for both space and binding indicates 'not present'
typedef struct SHBindings {
	SHBinding arr[ESHBinaryType_Count];
} SHBindings;

typedef struct SHTextureFormat {	//Primitive is set for DXIL always and formatId is only for SPIRV (only when RW)
	U8 primitive;					//Optional for readonly registers: ESHTexturePrimitive must match format approximately
	U8 formatId;					//Optional for write registers: ETextureFormatId Must match formatPrimitive and uncompressed
} SHTextureFormat;

typedef struct SHRegister {

	SHBindings bindings;

	U8 registerType;				//ESHRegisterType
	U8 isUsedFlag;					//Per ESHBinaryType if the register is used

	union {
		U16 padding;				//Used for samplers, (RW)BAB or AS (should be 0)
		U16 shaderBufferId;			//Used only at serialization (Buffer registers only, ex )
		U16 inputAttachmentId;		//U16_MAX indicates "nothing", otherwise <7, only valid for SubpassInput
		SHTextureFormat texture;	//Read/write textures
	};

	U16 arrayId;					//Used at serialization time only, can't be used on subpass inputs
	U16 nameId;

} SHRegister;

//Final file format; please manually parse the members.
//Verify if everything's in bounds.
//Verify if SHFile includes any invalid data.

SHFile {

    SHHeader header;

    //No magic number, no encryption/compression/SHA256 (see oiDL.md).
    //strings[len - semanticCount,  len] contains semantics for inputs/outputs.
    //strings[^ - stageCount,       ^ - semanticCount] contains entrypoint names.
    //strings[^ - includeFileCount, ^ - stageCount] contains (relative) include names.
    //strings[^ - stageCount, 		^ - includeFileCount] contains unique register names.
    //strings[0,                    ^ - registerNameCount] contains uniform values & names and register names.
    DLFile strings;

    //No magic number, no encryption/compression/SHA256 (see oiDL.md).
    //Each entry includes a oiSB file (see oiSB.md) that describes the shader buffer.
    //Each shader buffer has to be referenced by a register.
    //Each oiSB is a subfile, so doesn't preserve oiSB magic number.
    DLFile shaderBuffers;

    BinaryInfoFixedSize binaryInfos[binaryCount];
    EntryInfoFixedSize pipelineStages[stageCount];
    U32 includeFileHashes[includeFileCount];
    U8 arrayDims[arrayDimCount];		//Max of 32 per array dim

    for i < arrayDimCount:
	    U32 arraySizes[arrayDims[i]];

    for i < binaryCount:

    	U16 uniformNames[binaryInfos[i].uniformCount];	//offset to strings[0]
    	U16 uniformValues[binaryInfos[i].uniformCount];	//^ [uniformCount]

    	SHRegister registers[binaryInfos[i].registerCount];	//Name starts after all uniform names & values

        if binary[i] has SPIRV:
            EXXDataSizeType<spirvType> spirvLength;

        if binary[i] has DXIL:
            EXXDataSizeType<dxilType> dxilLength;

        foreach binary with [ spirvLength, dxilLength ]:
            U8[binary.size] data;

    for pipelineStages[i]	//Entrypoint name is stored at [strings.len - stageCount]

	    if is graphics:

    		U8 inputsAvailAndHasSemantics;		//Upper bit: has semantics
    		U8 outputsAvail;

		    //Each element: [ ESBStride as U2, ESBPrimitive as U2, ESBVector as U2 ]
    		U8 inputs[inputsAvail];
    		U8 outputs[outputsAvail];

    		//If shader source supports it, stores the index of the semantic in strings[].
    		//But only for valid inputs/outputs (gaps not stored).
    		//U4 semanticId, U4 semanticName.
    		//semanticName of 0 is reserved as TEXCOORD or SV_TARGET (pixel shader output only).
    		//This leaves up to 15 unique semantics (input has a unique one and output too).
    		if(hasSemantics)
                U8 semanticCounts;						//U4 input, output: Unique semantic counts
     			U8 inputSemantics[validInputs];			//Starting at: semantics[0]
     			U8 outputSemantics[validOutputs];		//Starting at: semantics[uniqueInputSemantics]

	    if compute, mesh, task or workgraph:
		    U16x4 groups;
               	groups.w = waveSize: U4[4] required, min, max, recommended
                    Where the U4: 0 = None, 3-8 = 4-128 (log2), else invalid
                    groups.w should be 0 for mesh or task shader

    	if closestHit, anyHit or intersection:
    	    U8 intersectionSize;

	    if miss,closestHit,anyHit or intersection
   	 		U8 payloadSize;

    	U16 binaryIds[pipelineStages[i].binaryCount];
}
```

The types are Oxsomi types; `U<X>`: x-bit unsigned integer, `I<X>` x-bit signed integer.

The magic number in the header can only be absent if embedded in another file. An example is when embedded in an oiSC file.

## CRC32C

CRC32C hashes are used for the source and include directories to see if they're dirty. CRC32C first checks for \r and removes it. This is because Windows uses \r\n and Unix/OSX use \n. Windows can allow either, but will sometimes pick \r\n and sometimes \n. To mitigate this triggering random recompiles, even though the real source isn't dirty. CRC32C is a variation of CRC32 optimized for performance, since there is integrated hardware support for it.

## entrypointType

Entrypoint type clarifies what type of entrypoint was compiled as (ESHPipelineStage). If compiled as a library (e.g. raytracing, workgraphs, etc.), this value should only be looked at from the user perspective if there are duplicates. This is because the entrypointType only has meaning if binary was compiled for a specific target (using [stage("")] for example).

If this is not the case, then the compiler can decide how to combine entrypoints. It will be allowed for raygen, miss and hit shaders to be compiled in a single go, and for the stage type to be raygen for all of them. But if the stage is determined to need a specialized compile, then it can be combined with only the stages it is compatible with (for example; if pixel and vertex shaders have different internal defines or compiler settings, then they can still be separated if need be).

## Includes

All includes must have reproducible hashes (regardless of OS) as noted in the CRC32C section. Timestamps aren't included, as this would not make binaries reproducible. Which might generate issues with source control if someone decided to check these files in. The includes should also be reproducible even from other PCs. This means that the include names include relative paths (relative to the source file) rather than absolute. This way, recompiling the same binary will always give the same result, even with different check out directories and different time. For consistency, the include paths should also be sorted (ascending); this will ensure reproducibility.

## Language spec

The following define the requirements of binaries embedded in oiSH files.

- All capabilities have to match the extensions known by oiSH, otherwise it is considered to be an incompatible binary and undefined behavior is imminent if produced by a non standard oiSH writer. As an example; if RayReorder is used in a SPIRV shader, it must be present as an extension for that binary too.
- Payload + intersection size must stay within <=128 and <=32 bytes respectively.

### SPIRV spec

- Main entrypoint should be called "main" unless it's compiled as a lib file (raytracing, workgraphs, etc.).
- Capabilities:
  - Always supported:
    - Shader
    - Matrix
    - AtomicStorage
    - UniformTexelBufferArrayDynamicIndexing
    - StorageTexelBufferArrayDynamicIndexing
    - UniformBufferArrayNonUniformIndexing
    - SampledImageArrayNonUniformIndexing
    - StorageBufferArrayNonUniformIndexing
    - StorageImageArrayNonUniformIndexing
    - UniformTexelBufferArrayNonUniformIndexing
    - StorageTexelBufferArrayNonUniformIndexing
    - UniformBufferArrayDynamicIndexing
    - SampledImageArrayDynamicIndexing
    - StorageBufferArrayDynamicIndexing
    - StorageImageArrayDynamicIndexing
    - InputAttachment
    - MinLod
    - ShaderNonUniform
    - RuntimeDescriptorArray
    - StorageImageExtendedFormats
    - ImageQuery
    - DerivativeControl
    - SampledCubeArray
    - Sampled1D
    - Image1D
    - ImageCubeArray
  - Anything to do with kernels (OpenCL) is unsupported.
  - Int64Atomics as I64 | AtomicI64.
  - Int64 as I64.
  - Float64 as F64.
  - Float16 and Int16 as 16BitTypes.
    - Also:
    - StorageBuffer16BitAccess
    - StorageUniform16
    - StoragePushConstant16
    - StorageInputOutput16
  - Multiview: MultiView
  - SubgroupOperations: 
    - GroupNonUniform
    - GroupNonUniformVote or SubgroupVoteKHR
    - GroupNonUniformBallot or SubgroupBallotKHR
  - ShaderInvocationReorderNV as RayReorder.
  - RayTracingMotionBlurNV as RayMotionBlur.
  - RayQueryKHR as RayQuery.
  - RayTracingOpacityMicromapEXT as RayMicromapOpacity.
  - AtomicFloat32AddEXT or AtomicFloat32MinMaxEXT as AtomicF32.
  - AtomicFloat64AddEXT or AtomicFloat64MinMaxEXT as AtomicF64.
  - ComputeDerivativeGroupLinearNV as ComputeDeriv.
  - GroupNonUniformArithmetic as SubgroupArithmetic.
  - GroupNonUniformShuffle as SubgroupShuffle.
  - ImageMSArray as WriteMSTexture.
  - RayTracingKHR required if Raytracing shader stage.
  - Tessellation required if Hull or Domain shader stage.
  - Geometry required if Geometry shader stage.
  - Registers of type STORAGE_TEXEL_BUFFER, UNIFORM_TEXEL_BUFFER, UNIFORM_BUFFER_DYNAMIC or STORAGE_BUFFER_DYNAMIC *are unsupported*.
  - Other capabilities *are unsupported*.

### DXIL spec

- Semantics should use TEXCOORD[N] rather than for example NORMAL, TANGENT, etc. To be compatible with SPIRV. Though of course SV_TARGET and other SVs are accepted.
- Extensions available by default:
  - D3D_SHADER_REQUIRES_TYPED_UAV_LOAD_ADDITIONAL_FORMATS
  - D3D_SHADER_REQUIRES_STENCIL_REF
  - D3D_SHADER_REQUIRES_EARLY_DEPTH_STENCIL
  - D3D_SHADER_REQUIRES_UAVS_AT_EVERY_STAGE
  - D3D_SHADER_REQUIRES_64_UAVS
  - D3D_SHADER_REQUIRES_LEVEL_9_COMPARISON_FILTERING
- Extensions:
  - D3D_SHADER_REQUIRES_RAYTRACING_TIER_1_1 as RayQuery.
  - D3D_SHADER_REQUIRES_NATIVE_16BIT_OPS as 16BitTypes.
  - D3D_SHADER_REQUIRES_INT64_OPS as I64.
  - D3D_SHADER_REQUIRES_VIEW_ID as Multiview.
  - D3D_SHADER_REQUIRES_DOUBLES or D3D_SHADER_REQUIRES_11_1_DOUBLE_EXTENSIONS as F64.
  - D3D_SHADER_REQUIRES_ATOMIC_INT64_ON_TYPED_RESOURCE or D3D_SHADER_REQUIRES_ATOMIC_INT64_ON_GROUP_SHARED as AtomicI64.
  - D3D_SHADER_REQUIRES_DERIVATIVES_IN_MESH_AND_AMPLIFICATION_SHADERS as MeshTaskTexDeriv.
  - D3D_SHADER_REQUIRES_WAVE_OPS as SubgroupOperations.
- NV extensions should be properly marked using annotations, or the oiSH is illegal. The OxC3 validator can't check this yet, since DXIL doesn't understand these; it's the driver which parses DXIL into these special opcodes.

## ESHExtension_Bindless / ESHExtension_UnboundArraySize

"Bindless" extension is present when more than the maximum bindful registers are detected. This extension is generally well supported across modern hardware (basically everywhere on PC) but not always properly supported on mobile / web. For that reason, this has become a dedicated extension. This extension is just a hint based on reasonable device limits in the wild right now.

These extensions are special in that the user doesn't have to enable them, they are automatically enabled if the bindful limit is exceeded. This is to simplify the shader creation process.

This can be coupled with `ESHExtension_UnboundArraySize` which indicates that the register size can be determined by the engine/framework running it (such as allowing more registers based on the binding limits). This has syntax such as `Texture2D textures[]` in HLSL.

### Binding limits (tier 0)

Tier 0 (without bindless) has the following limits:

- maxBoundDescriptorSets of 4.
- maxPerStageDescriptorSamplers of 16.
- maxPerStageDescriptorUniformBuffers/CBVs of 12.
- maxPerStageDescriptorStorageBuffers of 8.
- maxPerStageDescriptorSampledImages of 16.
- maxPerStageDescriptorStorageImages of 4.
- maxPerStageResources of 44.
- maximum UAVs of 64.
- maximum SRVs of 128.
- Acceleration structures: 16 per binary if RT is supported. Due to Intel not supporting more.

### Binding limits (tier 1)

Tier 1 (with bindless) has the following limits:

- maxBoundDescriptorSets of 4.
- maxPerStageDescriptorSamplers of 2048.
- maxPerStageDescriptorUniformBuffers of 12.
- maxPerStageDescriptorStorageBuffers of 500k.
- maxPerStageDescriptorSampledImages of 250k.
- maxPerStageDescriptorStorageImages of 250k.
- maxPerStageResources of 1M.
- maximum UAV+SRV+CBV of 1M.
- Acceleration structures: 16 per binary if RT is supported. Due to Intel not supporting more.

## Quirks between DXIL and SPIRV

When combining DXIL and SPIRV binaries and/or switching binary type, there are a few quirks that are important to know:

- In DXIL, registers and elements in buffers all get flattened to a 1D array, even if they were from a 2D array. The array is effectively flattened. Combining a SPIRV input with a DXIL input will unflatten this array (since SPIRV maintains this info).
- In DXIL, samplers and textures are always separated, there exist no combined samplers. As such, they will be presented as two separate registers. When SPIRV is combined or used, it will merge the texture by name into a combined sampler. In this case, the separate sampler itself will not exist for SPIRV (only the texture ala combined sampler) but will for DXIL. In a DXIL+SPIRV merged binary, the texture is marked as combined sampler: ESHRegisterType_IsCombinedSampler and the separate sampler will only be available with DXIL bindings.
- SPIRV has the concept of subpass inputs, but DXIL doesn't. This means the bindings of input attachments should only be valid for SPIRV.
- DXIL has the concept of sampler comparison states, but SPIRV just sees them as samplers. If DXIL and SPIRV binaries are merged it will promote sampler register type to sampler comparison register type.
- DXIL has more info about the texture primitive than SPIRV, though SPIRV has a format (which DXIL doesn't have). This means that formatId will always come from SPIRV and texture primitive from DXIL. SPIRV's texture primitive is unreliable for use for DXIL.
- When stripping SPIRV info from one that has both DXIL and SPIRV, it will keep the reflection data it gained from merging the two. This is intentional, as this would allow you to re-gain some reflection info that is missing from DXIL and keeps the reflection data consistent across two different splits.
- Combining DXIL and SPIRV underestimates dormant extensions, it could be possible a certain extension isn't present in DXIL or SPIRV, however if it can't be queried by the underlying format then it's impossible to tell. In this case, it will assume the extension the shader was compiled with is leading and so remove it from being dormant.
  - Dormant extensions include all extensions that are supported by the shader mode (DXIL or SPIRV) but that weren't detected. This allows easy merging of dormant extensions by just bitwise ANDing them and getting an underestimated version of dormant extensions.

## Changelog

0.1: Old ocore1 specification. Represented multiple shaders and had too much reflection information that is now irrelevant.

1.2: Basic format specification. Added support for various extensions, stages and binary types. Maps closer to real binary formats.

1.2(.1): No major bump, because no oiSH files exist in the wild yet. Made extensions per stage, made file format more efficient, now allowing multiple binaries to exist allowing 1 compile for all entries even for non lib formats. Added uniforms. Also swapped binaries and stages. Added include files (relative paths) and CRC32Cs for dirty checking. Also added a better language spec about what is legal to be contained in a oiSH file (SPIRV and DXIL subsets). Various extensions and abilities to use HLSL or GLSL specific features for all backends.

1.2(.2): No major bump, because no oiSH files exist in the wild yet. Added 'dormant' extension, which is an extension enabled by the shader compiler but isn't used by the final executable. If for example SPIRV and DXIL are merged, then only extensions present by both can be marked as dormant, because for example SER (Shader Execution Reordering) can be present in DXIL but is undetectable without writing custom code processing DXIL due to NVAPI hackery. Due to this it could be possible that for example SER is enabled but isn't present in DXIL or SPIRV but since both are merged it (DXIL can't detect) it can't be certain that this extension is dormant. Same is true for SPIRV only extensions that DXIL doesn't have or vice versa.


# oiSH (Oxsomi SHader)

*The oiSH format is an [oiXX format](oiXX.md), as such it inherits the properties from that such as compression, encryption and endianness (though enc/comp is not supported, since oiSH is most often packaged inside of an oiCA/oiDL file).*

**NOTE: oiSH (1.2) is the successor of the original oiSH (0.1) from ocore. This isn't the same format anymore. oiSH (0.1) lacked a lot of important features and is deprecated.**

oSH is a single shader represented in a single or multiple binary/text format(s). The only definition is that the shader type must match one of the following:

- SPIRV
- DXIL
- (**unsupported for now**): MSL
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
typedef struct SHHeader {

	U32 magicNumber;			//oiSH (0x4853696F); optional if it's part of an oiSC.
    
    U32 compilerVersion;

	U32 hash;					 //CRC32C of contents starting at version
    
    U32 sourceHash;				//CRC32C of source(s), for determining if it's dirty
    
    U32 timeMsLow;
    
    U16 timeMsHigh;
	U8 version;					//major.minor (%10 = minor, /10 = major (+1 to get real major)) at least 1
    U8 sizeTypes;				//Every 2 bits size type of spirv, dxil
    
    U16 binaryCount;			//How many unique binaries are stored
    U16 stageCount;				//How many stages reference into the binaries

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
	//ESHBinaryFlags_HasMSL					= 1 << 2,
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

	ESHExtension_PAQ						= 1 << 15		//Payload access qualifiers

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
	ESHVendor_Count
} ESHVendor;

typedef struct BinaryInfoFixedSize {
    
	U8 shaderModel;				//U4 major, minor
	U8 entrypointType;			//ESHPipelineStage if entrypoint is not U16_MAX
	U16 entrypoint;				//U16_MAX if library, otherwise index into stageNames

	U16 vendorMask;				//Bitset of ESHVendor
	U8 uniformCount;
	U8 binaryFlags;				//ESHBinaryFlags
    
    ESHExtension extensions;
    
} BinaryInfoFixedSize;

typedef enum ESHPrimitive {
  	ESHPrimitive_Invalid,
    ESHPrimitive_Float,		//Float, unorm, snorm
  	ESHPrimitive_Int,
  	ESHPrimitive_UInt
} ESHPrimitive;

typedef enum ESHVector {
    ESHVector_N1,
    ESHVector_N2,
    ESHVector_N3,
    ESHVector_N4
} ESHVector;

//Final file format; please manually parse the members.
//Verify if everything's in bounds.
//Verify if SHFile includes any invalid data.

SHFile {

    SHHeader header;

    //No header, no encryption/compression/SHA256 (see oiDL.md)
    //strings[i < stageCount] contains entrypoint names.
    //strings[i >= stageCount] contains uniform values and names.
    DLFile strings;

    EntryInfoFixedSize pipelineStages[stageCount];
    BinaryInfoFixedSize binaryInfos[binaryCount];

    for pipelineStages[i]

	    if compute or workgraph:
		    U16x4 groups;
               	groups.w = waveSize: U4[4] required, min, max, recommended
                    Where the U4: 0 = None, 3-8 = 4-128 (log2), else invalid

	    else if is graphics:
    
    		U8 inputsAvail, outputsAvail;
    
		    //Each element: [ ESHPrimitive, ESHVector ]
    		U4 inputs[inputsAvail align 2];
    		U4 outputs[outputsAvail align 2];

    	else if intersection:
    	    U8 intersectionSize;
    
	    if miss,closestHit,anyHit or intersection
   	 		U8 payloadSize;
    
    	U16 binaryIds[pipelineStages[i].binaryCount];

    for i < binaryCount:
    
    	U16 uniformNames[binaryInfos[i].uniformCount];	//offset to strings[stageCount]
    	U16 uniformValues[binaryInfos[i].uniformCount];	//^ [stageCount + uniforms]
    
        if binary[i] has SPIRV:
            EXXDataSizeType<spirvType> spirvLength;

        if binary[i] has DXIL:
            EXXDataSizeType<dxilType> dxilLength;

        foreach binary with [ spirvLength, dxilLength ]:
            U8[binary.size] data;
}
```

The types are Oxsomi types; `U<X>`: x-bit unsigned integer, `I<X>` x-bit signed integer.

The magic number in the header can only be absent if embedded in another file. An example is the file name table in an oiCA file.

## Changelog

0.1: Old ocore1 specification. Represented multiple shaders and had too much reflection information that is now irrelevant.

1.2: Basic format specification. Added support for various extensions, stages and binary types. Maps closer to real binary formats.

1.2(.1): No major bump, because no oiSH files exist in the wild yet. Made extensions per stage, made file format more efficient, now allowing multiple binaries to exist allowing 1 compile for all entries even for non lib formats. Added uniforms.


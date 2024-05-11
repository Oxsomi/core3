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

Finally, this would become a part of an oiSC file (Oxsomi Shader Cache). Which includes a list of all binaries and the pipeline info to create it. Where header is optional.

## File format spec

```c

typedef enum ESHFlags {

	ESHFlags_None 					= 0,

    //What type of binaries it includes
    //Must be one at least

    ESHFlags_HasSPIRV				= 1 << 1,
    ESHFlags_HasDXIL				= 1 << 2,
    
    //Reserved
    //ESHFlags_HasMSL				= 1 << 3,
    //ESHFlags_HasWGSL				= 1 << 4
    
    ESHFlags_HasBinary				= ESHFlags_HasSPIRV | ESHFlags_HasDXIL,
    //ESHFlags_HasText				= ESHFlags_HasMSL | ESHFlags_HasWGSL
    ESHFlags_HasSource				= ESHFlags_HasBinary // | ESHFlags_HasText

} ESHFlags;

typedef enum ESHExtension {
    
    ESHExtension_None						= 0,
    
    //These losely map to EGraphicsDataTypes in OxC3 graphics
    
    ESHExtension_F64						= 1 << 0,
    ESHExtension_I64						= 1 << 1,
    ESHExtension_F16						= 1 << 2,
    ESHExtension_I16						= 1 << 3,
    
    ESHExtension_AtomicI64					= 1 << 4,
    ESHExtension_AtomicF32					= 1 << 5,
    ESHExtension_AtomicF64					= 1 << 6,
    
    //Some of them are present in EGraphicsFeatures in OxC3 graphics
    
    ESHExtension_SubgroupArithmetic			= 1 << 7,
    ESHExtension_SubgroupShuffle			= 1 << 8,
    
	ESHExtension_RayQuery					= 1 << 9,
	ESHExtension_RayMicromapOpacity			= 1 << 10,
	ESHExtension_RayMicromapDisplacement	= 1 << 11,
	ESHExtension_RayMotionBlur				= 1 << 12,
	ESHExtension_RayReorder					= 1 << 13
    
} ESHExtension;

typedef struct SHHeader {

	U32 magicNumber;			//oiSH (0x4853696F); optional if it's part of an oiSC.

	U8 version;					//major.minor (%10 = minor, /10 = major (+1 to get real major)) at least 1
	U8 flags;					//ESHFlags
	U8 sizeTypes;				//EXXDataSizeTypes: spirvType | (dxilType << 2) | (mslType << 4) | (wgslType << 6)
    U8 padding;
    
    ESHExtension extensions;

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
	ESHPipelineStage_TaskExt
    
} ESHPipelineStage;

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
    
    DLFile stageNames;			//No header, no encryption/compression (see oiDL.md)
    
    //ESHPipelineStage.
    //Only allowed to be >1 if non graphics or compute
    U8 pipelineStages[stageNames.length];
    
    for pipelineStages[i]
    
	    if compute or workgraph:
		    U16x4 groups[stageNames.length];
    
	    if is graphics:
    		U4 inputs[16];					//Each element: [ ESHPrimitive, ESHVector ]
    		U4 outputs[16];
    
	    if miss,closestHit,anyHit or intersection
    	    U8 intersectionSize;
   	 		U8 payloadSize

    if has SPIRV:
	    EXXDataSizeType<spirvType> spirvLength;
    
    if has DXIL:
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


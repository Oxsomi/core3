# oiSB (Oxsomi Shader Buffer)

*The oiSB format is an [oiXX format](oiXX.md), as such it inherits the properties from that such as compression, encryption and endianness (though enc/comp is not supported, since oiSH is most often packaged inside of an oiCA/oiDL file).*

**NOTE: oiSB (1.2) is the successor of the original oiSB (0.1) from OxC1. This isn't the same format anymore. oiSB (0.1) lacked a lot of important features and is deprecated.**

oSB is a single shader buffer layout which specifies the exact layout and data types that make up the shader buffer.

Just like any oiXX file it's made with the following things in mind:

- Ease of read + write.
- An easy spec.
- Good security for parsing + writing.

## File format spec

```c
typedef struct SBHeader {

	U32 magicNumber;			//oiSB (0x4253696F); optional if it's part of an oiSH.
    
	U8 version;					//major.minor (%10 = minor, /10 = major (+1 to get real major)) at least 1
    U8 flags;					//& 1 = isTightlyPacked
    U16 arrays;

    U16 structs;
    U16 vars;					//Should always contain at least 1
    
    U32 bufferSize;

} SBHeader;

typedef enum ESBPrimitive {
  	ESBPrimitive_Invalid,
    ESBPrimitive_Float,
  	ESBPrimitive_Int,
  	ESBPrimitive_UInt
} ESBPrimitive;

typedef enum ESBStride {
    ESBStride_X8,
    ESBStride_X16,
    ESBStride_X32,
    ESBStride_X64
} ESBStride;

typedef enum ESBVector {
    ESBVector_N1,
    ESBVector_N2,
    ESBVector_N3,
    ESBVector_N4
} ESBVector;

typedef enum ESBMatrix {
    ESBMatrix_N1,
    ESBMatrix_N2,
    ESBMatrix_N3,
    ESBMatrix_N4
} ESBMatrix;

//Without flags every matrix is 16-byte aligned. F32x3x4 becomes the same size as F32x4x4 with unused bytes.
//F32x3x4 represents a F32x3[4] with stride 16. So even F32x1x4 becomes the same size as F32x4x4.
//**This is only true if ESBVarFlag_IsTightlyPacked isn't set!**
define ESBType as U8: [ U2 from ESBStride, U2 from ESBPrimitive, U2 from ESBVector, U2 from ESBMatrix ]

typedef struct SBStruct {
    U32 stride;			//stride to next element (e.g. next neighbor or next array entry)
} SBStruct;

typedef enum ESBVarFlag {
    ESBVarFlag_None				= 0,
    ESBVarFlag_IsUsedVarSPIRV	= 1 << 0,		//Variable is used by shader (SPIRV)
    ESBVarFlag_IsUsedVarDXIL	= 1 << 1		//Variable is used by shader (DXIL)
} ESBVarFlag;

typedef struct SBVar {
    
    U16 structId;		//If not U16_MAX, ESBType needs to be 0 and this should be a valid struct
    U16 arrayIndex;		//U16_MAX identifies "none"
    
    U32 offset;
    
    U8 type;			//ESBType if structId == U16_MAX
    U8 flags;			//ESBStructVarFlag
    U16 parentId;		//root = U16_MAX
    
} SBVar;

//Final file format; please manually parse the members.
//Verify if everything's in bounds.
//Verify if SBFile includes any invalid data.

SBFile {

    SBHeader header;

    //No magic number, no encryption/compression/SHA256 (see oiDL.md).
    //[0, 			structs]  		= structNames
    //[structs, 	structs + vars] = varNames
    //Empty strings are valid, as nameless vars/structs exist.
    //Duplicates are also allowed, as one struct can have multiple layouts.
    DLFile strings;
    
    SBStruct structs[header.structs];
    SBVar vars[header.vars];				//Last variable is the root
    U8 arrayDimCount[header.arrays];		//For example 1 = [], 2 = [][], etc.

    for i < header.arrays:
    	U32 dimensions[arrayDimCount[i]]
}
```

The types are Oxsomi types; `U<X>`: x-bit unsigned integer, `I<X>` x-bit signed integer.

The magic number in the header can only be absent if embedded in another file. An example is when embedded in oiSH files.

ESBType: F32x1x4 is defined as a F32x1 (single F32) of 4 (F32x1[4]) with an implied stride of 16. If tightly packing is set in the header (ESBSettingsFlags_IsTightlyPacked), this implied stride becomes 0 (tightly packed). With tightly packed, F32x1x4 is the same size as a F32x4, while without it would be the size of F32x4x4. The order is different than HLSL; where float3x4 = F32x4x3.

Without the tightly packing flag, HLSL/DX-like CBuffer packing is used. This means that every struct and array needs to start at a 16-byte aligned offset. Matrices also have this, the 2nd+ matrix vector element needs to start at a 16-byte aligned position too. When a type spans a 16-byte boundary, it needs to be aligned to a 16-byte boundary.

With the tightly packing flag, HLSL/DX-like StructuredBuffer packing is used. This means that every struct is aligned to the biggest primitive type (similar to C++) and that the stride for arrays is dependent on that too (so struct of only U8 needs 1-byte alignment, U16 needs 2-byte alignment, U32 needs 4-byte alignment, U64 needs 8-byte alignment). All other types are self aligned, so 8-byte types (U64x2, U64, etc.) need to be 8-byte aligned too. 

Overlapping memory is allowed, to allow (future) support for unions.

The SBFile's ListSBVar and ListSBStruct can have duplicate names (which is fine), but a single struct (or the shader buffer itself) can't have duplicate names in its members (so `F32 a, a;` would be disallowed). However, duplicate struct names (even in current scope) is allowed.

## Hashing & comparing

Variables, structs and arrays should be appended in the order they're first seen. This means that without unions, every member would be located beyond the next, none would have an offset that's lower than the last. With unions, it's possible that memory offset decreases, but that's only allowed to be in the union itself and the structs in the union should still increase the same way. This allows simpler behavior with comparing and other operations such as hashing. This also means that threading SBFile generation is off limits.

Hashes are generated like following:

- FNV-1a64 is used (64-bit FNV-1a).
- On create, ESBSettingsFlags and U32 are treated as a U64 and FNVed.
- Every time a struct is appended:
  - The struct | (nameLength << 32) is FNVed.
- Every time a SBVar is appended:
  - The SBVar is treated as a U64, U32 and the U32 is cast to an U64 and (nameLength << 32) is ored into it. These two U64s are FNVed.
  - Every time a unique array is appended:
    - The array length is FNVed and every 2 U32 array count are treated as a single U64 and FNVed, while the last element (if odd) is treated as a single U32 and FNVed.

This hash can be used for quick comparison and is only available at runtime.

## Quirk(s) between DXIL/SPIRV

The biggest quirk between DXIL and SPIRV is that DXIL doesn't have anything beyond flattened arrays, though SPIRV does. To support this, oiSB will flatten arrays too when DXIL is used and unflatten if SPIRV is used. If DXIL and SPIRV oiSB files are merged through SBFile_combine it is safe to merge one way (so from flattened to unflattened). For example: `[16]` could be recast to `[4][4]` or `[8][2]`. But once it has been unflattened, it can never be flattened again and can't merge with incompatible multi dimensional arrays (or ones with a mismatching flattened count). So merging DXIL and SPIRV will always result in the array info that SPIRV has. Other than that, it forces SPIRV to use DX-like alignment rules for structured/storage buffers and constant buffers.

## Changelog

0.1: Old ocore1 specification. A lot was still relevant, but some useful functionality is missing (such as marking data as 'unused', which can be useful for optimization).

1.2: Basic format specification. Formalized from older version and added some nice to haves such as moving ESHType to ESBType. TextureFormat was used in OxC1, but isn't as nice, as a texture format != variable format (some don't even exist). Also added a missing stride to struct. Struct is now just a container rather than a variable placed at a position.


# oiSR (Oxsomi Shader Reflection)

*The oiSR format is an [oiXX format](oiXX.md), as such it inherits the properties from that such as compression, encryption and endianness (though enc/comp is not supported, since oiSR is most often packaged inside of an oiSH, which itself lives inside an oiCA/oiDL file).*

oiSR stores the **source-level (pre-codegen) symbol AST** of a shader: the entrypoints, functions, user types (struct/enum/union/interface/typedef), resources, namespaces, parameters and control-flow scopes as they appear in the HLSL *before* DXIL/SPIR-V is generated, together with (optionally) their source locations. It is a compiler-*frontend* reflection, complementing oiSB (buffer layouts) and the oiSH register reflection (backend bindings).

It exists to power editor intelligence: semantic highlighting, document outline, go-to-definition and hover, on the web frontend and in tooling.

It is produced by walking a backend reflector into this neutral model. Today that reflector is DXC's fork-custom `IHLSLReflectionData` (`Compiler_reflect`), but the model, not the producer, is the contract: a future Slang/clang walker emits the same SRFile. The node model is a close (near 1:1) mirror of the frontend AST with slight Oxsomi abstractions.

Just like any oiXX file it's made with the following things in mind:

- Ease of read + write.
- An easy spec.
- Good security for parsing + writing.

## File format spec

```c
typedef struct SRHeader {		//Should be aligned to 4-byte

	U32 magicNumber;			//oiSR (0x5253696F); optional if it's part of an oiSH.

	U8 version;					//ESRVersion; at least 1 (1 = V1_0)
	U8 flags;					//ESRFlag: & 1 = HasSymbols (the symbols[] location array is present)
	U16 padding;

	U32 features;				//ESRFeature bitset: which reflection tiers this file carries

	U32 nodeCount;
	U32 annotationCount;

} SRHeader;					//Note: unlike SBHeader the magic + header aren't a single struct on disk;
							// when magic is present it's a separate U32 written before SRHeader.

//Which reflection tiers a file carries, mirroring the backend reflector's feature mask.
//Stripping ESRFeature_SymbolInfo removes names and source locations; the others gate node categories.
typedef enum ESRFeature {
	ESRFeature_None        = 0,
	ESRFeature_Basics      = 1 << 0,	//cbuffer and registers only
	ESRFeature_Functions   = 1 << 1,
	ESRFeature_Namespaces  = 1 << 2,
	ESRFeature_UserTypes   = 1 << 3,	//struct, enum, typedef, union, interface
	ESRFeature_Scopes      = 1 << 4,	//variables/structs/functions defined inside functions (control flow)
	ESRFeature_SymbolInfo  = 1 << 16,	//names and file/line/column info
	ESRFeature_All         = ESRFeature_SymbolInfo - 1
} ESRFeature;

//A node's kind. Kept in the same order as the producer's enum (D3D12_HLSL_NODE_TYPE) so the walk is a validated cast.
typedef enum ESRNodeType {
	ESRNodeType_Register, ESRNodeType_Function, ESRNodeType_Enum, ESRNodeType_EnumValue, ESRNodeType_Namespace,
	ESRNodeType_Variable, ESRNodeType_Typedef,			//localId points to the type
	ESRNodeType_Struct, ESRNodeType_Union,				//child variables like buffers; localId = typeId (if not fwd decl)
	ESRNodeType_StaticVariable,
	ESRNodeType_Interface, ESRNodeType_Parameter,
	ESRNodeType_IfRoot, ESRNodeType_Scope, ESRNodeType_Do, ESRNodeType_Switch,	//control flow
	ESRNodeType_While, ESRNodeType_For,
	ESRNodeType_GroupsharedVariable,
	ESRNodeType_Case, ESRNodeType_Default,				//branches
	ESRNodeType_Using,
	ESRNodeType_IfFirst, ESRNodeType_ElseIf, ESRNodeType_Else,
	ESRNodeType_Count
} ESRNodeType;

//Mirror of D3D_INTERPOLATION_MODE.
typedef enum ESRInterpolation {
	ESRInterpolation_Undefined, ESRInterpolation_Constant, ESRInterpolation_Linear, ESRInterpolation_LinearCentroid,
	ESRInterpolation_LinearNoperspective, ESRInterpolation_LinearNoperspectiveCentroid,
	ESRInterpolation_LinearSample, ESRInterpolation_LinearNoperspectiveSample,
	ESRInterpolation_Count
} ESRInterpolation;

typedef enum ESRNodeFlag {
	ESRNodeFlag_None         = 0,
	ESRNodeFlag_IsFwdDeclare = 1 << 0	//This node is a forward declaration; fwdBckDeclareNode has the definition
} ESRNodeFlag;

//A symbol AST node. POD and hashable (treated as a raw byte buffer).
//Node order matches the producer, so parent/child indices are direct.
//A node's direct children are those nodes whose parent == this node's index.
typedef struct SRNode {

	U32 nameId;					//Into strings, U32_MAX = anonymous (also U32_MAX for every node when SymbolInfo is absent)
	U32 semanticId;				//Into strings, U32_MAX = none

	U32 localId;				//Type/enum/backref id, meaning depends on type; opaque
	U32 parent;					//Parent node index, U32_MAX = root

	U32 fwdBckDeclareNode;		//Linked forward/backward declaration node, U32_MAX = none
	U32 childCount;				//Number of direct children (informational; children are also found by scanning parent)

	U32 annotationStart;		//First annotation into annotations[], U32_MAX = none
	U16 annotationCount;

	U8 type;					//ESRNodeType
	U8 interpolation;			//ESRInterpolation

	U8 flags;					//ESRNodeFlag
	U8 padding[3];

} SRNode;						//36 bytes

//Source location of a node, the strippable "SymbolInfo" tier.
//When present, symbols[] is parallel to nodes[] (symbols[i] describes nodes[i]).
typedef struct SRSymbol {

	U32 fileNameId;				//Into strings, U32_MAX = none

	U32 line;					//1-based first line
	U32 lineCount;

	U32 columnStart;
	U32 columnEnd;

	U32 padding;

} SRSymbol;						//24 bytes

//An annotation on a node ([shader(...)]/[numthreads(...)] builtins, or [[oxc::...]] / [[vk::...]] custom attributes).
typedef struct SRAnnotation {
	U32 nameId;					//Into strings
	U8 isBuiltin;				//[name] (builtin) vs [[name]] (custom)
	U8 padding[3];
} SRAnnotation;					//8 bytes

//Final file format; please manually parse the members.
//Verify if everything's in bounds.
//Verify if SRFile includes any invalid data.

SRFile {		//Has to be 16-byte aligned

	SRHeader header;

	SRNode nodes[header.nodeCount];
	SRSymbol symbols[header.nodeCount];		//Only present if (header.flags & HasSymbols); else 0 bytes
	SRAnnotation annotations[header.annotationCount];

	U8[N] pad;								//Padding to align to 16-byte

	//No magic number, no encryption/compression/SHA256 (see oiDL.md).
	//A single flat string pool; records reference it by explicit id (nameId/semanticId/fileNameId/annotation.nameId),
	// with U32_MAX meaning "no string". Unlike oiSB there is NO positional convention: order is producer order and
	// duplicates are deduplicated by the writer.
	//Empty strings are valid; all strings must fit into 1 MiB max (all loaded at DLFile_read time).
	DLFile strings;
}
```

The types are Oxsomi types; `U<X>`: x-bit unsigned integer, `I<X>` x-bit signed integer.

The magic number can only be absent if embedded in another file. The intended host is an oiSH: an oiSR is written as a subfile (HideMagicNumber) only when the shader is compiled with reflection enabled, keyed to the entrypoint / define / uniform combination it was reflected under (reflection only navigates to the code paths the active defines/uniforms expose).

### Sentinels & invariants

- `nameId == U32_MAX` means anonymous. **Names come from the SymbolInfo tier**, so when `ESRFeature_SymbolInfo` is absent every node's `nameId` is `U32_MAX`; `semanticId` is independent of that tier.
- `parent == U32_MAX` is a root node. `fwdBckDeclareNode == U32_MAX` means no linked declaration. `annotationStart == U32_MAX` when `annotationCount == 0`.
- `HasSymbols` (header flag) and `ESRFeature_SymbolInfo` (features) must agree, and when set `symbols[]` has exactly `nodeCount` entries. When clear, `symbols[]` is absent (0 bytes).
- All `nameId`/`semanticId`/`fileNameId`/`annotation.nameId` must be `U32_MAX` or `< strings.length`; `parent`/`fwdBckDeclareNode` must be `U32_MAX` or `< nodeCount`; `type < ESRNodeType_Count`; `interpolation < ESRInterpolation_Count`.

## Hashing & comparing

Nodes, symbols and annotations are appended in producer order (the same order as the frontend AST). This deterministic ordering allows simple comparison and hashing and means threading SRFile generation is off limits.

Hashes are generated like following:

- FNV-1a64 is used (64-bit FNV-1a).
- The seed is `(features << 32) | (flags & ~HideMagicNumber)` treated as a U64 and FNVed (HideMagicNumber is a serialization detail and never influences the hash).
- The whole `nodes[]` byte buffer is FNVed, then the whole `symbols[]` byte buffer, then the whole `annotations[]` byte buffer.
- Every string in the pool (in order) is FNVed by its bytes.

This hash is refreshed by `SRFile_finalize` (and on read). It can be used for quick comparison (e.g. deduplicating identical reflection trees across entrypoint combinations in an oiSH) and is only available at runtime.

## Relationship to the producer (DXC / Slang)

`ESRNodeType`, `ESRInterpolation` and `ESRFeature` mirror the fork-custom `IHLSLReflectionData` model (`D3D12_HLSL_NODE_TYPE`, `D3D_INTERPOLATION_MODE`, `D3D12_HLSL_REFLECTION_FEATURE`) bit-for-bit / order-for-order, so the DXC walk (`Compiler_reflect`) is a validated cast. The root-parent sentinel `0xFFFF` and "no fwd/back declare" `UINT_MAX` from that API are both normalized to the oiSR `U32_MAX`. A different producer (e.g. Slang) maps its own declaration tree + source locations onto the same structs; consumers only ever see SRFile.

## Changelog

1.0: Initial format specification.

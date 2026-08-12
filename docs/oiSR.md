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

	U8 version;					//ESRVersion; on-disk byte 1 (ESRVersion_V1_1), displayed as 1.1
	U8 flags;					//ESRFlag: & 1 = HasSymbols (the symbols[] location array is present)
	U16 padding;

	U32 features;				//ESRFeature bitset: which reflection tiers this file carries

	U32 nodeCount;
	U32 annotationCount;

	U32 registerCount;			//Frontend bind info records (for Register nodes)
	U32 enumValueCount;			//Enumerator records (for EnumValue nodes)

	U32 typeCount;				//Per-node type records (for Variable/Typedef/Struct/Union nodes)
	U32 arrayDimCount;			//Shared multi-dimensional array-length pool
	U32 interfaceCount;			//Interface-implementation edges

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
	ESRNodeFlag_IsFwdDeclare = 1 << 0,	//This node is a forward declaration; fwdBckDeclareNode has the definition
	ESRNodeFlag_HasReturn    = 1 << 1,	//Function nodes: the function returns a value (rather than void)
	ESRNodeFlag_ParamReturn  = 1 << 2,	//Parameter nodes: the function's return-value slot (D3D_RETURN_PARAMETER_INDEX)
	ESRNodeFlag_ParamIn      = 1 << 3,	//Parameter nodes: 'in'  (in + out = inout)
	ESRNodeFlag_ParamOut     = 1 << 4	//Parameter nodes: 'out'
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

//Frontend bind info for a Register node (kind/shape only; space/bindPoint are backend concerns, left invalid).
//type/dimension/returnType mirror D3D_SHADER_INPUT_TYPE / D3D_SRV_DIMENSION / D3D_RESOURCE_RETURN_TYPE
// (see ESRResourceType / ESRResourceDimension / ESRResourceReturnType).
typedef struct SRRegister {
	U32 nodeId;					//The Register node this describes
	U8 type;					//ESRResourceType (cbuffer/Texture/SamplerState/RW.../RaytracingAccelerationStructure/...)
	U8 dimension;				//ESRResourceDimension
	U8 returnType;				//ESRResourceReturnType (0 = none)
	U8 arrayDimCount;			//Resource-array dimension count in arrayDims[] (0/1 = use bindCount; >=2 = multi-dim)
	U32 bindCount;				//Descriptor count (array size, flattened); 0 = unbounded, 1 = single
	U32 arrayDimStart;			//Into SRFile::arrayDims when arrayDimCount >= 2, else U32_MAX
} SRRegister;					//16 bytes

//A single enumerator. Keyed by its EnumValue node; enumType is the parent enum's underlying type (ESREnumType,
// mirror of D3D12_HLSL_ENUM_TYPE: uint/int/uint64_t/int64_t/uint16_t/int16_t).
typedef struct SREnumValue {
	U32 nodeId;					//The EnumValue node this describes
	U8 enumType;				//ESREnumType (the parent enum's underlying integer type)
	U8 padding[3];
	I64 value;
} SREnumValue;					//16 bytes

//The resolved type of a value node. Keyed by node index; typeName is the underlying (resolved) spelling ("float3",
// "Light", "Texture2D"), and typeClass/rows/cols/elements let a consumer tell scalar/vector/matrix/struct/object apart
// without parsing the name. displayName is the alias the source actually wrote (the sugar a tooltip shows): for
// "typedef float3 myvec; myvec v;" the variable v has typeName "float3" and displayName "myvec"; it is U32_MAX when it
// equals the underlying name. Produced for Variable/Typedef/Struct/Union/StaticVariable/GroupsharedVariable nodes
// (whose localId indexes the frontend type table, read via GetDesc1 for the display name) and for Parameter nodes
// including the return slot (whose type comes from the function-parameter reflection instead of a type localId, so
// only builtin scalar/vector/matrix names are reconstructed; struct/object parameters carry the class but no name).
// defNodeId is the go-to-definition target: the Struct/Union node defining this type (resolved by name, since the
// reflector reuses type indices across uses), U32_MAX for builtins. baseNodeId is the base-class struct node for single
// inheritance. Members need no separate graph: a struct's members are its child nodes, each carrying its own SRType (so
// member types + go-to-def come for free). typeClass mirrors D3D_SHADER_VARIABLE_CLASS (ESRTypeClass: scalar/vector/
// matrixRows/matrixColumns/object/struct/interfaceClass/interfacePointer).
typedef struct SRType {
	U32 nodeId;					//The value node this type describes
	U8 typeClass;				//ESRTypeClass
	U8 rows;					//Matrix row count (1 for scalar/vector/object)
	U8 cols;					//Vector width / matrix column count (1 for scalar/object)
	U8 arrayDimCount;			//Array dimension count in arrayDims[] (0/1 = use `elements`; >=2 = multi-dim)
	U32 elements;				//Total array element count (product of dims), 0 = not an array
	U32 typeNameId;				//Underlying (resolved) type spelling into strings[], or U32_MAX
	U32 displayNameId;			//Display/alias spelling into strings[] (for tooltips); U32_MAX = same as typeName
	U32 defNodeId;				//Struct/Union node defining this type (go-to-definition), U32_MAX = builtin/none
	U32 baseNodeId;				//Base-class struct node (single inheritance), U32_MAX = none; set on the definition only
	U32 arrayDimStart;			//Into SRFile::arrayDims when arrayDimCount >= 2, else U32_MAX
} SRType;						//32 bytes

//Shared pool of multi-dimensional array lengths (a plain U32[]), referenced by SRType / SRRegister via
//(arrayDimStart, arrayDimCount) for arrays of 2+ dimensions; the flattened total stays in elements / bindCount.

//One interface a struct/union implements. A struct can implement several, so these are edges in their own table keyed
//by the implementing node (not a field on SRType). Like baseNodeId, they're recorded on the type DEFINITION only; a
//variable of the type reaches its base/interfaces through defNodeId.
typedef struct SRInterface {
	U32 nodeId;					//The Struct/Union node that implements the interface
	U32 interfaceNodeId;		//The Interface node it implements
} SRInterface;					//8 bytes

//Final file format; please manually parse the members.
//Verify if everything's in bounds.
//Verify if SRFile includes any invalid data.

SRFile {		//Has to be 16-byte aligned

	SRHeader header;

	SRNode nodes[header.nodeCount];
	SRSymbol symbols[header.nodeCount];		//Only present if (header.flags & HasSymbols); else 0 bytes
	SRAnnotation annotations[header.annotationCount];
	SRRegister registers[header.registerCount];
	SREnumValue enumValues[header.enumValueCount];
	SRType types[header.typeCount];
	U32 arrayDims[header.arrayDimCount];	//Shared multi-dimensional array-length pool
	SRInterface interfaces[header.interfaceCount];	//Interface-implementation edges

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
- All `nameId`/`semanticId`/`fileNameId`/`annotation.nameId` must be `U32_MAX` or `< strings.length`; `type < ESRNodeType_Count`; `interpolation < ESRInterpolation_Count`.
- `parent` must be `U32_MAX` (root) or `< i` — parents strictly precede their children, so the tree is acyclic and topologically ordered (node 0 is always a root). `i + childCount <= nodeCount`.
- `fwdBckDeclareNode` must be `U32_MAX` or reference a node of the same `type` and (when symbols present) the same name; a forward declaration (`IsFwdDeclare`) points forward to its non-forward definition, a definition's back-link points backward to its forward declaration. The `IsFwdDeclare` flag is only valid on Function/Enum/Struct/Union/Interface nodes.
- Each `registers[k].nodeId` references a `Register` node; each `enumValues[k].nodeId` references an `EnumValue` node; their `type`/`dimension`/`returnType`/`enumType` are `< ` their respective `_Count`.

## Hashing & comparing

Nodes, symbols and annotations are appended in producer order (the same order as the frontend AST). This deterministic ordering allows simple comparison and hashing and means threading SRFile generation is off limits.

Hashes are generated like following:

- FNV-1a64 is used (64-bit FNV-1a).
- The seed is `(features << 32) | (flags & ~HideMagicNumber)` treated as a U64 and FNVed (HideMagicNumber is a serialization detail and never influences the hash).
- The whole `nodes[]` byte buffer is FNVed, then `symbols[]`, then `annotations[]`, then `registers[]`, then `enumValues[]`.
- Every string in the pool (in order) is FNVed by its bytes.

This hash is refreshed by `SRFile_finalize` (and on read). It can be used for quick comparison (e.g. deduplicating identical reflection trees across entrypoint combinations in an oiSH) and is only available at runtime.

## Relationship to the producer (DXC / Slang)

`ESRNodeType`, `ESRInterpolation` and `ESRFeature` mirror the fork-custom `IHLSLReflectionData` model (`D3D12_HLSL_NODE_TYPE`, `D3D_INTERPOLATION_MODE`, `D3D12_HLSL_REFLECTION_FEATURE`) bit-for-bit / order-for-order, so the DXC walk (`Compiler_reflect`) is a validated cast. The root-parent sentinel `0xFFFF` and "no fwd/back declare" `UINT_MAX` from that API are both normalized to the oiSR `U32_MAX`. A different producer (e.g. Slang) maps its own declaration tree + source locations onto the same structs; consumers only ever see SRFile.

## Changelog

1.1: Initial format specification (no shipped file predates it, so it evolves in place rather than versioning). Carries the node tree (kinds, parent/child topology, names, semantics, forward-declaration links, annotations), the optional per-node source-location tier, and the detail tiers: per-Register frontend bind info, per-EnumValue enumerator values, a `HasReturn` flag on Function nodes, per-Parameter direction (`ParamReturn`/`ParamIn`/`ParamOut`, `in`+`out` = `inout`), and the per-node type tier (`SRType`, `header.typeCount`) giving each value node a serialized type (underlying name + display/alias name + class + rows/cols + array elements; a tooltip shows the alias, e.g. `Variable pos (myvec)`, with verbose adding `aka float3`). Types are resolved for `Variable`/`Typedef`/`Struct`/`Union`/`StaticVariable`/`GroupsharedVariable` nodes (whose `localId` indexes the frontend type table) and for `Parameter` nodes including the return slot (typed from the function-parameter reflection; builtin scalar/vector/matrix names are reconstructed, struct/object parameters carry only their class). Each `SRType` also carries the type graph: `defNodeId` links a value to the Struct/Union node defining its type (go-to-definition; members are child nodes so their types + go-to-def come for free), `baseNodeId` links a struct to its single base class, and the separate `SRInterface` table (`header.interfaceCount`) records each interface a struct implements. Base class + interfaces are recorded on the type DEFINITION only (a variable reaches them via `defNodeId`). Multi-dimensional value/resource arrays list their per-dim lengths in the shared `arrayDims` pool (`header.arrayDimCount`) referenced by `SRType`/`SRRegister`. Deferred: constant-buffer byte layouts (backend-specific packing -> oiSH/oiSB), and struct/object parameter type names (`D3D12_PARAMETER_DESC` exposes only the class for them, no name).

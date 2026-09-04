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

//formats/oiSR/sr_file.h

#pragma once
#include "formats/oiDL/dl_file.h"
#include "types/container/list.h"
#include "types/container/list_basic_types.h"        //ListU32 (arrayDims pool)

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct RefPtr RefPtr;
typedef RefPtr StreamRef;

//oiSR (Oxsomi Shader Reflection) stores the source-level (pre-codegen) symbol AST of a shader.
//It's a compiler-frontend reflection: entrypoints, functions, user types, resources, enums and their
// source locations, before DXIL/SPIR-V is generated.
//This is what powers editor intelligence (semantic highlighting, outline, go-to-definition).
//It's produced by walking a backend reflector (today DXC's IHLSLReflectionData) into this neutral model,
// so the model, not the producer, is the stable contract; a future Slang walker emits the same SRFile.
//Usually it's embedded as a subfile of an oiSH (only when reflection is enabled), but it's standalone here.

//Which reflection tiers a file carries, mirroring the backend's feature mask.
//Stripping ESRFeature_SymbolInfo removes names and source locations; the others gate node categories.

typedef enum ESRFeature {

	ESRFeature_None        = 0,

	ESRFeature_Basics      = 1 << 0,        //cbuffer and registers only
	ESRFeature_Functions   = 1 << 1,
	ESRFeature_Namespaces  = 1 << 2,
	ESRFeature_UserTypes   = 1 << 3,        //struct, enum, typedef, union, interface
	ESRFeature_Scopes      = 1 << 4,        //variables, structs, functions defined inside functions (control flow)

	ESRFeature_SymbolInfo  = 1 << 16,       //Names and file/line/column info

	ESRFeature_All         = ESRFeature_SymbolInfo - 1

} ESRFeature;

//A node's kind, a backend-neutral mirror of the frontend AST symbol categories.
//Keep this in the same order as the producer's enum so the walker is a validated cast.

typedef enum ESRNodeType {

	ESRNodeType_Register,
	ESRNodeType_Function,
	ESRNodeType_Enum,
	ESRNodeType_EnumValue,
	ESRNodeType_Namespace,

	ESRNodeType_Variable,                   //localId points to the type
	ESRNodeType_Typedef,                    //^

	ESRNodeType_Struct,                     //Has child variables like buffers do, localId is the typeId (if not fwd decl)
	ESRNodeType_Union,                      //^

	ESRNodeType_StaticVariable,

	ESRNodeType_Interface,
	ESRNodeType_Parameter,

	//Control flow, for full inspection of what variables exist where and in which scope

	ESRNodeType_IfRoot,
	ESRNodeType_Scope,
	ESRNodeType_Do,
	ESRNodeType_Switch,
	ESRNodeType_While,
	ESRNodeType_For,

	ESRNodeType_GroupsharedVariable,

	//Branches

	ESRNodeType_Case,
	ESRNodeType_Default,

	ESRNodeType_Using,

	ESRNodeType_IfFirst,
	ESRNodeType_ElseIf,
	ESRNodeType_Else,

	ESRNodeType_Count

} ESRNodeType;

const C8 *ESRNodeType_name(ESRNodeType type);

//Interpolation qualifier of a variable/parameter, mirror of D3D_INTERPOLATION_MODE.

typedef enum ESRInterpolation {
	ESRInterpolation_Undefined,
	ESRInterpolation_Constant,
	ESRInterpolation_Linear,
	ESRInterpolation_LinearCentroid,
	ESRInterpolation_LinearNoperspective,
	ESRInterpolation_LinearNoperspectiveCentroid,
	ESRInterpolation_LinearSample,
	ESRInterpolation_LinearNoperspectiveSample,
	ESRInterpolation_Count
} ESRInterpolation;

const C8 *ESRInterpolation_name(ESRInterpolation mode);

typedef enum ESRNodeFlag {
	ESRNodeFlag_None         = 0,
	ESRNodeFlag_IsFwdDeclare = 1 << 0,      //This node is a forward declaration (its fwdBckDeclareNode has the definition)
	ESRNodeFlag_HasReturn    = 1 << 1,      //Function nodes: the function returns a value (rather than void)
	ESRNodeFlag_ParamReturn  = 1 << 2,      //Parameter nodes: this is the return-value slot (D3D_RETURN_PARAMETER_INDEX)
	ESRNodeFlag_ParamIn      = 1 << 3,      //Parameter nodes: has 'in'  (in + out = inout)
	ESRNodeFlag_ParamOut     = 1 << 4,      //Parameter nodes: has 'out'
	ESRNodeFlag_Invalid      = 0xFF << 5
} ESRNodeFlag;

//Resource kind of a Register node, a mirror of D3D_SHADER_INPUT_TYPE (frontend reflection: kind/shape only,
// the space/bindPoint are backend concerns and left invalid here).

typedef enum ESRResourceType {
	ESRResourceType_CBuffer,
	ESRResourceType_TBuffer,
	ESRResourceType_Texture,
	ESRResourceType_Sampler,
	ESRResourceType_UAVRWTyped,
	ESRResourceType_Structured,
	ESRResourceType_UAVRWStructured,
	ESRResourceType_ByteAddress,
	ESRResourceType_UAVRWByteAddress,
	ESRResourceType_UAVAppendStructured,
	ESRResourceType_UAVConsumeStructured,
	ESRResourceType_UAVRWStructuredWithCounter,
	ESRResourceType_RaytracingAccelerationStructure,
	ESRResourceType_UAVFeedbackTexture,
	ESRResourceType_Count
} ESRResourceType;

const C8 *ESRResourceType_name(ESRResourceType type);

//Resource dimension, a mirror of D3D_SRV_DIMENSION.

typedef enum ESRResourceDimension {
	ESRResourceDimension_Unknown,
	ESRResourceDimension_Buffer,
	ESRResourceDimension_Texture1D,
	ESRResourceDimension_Texture1DArray,
	ESRResourceDimension_Texture2D,
	ESRResourceDimension_Texture2DArray,
	ESRResourceDimension_Texture2DMS,
	ESRResourceDimension_Texture2DMSArray,
	ESRResourceDimension_Texture3D,
	ESRResourceDimension_TextureCube,
	ESRResourceDimension_TextureCubeArray,
	ESRResourceDimension_BufferEx,
	ESRResourceDimension_Count
} ESRResourceDimension;

//Component return type of a typed resource, a mirror of D3D_RESOURCE_RETURN_TYPE (0 = none/unknown).

typedef enum ESRResourceReturnType {
	ESRResourceReturnType_None,
	ESRResourceReturnType_UNorm,
	ESRResourceReturnType_SNorm,
	ESRResourceReturnType_SInt,
	ESRResourceReturnType_UInt,
	ESRResourceReturnType_Float,
	ESRResourceReturnType_Mixed,
	ESRResourceReturnType_Double,
	ESRResourceReturnType_Continued,
	ESRResourceReturnType_Count
} ESRResourceReturnType;

//Underlying integer type of an enum, a mirror of D3D12_HLSL_ENUM_TYPE.

typedef enum ESREnumType {
	ESREnumType_U32,
	ESREnumType_I32,
	ESREnumType_U64,
	ESREnumType_I64,
	ESREnumType_U16,
	ESREnumType_I16,
	ESREnumType_Count
} ESREnumType;

const C8 *ESREnumType_name(ESREnumType type);

//Type class of a value node's type (Variable/Parameter/Typedef/Struct/...), a mirror of D3D_SHADER_VARIABLE_CLASS.

typedef enum ESRTypeClass {
	ESRTypeClass_Scalar,
	ESRTypeClass_Vector,
	ESRTypeClass_MatrixRows,
	ESRTypeClass_MatrixColumns,
	ESRTypeClass_Object,               //Textures, samplers, buffers and other resource/handle types
	ESRTypeClass_Struct,
	ESRTypeClass_InterfaceClass,
	ESRTypeClass_InterfacePointer,
	ESRTypeClass_Count
} ESRTypeClass;

const C8 *ESRTypeClass_name(ESRTypeClass type);

//Source location of a node, the strippable "SymbolInfo" tier.
//Symbols, when present, are parallel to nodes (symbols[i] describes nodes[i]).

typedef struct SRSymbol {

	U32 fileNameId;                         //Into SRFile::names, U32_MAX = none

	U32 line;                               //1-based first line
	U32 lineCount;

	U32 columnStart;
	U32 columnEnd;

	U32 padding;

} SRSymbol;

//A symbol AST node.
//POD and hashable (treated as a raw byte buffer).
//Node order matches the producer, so parent/child indices are direct.
//Direct children are those nodes whose parent == this node's index.

typedef struct SRNode {

	U32 nameId;                             //Into SRFile::names, U32_MAX = anonymous
	U32 semanticId;                         //Into SRFile::names, U32_MAX = none

	U32 localId;                            //Type/enum/backref id, meaning depends on type; U32_MAX = none
	U32 parent;                             //Parent node index, U32_MAX = root

	U32 fwdBckDeclareNode;                  //Linked forward/backward declaration node, U32_MAX = none
	U32 childCount;                         //Number of direct children

	U32 annotationStart;                    //First annotation into SRFile::annotations, U32_MAX = none
	U16 annotationCount;

	U8 type;                                //ESRNodeType
	U8 interpolation;                       //ESRInterpolation

	U8 flags;                               //ESRNodeFlag
	U8 padding[3];

} SRNode;

//An annotation on a node ([shader(...)] builtins, or [[oxc::...]] / [[vk::...]] custom attributes).

typedef struct SRAnnotation {
	U32 nameId;                             //Into SRFile::names
	U8 isBuiltin;                           //[name] (builtin) vs [[name]] (custom)
	U8 padding[3];
} SRAnnotation;

//Frontend bind info for a Register node (kind/shape).
//Keyed by node index; space/bindPoint are backend-only.

typedef struct SRRegister {

	U32 nodeId;                             //The Register node this describes

	U8 type;                                //ESRResourceType
	U8 dimension;                           //ESRResourceDimension
	U8 returnType;                          //ESRResourceReturnType
	U8 arrayDimCount;                       //Resource-array dimension count in arrayDims[] (0/1 = use bindCount; >=2 multi)

	U32 bindCount;                          //Descriptor count (array size); 0 = unbounded, 1 = single

	U32 arrayDimStart;                      //Into SRFile::arrayDims when arrayDimCount >= 2, else U32_MAX

} SRRegister;

//A single enumerator value.
//Keyed by its EnumValue node; enumType is the parent enum's underlying type.

typedef struct SREnumValue {

	U32 nodeId;                             //The EnumValue node this describes
	U8 enumType;                            //ESREnumType (the parent enum's underlying integer type)
	U8 padding[3];

	I64 value;

} SREnumValue;

//The resolved type of a value node (Variable/Parameter/StaticVariable/GroupsharedVariable/Typedef/Struct/Union).
//Keyed by node index; typeName is the frontend spelling ("float3", "Light", "Texture2D"), the class/rows/cols/elements
// let a consumer distinguish scalar/vector/matrix/struct/object without parsing the name.
//This resolves the node localIds that would otherwise dangle (a value node's localId indexes the frontend type table,
// which isn't serialized).

typedef struct SRType {

	U32 nodeId;                             //The value node this type describes

	U8 typeClass;                           //ESRTypeClass
	U8 rows;                                //Matrix row count (1 for scalar/vector/object)
	U8 cols;                                //Vector width / matrix column count (1 for scalar/object)
	U8 arrayDimCount;                       //Array dimension count in arrayDims[] (0/1 = use `elements`; >=2 = multi-dim)

	U32 elements;                           //Total array element count (product of dims), 0 = not an array

	U32 typeNameId;                         //Underlying type spelling into names[] (resolved), or U32_MAX
	U32 displayNameId;                      //Display type spelling into names[] (the alias the user wrote, for tooltips);
	// U32_MAX = same as typeName

	U32 defNodeId;                          //Struct/Union node that DEFINES this type (go-to-definition), U32_MAX = none
	U32 baseNodeId;                         //Base-class struct node (single inheritance), U32_MAX = none
	U32 arrayDimStart;                      //Into SRFile::arrayDims when arrayDimCount >= 2, else U32_MAX

} SRType;

//One interface a struct/union implements.
//A struct can implement several, so these are edges in their own table (keyed by the implementing node) rather than a
// field on SRType.
//The concrete base class stays on SRType.baseNodeId.

typedef struct SRInterface {

	U32 nodeId;                             //The Struct/Union node that implements the interface
	U32 interfaceNodeId;                    //The Interface node it implements

} SRInterface;

TList(SRNode);
TList(SRSymbol);
TList(SRAnnotation);
TList(SRRegister);
TList(SREnumValue);
TList(SRType);
TList(SRInterface);

typedef enum ESRSettingsFlags {
	ESRSettingsFlags_None             = 0,
	ESRSettingsFlags_HideMagicNumber  = 1 << 0,      //Set when embedded as a subfile (e.g. inside oiSH); not hashed
	ESRSettingsFlags_HasSymbols       = 1 << 1,      //Per-node source locations present (symbols parallel to nodes)
	ESRSettingsFlags_CreateNoReserve  = 1 << 2,      //Only for SRFile_create: skip the initial reserve
	ESRSettingsFlags_Invalid          = 0xFFFFFFFF << 3
} ESRSettingsFlags;

typedef struct SRFile {

	DLFile names;                //String pool: node names, semantic names, annotation names, file names

	ListSRNode nodes;
	ListSRSymbol symbols;        //Empty if symbols weren't reflected, else parallel to nodes (length == nodes.length)
	ListSRAnnotation annotations;

	ListSRRegister registers;    //Frontend bind info for Register nodes
	ListSREnumValue enumValues;  //Enumerators for Enum nodes
	ListSRType types;            //Resolved types for value nodes (Variable/Parameter/Typedef/Struct/...)
	ListU32 arrayDims;           //Shared pool of multi-dimensional array lengths, referenced by SRType/SRRegister
	ListSRInterface interfaces;  //Interface-implementation edges (struct/union node -> interface node)

	ESRSettingsFlags flags;
	U32 features;                //ESRFeature bitset: which reflection tiers this file carries

	U64 hash;                    //Refreshed by SRFile_finalize

} SRFile;

TList(SRFile);

Bool SRFile_create(
	ESRSettingsFlags flags,
	U32 features,
	const Allocator *alloc,
	SRFile *srFile,
	Error *e_rr
);

Bool SRFile_createCopy(const SRFile *src, const Allocator *alloc, SRFile *srFile, Error *e_rr);

void SRFile_free(SRFile *srFile, const Allocator *alloc);

void ListSRFile_freeUnderlying(ListSRFile *files, const Allocator *alloc);

//Add a string to the pool (deduplicated), returning its id.
//U32_MAX means "no string"; passing an empty/null CharString returns U32_MAX without adding.
Bool SRFile_addString(SRFile *srFile, CharString *str, const Allocator *alloc, U32 *id, Error *e_rr);

//Index of the first node named `name` whose type is `type` (Struct/Union both match ESRNodeType_Struct, they're the
// record kinds), or U32_MAX.
//Resolves a type/base/interface name back to its defining node (references are by name because the frontend reflector
// reuses type indices across uses of the same type).
U32 SRFile_findNodeByName(const SRFile *srFile, CharString name, ESRNodeType type);

//Recompute the content hash.
//Call once the nodes/symbols/annotations/strings are finalized.
Bool SRFile_finalize(SRFile *srFile, const Allocator *alloc, Error *e_rr);

Bool SRFile_write(
	const SRFile *srFile,
	const Allocator *alloc,
	StreamRef *streamRef,        //Pass NULL to calculate length only (*offset)
	U64 *offset,
	Error *e_rr
);

Bool SRFile_read(StreamRef *streamRef, U64 *offset, Bool isSubFile, const Allocator *alloc, SRFile *srFile, Error *e_rr);

//Logs a stringified SRFile tree directly.
//isVerbose dumps every field (ids, localId, parent/child, flags, full source spans, and the raw register/enum-value
// records) so a serialized oiSR can be reviewed exactly.
//collapseBuiltins folds nodes from builtin includes (@types.hlsli etc.) and their descendants into a per-file
// summary, so a shader's own symbols aren't buried under the couple hundred builtin symbols the includes pull in.
void SRFile_print(const SRFile *srFile, U64 indenting, Bool isVerbose, Bool collapseBuiltins, const Allocator *alloc);

//File headers (file spec: docs/oiSR.md)

typedef enum ESRVersion {
	ESRVersion_Undefined,
	ESRVersion_V1_1            //Current (on-disk version byte 1, displayed as major.minor 1.1); no shipped file predates it
} ESRVersion;

typedef enum ESRFlag {
	ESRFlag_None        = 0,
	ESRFlag_HasSymbols  = 1 << 0,
	ESRFlag_Unsupported = 0xFF << 1
} ESRFlag;

typedef struct SRHeader {

	U8 version;                //ESRVersion
	U8 flags;                  //ESRFlag
	U16 padding;

	U32 features;              //ESRFeature

	U32 nodeCount;
	U32 annotationCount;

	U32 registerCount;
	U32 enumValueCount;

	U32 typeCount;             //Per-node type records (Variable/Typedef/Struct/Union)
	U32 arrayDimCount;         //Shared multi-dimensional array-length pool
	U32 interfaceCount;        //Interface-implementation edges

} SRHeader;

#define SRHeader_MAGIC 0x5253696F

#ifdef __cplusplus
	}
#endif

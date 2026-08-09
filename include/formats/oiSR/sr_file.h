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

typedef enum ESRNodeFlag {
	ESRNodeFlag_None         = 0,
	ESRNodeFlag_IsFwdDeclare = 1 << 0,      //This node is a forward declaration (its fwdBckDeclareNode has the definition)
	ESRNodeFlag_Invalid      = 0xFF << 1
} ESRNodeFlag;

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

//A symbol AST node. POD and hashable (treated as a raw byte buffer).
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

TList(SRNode);
TList(SRSymbol);
TList(SRAnnotation);

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

//Recompute the content hash. Call once the nodes/symbols/annotations/strings are finalized.
Bool SRFile_finalize(SRFile *srFile, const Allocator *alloc, Error *e_rr);

Bool SRFile_write(
	const SRFile *srFile,
	const Allocator *alloc,
	StreamRef *streamRef,        //Pass NULL to calculate length only (*offset)
	U64 *offset,
	Error *e_rr
);

Bool SRFile_read(StreamRef *streamRef, U64 *offset, Bool isSubFile, const Allocator *alloc, SRFile *srFile, Error *e_rr);

//Logs a stringified SRFile tree directly
void SRFile_print(const SRFile *srFile, U64 indenting, const Allocator *alloc);

//File headers (file spec: docs/oiSR.md)

typedef enum ESRVersion {
	ESRVersion_Undefined,
	ESRVersion_V1_0            //Current
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

} SRHeader;

#define SRHeader_MAGIC 0x5253696F

#ifdef __cplusplus
	}
#endif

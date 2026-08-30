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

//shader_compiler/test/test_shader_compiler_reflect_sr.c

#include "test_shader_compiler_shared.h"
#include "shader_compiler/compiler.h"
#include "platforms/platform.h"
#include "formats/oiSR/sr_file.h"
#include "types/container/log.h"
#include "types/container/memory_stream.h"
#include "types/base/error.h"
#include "types/base/string_base.h"
#include "types/base/string_read_helper.h"

//Find the first node of a given type whose name matches, U32_MAX if none.
static U32 srFindNode(const SRFile *sr, ESRNodeType type, const C8 *name) {

	CharString n = CharString_createRefCStrConst(name);

	for (U64 i = 0; i < sr->nodes.length; ++i) {

		const SRNode *node = &sr->nodes.ptr[i];

		if (node->type != type || node->nameId == U32_MAX)
			continue;

		if (CharString_equalsStringSensitive(&sr->names.entryStrings.ptr[node->nameId], &n))
			return (U32) i;
	}

	return U32_MAX;
}

//Reflect a small shader (struct + annotated compute entry with a semantic parameter) through the real DXC
// IHLSLReflection frontend and verify Compiler_reflect fills an SRFile, then that the SRFile round-trips.

void Test_shaderCompilerReflectSR(Test *t) {

	Test_setModule(t, "Compiler reflect (oiSR)");

	const Allocator *alloc = Platform_instance->alloc;
	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	Compiler comp = (Compiler) { 0 };
	SRFile reflection = (SRFile) { 0 };
	SRFile roundTrip = (SRFile) { 0 };
	StreamRef *stream = NULL;
	Bool created = false;

	const RefPtrType streamType = MemoryStream_makeType(alloc);

	static const C8 *src =
		"struct Light { float3 pos; float3 color; };\n"
		"enum Mode { ModeA, ModeB };\n"
		"interface IArea { float area(); };\n"
		"struct Circle : IArea { float r; float area() { return r * r; } };\n"
		"RWByteAddressBuffer buf;\n"
		"StructuredBuffer<Light> lights;\n"
		"uint dbl(uint x) { return x * 2; }\n"
		"[[oxc::stage(\"compute\")]]\n"
		"[numthreads(1, 1, 1)]\n"
		"void main(uint id : SV_DispatchThreadID) {\n"
		"\tCircle c; c.r = 2;\n"
		"\tbuf.Store<uint>(id * 4, dbl(id) + (uint) lights[0].pos.x + (uint) c.area());\n"
		"}\n";

	gotoIfError3(clean, Compiler_create(alloc, &comp, e_rr));
	created = true;

	CompilerSettings settings = (CompilerSettings) {
		.string = CharString_createRefCStrConst(src),
		.path = CharString_createRefCStrConst("test_reflect_sr.hlsl"),
		.format = ECompilerFormat_HLSL,
		.outputType = ESHBinaryType_SPIRV
	};

	gotoIfError3(clean, Compiler_reflect(&comp, &settings, alloc, &reflection, e_rr));

	Test_assert(t, "reflect produced nodes", reflection.nodes.length > 0);

	//The source-location tier is present only when the linked dxc guards GetNodeSymbolDesc.
	//Accept either state: if present it must be parallel to nodes with the SymbolInfo feature; if absent there's no
	// location array.

	Bool hasSyms = (reflection.flags & ESRSettingsFlags_HasSymbols) != 0;
	Test_assert(t, "symbol tier consistent",
		hasSyms
			? (reflection.symbols.length == reflection.nodes.length && (reflection.features & ESRFeature_SymbolInfo))
			: (reflection.symbols.length == 0));

	//The struct and its members should be present (USER_TYPES tier)

	U32 lightId = srFindNode(&reflection, ESRNodeType_Struct, "Light");
	Test_assert(t, "struct Light reflected", lightId != U32_MAX);

	//The annotated entrypoint should be present with at least one annotation ([[oxc::stage]] / [numthreads])

	U32 mainId = srFindNode(&reflection, ESRNodeType_Function, "main");
	Test_assert(t, "function main reflected", mainId != U32_MAX);
	Test_assert(t, "some annotations captured", reflection.annotations.length > 0);

	if (mainId != U32_MAX)
		Test_assert(t, "main has annotations", reflection.nodes.ptr[mainId].annotationCount > 0);

	//Detail tiers: resource bind info (buf), enum values (ModeA/ModeB), function return (dbl)

	Test_assert(t, "register reflected", reflection.registers.length > 0);
	Test_assert(t, "enum values reflected", reflection.enumValues.length >= 2);

	//Type tier: Variable/Struct/Typedef/Union nodes carry a resolved type.
	//Light's members (pos/color) are float3 vectors, and the Light struct node resolves to its own "Light" struct type.

	Test_assert(t, "types reflected", reflection.types.length > 0);

	U32 posId = srFindNode(&reflection, ESRNodeType_Variable, "pos");
	Test_assert(t, "member pos reflected", posId != U32_MAX);

	if (posId != U32_MAX) {

		const SRType *posTy = NULL;

		for (U64 i = 0; i < reflection.types.length; ++i)
			if (reflection.types.ptr[i].nodeId == posId) { posTy = &reflection.types.ptr[i]; break; }

		Test_assert(t, "member pos has a type", posTy != NULL);

		if (posTy) {

			Test_assert(t, "member pos type is a 3-wide vector",
				posTy->typeClass == ESRTypeClass_Vector && posTy->cols == 3);

			CharString tn = posTy->typeNameId != U32_MAX ?
				reflection.names.entryStrings.ptr[posTy->typeNameId] : CharString_createNull();
			CharString f3 = CharString_createRefCStrConst("float3");
			Test_assert(t, "member pos type name is float3", CharString_equalsStringSensitive(&tn, &f3));
		}
	}

	if (lightId != U32_MAX) {

		const SRType *lightTy = NULL;

		for (U64 i = 0; i < reflection.types.length; ++i)
			if (reflection.types.ptr[i].nodeId == lightId) { lightTy = &reflection.types.ptr[i]; break; }

		Test_assert(t, "struct Light resolves to a struct type", lightTy && lightTy->typeClass == ESRTypeClass_Struct);
	}

	//Go-to-definition: the StructuredBuffer<Light> element is a value of struct type, so its type record links back to
	// the Light struct's defining node (resolved by name, since the reflector reuses type indices across uses).

	U32 elemId = srFindNode(&reflection, ESRNodeType_Variable, "$Element");
	Test_assert(t, "structured-buffer element reflected", elemId != U32_MAX);

	if (elemId != U32_MAX && lightId != U32_MAX) {

		const SRType *elemTy = NULL;

		for (U64 i = 0; i < reflection.types.length; ++i)
			if (reflection.types.ptr[i].nodeId == elemId) { elemTy = &reflection.types.ptr[i]; break; }

		Test_assert(t, "element has a type", elemTy != NULL);
		Test_assert(t, "element type points at the Light definition", elemTy && elemTy->defNodeId == lightId);
	}

	//Interface implementations: struct Circle implements interface IArea, captured as an edge in the interfaces table.

	U32 circleId = srFindNode(&reflection, ESRNodeType_Struct, "Circle");
	U32 iareaId = srFindNode(&reflection, ESRNodeType_Interface, "IArea");
	Test_assert(t, "interface + implementing struct reflected", circleId != U32_MAX && iareaId != U32_MAX);

	if (circleId != U32_MAX && iareaId != U32_MAX) {

		Bool found = false;

		for (U64 i = 0; i < reflection.interfaces.length; ++i)
			if (reflection.interfaces.ptr[i].nodeId == circleId && reflection.interfaces.ptr[i].interfaceNodeId == iareaId)
				found = true;

		Test_assert(t, "Circle implements IArea edge captured", found);
	}

	//Parameter types come from the function-parameter reflection (D3D12_PARAMETER_DESC), not a type localId.
	//The entrypoint's "id" parameter is a uint scalar; verify the reconstructed builtin name.

	U32 idParam = srFindNode(&reflection, ESRNodeType_Parameter, "id");
	Test_assert(t, "parameter id reflected", idParam != U32_MAX);

	if (idParam != U32_MAX) {

		const SRType *idTy = NULL;

		for (U64 i = 0; i < reflection.types.length; ++i)
			if (reflection.types.ptr[i].nodeId == idParam) { idTy = &reflection.types.ptr[i]; break; }

		Test_assert(t, "parameter id has a type", idTy != NULL);

		if (idTy) {

			CharString tn = idTy->typeNameId != U32_MAX ?
				reflection.names.entryStrings.ptr[idTy->typeNameId] : CharString_createNull();
			CharString u = CharString_createRefCStrConst("uint");
			Test_assert(t, "parameter id type is uint",
				idTy->typeClass == ESRTypeClass_Scalar && CharString_equalsStringSensitive(&tn, &u));
		}
	}

	U32 dblId = srFindNode(&reflection, ESRNodeType_Function, "dbl");
	Test_assert(t, "returning function reflected", dblId != U32_MAX);
	if (dblId != U32_MAX)
		Test_assert(t, "returning function has HasReturn", (reflection.nodes.ptr[dblId].flags & ESRNodeFlag_HasReturn) != 0);

	//Every node with a symbol should point at a valid line (basic sanity of the location tier)

	if (reflection.symbols.length == reflection.nodes.length && lightId != U32_MAX)
		Test_assert(t, "struct has a source line", reflection.symbols.ptr[lightId].line > 0);

	//Round-trip the produced SRFile through write/read

	{
		U64 off = 0;

		if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &streamType, &stream, e_rr)) {
			Test_assert(t, "create stream", false);
			goto clean;
		}

		gotoIfError3(clean, SRFile_write(&reflection, alloc, stream, &off, e_rr));

		U64 readOff = 0;
		gotoIfError3(clean, SRFile_read(stream, &readOff, false, alloc, &roundTrip, e_rr));

		Test_assert(t, "round-trip node count", roundTrip.nodes.length == reflection.nodes.length);
		Test_assert(t, "round-trip annotation count", roundTrip.annotations.length == reflection.annotations.length);
		Test_assert(t, "round-trip type count", roundTrip.types.length == reflection.types.length);
		Test_assert(t, "round-trip interface count",
			roundTrip.interfaces.length == reflection.interfaces.length && reflection.interfaces.length > 0);
		Test_assert(t, "round-trip hash matches", roundTrip.hash == reflection.hash);
	}

clean:

	Test_assert(t, "reflect produced no error", s_uccess);

	SRFile_free(&reflection, alloc);
	SRFile_free(&roundTrip, alloc);
	RefPtr_dec(&stream);

	if (created)
		Compiler_free(&comp, alloc);

	Error_print(alloc, e_rr, ELogLevel_Error, ELogOptions_Default);
}

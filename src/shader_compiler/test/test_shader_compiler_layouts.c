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

//shader_compiler/test/test_shader_compiler_layouts.c

#include "test_shader_compiler_shared.h"
#include "shader_compiler/compiler.h"
#include "formats/oiSH/sh_file.h"
#include "formats/oiSB/sb_file.h"
#include "platforms/platform.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/container/log.h"

//Every buffer shape whose layout differs between -spirv-layout dx, gl and scalar, compiled under each layout.
//Each case pins what oiSB records: the size, the packing, the shared claim, the member offsets and the element and
// matrix strides its packing implies.
//Every compiled case is also checked against the binary's own Offset, ArrayStride and MatrixStride decorations, so a pin
// can't hold a layout the binary doesn't have.
//A spacing oiSB's packing can't describe is pinned as a refusal instead; the same shape under the other layouts is
// the control that it's the layout being refused and not the shape.

#define LAYOUT_SOURCE(decl, ext, body)                                                                      \
	decl "\nRWByteAddressBuffer o;\n" ext "[[oxc::stage(\"compute\")]]\n[numthreads(1, 1, 1)]\n"              \
	"void main(uint id : SV_DispatchThreadID) { " body " }\n"

#define SRC_CB_STRADDLE LAYOUT_SOURCE("cbuffer CB { uint pad; uint4 test; }", "", "o.Store<uint>(0, pad + test.x);")

#define SRC_CB_ARRAY LAYOUT_SOURCE(                                                                          \
	"cbuffer CB { float a; float arr[3]; float b; }", "", "o.Store<float>(0, a + arr[1] + b);"               \
)

#define SRC_CB_STRUCT LAYOUT_SOURCE(                                                                         \
	"struct Inner { float x; float2 y; };\ncbuffer CB { float a; Inner s; float b; }", "",                   \
	"o.Store<float>(0, a + s.y.x + b);"                                                                      \
)

#define SRC_CB_MATRIX LAYOUT_SOURCE(                                                                         \
	"cbuffer CB { float a; float3x3 m; float b; }", "", "o.Store<float>(0, a + m[1][2] + b);"                \
)

#define SRC_CB_MAT_ARRAY LAYOUT_SOURCE("cbuffer CB { float a; float3x3 m[2]; }", "", "o.Store<float>(0, a + m[1][1][2]);")

#define SRC_CB_F64_MATRIX LAYOUT_SOURCE(                                                                     \
	"cbuffer CB { double a; double3x3 m; }", "[[oxc::extension(\"F64\")]]\n",                                \
	"o.Store<float>(0, (float) (a + m[1][2]));"                                                              \
)

#define SRC_SB_STRADDLE LAYOUT_SOURCE(                                                                       \
	"struct E { uint pad; uint4 test; };\nStructuredBuffer<E> sb;", "", "o.Store<uint>(0, sb[id].test.y);"   \
)

#define SRC_SB_VEC3_ARRAY LAYOUT_SOURCE(                                                                     \
	"struct E { float a; float3 arr[2]; };\nStructuredBuffer<E> sb;", "", "o.Store<float>(0, sb[id].arr[1].y);" \
)

#define SRC_SB_STRUCT LAYOUT_SOURCE(                                                                         \
	"struct Inner { float x; float2 y; };\nstruct E { float a; Inner s; float b; };\nStructuredBuffer<E> sb;", \
	"", "o.Store<float>(0, sb[id].s.y.x + sb[id].b);"                                                        \
)

#define SRC_SB_MATRIX LAYOUT_SOURCE(                                                                         \
	"struct E { float a; float3x3 m; };\nStructuredBuffer<E> sb;", "", "o.Store<float>(0, sb[id].m[1][2]);"  \
)

#define SRC_SB_MAT_ELEM LAYOUT_SOURCE("StructuredBuffer<float3x3> sb;", "", "o.Store<float>(0, sb[id][1][2]);")

#define SRC_PC_VEC3 LAYOUT_SOURCE(                                                                           \
	"[[vk::push_constant]] struct PC { float a; float3 b; float c; } pc;", "",                               \
	"o.Store<float>(0, pc.a + pc.b.x + pc.c);"                                                               \
)

#define SRC_PC_ARRAY LAYOUT_SOURCE(                                                                          \
	"[[vk::push_constant]] struct PC { float a; float arr[3]; } pc;", "", "o.Store<float>(0, pc.a + pc.arr[2]);" \
)

#define SRC_PC_ARRAY_BIG LAYOUT_SOURCE(                                                                      \
	"[[vk::push_constant]] struct PC { float a; float arr[3]; float4x4 big; } pc;", "",                      \
	"o.Store<float>(0, pc.a + pc.arr[2] + pc.big[3][3]);"                                                    \
)

#define SRC_PC_MAT2 LAYOUT_SOURCE(                                                                           \
	"[[vk::push_constant]] struct PC { float a; float2x2 m; float4x4 big; } pc;", "",                        \
	"o.Store<float>(0, pc.a + pc.m[1][1] + pc.big[3][3]);"                                                   \
)

#define SRC_PC_MAT2_ROW LAYOUT_SOURCE(                                                                       \
	"[[vk::push_constant]] struct PC { float4 pad; float2x2 m; float4x4 big; } pc;", "",                     \
	"o.Store<float>(0, pc.pad.x + pc.m[1][1] + pc.big[3][3]);"                                               \
)

typedef struct LayoutCase {

	const C8 *name;
	const C8 *src;
	const C8 *refusedBecause;    //Non NULL when the compile must be refused, and why

	U8 layout;                  //ESpirvLayout
	U8 packed;
	U8 shared;
	U8 topCount;

	U32 size;

	U8 childCount;              //Children of the first struct variable, relative to it
	U8 padding;
	U16 elementStride;          //What the packing implies for the first array, 0 without one

	U16 matrixStride;           //What the packing implies for the first matrix's rows, 0 without one
	U16 padding1;

	U32 top[3];
	U32 child[3];

} LayoutCase;

static const LayoutCase layoutCases[] = {

	//A uint4 that would straddle a 16-byte row: dx and gl move it to the next row, scalar packs it right behind

	{ .name = "cb_straddle", .src = SRC_CB_STRADDLE, .layout = ESpirvLayout_Dx,
		.size = 32, .shared = 1, .topCount = 2, .top = { 0, 16 } },
	{ .name = "cb_straddle", .src = SRC_CB_STRADDLE, .layout = ESpirvLayout_Gl,
		.size = 32, .topCount = 2, .top = { 0, 16 } },
	{ .name = "cb_straddle", .src = SRC_CB_STRADDLE, .layout = ESpirvLayout_Scalar,
		.size = 32, .packed = 1, .topCount = 2, .top = { 0, 4 } },

	//Arrays: rows of 16 under dx and std140, back to back under scalar

	{ .name = "cb_array", .src = SRC_CB_ARRAY, .layout = ESpirvLayout_Dx,
		.size = 64, .shared = 1, .topCount = 3, .top = { 0, 16, 52 }, .elementStride = 16 },
	{ .name = "cb_array", .src = SRC_CB_ARRAY, .layout = ESpirvLayout_Gl,
		.size = 80, .topCount = 3, .top = { 0, 16, 64 }, .elementStride = 16 },
	{ .name = "cb_array", .src = SRC_CB_ARRAY, .layout = ESpirvLayout_Scalar,
		.size = 32, .packed = 1, .topCount = 3, .top = { 0, 4, 16 }, .elementStride = 4 },

	//A struct member: std140 rounds its size up to 16, scalar starts it off a row

	{ .name = "cb_struct", .src = SRC_CB_STRUCT, .layout = ESpirvLayout_Dx,
		.size = 32, .shared = 1, .topCount = 3, .top = { 0, 16, 28 }, .childCount = 2, .child = { 0, 4 } },
	{ .name = "cb_struct", .src = SRC_CB_STRUCT, .layout = ESpirvLayout_Gl,
		.size = 48, .topCount = 3, .top = { 0, 16, 32 }, .childCount = 2, .child = { 0, 4 } },
	{ .name = "cb_struct", .src = SRC_CB_STRUCT, .layout = ESpirvLayout_Scalar,
		.size = 32, .packed = 1, .topCount = 3, .top = { 0, 4, 16 }, .childCount = 2, .child = { 0, 4 } },

	//Matrices: rows of 16 under dx and std140 (std140 also pads the last), 12 byte rows under scalar

	{ .name = "cb_matrix", .src = SRC_CB_MATRIX, .layout = ESpirvLayout_Dx,
		.size = 64, .shared = 1, .topCount = 3, .top = { 0, 16, 60 }, .matrixStride = 16 },
	{ .name = "cb_matrix", .src = SRC_CB_MATRIX, .layout = ESpirvLayout_Gl,
		.size = 80, .topCount = 3, .top = { 0, 16, 64 }, .matrixStride = 16 },
	{ .name = "cb_matrix", .src = SRC_CB_MATRIX, .layout = ESpirvLayout_Scalar,
		.size = 48, .packed = 1, .topCount = 3, .top = { 0, 4, 40 }, .matrixStride = 12 },

	{ .name = "cb_mat_array", .src = SRC_CB_MAT_ARRAY, .layout = ESpirvLayout_Dx,
		.size = 112, .shared = 1, .topCount = 2, .top = { 0, 16 }, .elementStride = 48, .matrixStride = 16 },
	{ .name = "cb_mat_array", .src = SRC_CB_MAT_ARRAY, .layout = ESpirvLayout_Gl,
		.size = 112, .topCount = 2, .top = { 0, 16 }, .elementStride = 48, .matrixStride = 16 },
	{ .name = "cb_mat_array", .src = SRC_CB_MAT_ARRAY, .layout = ESpirvLayout_Scalar,
		.size = 80, .packed = 1, .topCount = 2, .top = { 0, 4 }, .elementStride = 36, .matrixStride = 12 },

	{ .name = "cb_f64_matrix", .src = SRC_CB_F64_MATRIX, .layout = ESpirvLayout_Dx,
		.size = 128, .shared = 1, .topCount = 2, .top = { 0, 32 }, .matrixStride = 32 },
	{ .name = "cb_f64_matrix", .src = SRC_CB_F64_MATRIX, .layout = ESpirvLayout_Gl,
		.size = 128, .topCount = 2, .top = { 0, 32 }, .matrixStride = 32 },
	{ .name = "cb_f64_matrix", .src = SRC_CB_F64_MATRIX, .layout = ESpirvLayout_Scalar,
		.size = 80, .packed = 1, .topCount = 2, .top = { 0, 8 }, .matrixStride = 24 },

	//Structured buffers: the element struct is the first struct, its members are the children

	{ .name = "sb_straddle", .src = SRC_SB_STRADDLE, .layout = ESpirvLayout_Dx,
		.size = 20, .packed = 1, .shared = 1, .topCount = 1, .childCount = 2, .child = { 0, 4 } },
	{ .name = "sb_straddle", .src = SRC_SB_STRADDLE, .layout = ESpirvLayout_Gl,
		.size = 32, .packed = 1, .topCount = 1, .childCount = 2, .child = { 0, 16 } },
	{ .name = "sb_straddle", .src = SRC_SB_STRADDLE, .layout = ESpirvLayout_Scalar,
		.size = 20, .packed = 1, .topCount = 1, .childCount = 2, .child = { 0, 4 } },

	{ .name = "sb_vec3_array", .src = SRC_SB_VEC3_ARRAY, .layout = ESpirvLayout_Dx,
		.size = 28, .packed = 1, .shared = 1, .topCount = 1, .childCount = 2, .child = { 0, 4 }, .elementStride = 12 },
	{ .name = "sb_vec3_array", .src = SRC_SB_VEC3_ARRAY, .layout = ESpirvLayout_Gl,
		.refusedBecause = "std430 spaces float3 array elements 16 apart, which a tightly packed buffer can't describe" },
	{ .name = "sb_vec3_array", .src = SRC_SB_VEC3_ARRAY, .layout = ESpirvLayout_Scalar,
		.size = 28, .packed = 1, .topCount = 1, .childCount = 2, .child = { 0, 4 }, .elementStride = 12 },

	{ .name = "sb_struct", .src = SRC_SB_STRUCT, .layout = ESpirvLayout_Dx,
		.size = 20, .packed = 1, .shared = 1, .topCount = 1, .childCount = 3, .child = { 0, 4, 16 } },
	{ .name = "sb_struct", .src = SRC_SB_STRUCT, .layout = ESpirvLayout_Gl,
		.refusedBecause = "DXC emits SPIR-V spirv-val rejects for this nested struct (DXC issue 2639)" },
	{ .name = "sb_struct", .src = SRC_SB_STRUCT, .layout = ESpirvLayout_Scalar,
		.size = 20, .packed = 1, .topCount = 1, .childCount = 3, .child = { 0, 4, 16 } },

	{ .name = "sb_matrix", .src = SRC_SB_MATRIX, .layout = ESpirvLayout_Dx,
		.size = 40, .packed = 1, .shared = 1, .topCount = 1, .childCount = 2, .child = { 0, 4 }, .matrixStride = 12 },
	{ .name = "sb_matrix", .src = SRC_SB_MATRIX, .layout = ESpirvLayout_Gl,
		.refusedBecause = "std430 spaces float3x3 rows 16 apart, which a tightly packed buffer can't describe" },
	{ .name = "sb_matrix", .src = SRC_SB_MATRIX, .layout = ESpirvLayout_Scalar,
		.size = 40, .packed = 1, .topCount = 1, .childCount = 2, .child = { 0, 4 }, .matrixStride = 12 },

	{ .name = "sb_mat_elem", .src = SRC_SB_MAT_ELEM, .layout = ESpirvLayout_Dx,
		.size = 36, .packed = 1, .shared = 1, .topCount = 1, .matrixStride = 12 },
	{ .name = "sb_mat_elem", .src = SRC_SB_MAT_ELEM, .layout = ESpirvLayout_Gl,
		.refusedBecause = "std430 sizes a float3x3 element at 48 bytes, which a tightly packed buffer can't describe" },
	{ .name = "sb_mat_elem", .src = SRC_SB_MAT_ELEM, .layout = ESpirvLayout_Scalar,
		.size = 36, .packed = 1, .topCount = 1, .matrixStride = 12 },

	//Push constants: DXC packs them tightly under dx and std430 under gl, recorded with rows under dx to combine with DXIL

	{ .name = "pc_vec3", .src = SRC_PC_VEC3, .layout = ESpirvLayout_Dx,
		.size = 20, .shared = 1, .topCount = 3, .top = { 0, 4, 16 } },
	{ .name = "pc_vec3", .src = SRC_PC_VEC3, .layout = ESpirvLayout_Gl,
		.size = 20, .packed = 1, .topCount = 3, .top = { 0, 4, 16 } },
	{ .name = "pc_vec3", .src = SRC_PC_VEC3, .layout = ESpirvLayout_Scalar,
		.size = 20, .packed = 1, .topCount = 3, .top = { 0, 4, 16 } },

	{ .name = "pc_array", .src = SRC_PC_ARRAY, .layout = ESpirvLayout_Dx,
		.refusedBecause = "the tightly packed array spacing differs from the rows DXIL's cbuffer uses, so it can't be shared" },
	{ .name = "pc_array", .src = SRC_PC_ARRAY, .layout = ESpirvLayout_Gl,
		.size = 16, .packed = 1, .topCount = 2, .top = { 0, 4 }, .elementStride = 4 },
	{ .name = "pc_array", .src = SRC_PC_ARRAY, .layout = ESpirvLayout_Scalar,
		.size = 16, .packed = 1, .topCount = 2, .top = { 0, 4 }, .elementStride = 4 },

	//Push constants big enough that the bounds checks can't see a spacing that's too small, so under dx only the
	// stride checks themselves refuse these

	{ .name = "pc_array_big", .src = SRC_PC_ARRAY_BIG, .layout = ESpirvLayout_Dx,
		.refusedBecause = "tight packing spaces the array 4 apart where the rows of DXIL's cbuffer imply 16" },
	{ .name = "pc_array_big", .src = SRC_PC_ARRAY_BIG, .layout = ESpirvLayout_Gl,
		.size = 80, .packed = 1, .topCount = 3, .top = { 0, 4, 16 }, .elementStride = 4, .matrixStride = 16 },
	{ .name = "pc_array_big", .src = SRC_PC_ARRAY_BIG, .layout = ESpirvLayout_Scalar,
		.size = 80, .packed = 1, .topCount = 3, .top = { 0, 4, 16 }, .elementStride = 4, .matrixStride = 16 },

	{ .name = "pc_mat2", .src = SRC_PC_MAT2, .layout = ESpirvLayout_Dx,
		.refusedBecause = "tight packing spaces float2x2 rows 8 apart where the rows of DXIL's cbuffer imply 16" },
	{ .name = "pc_mat2", .src = SRC_PC_MAT2, .layout = ESpirvLayout_Gl,
		.size = 96, .packed = 1, .topCount = 3, .top = { 0, 8, 32 }, .matrixStride = 8 },
	{ .name = "pc_mat2", .src = SRC_PC_MAT2, .layout = ESpirvLayout_Scalar,
		.size = 84, .packed = 1, .topCount = 3, .top = { 0, 4, 20 }, .matrixStride = 8 },

	{ .name = "pc_mat2_row", .src = SRC_PC_MAT2_ROW, .layout = ESpirvLayout_Dx,
		.refusedBecause = "a float2x2 starting its row still spaces its rows 8 apart where DXIL's rows imply 16" },
	{ .name = "pc_mat2_row", .src = SRC_PC_MAT2_ROW, .layout = ESpirvLayout_Gl,
		.size = 96, .packed = 1, .topCount = 3, .top = { 0, 16, 32 }, .matrixStride = 8 }
};

static const C8 *layoutNames[] = { "dx", "gl", "scalar" };

//A correctly reflected layout changed in one field, which the decoration check has to refuse.
//Each change is caught by a different comparison, so none of them can stop comparing unnoticed.

typedef enum ELayoutTamper {
	ELayoutTamper_Offset,           //The second variable moves by 4 bytes
	ELayoutTamper_ArrayDim,         //The second variable's array gains an element
	ELayoutTamper_Type,             //The second variable's vector gains a component
	ELayoutTamper_Packing,          //The packing flips, which changes every implied stride but no offset
	ELayoutTamper_ElementStride     //A structured buffer's element grows by 4 bytes
} ELayoutTamper;

typedef struct LayoutTamper {
	const C8 *what;
	const C8 *src;
	U8 layout;                      //ESpirvLayout
	U8 tamper;                      //ELayoutTamper
	U16 padding;
	U32 padding1;
} LayoutTamper;

static const LayoutTamper layoutTampers[] = {
	{ "a variable moved off its Offset", SRC_CB_ARRAY, ESpirvLayout_Gl, ELayoutTamper_Offset },
	{ "an array dimension the binary doesn't have", SRC_CB_ARRAY, ESpirvLayout_Gl, ELayoutTamper_ArrayDim },
	{ "a type the binary doesn't have", SRC_PC_VEC3, ESpirvLayout_Gl, ELayoutTamper_Type },
	{ "array elements spaced other than their ArrayStride", SRC_CB_ARRAY, ESpirvLayout_Scalar, ELayoutTamper_Packing },
	{ "matrix rows spaced other than their MatrixStride", SRC_SB_MAT_ELEM, ESpirvLayout_Scalar, ELayoutTamper_Packing },
	{ "an element stride other than the runtime array's", SRC_SB_STRADDLE, ESpirvLayout_Scalar, ELayoutTamper_ElementStride }
};

//Whether the reflected file records exactly what the case pins, logging the first difference.

static Bool layoutMatches(const LayoutCase *c, const SBFile *sb, const Allocator *alloc) {

	if (!sb) {
		Log_debugLn(alloc, "%s under %s: no buffer was reflected", c->name, layoutNames[c->layout]);
		return false;
	}

	const Bool packed = (sb->flags & ESBSettingsFlags_IsTightlyPacked) != 0;
	const Bool shared = (sb->flags & ESBSettingsFlags_HasSharedLayout) != 0;

	if (sb->bufferSize != c->size || packed != (c->packed != 0) || shared != (c->shared != 0)) {
		Log_debugLn(
			alloc, "%s under %s: size %u packed %u shared %u", c->name, layoutNames[c->layout],
			sb->bufferSize, (U32) packed, (U32) shared
		);

		return false;
	}

	U8 top = 0;
	U16 firstStruct = U16_MAX;
	U16 elementStride = 0;
	U16 matrixStride = 0;

	for (U64 i = 0; i < sb->vars.length; ++i) {

		const SBVar var = sb->vars.ptr[i];

		if (var.parentId == U16_MAX) {

			if (top >= c->topCount || c->top[top] != var.offset) {
				Log_debugLn(
					alloc, "%s under %s: top level variable %u at %u", c->name, layoutNames[c->layout], (U32) top, var.offset
				);

				return false;
			}

			++top;
		}

		if (var.structId != U16_MAX) {

			if (firstStruct == U16_MAX)
				firstStruct = (U16) i;

			continue;
		}

		const ESBType type = (ESBType) var.type;
		const U32 size = ESBType_getSize(type, packed);

		if (var.arrayDimOrArrayId && !elementStride)
			elementStride = (U16) (packed ? size : (size + 15) & ~15);

		if (ESBType_getMatrix(type) != ESBMatrix_N1 && !matrixStride) {
			const U32 row = ((U32) ESBType_getVector(type) + 1) << (U32) ESBType_getStride(type);
			matrixStride = (U16) (packed ? row : (row + 15) & ~15);
		}
	}

	U8 child = 0;

	for (U64 i = 0; firstStruct != U16_MAX && i < sb->vars.length; ++i) {

		const SBVar var = sb->vars.ptr[i];

		if (var.parentId != firstStruct)
			continue;

		if (child >= c->childCount || c->child[child] != var.offset - sb->vars.ptr[firstStruct].offset) {
			Log_debugLn(alloc, "%s under %s: member %u at %u", c->name, layoutNames[c->layout], (U32) child, var.offset);
			return false;
		}

		++child;
	}

	if (top != c->topCount || child != c->childCount || elementStride != c->elementStride || matrixStride != c->matrixStride) {
		Log_debugLn(
			alloc, "%s under %s: %u top, %u members, element stride %u, matrix stride %u", c->name, layoutNames[c->layout],
			(U32) top, (U32) child, (U32) elementStride, (U32) matrixStride
		);

		return false;
	}

	return true;
}

void Test_shaderCompilerLayouts(Test *t) {

	Test_setModule(t, "Compiler SPIRV layouts");

	const Allocator *alloc = Platform_instance->alloc;

	for (U64 i = 0; i < sizeof(layoutCases) / sizeof(layoutCases[0]); ++i) {

		const LayoutCase *c = &layoutCases[i];
		ListBuffer bufs = (ListBuffer) { 0 };
		SHFile file = (SHFile) { 0 };
		CharString label = CharString_createNull();
		Error err = Error_none();

		//A refusal is the expected result for a refused case, so that compile stays quiet into a local error

		const Bool compiled =
			compileInlineShadersLayout(
				alloc, &c->src, 1, (U8) EGfxBinaryType_SPIRV, 1, c->name, !c->refusedBecause, c->layout, &bufs, &err
			) &&
			bufs.length == 1 && Buffer_length(bufs.ptr[0]) &&
			readOiSH(alloc, bufs.ptr[0], &file, &err) && file.binaries.length >= 1;

		const SBFile *sb = NULL;

		for (U64 j = 0; compiled && j < file.binaries.ptr[0].registers.length && !sb; ++j)
			if (file.binaries.ptr[0].registers.ptr[j].shaderBuffer.bufferSize)
				sb = &file.binaries.ptr[0].registers.ptr[j].shaderBuffer;

		const Bool ok = c->refusedBecause ? !compiled : compiled && layoutMatches(c, sb, alloc);

		const Bool formatted = c->refusedBecause ?
			CharString_format(
				alloc, &label, NULL, "%s under %s is refused: %s", c->name, layoutNames[c->layout], c->refusedBecause
			) :
			CharString_format(alloc, &label, NULL, "%s under %s records the binary's layout", c->name, layoutNames[c->layout]);

		if (!formatted)
			label = CharString_createRefCStrConst(c->name);

		Test_assert(t, label.ptr, ok);

		//A refused case that compiles anyway is compared too, so its failure also says whether the layout it recorded is wrong

		if (!c->refusedBecause || compiled) {

			CharString_free(&label, alloc);

			const Bool relabeled = CharString_format(
				alloc, &label, NULL, "%s under %s records the layout its SPIR-V declares", c->name, layoutNames[c->layout]
			);

			if (!relabeled)
				label = CharString_createRefCStrConst(c->name);

			Test_assert(t, label.ptr, compiled && oiSHDescribesSpirvLayout(alloc, bufs.ptr[0], label.ptr, NULL));
		}

		CharString_free(&label, alloc);
		SHFile_free(&file, alloc);
		ListBuffer_freeUnderlying(&bufs, alloc);
	}

	for (U64 i = 0; i < sizeof(layoutTampers) / sizeof(layoutTampers[0]); ++i) {

		const LayoutTamper *tamper = &layoutTampers[i];
		ListBuffer bufs = (ListBuffer) { 0 };
		SHFile file = (SHFile) { 0 };
		CharString label = CharString_createNull();
		Error err = Error_none();

		const Bool compiled =
			compileInlineShadersLayout(
				alloc, &tamper->src, 1, (U8) EGfxBinaryType_SPIRV, 1, "layout_tamper", true, tamper->layout, &bufs, &err
			) &&
			bufs.length == 1 && Buffer_length(bufs.ptr[0]) &&
			readOiSH(alloc, bufs.ptr[0], &file, &err) && file.binaries.length >= 1;

		SBFile *sb = NULL;

		for (U64 j = 0; compiled && j < file.binaries.ptr[0].registers.length && !sb; ++j)
			if (file.binaries.ptr[0].registers.ptr[j].shaderBuffer.bufferSize)
				sb = &file.binaries.ptrNonConst[0].registers.ptrNonConst[j].shaderBuffer;

		//Checked untouched first, so the refusal below comes from the change and not from the case

		const Bool untouched = sb && oiSHFileDescribesSpirvLayout(alloc, &file, tamper->what, NULL);
		SBVar *second = untouched && sb->vars.length > 1 ? &sb->vars.ptrNonConst[1] : NULL;
		Bool tampered = false;

		switch (tamper->tamper) {

			case ELayoutTamper_Offset:

				tampered = second != NULL;

				if (tampered)
					second->offset += 4;

				break;

			case ELayoutTamper_ArrayDim:

				//A single dimension is stored inline, anything else is an array id with the top bit set

				tampered =
					second != NULL && second->arrayDimOrArrayId && !(second->arrayDimOrArrayId & (U16) I16_MIN);

				if (tampered)
					++second->arrayDimOrArrayId;

				break;

			case ELayoutTamper_Type:

				tampered =
					second != NULL && second->structId == U16_MAX &&
					ESBType_getVector((ESBType) second->type) != ESBVector_N4;

				if (tampered)
					++second->type;

				break;

			case ELayoutTamper_Packing:

				tampered = untouched;

				if (tampered)
					sb->flags = (ESBSettingsFlags) (sb->flags ^ ESBSettingsFlags_IsTightlyPacked);

				break;

			default:

				tampered = untouched;

				if (tampered)
					sb->bufferSize += 4;

				break;
		}

		if (!CharString_format(alloc, &label, NULL, "the SPIR-V layout check refuses %s", tamper->what))
			label = CharString_createRefCStrConst(tamper->what);

		Test_assert(t, label.ptr, untouched && tampered && !oiSHFileDescribesSpirvLayout(alloc, &file, NULL, NULL));

		CharString_free(&label, alloc);
		SHFile_free(&file, alloc);
		ListBuffer_freeUnderlying(&bufs, alloc);
	}
}

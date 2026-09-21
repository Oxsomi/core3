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

//types/mesh/test/test_mesh_attribute.c

#include "test_mesh_shared.h"
#include "types/base/error.h"

//The codec is written once over PRIMITIVE and bit depth rather than once per format, so what has to be tested
//is every primitive at every width and not every format. A round trip is the whole contract: what encode
//wrote, decode reads back.
//
//The carrier is F32 on both sides, so a 32 bit integer channel is exact only to 24 bits. Values here stay
// inside that on purpose; saturation past it is its own case below.

typedef struct AttributeCase {
	ETextureFormatId format;
	const C8 *name;
	U8 count;
	F32 value[4];
} AttributeCase;

static const AttributeCase attributeCases[] = {

	//SIGNED integers, which are what a sign extension bug hides in: the stored bits are two's complement and
	// a decode that reads them as unsigned turns every negative into a large positive.

	{ ETextureFormatId_R8i,     "R8i",     1, { -5, 0, 0, 0 } },
	{ ETextureFormatId_R8i,     "R8i min", 1, { -128, 0, 0, 0 } },
	{ ETextureFormatId_RG16i,   "RG16i",   2, { -32768, 32767, 0, 0 } },
	{ ETextureFormatId_RGBA32i, "RGBA32i", 4, { -8388608, -1, 0, 8388607 } },

	//UNSIGNED integers, so the same arm with no extension at all

	{ ETextureFormatId_R8u,     "R8u",     1, { 255, 0, 0, 0 } },
	{ ETextureFormatId_RG16u,   "RG16u",   2, { 0, 65535, 0, 0 } },
	{ ETextureFormatId_RGBA32u, "RGBA32u", 4, { 0, 1, 16777215, 42 } },

	//Floats, exact at both widths for values an F16 holds

	{ ETextureFormatId_R16f,    "R16f",    1, { 0.5f, 0, 0, 0 } },
	{ ETextureFormatId_RGBA32f, "RGBA32f", 4, { -1.25f, 0, 1e18f, 3.5f } }
};

static void Test_meshAttributeRoundTrip(Test *t) {

	Test_setModule(t, "mesh/attribute round trip");

	for(U64 i = 0; i < sizeof(attributeCases) / sizeof(attributeCases[0]); ++i) {

		const AttributeCase c = attributeCases[i];

		U8 record[16] = { 0 };
		F32 back[4] = { 0, 0, 0, 0 };

		MeshAttribute_encode(record, c.format, c.value, c.count);
		MeshAttribute_decode(record, c.format, back, c.count);

		Bool same = true;

		for(U8 k = 0; k < c.count; ++k)
			same &= back[k] == c.value[k];

		Test_assert(t, c.name, same);
	}
}

//Normalized channels carry a fraction, so the round trip is the quantization step and not the value.

static void Test_meshAttributeNormalized(Test *t) {

	Test_setModule(t, "mesh/attribute normalized");

	U8 record[16] = { 0 };
	F32 back[4] = { 0, 0, 0, 0 };

	const F32 snorm[4] = { -1, -0.5f, 0, 1 };
	MeshAttribute_encode(record, ETextureFormatId_RGBA8s, snorm, 4);
	MeshAttribute_decode(record, ETextureFormatId_RGBA8s, back, 4);

	Bool near = true;

	for(U8 k = 0; k < 4; ++k)
		near &= F32_abs(back[k] - snorm[k]) < 0.01f;

	Test_assert(t, "RGBA8s round trip", near);
	Test_assert(t, "SNorm endpoints exact", back[0] == -1 && back[3] == 1);

	const F32 unorm[4] = { 0, 0.25f, 0.75f, 1 };
	MeshAttribute_encode(record, ETextureFormatId_RGBA8, unorm, 4);
	MeshAttribute_decode(record, ETextureFormatId_RGBA8, back, 4);

	near = true;

	for(U8 k = 0; k < 4; ++k)
		near &= F32_abs(back[k] - unorm[k]) < 0.01f;

	Test_assert(t, "RGBA8 round trip", near);
	Test_assert(t, "UNorm endpoints exact", back[0] == 0 && back[3] == 1);
}

//A value past what the channel holds SATURATES. It is not wrapped, because a wrap turns an out of range index
// into a valid looking one, and the conversion that would wrap is undefined in C to begin with.

static void Test_meshAttributeRange(Test *t) {

	Test_setModule(t, "mesh/attribute range");

	U8 record[16] = { 0 };
	F32 back[4] = { 0, 0, 0, 0 };

	const F32 overSigned[2] = { 1e9f, -1e9f };
	MeshAttribute_encode(record, ETextureFormatId_RG16i, overSigned, 2);
	MeshAttribute_decode(record, ETextureFormatId_RG16i, back, 2);

	Test_assert(t, "SInt saturates high", back[0] == 32767);
	Test_assert(t, "SInt saturates low", back[1] == -32768);

	const F32 overUnsigned[2] = { 1e9f, -1e9f };
	MeshAttribute_encode(record, ETextureFormatId_RG16u, overUnsigned, 2);
	MeshAttribute_decode(record, ETextureFormatId_RG16u, back, 2);

	Test_assert(t, "UInt saturates high", back[0] == 65535);
	Test_assert(t, "UInt saturates low", back[1] == 0);

	//A NaN has no integer to convert to and converting one is undefined, so every arm but the float one
	// treats it as zero rather than as whatever the hardware happened to produce.

	const F32 q = F32_fromU32Bits(0x7FC00000);
	const F32 nan[4] = { q, q, q, q };

	MeshAttribute_encode(record, ETextureFormatId_RGBA8, nan, 4);
	MeshAttribute_decode(record, ETextureFormatId_RGBA8, back, 4);
	Test_assert(t, "UNorm NaN is zero", !back[0] && !back[1] && !back[2] && !back[3]);

	MeshAttribute_encode(record, ETextureFormatId_RGBA32i, nan, 4);
	MeshAttribute_decode(record, ETextureFormatId_RGBA32i, back, 4);
	Test_assert(t, "SInt NaN is zero", !back[0] && !back[1] && !back[2] && !back[3]);
}

//Components past what the caller supplies write zero, and past what the format holds are dropped. That is what
// lets an RGBA format carry a three component attribute without the caller knowing.

static void Test_meshAttributeComponents(Test *t) {

	Test_setModule(t, "mesh/attribute components");

	U8 record[16] = { 0xFF };
	F32 back[4] = { 1, 1, 1, 1 };

	for(U8 i = 0; i < 16; ++i)
		record[i] = 0xFF;

	const F32 three[3] = { 1, 2, 3 };
	MeshAttribute_encode(record, ETextureFormatId_RGBA32i, three, 3);
	MeshAttribute_decode(record, ETextureFormatId_RGBA32i, back, 4);

	Test_assert(t, "supplied components kept", back[0] == 1 && back[1] == 2 && back[2] == 3);
	Test_assert(t, "unsupplied component is zero", back[3] == 0);

	const F32 four[4] = { 7, 8, 9, 10 };
	F32 two[2] = { 0, 0 };

	MeshAttribute_encode(record, ETextureFormatId_RG16i, four, 4);
	MeshAttribute_decode(record, ETextureFormatId_RG16i, two, 2);

	Test_assert(t, "components past the format dropped", two[0] == 7 && two[1] == 8);

	//A decode asked for more than the format holds zeroes what it cannot fill rather than reading past it.

	for(U8 i = 0; i < 4; ++i)
		back[i] = 1;

	MeshAttribute_decode(record, ETextureFormatId_RG16i, back, 4);
	Test_assert(t, "decode past the format is zero", back[2] == 0 && back[3] == 0);
}

//The layout table, which is what keeps a record shape describable rather than compiled in. The two shapes
//EMeshFlags selects are ONE pair of entries out of what the constructor takes, and the cases below are the
// ones a consumer actually wants next: an attribute stream with no uv at all, and one with a tangent in it.

static void Test_meshAttributeLayout(Test *t) {

	Test_setModule(t, "mesh/attribute layout");

	//The two shapes the flags select, unchanged by going through the general constructor.

	const MeshAttributeLayout narrow = MeshAttributeLayout_fromFlags(EMeshFlags_None);
	const MeshAttributeLayout wide = MeshAttributeLayout_fromFlags(EMeshFlags_WideUvs);

	Test_assert(t, "flags narrow stride", narrow.stride == sizeof(MeshAttribute));
	Test_assert(t, "flags wide stride", wide.stride == sizeof(MeshAttributeWide));
	Test_assert(t, "flags uv follows the normal", narrow.entries[1].offset == sizeof(U32));

	//A normal with NO uv, which is what a renderer that shades from geometry alone wants and what the flags
	// have no spelling for.

	const MeshAttributeEntry normalOnly[1] = {
		{ .attribute = EMeshAttribute_Normal, .encoding = EMeshAttributeEncoding_Oct }
	};

	const MeshAttributeLayout lean = MeshAttributeLayout_create(normalOnly, 1);

	Test_assert(t, "uv-less stride", lean.stride == sizeof(U32));
	Test_assert(t, "uv-less carries no uv", !MeshAttributeLayout_find(&lean, EMeshAttribute_Uv0));
	Test_assert(t, "oct forces its own format", lean.entries[0].format == ETextureFormatId_R32u);

	//A tangent beside the normal, packed in the order given.

	const MeshAttributeEntry tangent[3] = {
		{ .attribute = EMeshAttribute_Normal, .encoding = EMeshAttributeEncoding_Oct },
		{ .attribute = EMeshAttribute_Tangent, .encoding = EMeshAttributeEncoding_Oct },
		{
			.attribute = EMeshAttribute_Uv0,
			.encoding = EMeshAttributeEncoding_Raw,
			.format = ETextureFormatId_RG16f
		}
	};

	const MeshAttributeLayout tangentLayout = MeshAttributeLayout_create(tangent, 3);
	const MeshAttributeEntry *found = MeshAttributeLayout_find(&tangentLayout, EMeshAttribute_Uv0);

	Test_assert(t, "tangent stride", tangentLayout.stride == 12);
	Test_assert(t, "tangent offset", tangentLayout.entries[1].offset == 4);
	Test_assert(t, "uv past the tangent", found && found->offset == 8);

	//An offset the caller filled in is recomputed, so a layout can only describe a packed record.

	const MeshAttributeEntry lying[1] = {
		{ .attribute = EMeshAttribute_Normal, .encoding = EMeshAttributeEncoding_Oct, .offset = 64 }
	};

	Test_assert(t, "offset is recomputed", !MeshAttributeLayout_create(lying, 1).entries[0].offset);

	//Refused outright rather than half built.

	const MeshAttributeEntry twice[2] = {
		{ .attribute = EMeshAttribute_Normal, .encoding = EMeshAttributeEncoding_Oct },
		{ .attribute = EMeshAttribute_Normal, .encoding = EMeshAttributeEncoding_Oct }
	};

	const MeshAttributeEntry compressed[1] = {
		{
			.attribute = EMeshAttribute_Color,
			.encoding = EMeshAttributeEncoding_Raw,
			.format = ETextureFormatId_BC7
		}
	};

	Test_assert(t, "same attribute twice refused", !MeshAttributeLayout_create(twice, 2).stride);
	Test_assert(t, "compressed format refused", !MeshAttributeLayout_create(compressed, 1).stride);
	Test_assert(t, "no entries refused", !MeshAttributeLayout_create(normalOnly, 0).stride);
	Test_assert(
		t, "past the entry cap refused",
		!MeshAttributeLayout_create(normalOnly, MeshAttributeLayout_maxEntries + 1).stride
	);
}

void Test_meshAttribute(Test *t) {
	Test_meshAttributeLayout(t);
	Test_meshAttributeRoundTrip(t);
	Test_meshAttributeNormalized(t);
	Test_meshAttributeRange(t);
	Test_meshAttributeComponents(t);
}

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

//The record SHAPE, not the codec that fills it: what a format does with a value is tested beside the format,
//in types/container. What is here is the table a mesh describes its attribute record with.

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
}

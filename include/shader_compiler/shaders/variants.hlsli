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

//shader_compiler/shaders/variants.hlsli
//
//Disabling combinations an entry's annotations would build but can't or shouldn't, so the annotations stay a full
//cross product (OxC3_tool.md, Disabling combinations). An entry's binaries are then only the ones left, and a lookup
//that names a disabled one falls through to the next extension line, as it does where one isn't supported.
//
//OXC_DISABLE_COMBINATION, written under an #if on what a combination is made of (its defines, $NAME, and its
//extensions, __OXC_EXT_NAME), drops that combination: it expands to a type DXC doesn't know, whose name OxC3 finds in
//the compile's errors, and a combination that reaches it compiles nothing, every uniform row included. It has to be
//written at global scope, and it expands to nothing where the file is only parsed for its entrypoints.
//
//oxcDisableBinary(), called on a path only a uniform decides (if($$MODE == 2) oxcDisableBinary();), drops each binary
//whose specialized code still reaches it, decided after the link. It writes to a marker resource, which survives in a
//binary exactly where the call does: OxC3 drops the binaries that use it and removes it from the rest. A call under a
//condition the specialization can't fold drops every binary, which OxC3 logs, as it logs every binary it drops.

#pragma once
#include "@types.hlsli"

#ifdef __OXC_PREPROCESS
	#define OXC_DISABLE_COMBINATION
#else
	#define OXC_DISABLE_COMBINATION __oxcDisableCombination__ _;
#endif

//Keep in sync with OXC3_DISABLE_SPACE in formats/gfx_util/gfx_util.h: a space of its own, next to OxC3's reserved one.

#define OXC3_DISABLE_SPACE space196

_vkBinding(0, 196) RWByteAddressBuffer _oxcDisableBinary : register(u0, OXC3_DISABLE_SPACE);

void oxcDisableBinary() {
	_oxcDisableBinary.Store(0, 0);
}

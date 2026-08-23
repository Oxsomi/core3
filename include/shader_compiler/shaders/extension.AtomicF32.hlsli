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

//AtomicF32 (32-bit float atomic add). There is no DXIL intrinsic for it, so it's SPIRV-only: an inline
//OpAtomicFAddEXT (SPV_EXT_shader_atomic_float_add). Pass the target memory (e.g. a RWStructuredBuffer element)
//as the first argument, plus a memory scope and semantics: see oxc::MemoryScope_* and
//oxc::MemorySemantics_* in types.hlsli rather than passing the raw SPIR-V numbers.
//The declaration is unconditional (not under #ifdef __spirv__): the vk:: attributes are no-ops off SPIRV, and it
//must still be visible during the (non-SPIRV) reflection pass so a shader body that calls it parses.

//The oxc:: type names and the memory scope/semantics constants both live in types.hlsli
#include "@types.hlsli"

namespace oxc {

	[[vk::ext_capability(/* AtomicFloat32AddEXT */ 6033)]]
	[[vk::ext_extension("SPV_EXT_shader_atomic_float_add")]]
	[[vk::ext_instruction(/* OpAtomicFAddEXT */ 6035)]]
	F32 AtomicAddF32([[vk::ext_reference]] F32 mem, U32 memoryScope, U32 memorySemantics, F32 value);

}

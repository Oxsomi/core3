R"(
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

//AtomicF64 (64-bit float atomic add). No DXIL intrinsic exists, so it's SPIRV-only: an inline OpAtomicFAddEXT
//(SPV_EXT_shader_atomic_float_add, the same instruction as F32 but the AtomicFloat64AddEXT capability). Pass the
//target memory (e.g. a RWStructuredBuffer element) plus a SPIR-V memory scope (1 = Device) and semantics (0 = Relaxed).
//Declared unconditionally (see extension.AtomicF32.hlsli): the vk:: attributes are no-ops off SPIRV, and it must be
//visible during the non-SPIRV reflection pass.

namespace oxc {

	[[vk::ext_capability(/* AtomicFloat64AddEXT */ 6034)]]
	[[vk::ext_extension("SPV_EXT_shader_atomic_float_add")]]
	[[vk::ext_instruction(/* OpAtomicFAddEXT */ 6035)]]
	double AtomicAddF64([[vk::ext_reference]] double mem, uint memoryScope, uint memorySemantics, double value);

}
)"

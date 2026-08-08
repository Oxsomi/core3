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

//shader_compiler/compiler_spv_internal.hpp

#pragma once
#include "shader_compiler/compiler.h"

#include "SPIRV-Reflect/spirv_reflect.h"

//Internal helpers shared between the SPIRV backend TUs.
// (compiler_spv.cpp, compiler_spv_capability.cpp, compiler_spv_reflect.cpp)
//These have C++ linkage and are not part of the public compiler interface.

Bool spvTypeToESBType(SpvReflectTypeDescription *desc, ESBType *type, Error *e_rr);

Bool spvMapCapabilityToESHExtension(SpvCapability capability, ESHExtension *extension, Error *e_rr);

Bool SpvReflectFormatToESBType(SpvReflectFormat format, ESBType *type, Error *e_rr);

Bool SpvCalculateStructLength(const SpvReflectTypeDescription *typeDesc, U64 *result, Error *e_rr);

Bool Compiler_convertMemberSPIRV(
	SBFile *sbFile,
	const SpvReflectBlockVariable *var,
	U16 parent,
	U32 offset,
	Bool isPacked,
	const Allocator *alloc,
	Error *e_rr
);

Bool Compiler_convertShaderBufferSPIRV(
	SpvReflectBlockVariable *block,
	Bool isPacked,
	const Allocator *alloc,
	SBFile *sbFile,
	Error *e_rr
);

Bool Compiler_convertRegisterSPIRV(
	ListSHRegisterRuntime *registers,
	SpvReflectDescriptorBinding *binding,
	U32 expectedSet,
	const Allocator *alloc,
	Error *e_rr
);

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

//shader_compiler/spirv_isa.h

#pragma once
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct CharString CharString;
typedef struct ListCharString ListCharString;
typedef struct Buffer Buffer;
typedef struct Error Error;
typedef struct Allocator Allocator;

//Whether the SPIR-V module has an offline AMD ISA path: every entrypoint must be raster (vertex/hull/domain/geometry/
// pixel), compute or mesh/task.
//Ray tracing has none (amdllpc emits an empty code section without pipeline context).
//Returns false for those (and for anything that isn't a parseable SPIR-V module), so callers can pre-filter what to
// disassemble.
//Uses the compiler's own SPIR-V entrypoint query, hence the allocator.

Bool SpvISA_stageHasOfflinePath(Buffer spirv, const Allocator *alloc);

//Disassembles a SPIR-V module to AMD ISA text for `gfxTarget` by driving the bundled amdllpc (SPIR-V -> ELF) then
// amdgpu-dis (ELF -> ISA text); this produces closer-to-final ISA than a device-independent path.
//`gfxTarget` is a gfx name ("gfx1100") or an explicit major.minor.step ("11.0.0").
//`entrypoint` selects which entrypoint to lower from a multi-entry (library) module; pass an empty string to lower
// the module's default (first) entrypoint.
//`isaOut` must be an empty buffer and receives the ISA text (caller frees).
//The bundled offline compiler supports gfx11xx (RDNA3) and gfx12xx (RDNA4).
//Requires process spawning (SUPPORTS_PROCESS); errors if unavailable on the platform.

Bool SpvISA_disassemble(
	Buffer spirv, CharString gfxTarget, CharString entrypoint, Buffer *isaOut, const Allocator *alloc, Error *e_rr
);

//Probes the bundled amdllpc for the AMD gfx targets it can actually compile for (this build supports only a subset of
// all AMD ISAs), appending one "gfxNNNN (Arch)" line per supported target to `out` (caller frees the strings).
//Reflects exactly what the offline ISA path accepts as a `-asic`.
//Requires process spawning (SUPPORTS_PROCESS).

Bool SpvISA_listSupportedTargets(const Allocator *alloc, ListCharString *out, Error *e_rr);

#ifdef __cplusplus
	}
#endif

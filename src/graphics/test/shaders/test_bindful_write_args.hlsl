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


#include "@types.hlsli"
#include "@indirect.hlsli"

//Writes indirect arguments on the GPU so a later scope can consume them without CPU involvement, the same
// thing test_write_args.hlsl does bindlessly, but through a classic register.
//Layout: IndirectDraw at 0 (3 vertices, 2 instances), IndirectDispatch at 16 (2 groups).
//Deliberately includes NO bindless headers.

RWByteAddressBuffer args : register(u0, space0);

[shader("compute")]
[numthreads(1, 1, 1)]
void main(U32 i : SV_DispatchThreadID) {
	args.Store4(0, U32x4(3, 2, 0, 0));
	args.Store4(16, U32x4(2, 1, 1, 0));
}

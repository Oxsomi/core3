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

//deadRead is only read under a uniform, so the binary specialized with it 0 never accesses it.
//On SPIRV the test is an OpSpecConstantOp over the uniform's spec constant, which has to be folded for the branch to die.

RWByteAddressBuffer usedOut : register(u0);
ByteAddressBuffer deadRead : register(t1);

[[oxc::uniforms(U32 READ_EXTRA = 0)]]
[[oxc::uniforms(U32 READ_EXTRA = 1)]]
[shader("compute")]
[numthreads(1, 1, 1)]
void main() {

	uint v = 1;

	#ifdef $$READ_EXTRA
		if(($$READ_EXTRA & 1) != 0)
			v = deadRead.Load(0);
	#endif

	usedOut.Store(0, v);
}

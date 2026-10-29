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

//types/math/test/consumer/consumer.cpp

#include "types/base/lock.hpp"
#include "types/math/vec4f.hpp"

static_assert(_ENABLE_SIMD == OXC3_EXPECTED_SIMD, "Consumer SIMD ABI differs from the library");

int main() {

	oxc::Lock lock;
	oxc::AcquiredLock acquired;

	if(!lock.acquire(acquired, 0))
		return 1;

	const oxc::F32x4 vector(1, 2, 3, 4);
	const oxc::F32x4 reversed(oxc::c::F32x4_wzyx(vector.handle()));

	return reversed.x() == 4 && reversed.y() == 3 && reversed.z() == 2 && reversed.w() == 1 ? 0 : 2;
}

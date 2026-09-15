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

//types/container/simd/none/none_buffer_hash.c

#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/container/buffer.h"
#include "types/container/host_crypto.h"
#include "types/base/mathi.h"

U32 Buffer_crc32cChained(const Buffer buf, U32 prevCrc) {
	return Buffer_crc32cFallbackChained(buf, prevCrc);
}

void Buffer_sha256(const Buffer buf, U32 output[8]) {

	if(!output)
		return;

	//Same shape as the SSE backend's ECPUFeatures_HwSHA256 check: use the fast path when the platform actually has one,
	// otherwise the software fallback.
	//On web "the platform" is the host's crypto.subtle rather than a CPU instruction (see types/container/host_crypto.h).

	#if _PLATFORM_TYPE == PLATFORM_WEB && defined(_OXC3_HOST_CRYPTO)
		if(HostCrypto_sha256(buf, output))
			return;
	#endif

	Buffer_sha256Fallback(buf, output);
}

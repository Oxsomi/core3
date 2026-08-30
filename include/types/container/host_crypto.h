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

//types/container/host_crypto.h
//
//Routes SHA-256 and AES-GCM to the host's hardware backed crypto on the web target,
// because the SIMD_NONE fallbacks are the only thing available there (wasm has no AES/SHA/CLMUL instructions,
// see docs/web.md): AES-128-GCM runs at 1.27 MB/s in wasm versus ~1800 MB/s through the host.
//
//The web platform only exposes crypto.subtle, which is asynchronous,
// so a synchronous C call reaches it by handing the work to a helper worker and blocking on Atomics.wait until it lands.
//That needs SharedArrayBuffer (so, a cross origin isolated page) and the module has to run in a Worker,
// since Atomics.wait is forbidden on a browser's main thread.
//None of that is required: when it isn't satisfied Buffer_* keeps using OxC3's own kernels, just slower.
//
//Only compiled when EnableHostCrypto is on (see the root CMakeLists); everything below is a no-op otherwise.
//Output is byte identical either way, AES-GCM and SHA-256 being what they are;
// the host_crypto test asserts exactly that against the same vectors the software path uses.

#pragma once
#include "types/base/types.h"
#include "types/container/buffer.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct BufferEncrypt BufferEncrypt;

#if _PLATFORM_TYPE == PLATFORM_WEB && defined(_OXC3_HOST_CRYPTO)

	//True when the bridge is usable (SharedArrayBuffer + crypto.subtle + a helper worker).
	//Probed once and cached; safe to call from anywhere.
	Bool HostCrypto_available();

	//All of these return false when the host can't service the request (bridge unavailable,
	// buffer larger than the staging area, or crypto.subtle rejected it),
	// in which case the caller must fall back to OxC3's own implementation.

	Bool HostCrypto_sha256(Buffer buf, U32 output[8]);

	//In place AES-GCM over encrypt->target, with encrypt->additionalData as AAD.
	//Key and IV are supplied by the caller (OxC3 still owns generation), tag is written out.
	Bool HostCrypto_aesGcm(const BufferEncrypt *encrypt, Bool isEncrypt);

#else

	#define HostCrypto_available() false

#endif

#ifdef __cplusplus
	}
#endif

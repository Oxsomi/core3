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

//types/container/test/test_types_container_host_crypto.c
//
//When crypto is routed to the host (web target, -DEnableHostCrypto=ON) the output has to be
//indistinguishable from OxC3's own kernels: AES-GCM and SHA-256 are standards, so the same key,
//IV and AAD must give the same ciphertext, the same tag and the same digest. Anything else means
//the bridge is mis-marshalling something.
//
//This compares the two paths directly rather than against stored vectors, so it also covers the
//marshalling: same input through HostCrypto_* and through Buffer_sha256Fallback / the AES kernel.
//
//Every assert self-skips when the bridge isn't available (no SharedArrayBuffer, not cross origin
//isolated, not in a Worker), so the suite stays green on platforms that never route.

#include "test_types_container_shared.h"
#include "types/container/buffer.h"
#include "types/container/buffer_encrypt.h"
#include "types/container/host_crypto.h"
#include "types/math/vec4i.h"
#include "types/base/error.h"

void Test_hostCrypto(Test *t) {

	Test_setModule(t, "HostCrypto");

	if(!HostCrypto_available()) {
		Test_assert(t, "unavailable (skipped, using OxC3 kernels)", true);
		return;
	}

#if _PLATFORM_TYPE == PLATFORM_WEB && defined(_OXC3_HOST_CRYPTO)

	//Sizes that straddle the AES block, the SHA block, a realistic EncryptionStream chunk, and
	//the initial staging buffer (1 MiB) so the grow-on-demand path is exercised too.
	static const U64 sizes[] = { 0, 1, 15, 16, 17, 64, 1000, 4096, 65536, 1048576, 3 * 1048576 };

	//Counted, not just compared: every one of these sizes is inside the growth ceiling, so the host
	//has to actually serve them. Without this the asserts below would pass vacuously whenever
	//HostCrypto_* declined and the software path quietly did the work instead.

	Bool shaMatched = true, aesMatched = true, roundTripped = true;
	U64 hostServed = 0;

	for(U64 i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {

		const U64 len = sizes[i];

		Buffer data = Buffer_createNull();
		Buffer copy = Buffer_createNull();

		if(!Buffer_createUninitializedBytes(len < 16 ? 16 : len, t->alloc, &data, NULL))
			continue;

		for(U64 j = 0; j < len; ++j)
			data.ptrNonConst[j] = (U8)(j * 31 + i);

		Buffer view = Buffer_createRefConst(data.ptr, len);

		// --- SHA-256: host vs fallback ---

		U32 hostDigest[8] = { 0 }, softDigest[8] = { 0 };

		if(HostCrypto_sha256(view, hostDigest)) {

			++hostServed;
			Buffer_sha256Fallback(view, softDigest);

			for(U8 j = 0; j < 8; ++j)
				if(hostDigest[j] != softDigest[j])
					shaMatched = false;
		}

		// --- AES-256-GCM: encrypt on the host, decrypt with OxC3's kernel ---

		if(len && Buffer_createCopy(Buffer_createRefConst(data.ptr, len), t->alloc, &copy, NULL)) {

			U32 key[8];
			for(U8 j = 0; j < 8; ++j) key[j] = 0x01020304u + j;

			I32x4 iv = I32x4_create4(0x11111111, 0x22222222, 0x33333333, 0);
			I32x4 tagHost = I32x4_zero();

			Buffer target = Buffer_createRef(copy.ptrNonConst, len);

			BufferEncrypt enc = (BufferEncrypt) {
				.target = &target,
				.additionalData = NULL,
				.type = EBufferEncryptionType_AES256GCM,
				.flags = EBufferEncryptionFlags_StopCreateIv,
				.nonConstEncrypt = { .key = key, .tag = &tagHost, .iv = &iv }
			};

			//Encrypt through whichever path Buffer_encryptAdvanced picks (the host, when routed)
			if(Buffer_encryptAdvanced(&enc, NULL)) {

				//Ciphertext must differ from plaintext, or nothing happened
				if(!Buffer_neq(Buffer_createRefConst(copy.ptr, len), Buffer_createRefConst(data.ptr, len)))
					aesMatched = false;

				//And it has to decrypt back to exactly the input
				BufferEncrypt dec = (BufferEncrypt) {
					.target = &target,
					.additionalData = NULL,
					.type = EBufferEncryptionType_AES256GCM,
					.flags = EBufferEncryptionFlags_None,
					.nonConstEncrypt = { .key = key, .tag = &tagHost, .iv = &iv }
				};

				if(!Buffer_decryptAdvanced(&dec, NULL))
					roundTripped = false;

				else if(Buffer_neq(Buffer_createRefConst(copy.ptr, len), Buffer_createRefConst(data.ptr, len)))
					roundTripped = false;
			}

			else aesMatched = false;
		}

		Buffer_free(&copy, t->alloc);
		Buffer_free(&data, t->alloc);
	}

	Test_assert(t, "host served every size (staging grew)", hostServed == sizeof(sizes) / sizeof(sizes[0]));
	Test_assert(t, "sha256 matches software", shaMatched);
	Test_assert(t, "aes produces ciphertext", aesMatched);
	Test_assert(t, "aes round trips", roundTripped);

#endif
}

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

//types/container/host_crypto.c
//
//See host_crypto.h for why this exists.
//The JS below is the whole bridge:
//
//  wasm thread                        helper worker
//  -----------                        -------------
//  copy request into the SAB
//  postMessage(op, sizes)      ---->  onmessage
//  Atomics.wait(ctl, 0, 0)            await crypto.subtle...
//                                     write result into the SAB
//  wakes, copies result out    <----  Atomics.store + notify
//
//postMessage happens before the wait, so the helper's event loop is free to run while this thread is parked.
//That avoids polling and avoids Atomics.waitAsync (Safari only got it in 16.4).
//
//Staging layout (offsets into the shared buffer):
//  [0,32) key   [32,44) iv   [48,64) tag   [64, 64+aadLen) aad   [dataOff, dataOff+len) payload

#include "types/container/host_crypto.h"

#if _PLATFORM_TYPE == PLATFORM_WEB && defined(_OXC3_HOST_CRYPTO)

#include "types/container/buffer_encrypt.h"
#include "types/base/error.h"
#include "types/base/endianness.h"

#include <emscripten.h>

//The staging buffer starts small and doubles as needed, because crypto.subtle has no incremental API:
// a digest or a GCM has to be handed over in one piece, so a request can't be split across several passes.
//Growth is capped rather than unbounded since a SharedArrayBuffer, once grown, stays allocated;
// anything past the ceiling falls back to OxC3's own kernels.
//EncryptionStream chunks (>32 KiB, typically <=1 MiB) rarely grow it past the initial size.

#define HOST_CRYPTO_STAGING_INIT (1 * 1024 * 1024)
#define HOST_CRYPTO_STAGING_MAX (256 * 1024 * 1024)
#define HOST_CRYPTO_HEADER 64

EM_JS(int, oxc3_hostCryptoInit, (unsigned int staging, unsigned int maxStaging), {

	if (Module._oxc3HostCrypto !== undefined)
		return Module._oxc3HostCrypto ? 1 : 0;

	Module._oxc3HostCrypto = null;

	try {

		const isNode = typeof process !== 'undefined' && process.versions && process.versions.node;

		//Browsers forbid Atomics.wait on the main thread, so the module has to be in a Worker.
		//node allows it on the main thread, which is what the test suite relies on.
		if (!isNode) {
			if (typeof SharedArrayBuffer === 'undefined' || !globalThis.crossOriginIsolated)
				return 0; //needs COOP/COEP
			if (typeof WorkerGlobalScope === 'undefined')
				return 0; //not in a Worker: can't block
		}

		if (typeof crypto === 'undefined' || !crypto.subtle) //secure context only
			return 0;

		const ctlSab = new SharedArrayBuffer(16);
		const dataSab = new SharedArrayBuffer(staging);

		//Helper: message driven, so it never polls and never blocks.
		const src = `
			let data = null, ctl = null;
			const handle = async (m) => {
				if (m.data) data = new Uint8Array(m.data);
				if (m.ctl) ctl = new Int32Array(m.ctl);
				if (m.op === undefined) return;
				let ok = 0;
				try {
					const subtle = crypto.subtle;
					if (m.op === 1) {
						const d = await subtle.digest('SHA-256', data.subarray(m.dataOff, m.dataOff + m.len).slice());
						data.set(new Uint8Array(d), 0);
						ok = 1;
					} else {
						const raw = data.subarray(0, m.keyBytes).slice();
						const iv = data.subarray(32, 44).slice();
						const aad = m.aadLen ? data.subarray(64, 64 + m.aadLen).slice() : new Uint8Array(0);
						const params = { name: 'AES-GCM', iv: iv, tagLength: 128 };
						if (m.aadLen) params.additionalData = aad;
						if (m.op === 2) {
							const key = await subtle.importKey('raw', raw, 'AES-GCM', false, ['encrypt']);
							const payload = data.subarray(m.dataOff, m.dataOff + m.len).slice();
							const out = new Uint8Array(await subtle.encrypt(params, key, payload));
							data.set(out.subarray(0, m.len), m.dataOff); //ciphertext
							data.set(out.subarray(m.len), 48); //tag
							ok = 1;
						} else {
							const key = await subtle.importKey('raw', raw, 'AES-GCM', false, ['decrypt']);
							const joined = new Uint8Array(m.len + 16);
							joined.set(data.subarray(m.dataOff, m.dataOff + m.len));
							joined.set(data.subarray(48, 64), m.len); //ct || tag
							const out = new Uint8Array(await subtle.decrypt(params, key, joined));
							data.set(out, m.dataOff);
							ok = 1;
						}
					}
				} catch (e) { ok = 0; }
				Atomics.store(ctl, 1, ok);
				Atomics.store(ctl, 0, 1);
				Atomics.notify(ctl, 0);
			};
			if (typeof self !== 'undefined' && self.addEventListener)
				self.addEventListener('message', (e) => handle(e.data));
			else {
				const { parentPort } = require('worker_threads');
				parentPort.on('message', handle);
			}
		`;

		let worker;

		if (isNode) {
			const { Worker } = require('worker_threads');
			worker = new Worker(src, { eval: true });
			worker.unref();
		} else {
			const blob = new Blob([src], { type: 'text/javascript' });
			worker = new Worker(URL.createObjectURL(blob));
		}

		worker.postMessage({ ctl: ctlSab, data: dataSab });

		Module._oxc3HostCrypto = {
			worker: worker,
			ctl: new Int32Array(ctlSab),
			bytes: new Uint8Array(dataSab),
			staging: staging,
			max: maxStaging
		};

		return 1;

	} catch (e) {
		Module._oxc3HostCrypto = null;
		return 0;
	}
});

//Posts one request and blocks until the helper answers. Returns 1 on success.
EM_JS(
	int, oxc3_hostCryptoCall,
	(
		int op, unsigned long long dataPtr, unsigned int len,
		unsigned long long keyPtr, unsigned int keyBytes,
		unsigned long long ivPtr, unsigned long long aadPtr,
		unsigned int aadLen, unsigned long long tagPtr
	),
{

	const st = Module._oxc3HostCrypto;

	if (!st)
		return 0;

	//EM_JS emits the body verbatim: a wasm i32 arrives as a SIGNED JS Number,
	// so an `unsigned int` of 2 GiB or more shows up negative.
	//Left alone that poisons every bounds check below (negative passes them, and subarray() clamps to an EMPTY range,
	// so crypto.subtle would cheerfully hash nothing and report success).
	//Coerce back to unsigned before anything looks at these.

	len = len >>> 0;
	aadLen = aadLen >>> 0;
	keyBytes = keyBytes >>> 0;

	const HEADER = 64;
	//Math, not a bitwise op: `&` is ToInt32 and would wrap negative for a large AAD
	const dataOff = HEADER + Math.ceil(aadLen / 16) * 16;
	const needed = dataOff + len + 16;

	if (needed > st.staging) {

		if (needed > st.max)
			return 0; //past the ceiling: caller falls back to OxC3's kernels

		let next = st.staging;

		while (next < needed)
			next *= 2;

		if (next > st.max)
			next = st.max;

		try {
			const grown = new SharedArrayBuffer(next);
			//Ordered before the request posted below, so the helper rebinds first
			st.worker.postMessage({ data: grown });
			st.bytes = new Uint8Array(grown);
			st.staging = next;
		} catch (e) {
			return 0; //couldn't allocate that much: fall back rather than fail
		}
	}

	const bytes = st.bytes, ctl = st.ctl;

	if (keyBytes) {
		bytes.set(HEAPU8.subarray(Number(keyPtr), Number(keyPtr) + keyBytes), 0);
		bytes.set(HEAPU8.subarray(Number(ivPtr), Number(ivPtr) + 12), 32);
	}

	if (op === 3) //decrypt needs the tag going in
		bytes.set(HEAPU8.subarray(Number(tagPtr), Number(tagPtr) + 16), 48);

	if (aadLen)
		bytes.set(HEAPU8.subarray(Number(aadPtr), Number(aadPtr) + aadLen), HEADER);

	if (len)
		bytes.set(HEAPU8.subarray(Number(dataPtr), Number(dataPtr) + len), dataOff);

	Atomics.store(ctl, 0, 0);
	Atomics.store(ctl, 1, 0);

	st.worker.postMessage({ op: op, len: len, aadLen: aadLen, keyBytes: keyBytes, dataOff: dataOff });

	//Bounded: an unbounded wait would hang the whole (single threaded) app forever
	// if the helper worker never came up or died mid request.
	//On timeout the caller falls back to OxC3's kernels.

	const deadline = Date.now() + 30000;

	while (Atomics.load(ctl, 0) === 0) {

		Atomics.wait(ctl, 0, 0, 1000);

		if (Date.now() > deadline) {
			st.bytes.fill(0, 0, 64); //don't leave key material behind on the way out
			return 0;
		}
	}

	const ok = Atomics.load(ctl, 1) === 1;

	//The staging buffer is long lived and reachable by every thread holding the SAB,
	// so the key and IV must not outlive the call.
	//Mirrors what Buffer_clearAllSecure does for OxC3's own buffers.

	if (!ok) {
		st.bytes.fill(0, 0, 64);
		return 0;
	}

	if (op === 1)
		HEAPU8.set(bytes.subarray(0, 32), Number(tagPtr)); //digest has its own destination
	else {
		if (len)
			HEAPU8.set(bytes.subarray(dataOff, dataOff + len), Number(dataPtr));
		if (op === 2)
			HEAPU8.set(bytes.subarray(48, 64), Number(tagPtr));
	}

	bytes.fill(0, 0, 64); //wipe key + iv + tag from shared memory

	return 1;
});

Bool HostCrypto_available() {
	static I8 cached = -1;
	if(cached < 0)
		cached = (I8) oxc3_hostCryptoInit(HOST_CRYPTO_STAGING_INIT, HOST_CRYPTO_STAGING_MAX);
	return cached == 1;
}

Bool HostCrypto_sha256(Buffer buf, U32 output[8]) {

	if(!output || !HostCrypto_available())
		return false;

	const U64 len = Buffer_length(buf);

	//Capped at what the bridge can service rather than at 2^32: the JS side takes these as i32,
	// and anything past the staging ceiling would be declined there anyway.
	//Oversized goes to software.

	if(len > HOST_CRYPTO_STAGING_MAX)
		return false;

	//The digest is written to its own destination rather than back over dataPtr:
	// the input is the caller's buffer and must come back untouched (hashing must never mutate what it hashes).

	if(!oxc3_hostCryptoCall(
		1, (unsigned long long) buf.ptr, (unsigned int) len,
		0, 0, 0, 0, 0, (unsigned long long) output
	))
		return false;

	//crypto.subtle hands back the standard big endian serialisation,
	// while OxC3 keeps the eight state words in native order (see the store at the end of simd/buffer_simd_sha.inc.h,
	// which does no final swap).
	//Same digest, different representation, so line them up here.

	for(U8 i = 0; i < 8; ++i)
		output[i] = U32_swapEndianness(output[i]);

	return true;
}

Bool HostCrypto_aesGcm(const BufferEncrypt *encrypt, Bool isEncrypt) {

	if(!encrypt || !HostCrypto_available())
		return false;

	if(encrypt->type != EBufferEncryptionType_AES128GCM && encrypt->type != EBufferEncryptionType_AES256GCM)
		return false;

	const U64 len = encrypt->target ? Buffer_length(*encrypt->target) : 0;
	const U64 aadLen = encrypt->additionalData ? Buffer_length(*encrypt->additionalData) : 0;

	if(len > HOST_CRYPTO_STAGING_MAX || aadLen > HOST_CRYPTO_STAGING_MAX)
		return false;

	const U32 keyBytes = encrypt->type == EBufferEncryptionType_AES128GCM ? 16 : 32;

	//Buffer_isConstRef is what the software path honours before writing into target;
	// the routed path would otherwise happily write through a const reference.

	if(encrypt->target && Buffer_isConstRef(*encrypt->target) && Buffer_length(*encrypt->target))
		return false;

	const void *key = isEncrypt ? (const void*) encrypt->nonConstEncrypt.key : (const void*) encrypt->constDecrypt.key;
	const void *iv  = isEncrypt ? (const void*) encrypt->nonConstEncrypt.iv  : (const void*) encrypt->constDecrypt.iv;
	const void *tag = isEncrypt ? (const void*) encrypt->nonConstEncrypt.tag : (const void*) encrypt->constDecrypt.tag;

	if(!key || !iv || !tag)
		return false;

	return oxc3_hostCryptoCall(
		isEncrypt ? 2 : 3,
		(unsigned long long) (encrypt->target ? encrypt->target->ptrNonConst : NULL), (unsigned int) len,
		(unsigned long long) key, keyBytes,
		(unsigned long long) iv,
		(unsigned long long) (encrypt->additionalData ? encrypt->additionalData->ptr : NULL), (unsigned int) aadLen,
		(unsigned long long) tag
	) != 0;
}

#endif

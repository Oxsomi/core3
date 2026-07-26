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

//tools/oxc3_cli/profile.c

#include "tools/oxc3_cli/cli.h"
#include "types/container/buffer.h"
#include "types/container/buffer_encrypt.h"
#include "types/container/string.h"
#include "types/container/log.h"
#include "types/base/string_read.h"
#include "types/base/thread.h"
#include "types/base/time.h"
#include "types/base/error.h"
#include "types/math/flp.h"
#include "types/math/vec4i.h"
#include "types/math/vec4f.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "types/base/constants.h"

typedef Bool (*ProfileOperation)(const ParsedArgs*, Buffer, Error*);

//Multi-threaded harness: each thread runs the op over its own slice, we time the whole thing and report aggregate.

typedef struct ProfileThreadArg {
	const ParsedArgs *args;
	ProfileOperation op;
	Buffer slice;
	Error err;
} ProfileThreadArg;

void CLI_profileThreadCb(void *ptr) {
	ProfileThreadArg *a = (ProfileThreadArg*) ptr;
	if(!a->op(a->args, a->slice, &a->err) && !a->err.genericError)
		a->err = Error_invalidState(0, "CLI_profileThreadCb() op failed");
}

Bool CLI_profileDataThreaded(const ParsedArgs *args, ProfileOperation op, Buffer dat, U64 threads, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = Platform_instance->alloc;
	Buffer targsBuf = Buffer_createNull(), thsBuf = Buffer_createNull();

	gotoIfError3(clean, Buffer_createUninitializedBytes(sizeof(ProfileThreadArg) * threads, alloc, &targsBuf, e_rr));
	gotoIfError3(clean, Buffer_createUninitializedBytes(sizeof(Thread*) * threads, alloc, &thsBuf, e_rr));

	ProfileThreadArg *targs = (ProfileThreadArg*) targsBuf.ptrNonConst;
	Thread **ths = (Thread**) thsBuf.ptrNonConst;

	const U64 sliceSize = (Buffer_length(dat) / threads) & ~(U64)63;        //64-byte aligned slices (AES/SIMD safe)

	Log_debugLnx("Running on %"PRIu64" threads, %"PRIu64" bytes each:", threads, sliceSize);

	const Ns then = Time_now();

	U64 spawned = 0;

	for(U64 i = 0; i < threads; ++i) {

		targs[i] = (ProfileThreadArg) {
			.args = args, .op = op,
			.slice = Buffer_createRef(dat.ptrNonConst + i * sliceSize, sliceSize),
			.err = Error_none()
		};

		ths[i] = NULL;

		if(!Thread_create(alloc, CLI_profileThreadCb, &targs[i], &ths[i], e_rr))
			break;

		++spawned;
	}

	for(U64 i = 0; i < spawned; ++i)
		Thread_waitAndCleanup(alloc, &ths[i], NULL);

	const Ns now = Time_now();
	const U64 total = sliceSize * spawned;

	Log_debugLnx(
		"Aggregate over %"PRIu64" threads: %"PRIu64" bytes in %fs (%f bytes/sec)",
		spawned, total, (F64)(now - then) / SECOND, (F64) total / (now - then) * SECOND
	);

clean:
	Buffer_free(&targsBuf, alloc);
	Buffer_free(&thsBuf, alloc);
	return s_uccess;
}

Bool CLI_profileData(const ParsedArgs *args, ProfileOperation op) {

	if(!args) return false;

	const U64 bufferSize = GIBI;

	Buffer dat = Buffer_createNull();
	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;

	//Parse -threads (default 1 = single threaded; 0 = all hardware threads)

	U64 threads = 1;

	if(args->parameters & EOperationHasParameter_ThreadCount) {

		CharString t = CharString_createNull();
		gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_ThreadCountShift, &t))

		if(!CharString_parseU64(t, &threads))
			retError(clean, Error_invalidState(0, "CLI_profileData() -threads must be a number (0 = all hardware threads)"));

		if(!threads)
			threads = Platform_getThreads();

		if(!threads)
			threads = 1;
	}

	gotoIfError3(clean, Buffer_createUninitializedBytes(bufferSize, Platform_instance->alloc, &dat, e_rr));

	if(!Buffer_csprng(dat))
		retError(clean, Error_invalidState(0, "CLI_profileData() Buffer_csprng failed"));

	if(threads <= 1) {
		gotoIfError3(clean, op(args, dat, e_rr));
	}

	else {
		gotoIfError3(clean, CLI_profileDataThreaded(args, op, dat, threads, e_rr));
	}

clean:

	if(err.genericError)
		Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_Default);

	Buffer_free(&dat, Platform_instance->alloc);
	return s_uccess;
}

typedef enum EProfileCastStep {
	EProfileCastStep_Regular,
	EProfileCastStep_Zero,
	EProfileCastStep_NaN,
	EProfileCastStep_Inf,
	EProfileCastStep_DeN
} EProfileCastStep;

U64 _CLI_profileCastStep(U64 l, U64 k, U64 j, const U8 *ptr, U64 i) {

	const EFloatType types[] = {
		EFloatType_F64,
		EFloatType_F32,
		EFloatType_F16
	};

	const EFloatType inputType = types[k];
	const EFloatType outputType = types[j];

	//Grab value

	U64 v = 0;

	switch (inputType) {

		case EFloatType_F64:
			v = ((const U64*)ptr)[i];
			break;

		case EFloatType_F32:
			v = ((const U32*)ptr)[i];
			break;

		default:
			v = ((const U16*)ptr)[i];
			break;
	}

	//Convert to ensure we're dealing with the right type

	switch (l) {

		case EProfileCastStep_Regular:

			if(EFloatType_isDeN(inputType, v))
				v += (U64)1 << EFloatType_exponentShift(inputType);

			else if(!EFloatType_isFinite(inputType, v))
				v -= (U64)1 << EFloatType_exponentShift(inputType);

			break;

		case EProfileCastStep_Zero:
			v &=~ EFloatType_signMask(inputType);
			break;

		case EProfileCastStep_NaN:

			if(!EFloatType_mantissa(inputType, v))
				v |= (U64)1 << EFloatType_mantissaShift(inputType);

			v |= EFloatType_exponentMask(inputType) << EFloatType_exponentShift(inputType);
			break;

		case EProfileCastStep_Inf:
			v &=~ EFloatType_signMask(inputType);
			v |= EFloatType_exponentMask(inputType) << EFloatType_exponentShift(inputType);
			break;

		case EProfileCastStep_DeN:
		default:
			v &=~ (EFloatType_exponentMask(inputType) << EFloatType_exponentShift(inputType));
			break;

	}

	return EFloatType_convert(inputType, v, outputType);
}

Bool _CLI_profileCast(const ParsedArgs *args, Buffer buf, Error *e_rr) {

	(void)args;
	Bool s_uccess = true;

	if(Buffer_length(buf) < GIBI)
		retError(clean, Error_invalidParameter(1, 0, "_CLI_profileCast() assumes buf to be >= 1 GIBI"));

	const U64 number = GIBI / sizeof(F64) / 32;
	const C8 *iterationNames[] = { "Non denormalized", "(Un)Signed zero", "NaN", "Inf", "DeN" };
	const C8 *floatTypeNames[] = { "F64", "F32", "F16" };

	const U64 itCount = sizeof(iterationNames) / sizeof(iterationNames[0]);
	const U64 floatTypes = sizeof(floatTypeNames) / sizeof(floatTypeNames[0]);

	const Ns thenOuter = Time_now();

	for(U64 l = 0; l < itCount; ++l) {

		for(U64 k = 0; k < floatTypes; ++k)
			for(U64 j = 0; j < floatTypes; ++j) {

				if(j == k)        //No profile needed
					continue;

				const Ns then = Time_now();
				U64 temp = 0;

				for (U64 i = 0; i < number; ++i)
					temp += _CLI_profileCastStep(l, k, j, buf.ptr, i);

				const Ns now = Time_now();

				Log_debugLnx(
					"%s: %"PRIu64"x %s -> %s within %fs (%fns/op). (Operation hash: %"PRIu64")",
					iterationNames[l], number,
					floatTypeNames[k], floatTypeNames[j],
					(F64)(now - then) / SECOND,
					(F64)(now - then) / number,
					temp
				);
			}
	}

	const Ns nowOuter = Time_now();
	const U64 totalIt = itCount * floatTypes * (floatTypes - 1) * number;

	Log_debugLnx(
		"Performed %"PRIu64" casts within %fs. Avg time per cast %fns.",
		totalIt,
		(F64)(nowOuter - thenOuter) / SECOND,
		(F64)(nowOuter - thenOuter) / totalIt
	);

clean:
	return s_uccess;
}

Bool CLI_profileCast(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_profileData(args, _CLI_profileCast);
}

Bool CLI_profileRNGImpl(const ParsedArgs *args, Buffer buf, Error *e_rr) {

	(void)args;
	(void) e_rr;

	const Ns then = Time_now();

	if(!Buffer_csprng(buf)) {
		if(e_rr) *e_rr = Error_invalidState(0, "CLI_profileRNGImpl() Buffer_csprng failed");
		return false;
	}

	const Ns now = Time_now();

	Log_debugLnx(
		"Profile RNG: %"PRIu64" bytes within %fs (%fns/byte, %fbytes/sec).",
		Buffer_length(buf),
		(F64)(now - then) / SECOND,
		(F64)(now - then) / Buffer_length(buf),
		(F64)Buffer_length(buf) / (now - then) * SECOND
	);

	return true;
}

Bool CLI_profileRNG(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_profileData(args, CLI_profileRNGImpl);
}

Bool CLI_profileCRC32CImpl(const ParsedArgs *args, Buffer buf, Error *e_rr) {

	(void)args;
	(void) e_rr;

	const Ns then = Time_now();
	const U32 hash = Buffer_crc32c(buf);
	const Ns now = Time_now();

	Log_debugLnx(
		"Profile CRC32C: %"PRIu64" bytes within %fs (%fns/byte, %fbytes/sec). Random hash %u.",
		Buffer_length(buf),
		(F64)(now - then) / SECOND,
		(F64)(now - then) / Buffer_length(buf),
		(F64)Buffer_length(buf) / (now - then) * SECOND,
		hash
	);

	return true;
}

Bool CLI_profileCRC32C(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_profileData(args, CLI_profileCRC32CImpl);
}

Bool CLI_profileFNV1A64Impl(const ParsedArgs *args, Buffer buf, Error *e_rr) {

	(void)args;
	(void) e_rr;

	const Ns then = Time_now();
	const U32 hash = Buffer_crc32c(buf);
	const Ns now = Time_now();

	Log_debugLnx(
		"Profile FNV1a64: %"PRIu64" bytes within %fs (%fns/byte, %fbytes/sec). Random hash %u.",
		Buffer_length(buf),
		(F64)(now - then) / SECOND,
		(F64)(now - then) / Buffer_length(buf),
		(F64)Buffer_length(buf) / (now - then) * SECOND,
		hash
	);

	return true;
}

Bool CLI_profileFNV1A64(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_profileData(args, CLI_profileFNV1A64Impl);
}

Bool CLI_profileSHA256Impl(const ParsedArgs *args, Buffer buf, Error *e_rr) {

	(void)args;
	(void) e_rr;

	const Ns then = Time_now();

	U32 hash[8];
	Buffer_sha256(buf, hash);

	const Ns now = Time_now();

	Log_debugLnx(
		"Profile SHA256: %"PRIu64" bytes within %fs (%fns/byte, %fbytes/sec). Random hash %08x%08x%08x%08x%08x%08x%08x%08x.",
		Buffer_length(buf),
		(F64)(now - then) / SECOND,
		(F64)(now - then) / Buffer_length(buf),
		(F64)Buffer_length(buf) / (now - then) * SECOND,
		hash[0], hash[1], hash[2], hash[3],
		hash[4], hash[5], hash[6], hash[7]
	);

	return true;
}

Bool CLI_profileSHA256(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_profileData(args, CLI_profileSHA256Impl);
}

Bool CLI_profileMD5Impl(const ParsedArgs *args, Buffer buf, Error *e_rr) {

	(void)args;
	(void) e_rr;

	const Ns then = Time_now();
	I32x4 md5 = Buffer_md5(buf);
	const Ns now = Time_now();

	Log_debugLnx(
		"Profile MD5: %"PRIu64" bytes within %fs (%fns/byte, %fbytes/sec). Random hash %04"PRIX32"%04"PRIX32"%04"PRIX32"%04"PRIX32".",
		Buffer_length(buf),
		(F64)(now - then) / SECOND,
		(F64)(now - then) / Buffer_length(buf),
		(F64)Buffer_length(buf) / (now - then) * SECOND,
		I32x4_x(md5), I32x4_y(md5), I32x4_z(md5), I32x4_w(md5)
	);

	return true;
}

Bool CLI_profileMD5(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_profileData(args, CLI_profileMD5Impl);
}

Bool CLI_profileEncryptionImpl(const ParsedArgs *args, Buffer buf, EBufferEncryptionType encryptionType, Error *e_rr) {

	(void)args;

	Bool s_uccess = true;
	Ns then = Time_now();

	U32 key[8];
	I32x4 iv, tag;

	const Buffer additionalData = Buffer_createNull();

	if(!Buffer_encryptAdvanced(&(BufferEncrypt) {
		.target = &buf,
		.additionalData = &additionalData,
		.type = encryptionType,
		.flags = EBufferEncryptionFlags_GenerateKey,
		.nonConstEncrypt = { .key = key, .tag = &tag, .iv = &iv }
	}, e_rr)) {
		s_uccess = false;
		goto clean;
	}

	Ns now = Time_now();

	Log_debugLnx(
		"Encrypt AES GCM: %"PRIu64" bytes within %fs (%fns/byte, %fbytes/sec).",
		Buffer_length(buf),
		(F64)(now - then) / SECOND,
		(F64)(now - then) / Buffer_length(buf),
		(F64)Buffer_length(buf) / (now - then) * SECOND
	);

	then = now;

	if(!Buffer_decryptAdvanced(&(BufferEncrypt) {
		.target = &buf,
		.additionalData = &additionalData,
		.type = encryptionType,
		.constDecrypt = { .key = key, .tag = &tag, .iv = &iv }
	}, e_rr)) {
		s_uccess = false;
		goto clean;
	}

	now = Time_now();

	Log_debugLnx(
		"Decrypt AES GCM: %"PRIu64" bytes within %fs (%fns/byte, %fbytes/sec).",
		Buffer_length(buf),
		(F64)(now - then) / SECOND,
		(F64)(now - then) / Buffer_length(buf),
		(F64)Buffer_length(buf) / (now - then) * SECOND
	);

clean:
	return s_uccess;
}

Bool CLI_profileAES256Impl(const ParsedArgs *args, Buffer buf, Error *e_rr) {
	return CLI_profileEncryptionImpl(args, buf, EBufferEncryptionType_AES256GCM, e_rr);
}

Bool CLI_profileAES128Impl(const ParsedArgs *args, Buffer buf, Error *e_rr) {
	return CLI_profileEncryptionImpl(args, buf, EBufferEncryptionType_AES128GCM, e_rr);
}

Bool CLI_profileAES256(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_profileData(args, CLI_profileAES256Impl);
}

Bool CLI_profileAES128(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_profileData(args, CLI_profileAES128Impl);
}

//Memory bandwidth: how fast the CPU can move / clear bytes (the number that bounds a lot of everything else).

Bool CLI_profileMemcpyImpl(const ParsedArgs *args, Buffer buf, Error *e_rr) {

	(void) args;

	Bool s_uccess = true;
	Buffer dst = Buffer_createNull();

	gotoIfError3(clean, Buffer_createUninitializedBytes(Buffer_length(buf), Platform_instance->alloc, &dst, e_rr));

	const U64 iters = 16;
	const Ns then = Time_now();

	for(U64 i = 0; i < iters; ++i)
		Buffer_memcpy(dst, buf);

	const Ns now = Time_now();
	const U64 total = Buffer_length(buf) * iters;

	//Consume a byte of the copy so the compiler can't elide the memcpys under LTO/-O2.
	const U8 sink = dst.ptr[Buffer_length(dst) - 1];

	Log_debugLnx(
		"Profile memcpy: %"PRIu64" bytes (%"PRIu64" x %"PRIu64") in %fs (%f GB/s). (sink 0x%02x)",
		total, iters, Buffer_length(buf),
		(F64)(now - then) / SECOND,
		(F64) total / (F64)(now - then),
		sink
	);

clean:
	Buffer_free(&dst, Platform_instance->alloc);
	return s_uccess;
}

Bool CLI_profileMemcpy(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_profileData(args, CLI_profileMemcpyImpl);
}

Bool CLI_profileMemsetImpl(const ParsedArgs *args, Buffer buf, Error *e_rr) {

	(void) args;
	(void) e_rr;

	const U64 iters = 16;
	const Ns then = Time_now();

	for(U64 i = 0; i < iters; ++i)
		Buffer_unsetAllBits(buf, NULL);

	const Ns now = Time_now();
	const U64 total = Buffer_length(buf) * iters;

	//Consume a byte so the compiler can't elide the memsets.
	const U8 sink = buf.ptr[Buffer_length(buf) - 1];

	Log_debugLnx(
		"Profile memset: %"PRIu64" bytes (%"PRIu64" x %"PRIu64") in %fs (%f GB/s). (sink 0x%02x)",
		total, iters, Buffer_length(buf),
		(F64)(now - then) / SECOND,
		(F64) total / (F64)(now - then),
		sink
	);

	return true;
}

Bool CLI_profileMemset(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_profileData(args, CLI_profileMemsetImpl);
}

//128-bit float SIMD throughput (add / mul / fma).

Bool CLI_profileVecImpl(const ParsedArgs *args, Buffer buf, Error *e_rr) {

	(void) args;
	(void) buf;
	(void) e_rr;

	const U64 iters = (U64) 1 << 28;        //~268M ops per test
	const F32x4 v = F32x4_create4(1.0000001f, 1.0000002f, 1.0000003f, 1.0000004f);

	F32x4 acc = F32x4_zero();
	Ns then = Time_now();
	for(U64 i = 0; i < iters; ++i)
		acc = F32x4_add(acc, v);
	Ns now = Time_now();
	Log_debugLnx(
		"Profile vec4f add: %"PRIu64" ops in %fs (%f Gop/s). (sink %f)",
		iters, (F64)(now - then) / SECOND, (F64) iters / (F64)(now - then), (F64) F32x4_x(acc)
	);

	acc = v;
	then = Time_now();
	for(U64 i = 0; i < iters; ++i)
		acc = F32x4_mul(acc, v);
	now = Time_now();
	Log_debugLnx(
		"Profile vec4f mul: %"PRIu64" ops in %fs (%f Gop/s). (sink %f)",
		iters, (F64)(now - then) / SECOND, (F64) iters / (F64)(now - then), (F64) F32x4_x(acc)
	);

	acc = F32x4_zero();
	then = Time_now();
	for(U64 i = 0; i < iters; ++i)
		acc = F32x4_fma(v, v, acc);
	now = Time_now();
	Log_debugLnx(
		"Profile vec4f fma: %"PRIu64" ops in %fs (%f Gop/s). (sink %f)",
		iters, (F64)(now - then) / SECOND, (F64) iters / (F64)(now - then), (F64) F32x4_x(acc)
	);

	return true;
}

Bool CLI_profileVec(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_profileData(args, CLI_profileVecImpl);
}

Bool CLI_profileAll(const ParsedArgs *args) {

	if(!args) return false;

	const OperationFunc all[] = {
		CLI_profileCast, CLI_profileRNG, CLI_profileCRC32C, CLI_profileFNV1A64, CLI_profileSHA256, CLI_profileMD5,
		CLI_profileAES256, CLI_profileAES128, CLI_profileMemcpy, CLI_profileMemset, CLI_profileVec
	};

	Bool s_uccess = true;

	for(U64 i = 0; i < sizeof(all) / sizeof(all[0]); ++i)
		if(!all[i](args))            //Keep going so one failure doesn't hide the other results
			s_uccess = false;

	return s_uccess;
}

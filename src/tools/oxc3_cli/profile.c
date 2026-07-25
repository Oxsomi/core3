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
#include "types/base/time.h"
#include "types/base/error.h"
#include "types/math/flp.h"
#include "types/math/vec4i.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "types/base/constants.h"

typedef Error (*ProfileOperation)(const ParsedArgs*, Buffer);

Bool CLI_profileData(const ParsedArgs *args, ProfileOperation op) {

	if(!args) return false;

	const U64 bufferSize = GIBI;

	Buffer dat = Buffer_createNull();
	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;

	gotoIfError3(clean, Buffer_createUninitializedBytes(bufferSize, Platform_instance->alloc, &dat, e_rr));

	if(!Buffer_csprng(dat))
		retError(clean, Error_invalidState(0, "CLI_profileData() Buffer_csprng failed"));

	gotoIfError2(clean, op(args, dat))

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

Error _CLI_profileCast(const ParsedArgs *args, Buffer buf) {

	(void)args;
	Error err = Error_none();

	if(Buffer_length(buf) < GIBI)
		gotoIfError(clean, Error_invalidParameter(1, 0, "_CLI_profileCast() assumes buf to be >= 1 GIBI"))

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
	return err;
}

Bool CLI_profileCast(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_profileData(args, _CLI_profileCast);
}

Error CLI_profileRNGImpl(const ParsedArgs *args, Buffer buf) {

	(void)args;

	const Ns then = Time_now();

	if(!Buffer_csprng(buf))
		return Error_invalidState(0, "CLI_profileRNGImpl() Buffer_csprng failed");

	const Ns now = Time_now();

	Log_debugLnx(
		"Profile RNG: %"PRIu64" bytes within %fs (%fns/byte, %fbytes/sec).",
		Buffer_length(buf),
		(F64)(now - then) / SECOND,
		(F64)(now - then) / Buffer_length(buf),
		(F64)Buffer_length(buf) / (now - then) * SECOND
	);

	return Error_none();
}

Bool CLI_profileRNG(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_profileData(args, CLI_profileRNGImpl);
}

Error CLI_profileCRC32CImpl(const ParsedArgs *args, Buffer buf) {

	(void)args;

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

	return Error_none();
}

Bool CLI_profileCRC32C(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_profileData(args, CLI_profileCRC32CImpl);
}

Error CLI_profileFNV1A64Impl(const ParsedArgs *args, Buffer buf) {

	(void)args;

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

	return Error_none();
}

Bool CLI_profileFNV1A64(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_profileData(args, CLI_profileFNV1A64Impl);
}

Error CLI_profileSHA256Impl(const ParsedArgs *args, Buffer buf) {

	(void)args;

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

	return Error_none();
}

Bool CLI_profileSHA256(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_profileData(args, CLI_profileSHA256Impl);
}

Error CLI_profileMD5Impl(const ParsedArgs *args, Buffer buf) {

	(void)args;

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

	return Error_none();
}

Bool CLI_profileMD5(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_profileData(args, CLI_profileMD5Impl);
}

Error CLI_profileEncryptionImpl(const ParsedArgs *args, Buffer buf, EBufferEncryptionType encryptionType) {

	(void)args;

	Ns then = Time_now();

	U32 key[8];
	I32x4 iv, tag;

	Error err = Error_none();

	const Buffer additionalData = Buffer_createNull();

	if(!Buffer_encryptAdvanced(&(BufferEncrypt) {
		.target = &buf,
		.additionalData = &additionalData,
		.type = encryptionType,
		.flags = EBufferEncryptionFlags_GenerateKey,
		.nonConstEncrypt = { .key = key, .tag = &tag, .iv = &iv }
	}, &err))
		goto clean;

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
	}, &err))
		goto clean;

	now = Time_now();

	Log_debugLnx(
		"Decrypt AES GCM: %"PRIu64" bytes within %fs (%fns/byte, %fbytes/sec).",
		Buffer_length(buf),
		(F64)(now - then) / SECOND,
		(F64)(now - then) / Buffer_length(buf),
		(F64)Buffer_length(buf) / (now - then) * SECOND
	);

clean:
	return err;
}

Error CLI_profileAES256Impl(const ParsedArgs *args, Buffer buf) {
	return CLI_profileEncryptionImpl(args, buf, EBufferEncryptionType_AES256GCM);
}

Error CLI_profileAES128Impl(const ParsedArgs *args, Buffer buf) {
	return CLI_profileEncryptionImpl(args, buf, EBufferEncryptionType_AES128GCM);
}

Bool CLI_profileAES256(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_profileData(args, CLI_profileAES256Impl);
}

Bool CLI_profileAES128(const ParsedArgs *args) {
	if(!args) return false;
	return CLI_profileData(args, CLI_profileAES128Impl);
}

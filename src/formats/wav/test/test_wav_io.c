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

//formats/wav/test/test_wav_io.c

#include "test_wav_shared.h"
#include "formats/wav/wav_file.h"
#include "formats/wav/wav_headers.h"
#include "types/container/memory_stream.h"
#include "types/container/buffer.h"
#include "types/container/test/failing_stream.h"

//The stream type has to outlive every stream made from it
static RefPtrType streamTypeStorage;

static const RefPtrType *streamType(Test *t) {
	streamTypeStorage = MemoryStream_makeType(t->alloc);
	return &streamTypeStorage;
}

static Bool makeStream(Test *t, const void *samples, U64 totalBytes, StreamRef **sr) {

	const RefPtrType *typePtr = streamType(t);

	if (!MemoryStream_create(totalBytes, EMemoryStreamFlags_IsWritable, typePtr, sr, &t->err))
		return false;

	if (samples) {
		OxStream *s = RefPtr_data(*sr, OxStream);
		s->write(s, 0, totalBytes, Buffer_createRefConst(samples, totalBytes), t->alloc, &t->err);
	}

	return true;
}

static void putU32(U8 *out, U32 v) {
	for (U32 i = 0; i < 4; ++i)
		out[i] = (U8) (v >> (i << 3));
}

//RIFF + fmt chunk (body as given, padded) + data chunk (dataLen declared, dataBytes actually present) + trailing bytes.
//The RIFF size covers everything but the trailing bytes, unless riffSize is non zero.
static U64 buildWav(
	U8 *out, const U8 *fmtBody, U32 fmtSize, U32 dataLen, U32 dataBytes, U32 trailing, U32 riffSize
) {

	U64 o = 12;

	putU32(out + o, RIFFFmtHeader_magic);
	putU32(out + o + 4, fmtSize);
	o += 8;

	for (U32 i = 0; i < fmtSize; ++i)
		out[o + i] = fmtBody[i];

	o += fmtSize;

	if(fmtSize & 1)
		out[o++] = 0;

	putU32(out + o, RIFFDataHeader_magic);
	putU32(out + o + 4, dataLen);
	o += 8;

	for (U32 i = 0; i < dataBytes; ++i)
		out[o + i] = (U8) (0x10 + i);

	o += dataBytes;

	if(dataBytes & 1)
		out[o++] = 0;

	putU32(out, RIFFHeader_magic);
	putU32(out + 4, riffSize ? riffSize : (U32) (o - 8));
	putU32(out + 8, RIFFWAVHeader_magic);

	for (U32 i = 0; i < trailing; ++i)
		out[o + i] = 0xEE;

	return o + trailing;
}

//PCM 16-bit mono 44100 Hz, 16 byte fmt body

static const U8 fmtPcm16[16] = { 1, 0, 1, 0, 0x44, 0xAC, 0, 0, 0x88, 0x58, 0x01, 0, 2, 0, 16, 0 };

static Bool readBytes(Test *t, const U8 *bytes, U64 size, U64 off, U64 len, WAVFile *wf, Error *err) {

	StreamRef *sr = NULL;

	if (!makeStream(t, bytes, size, &sr))
		return false;

	Bool ok = WAV_read(sr, off, len, t->alloc, wf, err);

	RefPtr_dec(&sr);
	return ok;
}

void Test_WAVReadAdditionalChunksAndRates(Test *t) {

	Test_setModule(t, "WAV read: unknown chunks and arbitrary sample rates");

	U8 file[160] = { 0 }, wav[128] = { 0 }, fmt[16];

	Buffer_memcpy(Buffer_createRef(fmt, sizeof(fmt)), Buffer_createRefConst(fmtPcm16, sizeof(fmtPcm16)));
	putU32(fmt + 4, 12345);
	putU32(fmt + 8, 24690);

	U64 wavSize = buildWav(wav, fmt, sizeof(fmt), 4, 4, 0, 0);
	WAVFile wf = (WAVFile) { 0 };

	Test_assert(t, "nonstandard frequency", readBytes(t, wav, wavSize, 0, 0, &wf, NULL) && wf.fmt.frequency == 12345);

	const U32 chunks[] = { 0x4B414550, 0x64696361, 0x74736574 };

	for(U32 i = 0; i < 3; ++i) {

		wavSize = buildWav(wav, fmtPcm16, sizeof(fmtPcm16), 4, 4, 0, 0);
		Buffer_memcpy(Buffer_createRef(file, 12), Buffer_createRefConst(wav, 12));
		putU32(file + 4, (U32) wavSize + 12 - 8);
		putU32(file + 12, chunks[i]);
		putU32(file + 16, 3);
		file[20] = 1;
		file[21] = 2;
		file[22] = 3;
		file[23] = 0;
		Buffer_memcpy(Buffer_createRef(file + 24, wavSize - 12), Buffer_createRefConst(wav + 12, wavSize - 12));
		Test_assert(t, "unknown odd chunk skipped", readBytes(t, file, wavSize + 12, 0, 0, &wf, NULL));
		Test_assert(t, "data located after padding", wf.dataStart == 56 && wf.dataLength == 4);
	}
}

//Offsets in the stream and bytes after the file: both used to be rejected (relative and absolute mixed)

void Test_WAVReadOffsetAndTrailing(Test *t) {

	Test_setModule(t, "WAV_read: stream offset and trailing bytes");

	U8 file[256] = { 0 };
	U8 wav[128];
	U64 wavSize = buildWav(wav, fmtPcm16, 16, 4, 4, 0, 0);        //48 bytes

	WAVFile wf = { 0 };

	Test_assert(t, "plain", readBytes(t, wav, wavSize, 0, 0, &wf, NULL));
	Test_assert(t, "plain data", wf.dataStart == 44 && wf.dataLength == 4);

	//Trailing bytes after the RIFF chunk

	U64 size = buildWav(file, fmtPcm16, 16, 4, 4, 5, 0);

	Test_assert(t, "trailing", readBytes(t, file, size, 0, 0, &wf, NULL));
	Test_assert(t, "trailing data", wf.dataStart == 44 && wf.dataLength == 4);

	//Nonzero offset, with and without a length, and trailing bytes

	for (U64 i = 0; i < 100; ++i)
		file[i] = 0xAA;

	size = 100 + buildWav(file + 100, fmtPcm16, 16, 4, 4, 7, 0);
	Test_assert(t, "offset 100", readBytes(t, file, size, 100, 0, &wf, NULL));
	Test_assert(t, "offset 100 data", wf.dataStart == 100 + 44 && wf.dataLength == 4);
	Test_assert(t, "offset 100 len", readBytes(t, file, size, 100, 48, &wf, NULL));
	Test_assert(t, "offset 100 len data", wf.dataStart == 144);

	//A length that cuts the file short is an error and so is one that passes the stream

	Test_assert(t, "len too short", !readBytes(t, file, size, 100, 47, &wf, NULL));
	Test_assert(t, "len too long", !readBytes(t, file, size, 100, size, &wf, NULL));
	Test_assert(t, "len header only", !readBytes(t, file, size, 100, 11, &wf, NULL));
	Test_assert(t, "offset past end", !readBytes(t, file, size, size, 0, &wf, NULL));
}

//Chunk sizes come from the file and are checked against the end of the file, not an offset relative to it

void Test_WAVReadChunkBounds(Test *t) {

	Test_setModule(t, "WAV_read: chunk sizes checked against the end");

	U8 file[128];
	WAVFile wf = { 0 };

	//Data chunk that declares 4 GiB

	U64 size = buildWav(file, fmtPcm16, 16, 0xFFFFFF00, 4, 0, 0);

	Test_assert(t, "huge data chunk", !readBytes(t, file, size, 0, 0, &wf, NULL));

	size = buildWav(file, fmtPcm16, 16, 0xFFFFFFFC, 4, 0, 0);
	Test_assert(t, "4 GiB - 4 data chunk", !readBytes(t, file, size, 0, 0, &wf, NULL));

	//Data chunk that goes past the RIFF chunk, though the stream has the bytes

	size = buildWav(file, fmtPcm16, 16, 4, 4, 0, 0);
	file[40] = 8;
	Test_assert(t, "data past the riff size", !readBytes(t, file, size + 16, 0, 0, &wf, NULL));

	//Data chunk that goes past the file, at an offset

	U8 shifted[160] = { 0 };
	U64 shiftedSize = 40 + buildWav(shifted + 40, fmtPcm16, 16, 4, 4, 0, 0);

	Test_assert(t, "shifted ok", readBytes(t, shifted, shiftedSize, 40, 0, &wf, NULL));
	shifted[40 + 40] = 5;
	Test_assert(t, "shifted data too long", !readBytes(t, shifted, shiftedSize, 40, 0, &wf, NULL));

	//RIFF size beyond the stream

	size = buildWav(file, fmtPcm16, 16, 4, 4, 0, 0x1000);
	Test_assert(t, "riff size beyond", !readBytes(t, file, size, 0, 0, &wf, NULL));

	//Half a chunk header at the end of the file

	size = buildWav(file, fmtPcm16, 16, 4, 4, 4, 0);
	file[4] += 4;
	Test_assert(t, "half chunk header", !readBytes(t, file, size, 0, 0, &wf, NULL));
}

//fmt chunk: the chunk size counts, with padding for odd sizes; extensible needs the whole 40 bytes

void Test_WAVReadFmtChunk(Test *t) {

	Test_setModule(t, "WAV_read: fmt chunk size and format tag");

	U8 file[160];
	U8 fmt[48] = { 0 };
	WAVFile wf = { 0 };

	for (U32 i = 0; i < 16; ++i)
		fmt[i] = fmtPcm16[i];

	//cbSize extension (18) and an odd size (17) with its pad byte: the data chunk is still found

	U64 size = buildWav(file, fmt, 18, 4, 4, 0, 0);

	Test_assert(t, "fmt size 18", readBytes(t, file, size, 0, 0, &wf, NULL));
	Test_assert(t, "fmt size 18 data", wf.dataStart == 12 + 8 + 18 + 8 && wf.dataLength == 4 && wf.fmt.stride == 16);

	size = buildWav(file, fmt, 17, 4, 4, 0, 0);
	Test_assert(t, "fmt size 17", readBytes(t, file, size, 0, 0, &wf, NULL));
	Test_assert(t, "fmt size 17 data", wf.dataStart == 12 + 8 + 18 + 8 && wf.dataLength == 4);

	size = buildWav(file, fmt, 40, 4, 4, 0, 0);
	Test_assert(t, "fmt size 40 non extensible", readBytes(t, file, size, 0, 0, &wf, NULL));
	Test_assert(t, "fmt size 40 data", wf.dataStart == 12 + 8 + 40 + 8);

	//Too small for the header it has to hold

	size = buildWav(file, fmt, 15, 4, 4, 0, 0);
	Test_assert(t, "fmt size 15", !readBytes(t, file, size, 0, 0, &wf, NULL));
	size = buildWav(file, fmt, 14, 4, 4, 0, 0);
	Test_assert(t, "fmt size 14", !readBytes(t, file, size, 0, 0, &wf, NULL));
	size = buildWav(file, fmt, 0, 4, 4, 0, 0);
	Test_assert(t, "fmt size 0", !readBytes(t, file, size, 0, 0, &wf, NULL));

	//Format tags at 32 bits: PCM and float only (stride 32, 4 bytes per block)

	U8 fmt32[16] = { 1, 0, 1, 0, 0x44, 0xAC, 0, 0, 0x10, 0xB1, 0x02, 0, 4, 0, 32, 0 };

	size = buildWav(file, fmt32, 16, 4, 4, 0, 0);
	Test_assert(t, "pcm32", readBytes(t, file, size, 0, 0, &wf, NULL) && wf.fmt.format == ERIFFAudioFormat_PCM);

	fmt32[0] = 3;
	size = buildWav(file, fmt32, 16, 4, 4, 0, 0);
	Test_assert(t, "float32", readBytes(t, file, size, 0, 0, &wf, NULL) && wf.fmt.format == ERIFFAudioFormat_IEEE754);

	U16 badTags[] = { 0, 2, 6, 7, 0x11, 0x55, 0xFFFF };

	for (U64 i = 0; i < sizeof(badTags) / sizeof(badTags[0]); ++i) {

		fmt32[0] = (U8) badTags[i];
		fmt32[1] = (U8) (badTags[i] >> 8);
		size = buildWav(file, fmt32, 16, 4, 4, 0, 0);
		Test_assert(t, "32-bit unknown tag", !readBytes(t, file, size, 0, 0, &wf, NULL));
	}

	//Extensible: 16 + 24 bytes. 36 would put the GUID past the chunk (it read the next chunk's bytes).

	U8 ext[48] = {
		0xFE, 0xFF, 1, 0, 0x44, 0xAC, 0, 0, 0x10, 0xB1, 0x02, 0, 4, 0, 32, 0,
		22, 0, 24, 0, 4, 0, 0, 0,
		1, 0, 0, 0, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71
	};

	size = buildWav(file, ext, 40, 4, 4, 0, 0);
	Test_assert(t, "extensible 40", readBytes(t, file, size, 0, 0, &wf, NULL) && wf.fmt.format == ERIFFAudioFormat_PCM);

	//Valid bits below the container are accepted (the container decides how samples are read)

	ext[18] = 20;
	size = buildWav(file, ext, 40, 4, 4, 0, 0);
	Test_assert(t, "extensible 20 valid bits", readBytes(t, file, size, 0, 0, &wf, NULL) && wf.fmt.stride == 32);

	ext[18] = 0;
	size = buildWav(file, ext, 40, 4, 4, 0, 0);
	Test_assert(t, "extensible 0 valid bits", !readBytes(t, file, size, 0, 0, &wf, NULL));

	ext[18] = 33;
	size = buildWav(file, ext, 40, 4, 4, 0, 0);
	Test_assert(t, "extensible valid bits above container", !readBytes(t, file, size, 0, 0, &wf, NULL));
	ext[18] = 24;

	for (U32 fmtSize = 16; fmtSize < 40; ++fmtSize) {
		size = buildWav(file, ext, fmtSize, 4, 4, 0, 0);
		Test_assert(t, "extensible too small", !readBytes(t, file, size, 0, 0, &wf, NULL));
	}

	//Subformat other than PCM and float

	ext[24] = 2;
	size = buildWav(file, ext, 40, 4, 4, 0, 0);
	Test_assert(t, "extensible subformat 2", !readBytes(t, file, size, 0, 0, &wf, NULL));
	ext[24] = 1;

	//Extensible with extra bytes after the GUID is fine

	size = buildWav(file, ext, 44, 4, 4, 0, 0);
	Test_assert(t, "extensible 44", readBytes(t, file, size, 0, 0, &wf, NULL));
	Test_assert(t, "extensible 44 data", wf.dataStart == 12 + 8 + 44 + 8);
}

//Failing each write in turn: a write that is lost must not be reported as success

typedef Bool (*WriteFunc)(Test*, StreamRef*, Error*);

static void failEachWrite(Test *t, const C8 *name, WriteFunc run) {

	const RefPtrType *typePtr = streamType(t);
	StreamRef *out = NULL;

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, typePtr, &out, &t->err)) {
		Test_assert(t, name, false);
		return;
	}

	FailingStream_arm(out, 0);
	Test_assert(t, name, run(t, out, &t->err));

	U64 writes = FailingStream_writes();

	Test_assert(t, name, writes > 0);
	RefPtr_dec(&out);

	for (U64 i = 1; i <= writes; ++i) {

		if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, typePtr, &out, &t->err)) {
			Test_assert(t, name, false);
			return;
		}

		FailingStream_arm(out, i);

		Error err = (Error) { 0 };

		Test_assert(t, name, !run(t, out, &err) && FailingStream_failed() && err.genericError);
		RefPtr_dec(&out);
	}
}

static Bool writeFromStream(Test *t, StreamRef *out, Error *err) {

	StreamRef *in = NULL;
	U8 samples[16] = { 0 };

	if (!makeStream(t, samples, sizeof(samples), &in))
		return false;

	Bool ok = WAV_write(out, in, 0, 0, sizeof(samples), true, 44100, 16, true, NULL, t->alloc, err);

	RefPtr_dec(&in);
	return ok;
}

static Bool writeHeaderOnly(Test *t, StreamRef *out, Error *err) {
	U64 dataOff = 0;
	return WAV_write(out, NULL, 0, 0, 16, true, 44100, 16, true, &dataOff, t->alloc, err);
}

static Bool convertWrite(Test *t, StreamRef *out, Error *err) {

	StreamRef *in = NULL;
	I16 samples[4] = { 1, 2, 3, 4 };

	if (!makeStream(t, samples, sizeof(samples), &in))
		return false;

	WAVConversionInfo info = {
		.format = EAudioFormat_WAV, .splitType = ESplitType_Untouched,
		.oldByteCountStereoPcm = (1 << 6) | 2, .newByteCountStereoPcm = (1 << 6) | 3
	};

	Bool ok = WAVFile_convert(in, 0, sizeof(samples), out, 0, info, 44100, true, t->alloc, err);

	RefPtr_dec(&in);
	return ok;
}

void Test_WAVWriteFailedWrites(Test *t) {

	Test_setModule(t, "WAV_write: failed writes are reported");

	failEachWrite(t, "WAV_write from stream", writeFromStream);
	failEachWrite(t, "WAV_write header only", writeHeaderOnly);
	failEachWrite(t, "WAVFile_convert", convertWrite);
}

//Sizes that don't fit the U32 RIFF fields are rejected instead of truncated; odd data gets a pad on every path

void Test_WAVWriteLimits(Test *t) {

	Test_setModule(t, "WAV_write: sizes and pad byte");

	const RefPtrType *typePtr = streamType(t);

	StreamRef *out = NULL;
	U64 dataOff = 0;

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, typePtr, &out, &t->err)) {
		Test_assert(t, "create", false);
		return;
	}

	const U64 lengths[] = { (U64) 1 << 32, ((U64) 1 << 32) + 4, (U64) U32_MAX - 20, (U64) U32_MAX - 35, (U64) 1 << 40 };

	for (U64 i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {

		Error err = (Error) { 0 };

		Test_assert(
			t, "length beyond the U32 riff size",
			!WAV_write(out, NULL, 0, 0, lengths[i], false, 44100, 8, true, &dataOff, t->alloc, &err) && err.genericError
		);

		Test_assert(t, "nothing written", RefPtr_data(out, OxStream)->size == 0);
	}

	RefPtr_dec(&out);

	//Odd length on the dataOutput path: the caller writes the data, the pad is already behind it

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, typePtr, &out, &t->err)) {
		Test_assert(t, "create", false);
		return;
	}

	Test_assert(
		t, "odd header only",
		WAV_write(out, NULL, 0, 0, 3, false, 44100, 8, true, &dataOff, t->alloc, &t->err)
	);

	Test_assert(t, "data offset", dataOff == 44);
	Test_assert(t, "size covers data and pad", RefPtr_data(out, OxStream)->size == 44 + 3 + 1);

	OxStream *s = RefPtr_data(out, OxStream);
	U8 data[3] = { 7, 8, 9 };

	Test_assert(t, "data write", s->write(s, dataOff, 3, Buffer_createRefConst(data, 3), t->alloc, &t->err));

	const U8 *bytes = RefPtr_data(out, MemoryStream)->data.ptr;

	Test_assert(t, "data kept", bytes[44] == 7 && bytes[45] == 8 && bytes[46] == 9);
	Test_assert(t, "pad is 0", bytes[47] == 0);
	Test_assert(t, "riff size", bytes[4] == 48 - 8 && bytes[5] == 0 && bytes[6] == 0 && bytes[7] == 0);

	WAVFile wf = { 0 };

	Test_assert(t, "reads back", WAV_read(out, 0, 0, t->alloc, &wf, &t->err) && wf.dataLength == 3);

	RefPtr_dec(&out);
}

void Test_WAVWriteAdditionalRates(Test *t) {

	Test_setModule(t, "WAV write: arbitrary sample rates and representable byte rates");

	const U32 rates[] = { 12345, 16000, 24000, 88200, 384000 };

	for(U32 i = 0; i < sizeof(rates) / sizeof(rates[0]); ++i) {

		StreamRef *out = NULL;
		U64 dataOff = 0;

		if (!Test_assert(t, "create output", MemoryStream_create(
			0, EMemoryStreamFlags_WriteResize, streamType(t), &out, &t->err
		)))
			continue;

		Bool ok = WAV_write(out, NULL, 0, 0, 4, false, rates[i], 16, true, &dataOff, t->alloc, &t->err);

		Test_assert(t, "write supported rate", ok);

		if(ok) {

			const U32 samples = 0;
			OxStream *s = RefPtr_data(out, OxStream);

			Test_assert(t, "write samples", s->write(
				s, dataOff, sizeof(samples), Buffer_createRefConst(&samples, sizeof(samples)), t->alloc, &t->err
			));

			WAVFile wf = (WAVFile) { 0 };

			Test_assert(t, "read sample rate", WAV_read(out, 0, 0, t->alloc, &wf, &t->err) && wf.fmt.frequency == rates[i]);
			Test_assert(t, "byte rate is exact", wf.fmt.bytesPerSecond == rates[i] * 2);
		}

		Test_assert(
			t, "unrepresentable byte rate refused",
			!WAV_write(out, NULL, 0, 0, 4, false, U32_MAX, 16, true, &dataOff, t->alloc, NULL)
		);

		RefPtr_dec(&out);
	}
}

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

//formats/oiCA/test/test_oiCA_review.c

#include "test_oiCA_shared.h"
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include "types/container/memory_stream.h"
#include "types/container/encryption_stream.h"
#include "types/container/test/failing_stream.h"
#include "types/container/test/basic_alloc.h"
#include "types/base/time.h"
#include "types/base/string_read_helper.h"
#include "formats/oiCA/ca_file.h"
#include "formats/oiCA/ca_headers.h"
#include "formats/oiCA/ca_edit.h"
#include "formats/oiCA/ca_lookup.h"
#include "formats/oiCA/ca_props.h"
#include "formats/oiCA/ca_combine.h"
#include "formats/oiCA/ca_compare.h"
#include "formats/oiDL/dl_entry.h"

extern const CASettings kCASettings;
extern const CASettings kCASettingsDate;

CAHandle addFile(Test *t, CAFile *ca, CAHandle parent, const C8 *name, Ns time, Bool failIsSuccess);
CAHandle addFolder(Test *t, CAFile *ca, CAHandle parent, const C8 *name, Bool failIsSuccess);

static void fillBytes(U8 *ptr, U8 value, U64 length) {

	for (U64 i = 0; i < length; ++i)
		ptr[i] = value;
}

static void copyName(C8 *dst, const C8 *src) {
	U64 length = CharString_length(CharString_createRefCStrConst(src));
	Buffer_memcpy(Buffer_createRef(dst, length + 1), Buffer_createRefConst(src, length + 1));
}

static Bool sameName(const C8 *a, const C8 *b) {
	CharString str = CharString_createRefCStrConst(a);
	return CharString_equalsCStringSensitive(&str, b);
}

static CAHandle resolveStr(CAFile *ca, const C8 *path, Bool folder) {
	CharString str = CharString_createRefCStrConst(path);
	return folder ? CAFile_resolveFolder(ca, str) : CAFile_resolveFile(ca, str);
}

static Bool writeToMemory(Test *t, CAFile *ca, const RefPtrType *memType, const RefPtrType *encType, StreamRef **stream) {

	U64 offset = 0;

	return
		MemoryStream_create(0, EMemoryStreamFlags_WriteResize, memType, stream, &t->err) &&
		CAFile_write(ca, encType, *stream, &offset, t->alloc, &t->err);
}

//Counts the big allocations made through it, so a test can tell how much memory an operation asked for.

typedef struct CountingAllocator {

	const Allocator *inner;
	U64 threshold;
	U64 bigAllocs;
} CountingAllocator;

static Bool CountingAllocator_alloc(void *ptr, U64 length, Buffer *output, Error *e_rr) {

	CountingAllocator *counting = (CountingAllocator*) ptr;

	if (length >= counting->threshold)
		++counting->bigAllocs;

	return counting->inner->alloc(counting->inner->ptr, length, output, e_rr);
}

static void CountingAllocator_free(void *ptr, Buffer buf) {
	const CountingAllocator *counting = (const CountingAllocator*) ptr;
	counting->inner->free(counting->inner->ptr, buf);
}

//Every index the archive keeps has to agree: each folder's folders and files are contiguous blocks that point back
// at it, each entry is in exactly one block, folders come after their parents and the name and content lists match.

static const C8 *reason = "";

//Formatted text for messages; each slot stays valid until the next format into it.

static CharString formats[4];

static const C8 *format(U64 slot, const C8 *fmt, ...) {

	va_list args;
	va_start(args, fmt);

	CharString_free(&formats[slot], &BasicAllocator_instance);
	formats[slot] = CharString_createNull();
	CharString_formatVariadic(&BasicAllocator_instance, &formats[slot], NULL, fmt, args);

	va_end(args);
	return formats[slot].ptr ? formats[slot].ptr : "";
}

static void freeFormats() {

	for (U64 i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i)
		CharString_free(&formats[i], &BasicAllocator_instance);

	reason = "";
}

static Bool checkInvariants(const CAFile *ca) {

	const U64 folders = ca->folders.length, files = ca->files.length;

	if (ca->names.entryStreams.length != folders + files) {
		reason = format(0, "names length %"PRIu64" != %"PRIu64, ca->names.entryStreams.length, folders + files);
		return false;
	}

	if (ca->content.entryStreams.length != files) {
		reason = format(0, "content length %"PRIu64" != %"PRIu64, ca->content.entryStreams.length, files);
		return false;
	}

	if (ca->folders.ptr[0].parent) {
		reason = format(0, "root has a parent");
		return false;
	}

	U8 *seen = NULL;
	Buffer seenBuf = Buffer_createNull();

	if (!Buffer_createZeroBits((folders + files) << 3, &BasicAllocator_instance, &seenBuf, NULL)) {
		reason = format(0, "out of memory");
		return false;
	}

	seen = seenBuf.ptrNonConst;

	Bool ok = true;

	for (U64 f = 0; f < folders && ok; ++f) {

		const CAFolderInfo *info = &ca->folders.ptr[f];

		if (f && info->parent >= f) {

			reason = format(0, "folder %"PRIu64" has parent %u (not before it)", f, info->parent);
			ok = false;
			break;
		}

		for (U64 i = 0; i < info->dirCount && ok; ++i) {

			U64 c = (U64) info->dirOffset + i;

			if (c >= folders || !c || ca->folders.ptr[c].parent != f || seen[c]++) {
				reason = format(0, "folder %"PRIu64" has a bad folder block entry %"PRIu64, f, c);
				ok = false;
			}
		}

		for (U64 i = 0; i < info->fileCount && ok; ++i) {

			U64 c = info->fileOffset + i;

			if (c >= files || CAFileInfo_getParent(ca->files.ptr[c]) != f || seen[folders + c]++) {
				reason = format(0, "folder %"PRIu64" has a bad file block entry %"PRIu64, f, c);
				ok = false;
			}
		}
	}

	for (U64 i = 1; i < folders + files && ok; ++i)
		if (seen[i] != 1) {
			reason = format(0, "entry %"PRIu64" is in %u blocks", i, (unsigned) seen[i]);
			ok = false;
		}

	Buffer_free(&seenBuf, &BasicAllocator_instance);
	return ok;
}

//Model based test: the archive is edited at random and an independent model of the tree has to agree after every op.

#define MODEL_MAX 120

typedef struct ModelNode {

	Bool alive, isFolder;
	I32 parent;
	I32 id;
	C8 name[24];
} ModelNode;

typedef struct Model {

	ModelNode nodes[MODEL_MAX];    //0 is the root
	I32 count;
	I32 nextId;
	U64 rng;
} Model;

static U64 Model_rand(Model *m) {

	m->rng ^= m->rng << 13;
	m->rng ^= m->rng >> 7;
	m->rng ^= m->rng << 17;
	return m->rng;
}

static void Model_path(const Model *m, I32 n, C8 *out, U64 cap) {

	I32 chain[64];
	U64 depth = 0, len = 0;

	for (I32 c = n; c > 0 && depth < 64; c = m->nodes[c].parent)
		chain[depth++] = c;

	for (U64 i = depth; i > 0; --i) {

		const C8 *name = m->nodes[chain[i - 1]].name;
		U64 nameLen = CharString_length(CharString_createRefCStrConst(name));

		if (len + nameLen + 2 > cap)
			break;

		Buffer_memcpy(Buffer_createRef(out + len, nameLen), Buffer_createRefConst(name, nameLen));
		len += nameLen;

		if (i > 1)
			out[len++] = '/';
	}

	out[len] = 0;
}

static Bool Model_isUnder(const Model *m, I32 n, I32 ancestor) {

	for (I32 c = n; c > 0; c = m->nodes[c].parent)
		if (c == ancestor)
			return true;

	return false;
}

static CAHandle Model_handle(const Model *m, CAFile *ca, I32 n) {

	if (!n)
		return CAHandle_Root;

	C8 path[512];

	Model_path(m, n, path, sizeof(path));
	return resolveStr(ca, path, m->nodes[n].isFolder);
}

static U64 Model_dataLength(I32 id) { return 1 + (U64) id % 13; }

static Bool Model_compare(const Model *m, CAFile *ca) {

	U64 files = 0, folders = 0;

	for (I32 n = 1; n < m->count; ++n) {

		if (!m->nodes[n].alive)
			continue;

		C8 path[512];

		Model_path(m, n, path, sizeof(path));

		if (m->nodes[n].isFolder) {

			++folders;

			if (resolveStr(ca, path, true) == CAHandle_Invalid) {
				reason = format(0, "folder %s is missing", path);
				return false;
			}

			continue;
		}

		++files;

		CAHandle h = resolveStr(ca, path, false);

		if (h == CAHandle_Invalid) {
			reason = format(0, "file %s is missing", path);
			return false;
		}

		Bool valid = false;
		Buffer data = CAFile_getDataConst(ca, h, &valid);

		if (!valid || Buffer_length(data) != Model_dataLength(m->nodes[n].id) || data.ptr[0] != (U8) m->nodes[n].id) {
			reason = format(0, "file %s has the wrong data", path);
			return false;
		}

		CharString full = CharString_createNull();
		Bool same =
			CAFile_getFullName(ca, h, &BasicAllocator_instance, &full, NULL) &&
			CharString_equalsCStringSensitive(&full, path);

		CharString_free(&full, &BasicAllocator_instance);

		if (!same) {
			reason = format(0, "full name of %s is wrong", path);
			return false;
		}
	}

	if (CAFile_fileCount(ca, CAHandle_Root, true) != files || CAFile_dirCount(ca, CAHandle_Root, true) != folders) {
		reason = format(0, "recursive counts disagree with the model");
		return false;
	}

	if (ca->files.length != files || ca->folders.length != folders + 1) {
		reason = format(0, "list lengths disagree with the model");
		return false;
	}

	return checkInvariants(ca);
}

static I32 Model_pick(Model *m, Bool onlyFolders) {

	I32 candidates[MODEL_MAX];
	U64 k = 0;

	if (onlyFolders)
		candidates[k++] = 0;

	for (I32 n = 1; n < m->count; ++n)
		if (m->nodes[n].alive && (!onlyFolders || m->nodes[n].isFolder))
			candidates[k++] = n;

	return k ? candidates[Model_rand(m) % k] : -1;
}

static Bool Model_nameUsed(const Model *m, I32 parent, const C8 *name) {

	for (I32 n = 1; n < m->count; ++n)
		if (m->nodes[n].alive && m->nodes[n].parent == parent && sameName(m->nodes[n].name, name))
			return true;

	return false;
}

static Bool Model_run(Test *t, U64 seed, U64 steps) {

	const RefPtrType memType = MemoryStream_makeType(t->alloc);
	Bool s_uccess = true;
	CAFile ca = { 0 }, back = { 0 };
	StreamRef *stream = NULL;
	Model m = { .count = 1, .nextId = 1, .rng = 0x1234567 ^ (seed * 0x9E3779B97F4A7C15) };

	m.nodes[0] = (ModelNode) { .alive = true, .isFolder = true, .parent = -1 };

	for (U64 i = 0; i < 10; ++i)
		Model_rand(&m);

	if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err))
		return false;

	for (U64 step = 0; step < steps && m.count < MODEL_MAX - 2; ++step) {

		const U64 op = Model_rand(&m) % 10;
		const C8 *desc = "";

		if (op < 3 || m.count < 4 || op < 6) {

			const Bool isFolder = op < 3 || m.count < 4;
			I32 parent = Model_pick(&m, true);
			const C8 *name = format(2, "%c%d", isFolder ? 'd' : 'f', m.nextId);

			desc = format(1, "step %"PRIu64" add %s under node %d", step, name, parent);

			CAHandle parentHandle = Model_handle(&m, &ca, parent);
			CAHandle h = isFolder ? addFolder(t, &ca, parentHandle, name, true) : addFile(t, &ca, parentHandle, name, 0, true);

			if (h == CAHandle_Invalid) {
				reason = format(0, "add failed");
				goto fail;
			}

			if (!isFolder) {

				Buffer data = Buffer_createNull();

				if (!Buffer_createUninitializedBytes(Model_dataLength(m.nextId), t->alloc, &data, &t->err))
					goto fail;

				fillBytes(data.ptrNonConst, (U8) m.nextId, Buffer_length(data));

				if (!CAFile_setData(&ca, h, t->alloc, &data, &t->err)) {
					Buffer_free(&data, t->alloc);
					goto fail;
				}
			}

			m.nodes[m.count] = (ModelNode) { .alive = true, .isFolder = isFolder, .parent = parent, .id = m.nextId };
			copyName(m.nodes[m.count++].name, name);
			++m.nextId;
		}

		else if (op < 8) {

			I32 n = Model_pick(&m, false), to = Model_pick(&m, true);

			if (n < 0 || m.nodes[n].parent == to || (m.nodes[n].isFolder && Model_isUnder(&m, to, n)))
				continue;

			if (Model_nameUsed(&m, to, m.nodes[n].name))
				continue;

			desc = format(1, "step %"PRIu64" move node %d (%s) into node %d", step, n, m.nodes[n].name, to);

			if (!CAFile_move(&ca, Model_handle(&m, &ca, n), Model_handle(&m, &ca, to), t->alloc, &t->err)) {
				reason = format(0, "move failed");
				goto fail;
			}

			m.nodes[n].parent = to;
		}

		else if (op == 8) {

			I32 n = Model_pick(&m, false);

			if (n < 0)
				continue;

			desc = format(1, "step %"PRIu64" remove node %d (%s)", step, n, m.nodes[n].name);

			CAHandle h = Model_handle(&m, &ca, n);

			Bool removed =
				m.nodes[n].isFolder ? CAFile_removeFolder(&ca, h, t->alloc, &t->err) :
				CAFile_removeFile(&ca, h, t->alloc, &t->err);

			if (!removed) {
				reason = format(0, "remove failed");
				goto fail;
			}

			for (I32 k = 1; k < m.count; ++k)
				if (k == n || Model_isUnder(&m, k, n))
					m.nodes[k].alive = false;
		}

		else {

			I32 n = Model_pick(&m, false);

			if (n < 0)
				continue;

			const C8 *name = format(2, "r%d", m.nextId);

			desc = format(1, "step %"PRIu64" rename node %d (%s) to %s", step, n, m.nodes[n].name, name);

			CharString str = CharString_createNull();

			if (!CharString_createCopy(CharString_createRefCStrConst(name), t->alloc, &str, &t->err))
				goto fail;

			Bool renamed = CAFile_rename(&ca, Model_handle(&m, &ca, n), t->alloc, &str, &t->err);

			CharString_free(&str, t->alloc);

			if (!renamed) {
				reason = format(0, "rename failed");
				goto fail;
			}

			copyName(m.nodes[n].name, name);
			++m.nextId;
		}

		if (!Model_compare(&m, &ca))
			goto fail;

		continue;

	fail:
		Test_assert(t, desc, false);
		Test_assert(t, reason, false);
		s_uccess = false;
		goto clean;
	}

	//What the model agrees with has to survive being written and read

	if (!writeToMemory(t, &ca, &memType, NULL, &stream)) {

		Test_assert(t, "model archive writes", false);
		s_uccess = false;
		goto clean;
	}

	if (!CAFile_read(stream, NULL, 0, NULL, t->alloc, &back, &t->err)) {

		Test_assert(t, "model archive reads back", false);
		s_uccess = false;
		goto clean;
	}

	if (!Model_compare(&m, &back)) {

		Test_assert(t, "model agrees after a round trip", false);
		Test_assert(t, reason, false);
		s_uccess = false;
	}

clean:
	RefPtr_dec(&stream);
	CAFile_free(&ca, t->alloc);
	CAFile_free(&back, t->alloc);
	return s_uccess;
}

//Moving a folder has to keep the whole subtree reachable and the archive readable.

void Test_CAMoveFolders(Test *t) {

	Test_setModule(t, "CAFile_move folders");

	const RefPtrType memType = MemoryStream_makeType(t->alloc);

	{                            //Moving a folder shifts the folders after it: nothing becomes its own parent or unreachable

		CAFile ca = { 0 }, back = { 0 };
		StreamRef *stream = NULL;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for folder move", false);
			goto doneShift;
		}

		CAHandle a = addFolder(t, &ca, CAHandle_Root, "A", false);

		addFolder(t, &ca, a, "B", false);
		addFolder(t, &ca, a, "C", false);
		addFolder(t, &ca, CAHandle_Root, "D", false);

		a = resolveStr(&ca, "A", true);
		addFile(t, &ca, resolveStr(&ca, "A/C", true), "c.txt", 1700000000 * SECOND, false);
		addFile(t, &ca, resolveStr(&ca, "D", true), "d.txt", 0, false);
		addFile(t, &ca, a, "a.txt", 0, false);

		CAHandle d = resolveStr(&ca, "D", true), b = resolveStr(&ca, "A/B", true);

		Test_assert(t, "move folder D into A/B", CAFile_move(&ca, d, b, t->alloc, &t->err));

		Test_assert(t, "A/B/D found",         resolveStr(&ca, "A/B/D", true) != CAHandle_Invalid);
		Test_assert(t, "A/B still found",     resolveStr(&ca, "A/B", true) != CAHandle_Invalid);
		Test_assert(t, "A/C still found",     resolveStr(&ca, "A/C", true) != CAHandle_Invalid);
		Test_assert(t, "D is gone",           resolveStr(&ca, "D", true) == CAHandle_Invalid);
		Test_assert(t, "A/B/D/d.txt found",   resolveStr(&ca, "A/B/D/d.txt", false) != CAHandle_Invalid);
		Test_assert(t, "A/C/c.txt found",     resolveStr(&ca, "A/C/c.txt", false) != CAHandle_Invalid);
		Test_assert(t, "A/a.txt found",       resolveStr(&ca, "A/a.txt", false) != CAHandle_Invalid);
		Test_assert(t, "file time kept",      CAFile_fileTime(&ca, resolveStr(&ca, "A/C/c.txt", false)) == 1700000000 * SECOND);
		Test_assert(t, "tree is consistent",  checkInvariants(&ca));

		//Back to the root, then the written archive has to read

		Test_assert(t, "move folder back to root", CAFile_move(
			&ca, resolveStr(&ca, "A/B/D", true), CAHandle_Root, t->alloc, &t->err
		));

		Test_assert(t, "D found again",       resolveStr(&ca, "D/d.txt", false) != CAHandle_Invalid);
		Test_assert(t, "tree is consistent again", checkInvariants(&ca));

		Test_assert(t, "write", writeToMemory(t, &ca, &memType, NULL, &stream));
		Test_assert(t, "read", CAFile_read(stream, NULL, 0, NULL, t->alloc, &back, &t->err));
		Test_assert(t, "read archive is consistent", checkInvariants(&back));
		Test_assert(t, "D/d.txt found after read", resolveStr(&back, "D/d.txt", false) != CAHandle_Invalid);
		Test_assert(t, "A/B found after read", resolveStr(&back, "A/B", true) != CAHandle_Invalid);

	doneShift:
		RefPtr_dec(&stream);
		CAFile_free(&ca, t->alloc);
		CAFile_free(&back, t->alloc);
	}

	{                            //A folder can't move into itself or into what it contains

		CAFile ca = { 0 };

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for illegal move", false);
			goto doneIllegal;
		}

		CAHandle top = addFolder(t, &ca, CAHandle_Root, "top", false);

		addFolder(t, &ca, resolveStr(&ca, "top", true), "mid", false);
		addFolder(t, &ca, resolveStr(&ca, "top/mid", true), "leaf", false);
		top = resolveStr(&ca, "top", true);

		Test_assert(t, "into itself fails", !CAFile_move(&ca, top, top, t->alloc, NULL));
		Test_assert(t, "into child fails", !CAFile_move(&ca, top, resolveStr(&ca, "top/mid", true), t->alloc, NULL));
		Test_assert(t, "into grandchild fails", !CAFile_move(&ca, top, resolveStr(&ca, "top/mid/leaf", true), t->alloc, NULL));
		Test_assert(t, "tree untouched", checkInvariants(&ca));
		Test_assert(t, "leaf still found", resolveStr(&ca, "top/mid/leaf", true) != CAHandle_Invalid);

		//Moving a middle folder up is legal

		CAHandle leaf = resolveStr(&ca, "top/mid/leaf", true);

		Test_assert(t, "leaf to root", CAFile_move(&ca, leaf, CAHandle_Root, t->alloc, &t->err));

		CAHandle mid = resolveStr(&ca, "top/mid", true);

		leaf = resolveStr(&ca, "leaf", true);
		Test_assert(t, "mid to leaf", CAFile_move(&ca, mid, leaf, t->alloc, &t->err));
		Test_assert(t, "leaf/mid found", resolveStr(&ca, "leaf/mid", true) != CAHandle_Invalid);
		Test_assert(t, "tree is consistent", checkInvariants(&ca));

	doneIllegal:
		CAFile_free(&ca, t->alloc);
	}

	{                            //A move that pushes something below the folder past the path limit is refused

		CAFile ca = { 0 };

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for long path move", false);
			goto doneLong;
		}

		C8 longName[97], fileName[91];

		fillBytes((U8*) longName, 'd', 96);
		fillBytes((U8*) fileName, 'f', 90);
		longName[96] = fileName[90] = 0;

		addFolder(t, &ca, CAHandle_Root, longName, false);

		CAHandle x = addFolder(t, &ca, CAHandle_Root, "xxxxxxxxxx", false);

		addFile(t, &ca, resolveStr(&ca, "xxxxxxxxxx", true), fileName, 0, false);
		x = resolveStr(&ca, "xxxxxxxxxx", true);

		Test_assert(t, "child path too long fails", !CAFile_move(&ca, x, resolveStr(&ca, longName, true), t->alloc, NULL));
		Test_assert(t, "tree untouched", checkInvariants(&ca));
		Test_assert(t, "folder still at root", resolveStr(&ca, "xxxxxxxxxx", true) != CAHandle_Invalid);

	doneLong:
		CAFile_free(&ca, t->alloc);
	}

	{                            //Random edits (with folder moves) agree with a model of the tree after every op

		static const U64 seeds[] = { 2, 3, 5, 7, 11, 12 };

		for (U64 i = 0; i < sizeof(seeds) / sizeof(seeds[0]); ++i)
			if (!Model_run(t, seeds[i], 120))
				break;

		Test_assert(t, "model fuzz done", true);
	}

	freeFormats();
}

//A failed write to the stream has to fail the write, wherever it happens and padding included.
//The key that was generated for a failed write isn't handed to the caller.

static void testWriteFailures(Test *t, Bool encrypted) {

	const RefPtrType memType = MemoryStream_makeType(t->alloc);
	const RefPtrType encType = EncryptionStream_makeType(t->alloc);

	//Different name lengths so some variants end the names oiDL off a 16-byte boundary and need padding

	for (U64 variant = 0; variant < 16; ++variant) {

		CASettings settings = kCASettings;

		if (encrypted)
			settings.encryptionType = EXXEncryptionType_AES256GCM;

		CAFile ca = { 0 };
		StreamRef *stream = NULL;

		if (!CAFile_create(&settings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "failing write: create", false);
			return;
		}

		C8 name[32] = { 0 };

		fillBytes((U8*) name, 'a', 2 + variant);

		CAHandle folder = addFolder(t, &ca, CAHandle_Root, "docs", false);
		CAHandle file = addFile(t, &ca, folder, name, 0, false);

		addFile(t, &ca, CAHandle_Root, "b.bin", 0, false);

		Buffer data = Buffer_createNull();

		Buffer_createUninitializedBytes(5 + variant, t->alloc, &data, &t->err);
		fillBytes(data.ptrNonConst, 0x42, Buffer_length(data));
		CAFile_setData(&ca, file, t->alloc, &data, &t->err);

		//Count the writes of a succeeding write

		const U64 start = 16;
		U64 writes = 0, offset = start;

		if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &memType, &stream, &t->err)) {
			Test_assert(t, "failing write: stream", false);
			goto clean;
		}

		FailingStream_arm(stream, 0);

		const RefPtrType *streamType = encrypted ? &encType : NULL;

		if (!Test_assert(t, "failing write: write", CAFile_write(&ca, streamType, stream, &offset, t->alloc, &t->err)))
			goto clean;

		writes = FailingStream_writes();

		if (encrypted)
			Test_assert(t, "failing write: key generated on success", Buffer_neq(
				Buffer_createRefConst(ca.settings.encryptionKey, 32), Buffer_createRefConst(settings.encryptionKey, 32)
			));

		RefPtr_dec(&stream);

		U64 failures = 0;

		for (U64 n = 1; n <= writes; ++n) {

			Buffer_unsetAllBits(Buffer_createRef(ca.settings.encryptionKey, sizeof(ca.settings.encryptionKey)), NULL);
			offset = start;

			if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &memType, &stream, &t->err))
				goto clean;

			FailingStream_arm(stream, n);

			if (!CAFile_write(&ca, encrypted ? &encType : NULL, stream, &offset, t->alloc, NULL)) {

				++failures;
				Test_assert(t, "failing write: offset unchanged", offset == start);

				U32 zero[8] = { 0 };

				Buffer key = Buffer_createRefConst(ca.settings.encryptionKey, 32);

				if (encrypted && Buffer_neq(key, Buffer_createRefConst(zero, 32)))
					Test_assert(t, "failing write: key not published", false);
			}

			RefPtr_dec(&stream);
		}

		if (failures != writes) {
			Test_assert(t, format(3, "failing write: every failed write is reported (variant %"PRIu64")", variant), false);
		}

	clean:
		RefPtr_dec(&stream);
		CAFile_free(&ca, t->alloc);
	}

	Test_assert(t, "failing write: sweep done", true);
}

void Test_CAWriteFailures(Test *t) {

	Test_setModule(t, "CAFile_write failing stream");

	testWriteFailures(t, false);
	testWriteFailures(t, true);
	freeFormats();
}

void Test_CAReadLargeNames(Test *t) {

	Test_setModule(t, "CAFile_read names beyond the DLFile cache");

	const RefPtrType memType = MemoryStream_makeType(t->alloc);
	const U64 folders = 128, filesPerFolder = 90;
	CAFile ca = { 0 }, back = { 0 };
	StreamRef *stream = NULL;
	C8 fileName[97];

	fillBytes((U8*) fileName, 'n', sizeof(fileName) - 1);
	fileName[96] = 0;

	if (!Test_assert(t, "large names: create", CAFile_create(
		&kCASettings, folders * filesPerFolder, folders, t->alloc, &ca, &t->err
	)))
		goto clean;

	for (U64 i = 0; i < folders; ++i)
		if (addFolder(t, &ca, CAHandle_Root, format(3, "dir%"PRIu64, i), false) == CAHandle_Invalid)
			goto clean;

	for (U64 i = 0; i < folders; ++i)
		for (U64 j = 0; j < filesPerFolder; ++j) {

			fileName[0] = (C8) ('0' + j / 10);
			fileName[1] = (C8) ('0' + j % 10);

			if (addFile(t, &ca, CAHandle_makeFolder(i + 1), fileName, 0, false) == CAHandle_Invalid)
				goto clean;
		}

	Test_assert(t, "large names: more than 1 MiB of names", folders * filesPerFolder * 96 > MIBI);

	if (!Test_assert(t, "large names: write", writeToMemory(t, &ca, &memType, NULL, &stream)))
		goto clean;

	if (!Test_assert(t, "large names: read", CAFile_read(stream, NULL, 0, NULL, t->alloc, &back, &t->err)))
		goto clean;

	RefPtr_dec(&stream);

	Bool sameNames = DLFile_entryCount(&back.names) == DLFile_entryCount(&ca.names);

	for (U64 i = 0; i < DLFile_entryCount(&ca.names) && sameNames; ++i)
		sameNames = DLFile_isFullyLoaded(&back.names, i) && CharString_equalsStringSensitive(
			&ca.names.entryStrings.ptr[i], &back.names.entryStrings.ptr[i]
		);

	Test_assert(t, "large names: every name survives stream release", sameNames);
	Test_assert(t, "large names: hierarchy", checkInvariants(&back));

clean:
	RefPtr_dec(&stream);
	CAFile_free(&back, t->alloc);
	CAFile_free(&ca, t->alloc);
	freeFormats();
}

void Test_CARenameSubtree(Test *t) {

	Test_setModule(t, "CAFile_rename descendant paths");

	const RefPtrType memType = MemoryStream_makeType(t->alloc);

	for (U8 folderChild = 0; folderChild < 2; ++folderChild) {

		CAFile ca = { 0 }, back = { 0 };
		StreamRef *stream = NULL;
		C8 descendantName[97], parentName[97];

		fillBytes((U8*) descendantName, 'd', 96);
		fillBytes((U8*) parentName, 'p', 96);
		descendantName[96] = parentName[96] = 0;

		if (!Test_assert(t, "rename subtree: create", CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)))
			goto clean;

		CAHandle parent = addFolder(t, &ca, CAHandle_Root, "a", false);

		if (folderChild)
			addFolder(t, &ca, parent, descendantName, false);
		else addFile(t, &ca, parent, descendantName, 0, false);

		CharString name = CharString_createRefCStrConst(parentName);

		Test_assert(t, "rename subtree: 193-char descendant rejected", !CAFile_rename(
			&ca, parent, t->alloc, &name, NULL
		));

		Test_assert(t, "rename subtree: parent unchanged", resolveStr(&ca, "a", true) == parent);
		Test_assert(t, "rename subtree: input unchanged", CharString_length(name) == 96);

		parentName[95] = 0;
		name = CharString_createRefCStrConst(parentName);
		Test_assert(t, "rename subtree: 192-char descendant accepted", CAFile_rename(
			&ca, parent, t->alloc, &name, &t->err
		));

		if (!Test_assert(t, "rename subtree: write", writeToMemory(t, &ca, &memType, NULL, &stream)))
			goto clean;

		Test_assert(t, "rename subtree: read", CAFile_read(stream, NULL, 0, NULL, t->alloc, &back, &t->err));

	clean:
		RefPtr_dec(&stream);
		CAFile_free(&back, t->alloc);
		CAFile_free(&ca, t->alloc);
	}
}

void Test_CAEmptyStreamCompare(Test *t) {

	Test_setModule(t, "CAFile_dataEqual empty stream entries");

	const RefPtrType memType = MemoryStream_makeType(t->alloc);
	CAFile a = { 0 }, b = { 0 };
	StreamRef *stream = NULL;
	ECompareResult result = ECompareResult_Gt;

	if (
		!Test_assert(t, "empty compare: create", CAFile_create(&kCASettings, 0, 0, t->alloc, &a, &t->err) &&
			CAFile_create(&kCASettings, 0, 0, t->alloc, &b, &t->err)
		)
	)
		goto clean;

	CAHandle aFile = addFile(t, &a, CAHandle_Root, "empty", 0, false);
	CAHandle bFile = addFile(t, &b, CAHandle_Root, "empty", 0, false);

	if (!Test_assert(t, "empty compare: first stream", MemoryStream_createFromBufferRegion(
		Buffer_createRefConst("abc", 3), 0, 0, EMemoryStreamFlags_None, &memType, &stream, &t->err
	)))
		goto clean;

	if (!Test_assert(t, "empty compare: first entry", CAFile_setDataStream(
		&a, aFile, t->alloc, &stream, 1, 0, &t->err
	)))
		goto clean;

	Test_assert(
		t, "empty compare: lazy and loaded equal", CAFile_dataEqual(
			&a, aFile, &b, bFile, t->alloc, &result, &t->err
		) && result == ECompareResult_Eq
	);

	if (!Test_assert(t, "empty compare: second stream", MemoryStream_createFromBufferRegion(
		Buffer_createRefConst("xyz", 3), 0, 0, EMemoryStreamFlags_None, &memType, &stream, &t->err
	)))
		goto clean;

	if (!Test_assert(t, "empty compare: second entry", CAFile_setDataStream(
		&b, bFile, t->alloc, &stream, 1, 0, &t->err
	)))
		goto clean;

	Test_assert(
		t, "empty compare: two lazy entries equal", CAFile_dataEqual(
			&a, aFile, &b, bFile, t->alloc, &result, &t->err
		) && result == ECompareResult_Eq
	);

clean:
	RefPtr_dec(&stream);
	CAFile_free(&a, t->alloc);
	CAFile_free(&b, t->alloc);
}

//An encrypted archive with a header bigger than the read cursor has to read from a stream that can't be written to.

void Test_CAReadBigHeader(Test *t) {

	Test_setModule(t, "CAFile_read big header");

	const RefPtrType memType = MemoryStream_makeType(t->alloc);
	const RefPtrType encType = EncryptionStream_makeType(t->alloc);

	CASettings settings = kCASettings;

	settings.encryptionType = EXXEncryptionType_AES256GCM;

	for (U32 i = 0; i < 8; ++i)
		settings.encryptionKey[i] = 0x01010101 * (i + 1);

	CAFile ca = { 0 }, back = { 0 };
	StreamRef *stream = NULL, *readOnly = NULL;

	if (!Test_assert(t, "big header: create", CAFile_create(&settings, 0, 0, t->alloc, &ca, &t->err)))
		goto clean;

	//2 byte parents: more than 32 KiB of fixed header
	const U64 folders = 300, filesPer = 60;

	for (U64 f = 0; f < folders; ++f) {

		CAHandle folder = addFolder(t, &ca, CAHandle_Root, format(3, "d%"PRIu64, f), false);

		for (U64 i = 0; i < filesPer; ++i) {
			addFile(t, &ca, folder, format(3, "f%"PRIu64, i), 0, false);
		}
	}

	if (!Test_assert(t, "big header: write", writeToMemory(t, &ca, &memType, &encType, &stream)))
		goto clean;

	const MemoryStream *written = RefPtr_data(stream, MemoryStream);

	if (!Test_assert(t, "big header: read only stream", MemoryStream_createFromBufferRegion(
		Buffer_createRefConst(written->data.ptr, written->parent.size), 0, 0,
		EMemoryStreamFlags_None, &memType, &readOnly, &t->err
	)))
		goto clean;

	Test_assert(t, "big header: read", CAFile_read(readOnly, &encType, 0, settings.encryptionKey, t->alloc, &back, &t->err));
	Test_assert(t, "big header: file count", CAFile_fileCount(&back, CAHandle_Root, true) == folders * filesPer);
	Test_assert(t, "big header: folder count", CAFile_dirCount(&back, CAHandle_Root, false) == folders);

clean:
	RefPtr_dec(&readOnly);
	RefPtr_dec(&stream);
	CAFile_free(&ca, t->alloc);
	CAFile_free(&back, t->alloc);
	freeFormats();
}

//Writing sizes its cursor for the fixed header, not the content that follows it.

void Test_CAWriteCursorSize(Test *t) {

	Test_setModule(t, "CAFile_write cursor size");

	const RefPtrType memType = MemoryStream_makeType(t->alloc);
	const U64 contentSize = 3 * MIBI;

	CAFile ca = { 0 };
	StreamRef *stream = NULL;
	Buffer data = Buffer_createNull();

	if (!Test_assert(t, "cursor size: create", CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)))
		goto clean;

	CAHandle file = addFile(t, &ca, CAHandle_Root, "big.bin", 0, false);

	if (!Test_assert(t, "cursor size: data", Buffer_createUninitializedBytes(contentSize, t->alloc, &data, &t->err)))
		goto clean;

	fillBytes(data.ptrNonConst, 0x7B, Buffer_length(data));

	if (!Test_assert(t, "cursor size: set data", CAFile_setData(&ca, file, t->alloc, &data, &t->err)))
		goto clean;

	//The stream already holds enough room, so only the writer's own allocations can be this big

	if (!Test_assert(t, "cursor size: stream", MemoryStream_create(
		contentSize + 4 * KIBI, EMemoryStreamFlags_WriteResize, &memType, &stream, &t->err
	)))
		goto clean;

	CountingAllocator counting = (CountingAllocator) { .inner = t->alloc, .threshold = contentSize };
	Allocator alloc = (Allocator) { .ptr = &counting, .alloc = CountingAllocator_alloc, .free = CountingAllocator_free };

	U64 offset = 0;

	Test_assert(t, "cursor size: write", CAFile_write(&ca, NULL, stream, &offset, &alloc, &t->err));
	Test_assert(t, "cursor size: content is not buffered whole", counting.bigAllocs == 0);

clean:
	RefPtr_dec(&stream);
	CAFile_free(&ca, t->alloc);
}

//Counts are only trusted as far as the stream has bytes for them.

void Test_CAReadHugeCounts(Test *t) {

	Test_setModule(t, "CAFile_read huge counts");

	const RefPtrType memType = MemoryStream_makeType(t->alloc);

	U8 bytes[16] = { 0 };
	CAHeader header = (CAHeader) {
		.magicNumber = 0x4143696F, .version = 0, .type = 0, .flags = (U16) ECAFlags_FilesCountLong
	};

	Buffer_memcpy(Buffer_createRef(bytes, sizeof(header)), Buffer_createRefConst(&header, sizeof(header)));

	const U32 fileCount = 1 << 24;

	Buffer_memcpy(Buffer_createRef(bytes + sizeof(header), 4), Buffer_createRefConst(&fileCount, 4));

	StreamRef *stream = NULL;
	CAFile ca = { 0 };

	if (!Test_assert(t, "huge counts: stream", MemoryStream_createFromBufferRegion(
		Buffer_createRefConst(bytes, sizeof(bytes)), 0, 0, EMemoryStreamFlags_None, &memType, &stream, &t->err
	)))
		return;

	CountingAllocator counting = (CountingAllocator) { .inner = t->alloc, .threshold = MIBI };
	Allocator alloc = (Allocator) { .ptr = &counting, .alloc = CountingAllocator_alloc, .free = CountingAllocator_free };

	Test_assert(t, "huge counts: read fails", !CAFile_read(stream, NULL, 0, NULL, &alloc, &ca, NULL));
	Test_assert(t, "huge counts: nothing reserved from the count", counting.bigAllocs == 0);

	CAFile_free(&ca, t->alloc);
	RefPtr_dec(&stream);
}

//An encrypted archive's counts are authenticated before anything is sized from them: forged as high as the stream has
// bytes for, they fail the tag check without the reader reserving for them. A key for a plaintext archive is refused.

void Test_CAReadForgedEncryptedCounts(Test *t) {

	Test_setModule(t, "CAFile_read authenticates counts first");

	const RefPtrType memType = MemoryStream_makeType(t->alloc);
	const RefPtrType encType = EncryptionStream_makeType(t->alloc);

	CASettings settings = kCASettings;
	settings.encryptionType = EXXEncryptionType_AES256GCM;

	for (U32 i = 0; i < 8; ++i)
		settings.encryptionKey[i] = 0x02020202 * (i + 1);

	CAFile ca = { 0 }, plainCa = { 0 }, back = { 0 };
	StreamRef *stream = NULL, *forged = NULL, *plain = NULL;
	Buffer bytes = Buffer_createNull();

	if (!Test_assert(t, "forged counts: create", CAFile_create(&settings, 0, 0, t->alloc, &ca, &t->err)))
		goto clean;

	addFile(t, &ca, CAHandle_Root, "a", 0, false);
	addFile(t, &ca, CAHandle_Root, "b", 0, false);

	if (!Test_assert(t, "forged counts: write", writeToMemory(t, &ca, &memType, &encType, &stream)))
		goto clean;

	const MemoryStream *written = RefPtr_data(stream, MemoryStream);
	const U64 size = written->parent.size, padding = 256 * KIBI;

	if (!Test_assert(t, "forged counts: copy", Buffer_createEmptyBytes(size + padding, t->alloc, &bytes, &t->err)))
		goto clean;

	Buffer_memcpy(Buffer_createRef(bytes.ptrNonConst, size), Buffer_createRefConst(written->data.ptr, size));

	//Two files and no folders: a short file count right after the fixed header, and one byte per file record, so the
	// padding has room for every forged record and the count passes the bytes-left check

	CAHeader header;
	Buffer_memcpy(Buffer_createRef(&header, sizeof(header)), Buffer_createRefConst(bytes.ptr, sizeof(header)));

	if (!Test_assert(t, "forged counts: short counts and no extension", !(
		header.flags & (ECAFlags_FilesCountLong | ECAFlags_HasExtendedData | ECAFlags_FilesHaveDate)
	)))
		goto clean;

	const U16 fileCount = 65000;

	Buffer_memcpy(
		Buffer_createRef(bytes.ptrNonConst + sizeof(header), sizeof(fileCount)),
		Buffer_createRefConst(&fileCount, sizeof(fileCount))
	);

	if (!Test_assert(t, "forged counts: stream", MemoryStream_createFromBufferRegion(
		Buffer_createRefConst(bytes.ptr, size + padding), 0, 0, EMemoryStreamFlags_None, &memType, &forged, &t->err
	)))
		goto clean;

	CountingAllocator counting = (CountingAllocator) { .inner = t->alloc, .threshold = 256 * KIBI };
	Allocator alloc = (Allocator) { .ptr = &counting, .alloc = CountingAllocator_alloc, .free = CountingAllocator_free };

	Test_assert(t, "forged counts: read fails", !CAFile_read(forged, &encType, 0, settings.encryptionKey, &alloc, &back, NULL));
	Test_assert(t, "forged counts: nothing reserved before the tag", counting.bigAllocs == 0);
	CAFile_free(&back, &alloc);

	//A key for an archive that isn't encrypted

	if (
		!Test_assert(t, "plaintext: create", CAFile_create(&kCASettings, 0, 0, t->alloc, &plainCa, &t->err)) ||
		addFile(t, &plainCa, CAHandle_Root, "a", 0, false) == CAHandle_Invalid ||
		!Test_assert(t, "plaintext: write", writeToMemory(t, &plainCa, &memType, NULL, &plain))
	)
		goto clean;

	Test_assert(t, "plaintext with a key: refused", !CAFile_read(
		plain, &encType, 0, settings.encryptionKey, t->alloc, &back, NULL
	));

clean:
	RefPtr_dec(&plain);
	RefPtr_dec(&forged);
	RefPtr_dec(&stream);
	Buffer_free(&bytes, t->alloc);
	CAFile_free(&back, t->alloc);
	CAFile_free(&plainCa, t->alloc);
	CAFile_free(&ca, t->alloc);
}

//A name or data that is a ref is copied by the archive, the caller's memory can go away or change.

void Test_CARefCopies(Test *t) {

	Test_setModule(t, "CAFile ref copies");

	CAFile ca = { 0 };

	if (!Test_assert(t, "ref copies: create", CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)))
		return;

	CAHandle file = addFile(t, &ca, CAHandle_Root, "orig", 0, false);

	C8 name[] = "renamed.txt";
	CharString nameRef = CharString_createRefSizedConst(name, 11, false);

	Test_assert(t, "ref copies: rename", CAFile_rename(&ca, file, t->alloc, &nameRef, &t->err));
	Test_assert(t, "ref copies: caller keeps its ref", nameRef.ptr == name);

	U8 data[] = { 'D', 'A', 'T', 'A' };
	Buffer dataRef = Buffer_createRefConst(data, sizeof(data));

	Test_assert(t, "ref copies: setData", CAFile_setData(&ca, file, t->alloc, &dataRef, &t->err));
	Test_assert(t, "ref copies: caller keeps its data ref", dataRef.ptr == data);

	fillBytes((U8*) name, 'Z', 11);
	fillBytes(data, 'Q', sizeof(data));

	CharString got = CAFile_getName(&ca, file);

	Test_assert(t, "ref copies: name is a copy", CharString_equalsCStringSensitive(&got, "renamed.txt"));

	Bool valid = false;
	Buffer gotData = CAFile_getDataConst(&ca, file, &valid);

	Test_assert(t, "ref copies: data is a copy", valid && Buffer_eq(gotData, Buffer_createRefConst("DATA", 4)));

	CAFile_free(&ca, t->alloc);
}

//A folder holds at most 65535 files, the 65536th would wrap the count.

void Test_CAFolderFileLimit(Test *t) {

	Test_setModule(t, "CAFile per folder file limit");

	const U64 count = U16_MAX;

	CAFile ca = { 0 };
	Buffer names = Buffer_createNull();

	if (!Test_assert(t, "file limit: create", CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)))
		return;

	//Filled directly, since adding this many one by one searches the folder for duplicates every time

	if (!Test_assert(t, "file limit: names", Buffer_createUninitializedBytes(count * 8, t->alloc, &names, &t->err)))
		goto clean;

	if (
		!Test_assert(t, "file limit: reserve", ListCAFileInfo_reserve(&ca.files, count, t->alloc, &t->err)) ||
		!Test_assert(t, "file limit: reserve names", DLFile_reserve(&ca.names, count + 1, t->alloc, &t->err)) ||
		!Test_assert(t, "file limit: reserve content", DLFile_reserve(&ca.content, count, t->alloc, &t->err))
	)
		goto clean;

	for (U64 i = 0; i < count; ++i) {

		C8 *dst = (C8*) names.ptrNonConst + i * 8;

		dst[0] = 'f';

		for (U64 digit = 0, rest = i; digit < 5; ++digit, rest /= 10)
			dst[5 - digit] = (C8) ('0' + rest % 10);

		dst[6] = 0;

		CharString name = CharString_createRefSizedConst(dst, 6, true);
		Buffer empty = Buffer_createNull();

		if (
			!ListCAFileInfo_pushBack(&ca.files, CAFileInfo_create(0, 0), t->alloc, &t->err) ||
			!DLFile_addEntry(&ca.content, &empty, t->alloc, &t->err) ||
			!DLFile_addEntryString(&ca.names, &name, t->alloc, &t->err)
		) {
			Test_assert(t, "file limit: fill", false);
			goto clean;
		}
	}

	ca.folders.ptrNonConst[0].fileCount = U16_MAX;
	ca.folders.ptrNonConst[0].fileOffset = 0;

	CAHandle folder = addFolder(t, &ca, CAHandle_Root, "sub", false);
	CAHandle inside = addFile(t, &ca, folder, "in.txt", 0, false);

	Test_assert(t, "file limit: file in sub folder", inside != CAHandle_Invalid);

	CAHandle extra = addFile(t, &ca, CAHandle_Root, "extra", 0, true);

	Test_assert(t, "file limit: the 65536th file is refused", extra == CAHandle_Invalid);
	Test_assert(t, "file limit: count intact", ca.folders.ptr[0].fileCount == U16_MAX && ca.files.length == count + 1);

	Test_assert(t, "file limit: moving into the full folder is refused", !CAFile_move(
		&ca, inside, CAHandle_Root, t->alloc, NULL
	));

	Test_assert(t, "file limit: still in the sub folder", resolveStr(&ca, "sub/in.txt", false) != CAHandle_Invalid);

clean:
	CAFile_free(&ca, t->alloc);
	Buffer_free(&names, t->alloc);
}

//An MS-DOS date that isn't a date fails the read, and combining archives that disagree on dates is refused.

void Test_CADateFlags(Test *t) {

	Test_setModule(t, "CAFile dates");

	const RefPtrType memType = MemoryStream_makeType(t->alloc);

	{

		CAFile ca = { 0 }, back = { 0 };
		StreamRef *stream = NULL;

		if (!CAFile_create(&kCASettingsDate, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "date: create", false);
			goto doneDate;
		}

		addFile(t, &ca, CAHandle_Root, "dated.txt", 1700000000 * SECOND, false);

		if (!Test_assert(t, "date: write", writeToMemory(t, &ca, &memType, NULL, &stream)))
			goto doneDate;

		Test_assert(t, "date: valid date reads", CAFile_read(stream, NULL, 0, NULL, t->alloc, &back, &t->err));
		CAFile_free(&back, t->alloc);
		back = (CAFile) { 0 };

		//header, fileCount (U16), dirCount (U8), parent (U8), then date (day in the low 5 bits)

		U8 *bytes = ((MemoryStream*) RefPtr_data(stream, OxStream))->data.ptrNonConst;
		U8 *date = bytes + sizeof(CAHeader) + 2 + 1 + 1;

		//Month 0 and day 0 aren't a date
		date[0] = 0;
		date[1] &= 0xFE;

		Test_assert(t, "date: invalid date fails the read", !CAFile_read(stream, NULL, 0, NULL, t->alloc, &back, NULL));
		Test_assert(t, "date: nothing left allocated", !back.folders.ptr);
		back = (CAFile) { 0 };

	doneDate:
		RefPtr_dec(&stream);
		CAFile_free(&ca, t->alloc);
		CAFile_free(&back, t->alloc);
	}

	{

		CAFile a = { 0 }, b = { 0 }, combined = { 0 };

		if (
			!CAFile_create(&kCASettings, 0, 0, t->alloc, &a, &t->err) ||
			!CAFile_create(&kCASettingsDate, 0, 0, t->alloc, &b, &t->err)
		) {
			Test_assert(t, "combine dates: create", false);
			goto doneCombine;
		}

		addFile(t, &a, CAHandle_Root, "a.txt", 0, false);
		addFile(t, &b, CAHandle_Root, "b.txt", 1700000000 * SECOND, false);

		Test_assert(t, "combine dates: mismatching date flags are refused", !CAFile_combine(
			&a, &b, EArchiveCombineMode_AcceptA, EArchiveCombineFlags_None, t->alloc, &combined, NULL
		));

		Test_assert(t, "combine dates: nothing left allocated", !combined.folders.ptr);

		Test_assert(t, "combine dates: mismatch the other way is refused", !CAFile_combine(
			&b, &a, EArchiveCombineMode_AcceptA, EArchiveCombineFlags_None, t->alloc, &combined, NULL
		));

		CAFile_free(&a, t->alloc);
		CAFile_create(&kCASettingsDate, 0, 0, t->alloc, &a, &t->err);
		addFile(t, &a, CAHandle_Root, "a.txt", 1700000000 * SECOND, false);

		Test_assert(t, "combine dates: same date flags combine", CAFile_combine(
			&a, &b, EArchiveCombineMode_AcceptA, EArchiveCombineFlags_None, t->alloc, &combined, &t->err
		));

		StreamRef *stream = NULL;

		Test_assert(t, "combine dates: combined archive writes", writeToMemory(t, &combined, &memType, NULL, &stream));
		RefPtr_dec(&stream);

	doneCombine:
		CAFile_free(&a, t->alloc);
		CAFile_free(&b, t->alloc);
		CAFile_free(&combined, t->alloc);
	}
}

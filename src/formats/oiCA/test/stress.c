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

#include "shared.h"
#include "formats/oiCA/ca_edit.h"
#include "formats/oiCA/ca_lookup.h"

extern const CASettings kCASettings;

CAHandle addFile(Test *t, CAFile *ca, CAHandle parent, const C8 *name, Ns time, Bool failIsSuccess);
CAHandle addFolder(Test *t, CAFile *ca, CAHandle parent, const C8 *name, Bool failIsSuccess);

static inline CAHandle CAFile_resolveCStr(CAFile *ca, const C8 *name) {
	return CAFile_resolve(ca, CharString_createRefCStrConst(name));
}

void assertResolve(Test *t, const CAFile *ca, const C8 *path, CAHandle expected, const C8 *label) {
	CharString p = CharString_createRefCStrConst(path);
	CAHandle got = CAFile_resolve(ca, p);
	Test_assert(t, label, got == expected);
}

static Bool setDataByPath(Test *t, CAFile *ca, const C8 *path, const U8 *bytes, U64 len) {

	CAHandle h = CAFile_resolve(ca, CharString_createRefCStrConst(path));
	if (h == CAHandle_Invalid) {
		Test_assert(t, path, false);
		return false;
	}

	Buffer buf = Buffer_createNull();
	if (!Buffer_createCopy(Buffer_createRefConst(bytes, len), t->alloc, &buf, &t->err)) {
		Test_assert(t, "Buffer_createCopy setDataByPath", false);
		return false;
	}

	if (!CAFile_setData(ca, h, t->alloc, &buf, &t->err)) {
		Buffer_free(&buf, t->alloc);
		Test_assert(t, "CAFile_setData setDataByPath", false);
		return false;
	}

	return true;
}

static void assertDataByPath(
	Test *t,
	const CAFile *ca,
	const C8 *path,
	const U8 *expected,
	U64 expectedLen,
	const C8 *label
) {
	CAHandle h = CAFile_resolve(ca, CharString_createRefCStrConst(path));
	Test_assert(t, label, h != CAHandle_Invalid);
	if (h == CAHandle_Invalid)
		return;

	Bool isValid = false;
	Buffer got = CAFile_getDataConst(ca, h, &isValid);
	Test_assert(t, label, isValid && Buffer_length(got) == expectedLen);

	if (isValid)
		Test_assert(t, label, Buffer_eq(got, Buffer_createRefConst(expected, expectedLen)));
}

//engine/
//  core/
//    types.h   (data: {0x01})
//    math.h    (data: {0x02, 0x03})
//  renderer/
//    gl.c      (data: {0x04})
//assets/
//  textures/
//    tile.png  (data: {0xFF})
//build.sh      (data: {0x42})

void Test_CAMixedTree(Test *t) {

	Test_setModule(t, "CAFile_mixedTree");

	{
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, 4, 4, t->alloc, &ca, &t->err)) {
			Test_assert(t, "Create ca for mixed tree", false);
			goto doneMixed;
		}

		//Build initial tree with data

		CAHandle hEngine   = addFolder(t, &ca, root,     "engine",   false);
		CAHandle hAssets   = addFolder(t, &ca, root,     "assets",   false);
		CAHandle hCore     = addFolder(t, &ca, hEngine,  "core",     false);
		CAHandle hRenderer = addFolder(t, &ca, hEngine,  "renderer", false);
		CAHandle hTextures = addFolder(t, &ca, hAssets,  "textures", false);

		CAHandle hTypes = addFile(t, &ca, hCore,     "types.h",  0, false);
		CAHandle hMath = addFile(t, &ca, hCore,     "math.h",   0, false);
		addFile(t, &ca, hRenderer, "gl.c",     0, false);
		addFile(t, &ca, hTextures, "tile.png", 0, false);
		addFile(t, &ca, root,      "build.sh", 0, false);

		Test_assert(t, "p1: folders 6",  ca.folders.length == 6);
		Test_assert(t, "p1: files 5",    ca.files.length   == 5);

		//Attach data payload to every file

		U8 dTypes[]  = { 0x01 };
		U8 dMath[]   = { 0x02, 0x03 };
		U8 dGl[]     = { 0x04 };
		U8 dTile[]   = { 0xFF };
		U8 dBuild[]  = { 0x42 };

		Test_assert(t, "p1: set types.h data",  setDataByPath(t, &ca, "engine/core/types.h",           dTypes,  1));
		Test_assert(t, "p1: set math.h data",   setDataByPath(t, &ca, "engine/core/math.h",            dMath,   2));
		Test_assert(t, "p1: set gl.c data",     setDataByPath(t, &ca, "engine/renderer/gl.c",          dGl,     1));
		Test_assert(t, "p1: set tile.png data", setDataByPath(t, &ca, "assets/textures/tile.png",      dTile,   1));
		Test_assert(t, "p1: set build.sh data", setDataByPath(t, &ca, "build.sh",                      dBuild,  1));

		//Verify data reads back correctly before any mutations

		assertDataByPath(t, &ca, "engine/core/types.h",      dTypes,  1, "p1: types.h data");
		assertDataByPath(t, &ca, "engine/core/math.h",       dMath,   2, "p1: math.h data");
		assertDataByPath(t, &ca, "engine/renderer/gl.c",     dGl,     1, "p1: gl.c data");
		assertDataByPath(t, &ca, "assets/textures/tile.png", dTile,   1, "p1: tile.png data");
		assertDataByPath(t, &ca, "build.sh",                 dBuild,  1, "p1: build.sh data");

		//We add a file and immediately remove it, this should leave other file handles in tact.

		{
			CAHandle hTemp = addFile(t, &ca, hCore, "temp.c", 0, false);
			Test_assert(t, "p2: temp added",   ca.files.length == 6);

			Test_assert(t, "p2: remove temp ok", CAFile_removeFile(&ca, hTemp, t->alloc, &t->err));
			Test_assert(t, "p2: files back 5",   ca.files.length == 5);
		}

		//Re-resolve after the remove, all file handles from phase 1 are stale

		assertDataByPath(t, &ca, "engine/core/types.h",      dTypes,  1, "p2: types.h data intact");
		assertDataByPath(t, &ca, "engine/core/math.h",       dMath,   2, "p2: math.h data intact");

		Test_assert(t, "p2: types.h still resolves", CAFile_resolveCStr(&ca, "engine/core/types.h") == hTypes);
		Test_assert(t, "p2: math.h still resolves", CAFile_resolveCStr(&ca, "engine/core/math.h") == hMath);
		Test_assert(t, "p2: renderer still resolves", CAFile_resolveCStr(&ca, "engine/renderer") == hRenderer);
		Test_assert(t, "p2: temp.c gone", CAFile_resolveCStr(&ca, "engine/core/temp.c") == CAHandle_Invalid);

		//move math.h from core to renderer

		Test_assert(t, "p3: move math.h ok", CAFile_move(&ca, hMath, hRenderer, t->alloc, &t->err));

		//After move, re-resolve all handles for count checks

		{
			CAHandle hCoreNow     = CAFile_resolveCStr(&ca, "engine/core");
			CAHandle hRendererNow = CAFile_resolveCStr(&ca, "engine/renderer");
			Test_assert(t, "p3: old path gone", CAFile_resolveCStr(&ca, "engine/core/math.h") == CAHandle_Invalid);
			Test_assert(t, "p3: new path found", CAFile_resolveCStr(&ca, "engine/renderer/math.h") != CAHandle_Invalid);
			Test_assert(t, "p3: core fileCount 1",     CAFile_fileCount(&ca, hCoreNow,     false) == 1);
			Test_assert(t, "p3: renderer fileCount 2", CAFile_fileCount(&ca, hRendererNow, false) == 2);
		}

		//Data must have survived the move unchanged

		assertDataByPath(t, &ca, "engine/renderer/math.h", dMath, 2, "p3: math.h data after move");

		//remove entire textures subtree and re-add
		//All handles from previous phases are stale; re-resolve everything.

		{
			CAHandle hTexNow = CAFile_resolve(&ca, CharString_createRefCStrConst("assets/textures"));
			Test_assert(t, "p4: textures resolve ok", hTexNow != CAHandle_Invalid);
			Test_assert(t, "p4: remove textures ok", CAFile_removeFolder(&ca, hTexNow, t->alloc, &t->err));
		}

		Test_assert(t, "p4: tile.png gone", CAFile_resolveCStr(&ca, "assets/textures/tile.png") == CAHandle_Invalid);
		Test_assert(t, "p4: textures gone", CAFile_resolveCStr(&ca, "assets/textures") == CAHandle_Invalid);

		//Re-add textures under assets; re-resolve assets first because folder
		//indices shifted when textures was removed.

		{
			CAHandle hAssetsNow = CAFile_resolve(&ca, CharString_createRefCStrConst("assets"));
			Test_assert(t, "p4: assets resolve ok", hAssetsNow != CAHandle_Invalid);

			CAHandle hTexNew  = addFolder(t, &ca, hAssetsNow, "textures", false);
			Test_assert(t, "p4: textures re-added", hTexNew != CAHandle_Invalid);

			CAHandle hTileNew = addFile(t, &ca, hTexNew, "tile.png", 0, false);
			Test_assert(t, "p4: tile.png re-added", hTileNew != CAHandle_Invalid);

			Test_assert(t, "p4: tile.png back", CAFile_resolveCStr(&ca, "assets/textures/tile.png") == hTileNew);
		}

		//Re-set tile.png data after re-creation

		Test_assert(t, "p4: re-set tile.png data", setDataByPath(t, &ca, "assets/textures/tile.png", dTile, 1));
		assertDataByPath(t, &ca, "assets/textures/tile.png", dTile, 1, "p4: tile.png data ok");

		//Rename build.sh -> build2.sh

		{
			CAHandle hBuildNow = CAFile_resolve(&ca, CharString_createRefCStrConst("build.sh"));
			Test_assert(t, "p5: build.sh resolves", hBuildNow != CAHandle_Invalid);

			CharString newBuild = CharString_createNull();
			CharString_createCopy(CharString_createRefCStrConst("build2.sh"), t->alloc, &newBuild, NULL);
			Test_assert(t, "p5: rename ok", CAFile_rename(&ca, hBuildNow, t->alloc, &newBuild, &t->err));
		}

		Test_assert(t, "p5: build.sh gone", CAFile_resolveCStr(&ca, "build.sh") == CAHandle_Invalid);
		Test_assert(t, "p5: build2.sh found", CAFile_resolveCStr(&ca, "build2.sh") != CAHandle_Invalid);

		//Data survives rename

		assertDataByPath(t, &ca, "build2.sh", dBuild, 1, "p5: build2.sh data after rename");

		//final counts and full-tree data verification

		Test_assert(t, "p6: root recursive files 5", CAFile_fileCount(&ca, root, true) == 5);
		Test_assert(t, "p6: root recursive dirs 5",  CAFile_dirCount(&ca, root, true)  == 5);

		assertDataByPath(t, &ca, "engine/core/types.h",      dTypes, 1, "p6: types.h data");
		assertDataByPath(t, &ca, "engine/renderer/math.h",   dMath,  2, "p6: math.h data");
		assertDataByPath(t, &ca, "engine/renderer/gl.c",     dGl,    1, "p6: gl.c data");
		assertDataByPath(t, &ca, "assets/textures/tile.png", dTile,  1, "p6: tile.png data");
		assertDataByPath(t, &ca, "build2.sh",                dBuild, 1, "p6: build2.sh data");

	doneMixed:
		CAFile_free(&ca, t->alloc);
	}
}

void Test_CAStress(Test *t) {

	#define STRESS_NUM_DIRS       8
	#define STRESS_FILES_PER_DIR  6
	#define STRESS_TOTAL_FILES    (STRESS_NUM_DIRS * STRESS_FILES_PER_DIR)

	Test_setModule(t, "CAFile_stress");

	static const C8 *dirNames[STRESS_NUM_DIRS] = {
		"alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta", "theta"
	};

	static const C8 *fileExts[STRESS_FILES_PER_DIR] = {
		"a.bin", "b.bin", "c.bin", "d.bin", "e.bin", "f.bin"
	};

	static const C8 *fileExtsRen[STRESS_FILES_PER_DIR] = {
		"a0.bin", "b0.bin", "c0.bin", "d0.bin", "e0.bin", "f0.bin"
	};

	{
		CAFile ca   = { 0 };
		CAHandle root = CAHandle_Root;

		if (!CAFile_create(&kCASettings, STRESS_TOTAL_FILES, STRESS_NUM_DIRS, t->alloc, &ca, &t->err)) {
			Test_assert(t, "stress: create ok", false);
			goto doneStress;
		}

		//build the tree with data + timestamps

		for (U64 d = 0; d < STRESS_NUM_DIRS; ++d) {

			CAHandle hDir = addFolder(t, &ca, root, dirNames[d], false);
			Test_assert(t, "stress: addFolder ok", hDir != CAHandle_Invalid);

			for (U64 f = 0; f < STRESS_FILES_PER_DIR; ++f) {

				//Unique timestamp: encode dir and file index

				Ns ts = (Ns)((d * 100 + f + 1)) * MS;

				CAHandle hFile = addFile(t, &ca, hDir, fileExts[f], ts, false);
				Test_assert(t, "stress: addFile ok", hFile != CAHandle_Invalid);

				//Unique 2-byte payload: {d, f}

				U8 payload[2] = { (U8)d, (U8)f };
				Buffer buf = Buffer_createNull();
				Buffer_createCopy(Buffer_createRefConst(payload, 2), t->alloc, &buf, NULL);
				Test_assert(t, "stress: setData ok", CAFile_setData(&ca, hFile, t->alloc, &buf, &t->err));
			}
		}

		Test_assert(t, "stress: folder count", ca.folders.length == STRESS_NUM_DIRS + 1);
		Test_assert(t, "stress: file count", ca.files.length == STRESS_TOTAL_FILES);

		//Verify every file resolves and carries correct data + timestamp

		for (U64 d = 0; d < STRESS_NUM_DIRS; ++d) {

			CAHandle dir = CAFile_resolveSubFolder(&ca, CAHandle_Root, CharString_createRefCStrConst(dirNames[d]));

			if (dir == CAHandle_Invalid) {
				Test_assert(t, "stress: couldn't find dir", false);
				continue;
			}

			for (U64 f = 0; f < STRESS_FILES_PER_DIR; ++f) {

				CAHandle h = CAFile_resolveSubFile(&ca, dir, CharString_createRefCStrConst(fileExts[f]));
				Test_assert(t, "stress S1: resolve ok", h != CAHandle_Invalid);

				if (h == CAHandle_Invalid)
					continue;

				Ns expectedTs = (Ns)((d * 100 + f + 1)) * MS;
				Test_assert(t, "stress S1: timestamp ok", CAFile_fileTime(&ca, h) == expectedTs);

				U8 expected[2] = { (U8)d, (U8)f };
				Bool isValid = false;
				Buffer got   = CAFile_getDataConst(&ca, h, &isValid);
				Test_assert(t, "stress S1: data ok", isValid && Buffer_eq(got, Buffer_createRefConst(expected, 2)));
			}
		}

		//remove all files from "delta"

		{
			//We can assume dir as valid, since we checked before.
			// Also, this dir handle will stay valid because we're only removing files.
			CAHandle dir = CAFile_resolveSubFolder(&ca, CAHandle_Root, CharString_createRefCStrConst("delta"));

			for (U64 f = 0; f < STRESS_FILES_PER_DIR; ++f) {
				CAHandle hFile = CAFile_resolveSubFile(&ca, dir, CharString_createRefCStrConst(fileExts[f]));
				Test_assert(t, "stress S2: pre-remove resolve", hFile != CAHandle_Invalid);
				Test_assert(t, "stress S2: remove ok", CAFile_removeFile(&ca, hFile, t->alloc, &t->err));
			}
		}

		Test_assert(t, "stress S2: file count", ca.files.length == STRESS_TOTAL_FILES - STRESS_FILES_PER_DIR);

		//Verify all other dirs still resolve and data is intact

		for (U64 d = 0; d < STRESS_NUM_DIRS; ++d) {

			CAHandle dir = CAFile_resolveSubFolder(&ca, CAHandle_Root, CharString_createRefCStrConst(dirNames[d]));

			if (d == 3) {	//delta, all files removed

				for (U64 f = 0; f < STRESS_FILES_PER_DIR; ++f) {
					CAHandle resolved = CAFile_resolveSubFile(&ca, dir, CharString_createRefCStrConst(fileExts[f]));
					Test_assert(t, "stress S2: delta file gone", resolved == CAHandle_Invalid);
				}
				continue;
			}

			for (U64 f = 0; f < STRESS_FILES_PER_DIR; ++f) {

				CAHandle h = CAFile_resolveSubFile(&ca, dir, CharString_createRefCStrConst(fileExts[f]));
				Test_assert(t, "stress S2: other dir resolve", h != CAHandle_Invalid);

				if (h == CAHandle_Invalid)
					continue;

				U8 expected[2] = { (U8)d, (U8)f };
				Bool isValid = false;
				Buffer got = CAFile_getDataConst(&ca, h, &isValid);
				Test_assert(t, "stress S2: data intact", isValid && Buffer_eq(got, Buffer_createRefConst(expected, 2)));
			}
		}

		//Move all files from "beta" into "alpha"

		for (U64 f = 0; f < STRESS_FILES_PER_DIR; ++f) {

			CAHandle hBeta = CAFile_resolveCStr(&ca, "beta");
			CAHandle hAlpha = CAFile_resolveCStr(&ca, "alpha");

			if (hBeta == CAHandle_Invalid || hAlpha == CAHandle_Invalid) {
				Test_assert(t, "stress: failed to get beta/alpha", false);
				break;
			}

			CAHandle hConflict = CAFile_resolveSubFile(&ca, hBeta, CharString_createRefCStrConst(fileExts[f]));

			//Rename the existing alpha file to alpha/X0.bin

			CharString str = CharString_createNull();
			CharString_createCopy(CharString_createRefCStrConst(fileExtsRen[f]), t->alloc, &str, &t->err);
			CAFile_rename(&ca, hConflict, t->alloc, &str, NULL);
			CharString_free(&str, t->alloc);

			//Re-resolve src and destination folder after possible rename

			CAHandle hSrc = CAFile_resolveSubFile(&ca, hBeta, CharString_createRefCStrConst(fileExtsRen[f]));
			Test_assert(t, "stress S3: pre-move resolve", hSrc != CAHandle_Invalid);
			Test_assert(t, "stress S3: move ok", CAFile_move(&ca, hSrc, hAlpha, t->alloc, &t->err));
		}

		//beta should now be empty

		{
			CAHandle hBeta = CAFile_resolveCStr(&ca, "beta");
			Test_assert(t, "stress S3: beta empty", hBeta != CAHandle_Invalid && CAFile_fileCount(&ca, hBeta, false) == 0);
		}

		//alpha should have 2 * FILES_PER_DIR files now

		CAHandle hAlpha = CAFile_resolveCStr(&ca, "alpha");
		Test_assert(t, "stress S3: alpha has 2x files",
			hAlpha != CAHandle_Invalid &&
			CAFile_fileCount(&ca, hAlpha, false) == 2 * STRESS_FILES_PER_DIR
		);

		//Verify data preserved on moved beta files (now in alpha/)

		CAHandle hBeta = CAFile_resolveCStr(&ca, "beta");

		for (U64 f = 0; f < STRESS_FILES_PER_DIR; ++f) {

			CAHandle h = CAFile_resolveSubFile(&ca, hAlpha, CharString_createRefCStrConst(fileExtsRen[f]));
			Test_assert(t, "stress S3: moved file resolve", h != CAHandle_Invalid);

			CAHandle hOg = CAFile_resolveSubFile(&ca, hAlpha, CharString_createRefCStrConst(fileExts[f]));
			Test_assert(t, "stress S3: original file resolve", hOg != CAHandle_Invalid);

			CAHandle hMoved = CAFile_resolveSubFile(&ca, hBeta, CharString_createRefCStrConst(fileExts[f]));
			Test_assert(t, "stress S3: moved file resolve", hMoved == CAHandle_Invalid);

			CAHandle hMovedRenamed = CAFile_resolveSubFile(&ca, hBeta, CharString_createRefCStrConst(fileExtsRen[f]));
			Test_assert(t, "stress S3: moved renamed file resolve", hMovedRenamed == CAHandle_Invalid);

			if (h == CAHandle_Invalid)
				continue;

			//The moved file came from beta (d = 1), so payload is {1, f}

			U8 expected[2] = { 1, (U8)f };
			Bool isValid = false;
			Buffer got = CAFile_getDataConst(&ca, h, &isValid);
			Test_assert(t, "stress S3: moved data ok", isValid && Buffer_eq(got, Buffer_createRefConst(expected, 2)));
		}

		//rename every remaining file in "gamma"

		CAHandle hGamma = CAFile_resolveSubFolder(&ca, CAHandle_Root, CharString_createRefCStrConst("gamma"));

		Test_assert(t, "stress: gamma valid", hGamma != CAHandle_Invalid);

		for (U64 f = 0; f < STRESS_FILES_PER_DIR; ++f) {

			CAHandle h = CAFile_resolveSubFile(&ca, hGamma, CharString_createRefCStrConst(fileExts[f]));
			Test_assert(t, "stress S4: pre-rename resolve", h != CAHandle_Invalid);

			const C8 *toString = "0123456789";		//Keep STRESS_FILES_PER_DIR < 9
			CharString currVal = CharString_createRefSizedConst(toString + f, 1, false);

			CharString ns = CharString_createNull();
			CharString_createCopy(currVal, t->alloc, &ns, NULL);
			Test_assert(t, "stress S4: rename ok", CAFile_rename(&ca, h, t->alloc, &ns, &t->err));

			//Old path gone, new path found

			h = CAFile_resolveSubFile(&ca, hGamma, CharString_createRefCStrConst(fileExts[f]));
			Test_assert(t, "stress S4: old gone", h == CAHandle_Invalid);

			CAHandle hNew = CAFile_resolveSubFile(&ca, hGamma, currVal);
			Test_assert(t, "stress S4: new found", hNew != CAHandle_Invalid);

			//Data survives rename

			U8 expected[2] = { 2, (U8)f };
			Bool isValid = false;
			Buffer got = CAFile_getDataConst(&ca, hNew, &isValid);
			Test_assert(t, "stress S4: data after rename", isValid && Buffer_eq(got, Buffer_createRefConst(expected, 2)));
		}

		//Final integrity check
		//Total file count:
		//  alpha:   2 * FILES_PER_DIR  (original + moved from beta)
		//  beta:    0
		//  gamma:   FILES_PER_DIR      (renamed)
		//  delta:   0
		//  epsilon..theta: FILES_PER_DIR each  (dirs 4-7, i.e. 4 dirs)
		//  total = 2 * F + F + 4 * F = 7 * F

		U64 expectedFiles = 7 * STRESS_FILES_PER_DIR;
		Test_assert(t, "stress S5: total file count", ca.files.length == expectedFiles);

		//Every surviving gamma file has the new name and correct data

		for (U64 f = 0; f < STRESS_FILES_PER_DIR; ++f) {
			const C8 *toString = "0123456789";		//Keep STRESS_FILES_PER_DIR < 9
			CharString currVal = CharString_createRefSizedConst(toString + f, 1, false);
			CAHandle hNew = CAFile_resolveSubFile(&ca, hGamma, currVal);
			Test_assert(t, "stress S5: gamma renamed resolve", hNew != CAHandle_Invalid);
		}

		//epsilon through theta still have all their original files + correct data

		for (U64 d = 4; d < STRESS_NUM_DIRS; ++d) {

			CAHandle dir = CAFile_resolveSubFolder(&ca, CAHandle_Root, CharString_createRefCStrConst(dirNames[d]));

			for (U64 f = 0; f < STRESS_FILES_PER_DIR; ++f) {

				CAHandle h = CAFile_resolveSubFile(&ca, dir, CharString_createRefCStrConst(fileExts[f]));
				Test_assert(t, "stress S5: intact dir resolve", h != CAHandle_Invalid);

				if (h == CAHandle_Invalid)
					continue;

				U8 expected[2] = { (U8)d, (U8)f };
				Bool isValid = false;
				Buffer got = CAFile_getDataConst(&ca, h, &isValid);
				Test_assert(t, "stress S5: intact dir data", isValid && Buffer_eq(got, Buffer_createRefConst(expected, 2)));
			}
		}

	doneStress:
		CAFile_free(&ca, t->alloc);
	}

	#undef STRESS_NUM_DIRS
	#undef STRESS_FILES_PER_DIR
	#undef STRESS_TOTAL_FILES
}

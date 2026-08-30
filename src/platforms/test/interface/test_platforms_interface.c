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

//platforms/test/interface/test_platforms_interface.c

//Platform interface (non-functional) unit tests.
//
//What is covered:
//  1. WindowManager   - lifecycle, magic, thread ownership
//  2. Window          - create / destroy (virtual), flags, format support
//  3. InputDevice     - keyboard + mouse creation, handle logic, state machine,
//                       axis values, flag helpers
//  4. File (physical) - add/has/getInfo/write/read/rename/move/remove, round-trip
//  5. File (virtual)  - load section embedded via add_virtual_files,
//                       has / getInfo / read / foreach / unload
//  6. DynamicLibrary  - path validation (+ load/free when SUPPORTS_DYNAMIC_LINKING)
//  7. Allocator       - the platform allocator meets OxC3's own alignment requirement
//
//Run in CI, no display, no human interaction required.

#include "platforms/platform.h"
#include "platforms/window_manager.h"
#include "platforms/window.h"
#include "platforms/keyboard.h"
#include "platforms/mouse.h"
#include "platforms/file.h"
#include "platforms/dynamic_library.h"
#include "types/test/test.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/container/memory_stream.h"
#include "types/base/string_read_helper.h"
#include "types/base/error.h"
#include "types/base/thread.h"

//Tmp physical path used for File tests (relative to working dir)
static const char *testDir         = "platform_test_tmp";
static const char *testFile        = "platform_test_tmp/data.bin";
static const char *testFileRenamed = "data_renamed.bin";
static const char *testFile2       = "platform_test_tmp/move_src.bin";
static const char *testMoveDir     = "platform_test_tmp/subdir";

//Virtual section name registered by CMake.
//A section lives under the target that packaged it, and this suite is compiled into two of them:
// OxC3_plinttst on desktop, OxC3_atest in the android bundle (android has no exec, see src/test/android).
//CMake passes the name so the paths follow whichever one is building.

#ifndef _OXC3_TEST_VFS_TARGET
	#define _OXC3_TEST_VFS_TARGET "OxC3_plinttst"
#endif

static const char *vLibrary        = "//" _OXC3_TEST_VFS_TARGET;
static const char *vSection        = "//" _OXC3_TEST_VFS_TARGET "/testdata";
static const char *vFileHello      = "//" _OXC3_TEST_VFS_TARGET "/testdata/hello.txt";
static const char *vFileSub        = "//" _OXC3_TEST_VFS_TARGET "/testdata/sub/world.txt";

// -- 1. WindowManager lifecycle ------------------------------------------------

static void Test_platformsWindowManager(Test *t) {

	Test_setModule(t, "WindowManager");

	WindowManager mgr = (WindowManager) { 0 };

	//Create with null callbacks is valid (no onCreate)
	WindowManagerCallbacks cbs = (WindowManagerCallbacks) { 0 };
	Test_assert(t, "create", WindowManager_create(cbs, 0, &mgr, &t->err));
	Test_assert(t, "isActive", mgr.isActive == WindowManager_magic);
	Test_assert(t, "isAccessible", WindowManager_isAccessible(&mgr));
	Test_assert(t, "owningThread set", mgr.owningThread == Thread_getId());

	//Double-free must not crash (implementation should guard)
	WindowManager_free(&mgr);
	Test_assert(t, "notAccessibleAfterFree", !WindowManager_isAccessible(&mgr));

	//Null guard
	Test_assert(t, "nullNotAccessible", !WindowManager_isAccessible(NULL));
}

// -- 2. Window create / destroy (virtual) -------------------------------------

static void Test_platformsWindowVirtual(Test *t) {

	Test_setModule(t, "Window/Virtual");

	WindowManager mgr = (WindowManager) { 0 };
	WindowRef *wRef = NULL;

	WindowManagerCallbacks cbs = (WindowManagerCallbacks) { 0 };
	if (!Test_assert(t, "WindowManager_create", WindowManager_create(cbs, 0, &mgr, &t->err)))
		goto clean;

	//Create a virtual window (headless, always supported)
	WindowCallbacks wcbs = (WindowCallbacks) { 0 };
	CharString title = CharString_createRefCStrConst("InterfaceTestWindow");
	I32x2 pos     = I32x2_create2(0, 0);
	I32x2 size    = EResolution_get(EResolution_SD);
	I32x2 minSize = EResolution_get(EResolution_SD);
	I32x2 maxSize = I32x2_create2(4096, 4096);

	Test_assert(t, "createVirtual", WindowManager_createWindow(
		&mgr, EWindowType_Virtual, pos, size, minSize, maxSize,
		EWindowHint_None, title, wcbs, EWindowFormat_AutoRGBA8, 0, &wRef, &t->err
	));

	Window *w = RefPtr_data(wRef, Window);
	Test_assert(t, "windowNotNull", w != NULL);
	Test_assert(t, "typeVirtual",   w && w->type == EWindowType_Virtual);
	Test_assert(t, "sizeW",  w && I32x2_x(w->size) == I32x2_x(EResolution_get(EResolution_SD)));
	Test_assert(t, "sizeH",  w && I32x2_y(w->size) == I32x2_y(EResolution_get(EResolution_SD)));
	Test_assert(t, "owner",  w && w->owner == &mgr);

	//Flags: not minimized, not fullscreen at creation
	Test_assert(t, "notMinimized",   !Window_isMinimized(w));
	Test_assert(t, "notFullscreen",  !Window_isFullScreen(w));

	//Terminate flag helper
	Test_assert(t, "terminate", Window_terminate(w));
	Test_assert(t, "shouldTerminate", w && (w->flags & EWindowFlags_ShouldTerminate));

	//Free window and manager
	RefPtr_dec(&wRef);
	Test_assert(t, "windowNullAfterFree", wRef == NULL);

clean:
	RefPtr_dec(&wRef);
	WindowManager_free(&mgr);
}

// -- 3a. InputDevice - Keyboard ------------------------------------------------

static void Test_platformsKeyboard(Test *t) {

	Test_setModule(t, "InputDevice/Keyboard");

	Keyboard kb = (Keyboard) { 0 };

	if (!Test_assert(t, "create", Keyboard_create(&kb, t->alloc, &t->err)))
		goto clean;

	Test_assert(t, "type", kb.type == EInputDeviceType_Keyboard);
	Test_assert(t, "buttonCount", kb.buttons == EKey_Count);
	Test_assert(t, "axisCount",   kb.axes == 0);

	//Build a handle for EKey_A
	InputHandle hA = InputDevice_createHandle(&kb, (U16)EKey_A, EInputType_Button);
	Test_assert(t, "validHandle",  InputDevice_isValidHandle(&kb, hA));
	Test_assert(t, "isButton",     InputDevice_isButton(&kb, hA));
	Test_assert(t, "notAxis",      !InputDevice_isAxis(&kb, hA));

	//Initial state must be Up
	Test_assert(t, "initialUp",    InputDevice_isUp(&kb, hA));
	Test_assert(t, "notDown",      !InputDevice_isDown(&kb, hA));

	//Simulate a press cycle: markUpdate -> setCurrentState(true) -> markUpdate -> setCurrentState(false)
	InputDevice_markUpdate(&kb);
	Test_assert(t, "setPress",     InputDevice_setCurrentState(&kb, hA, true));
	//After setting current but before next markUpdate the state should be Pressed
	//(prev=0/Up, curr=1) -> EInputState_Pressed
	Test_assert(t, "statePressed", InputDevice_isPressed(&kb, hA));

	InputDevice_markUpdate(&kb);
	//prev=1, curr=1 -> Down
	Test_assert(t, "stateDown",    InputDevice_isDown(&kb, hA));

	InputDevice_markUpdate(&kb);
	Test_assert(t, "setRelease",   InputDevice_setCurrentState(&kb, hA, false));
	//prev=1, curr=0 -> Released
	Test_assert(t, "stateReleased", InputDevice_isReleased(&kb, hA));

	InputDevice_markUpdate(&kb);
	//prev=0, curr=0 -> Up
	Test_assert(t, "stateUp",      InputDevice_isUp(&kb, hA));

	//Invalid handle guards
	InputHandle bad = InputDevice_invalidHandle();
	Test_assert(t, "invalidHandle", !InputDevice_isValidHandle(&kb, bad));
	Test_assert(t, "stateInvalid",  InputDevice_getState(&kb, bad) == EInputState_Up);

	//Flags
	Test_assert(t, "setFlag",   InputDevice_setFlag(&kb, EKeyboardFlags_Caps));
	Test_assert(t, "hasFlag",   InputDevice_hasFlag(&kb, EKeyboardFlags_Caps));
	Test_assert(t, "resetFlag", InputDevice_resetFlag(&kb, EKeyboardFlags_Caps));
	Test_assert(t, "notFlag",   !InputDevice_hasFlag(&kb, EKeyboardFlags_Caps));

clean:
	InputDevice_free(&kb, t->alloc);
}

// -- 3b. InputDevice - Mouse ---------------------------------------------------

static void Test_platformsMouse(Test *t) {

	Test_setModule(t, "InputDevice/Mouse");

	Mouse ms = (Mouse) { 0 };

	if (!Test_assert(t, "create", Mouse_create(&ms, t->alloc, &t->err)))
		goto clean;

	Test_assert(t, "type",        ms.type == EInputDeviceType_Mouse);
	Test_assert(t, "axisCount",   ms.axes  == EMouseAxis_Count);
	Test_assert(t, "buttonCount", ms.buttons == EMouseButton_Count);

	//Axis handle
	InputHandle hRX = InputDevice_createHandle(&ms, EMouseAxis_RX - EMouseAxis_Begin, EInputType_Axis);
	Test_assert(t, "validAxisHandle", InputDevice_isValidHandle(&ms, hRX));
	Test_assert(t, "isAxis",          InputDevice_isAxis(&ms, hRX));

	//Initial axis value is 0
	Test_assert(t, "initAxisZero", InputDevice_getCurrentAxis(&ms, hRX) == 0.f);

	//Set + read axis
	Test_assert(t, "setAxis", InputDevice_setCurrentAxis(&ms, hRX, 0.75f));
	Test_assert(t, "getAxis", InputDevice_getCurrentAxis(&ms, hRX) == 0.75f);

	InputDevice_markUpdate(&ms);
	//After markUpdate, previous = 0.75, current still 0.75 (not reset because resetOnInputLoss depends on axis)
	Test_assert(t, "prevAxis", InputDevice_getPreviousAxis(&ms, hRX) == 0.75f);

	//Delta should be 0 (curr == prev)
	Test_assert(t, "deltaZero", InputDevice_getDeltaAxis(&ms, hRX) == 0.f);

	//Button, left click
	InputHandle hLeft = InputDevice_createHandle(&ms, EMouseButton_Left - EMouseAxis_End, EInputType_Button);
	Test_assert(t, "leftIsButton", InputDevice_isButton(&ms, hLeft));

	InputDevice_markUpdate(&ms);
	InputDevice_setCurrentState(&ms, hLeft, true);
	Test_assert(t, "leftPressed", InputDevice_isPressed(&ms, hLeft));

clean:
	InputDevice_free(&ms, t->alloc);
}

// -- 3c. InputDevice - name/handle serialization round-trip -------------------

static void Test_keySerialization(Test *t) {

	Test_setModule(t, "InputDevice/Serialization");

	Keyboard kb = (Keyboard) { 0 };

	if (!Test_assert(t, "create", Keyboard_create(&kb, t->alloc, &t->err)))
		goto clean;

	//Build handles for a few well-known keys

	InputHandle hA     = InputDevice_createHandle(&kb, (U16)EKey_A,      EInputType_Button);
	InputHandle hSpace = InputDevice_createHandle(&kb, (U16)EKey_Space,  EInputType_Button);
	InputHandle hF1    = InputDevice_createHandle(&kb, (U16)EKey_F1,     EInputType_Button);

	CharString nameA     = InputDevice_getName(&kb, hA);
	CharString nameSpace = InputDevice_getName(&kb, hSpace);
	CharString nameF1    = InputDevice_getName(&kb, hF1);

	CharString expectA     = CharString_createRefCStrConst("EKey_A");
	CharString expectSpace = CharString_createRefCStrConst("EKey_Space");
	CharString expectF1    = CharString_createRefCStrConst("EKey_F1");

	Test_assert(t, "nameA",     CharString_equalsStringSensitive(&nameA, &expectA));
	Test_assert(t, "nameSpace", CharString_equalsStringSensitive(&nameSpace, &expectSpace));
	Test_assert(t, "nameF1",    CharString_equalsStringSensitive(&nameF1, &expectF1));

	//getHandle(getName(h)) must recover the original handle, the serialization contract
	InputHandle rA     = InputDevice_getHandle(&kb, nameA);
	InputHandle rSpace = InputDevice_getHandle(&kb, nameSpace);
	InputHandle rF1    = InputDevice_getHandle(&kb, nameF1);

	Test_assert(t, "roundtripA",     rA     == hA);
	Test_assert(t, "roundtripSpace", rSpace == hSpace);
	Test_assert(t, "roundtripF1",    rF1    == hF1);

	//Unknown name must return the invalid sentinel
	CharString unknown = CharString_createRefCStrConst("EKey_ThisDoesNotExist");
	Test_assert(t, "unknownInvalid", InputDevice_getHandle(&kb, unknown) == InputDevice_invalidHandle());

	//Invalid handle must return an empty name
	CharString badName = InputDevice_getName(&kb, InputDevice_invalidHandle());
	Test_assert(t, "invalidHandleName", !CharString_length(badName));

clean:
	InputDevice_free(&kb, t->alloc);
}

// -- 3d. InputDevice - dead zone + setFlagTo -----------------------------------

static void Test_mouseExtras(Test *t) {

	Test_setModule(t, "InputDevice/Extras");

	Mouse ms = (Mouse) { 0 };

	if (!Test_assert(t, "create", Mouse_create(&ms, t->alloc, &t->err)))
		goto clean;

	//Dead zone is stored per-axis and retrieved through the public helper
	InputHandle hRX = InputDevice_createHandle(&ms, (U16)(EMouseAxis_RX - EMouseAxis_Begin), EInputType_Axis);

	//The mouse axis was registered with some dead zone (may be 0.0); just
	//verify the call doesn't crash and returns a finite value.
	F32 dz = InputDevice_getDeadZone(&ms, hRX);
	Test_assert(t, "deadZoneFinite", dz >= 0.f && dz <= 1.f);

	//getDeadZone on a button handle must return 0
	InputHandle hLeft = InputDevice_createHandle(&ms, (U16)(EMouseButton_Left - EMouseAxis_End), EInputType_Button);
	Test_assert(t, "buttonDeadZeroFalse", InputDevice_getDeadZone(&ms, hLeft) == 0.f);

	//getDeadZone on invalid handle must return 0
	Test_assert(t, "invalidDeadZero", InputDevice_getDeadZone(&ms, InputDevice_invalidHandle()) == 0.f);

	// setFlagTo: set then clear via the combined helper
	Test_assert(t, "setFlagToTrue", InputDevice_setFlagTo(&ms, 0, true));
	Test_assert(t, "hasFlagAfterSetTrue", InputDevice_hasFlag(&ms, 0));

	Test_assert(t, "setFlagToFalse", InputDevice_setFlagTo(&ms, 0, false));
	Test_assert(t, "noFlagAfterSetFalse", !InputDevice_hasFlag(&ms, 0));

	//Out-of-range flag must fail gracefully (flag >= 32)
	Test_assert(t, "flagOOB_setFails",  !InputDevice_setFlag(&ms, 32));
	Test_assert(t, "flagOOB_hasFalse",  !InputDevice_hasFlag(&ms, 32));

clean:
	InputDevice_free(&ms, t->alloc);
}

// -- 4a. File (physical) --------------------------------------------------------

static void Test_platformsFilePhysical(Test *t) {

	Test_setModule(t, "File/Physical");

	CharString dir       = CharString_createRefCStrConst(testDir);
	CharString filePath  = CharString_createRefCStrConst(testFile);
	CharString file2Path = CharString_createRefCStrConst(testFile2);
	CharString moveDir   = CharString_createRefCStrConst(testMoveDir);
	CharString renamed   = CharString_createRefCStrConst(testFileRenamed);

	Buffer writeBuf = Buffer_createNull();
	Buffer readBuf  = Buffer_createNull();
	FileInfo info   = (FileInfo) { 0 };
	RefPtrType fhType = FileHandle_makeType(t->alloc);

	//Clean up from any previous failed run
	File_remove(&dir, 1 * SECOND, t->alloc, NULL);

	//Create directory
	Test_assert(t, "addDir", File_add(&dir, EFileType_Folder, false, t->alloc, &t->err));
	Test_assert(t, "hasDir", File_hasFolder(&dir, t->alloc));

	//Create file
	Test_assert(t, "addFile", File_add(&filePath, EFileType_File, false, t->alloc, &t->err));
	Test_assert(t, "hasFile", File_hasFile(&filePath, t->alloc));

	//getInfo
	Test_assert(t, "getInfo",      File_getInfo(&filePath, &info, t->alloc, &t->err));
	Test_assert(t, "infoType",     info.type == EFileType_File);
	Test_assert(t, "infoSizeZero", info.fileSize == 0);
	FileInfo_free(&info, t->alloc);

	//Write
	const C8 *msg = "OxC3 platform test data";
	U64 msgLen = 23;
	writeBuf = Buffer_createRefConst((const U8*)msg, msgLen);
	Test_assert(t, "write",          File_write(&writeBuf, &filePath, 0, 0, 50 * MS, false, &fhType, &t->err));

	//Read back
	Test_assert(t, "read",        File_read(&filePath, 50 * MS, 0, 0, &fhType, &readBuf, &t->err));
	Test_assert(t, "readLength",  Buffer_length(readBuf) == msgLen);
	Test_assert(t, "readContent", Buffer_eq(writeBuf, Buffer_createRefConst((const U8*)msg, msgLen)));
	Buffer_free(&readBuf, t->alloc);

	//getInfo after write
	Test_assert(t, "getInfoAfterWrite", File_getInfo(&filePath, &info, t->alloc, &t->err));
	Test_assert(t, "infoSizeAfterWrite", info.fileSize == msgLen);
	FileInfo_free(&info, t->alloc);

	//rename
	Test_assert(t, "rename", File_rename(&filePath, &renamed, 50 * MS, t->alloc, &t->err));
	CharString renamedPath = CharString_createNull();
	Test_assert(t, "format",        CharString_format(t->alloc, &renamedPath, &t->err, "%s/%s", testDir, testFileRenamed));
	Test_assert(t, "hasRenamed",    File_hasFile(&renamedPath, t->alloc));
	Test_assert(t, "origGone",      !File_hasFile(&filePath, t->alloc));
	CharString_free(&renamedPath, t->alloc);

	//move (put file2 into subdir)
	Test_assert(t, "addMoveDir", File_add(&moveDir, EFileType_Folder, false, t->alloc, &t->err));
	Test_assert(t, "addFile2",   File_add(&file2Path, EFileType_File, false, t->alloc, &t->err));
	Test_assert(t, "move",       File_move(&file2Path, &moveDir, 50 * MS, t->alloc, &t->err));
	Test_assert(t, "file2Gone",  !File_hasFile(&file2Path, t->alloc));

	//queryFileObjectCount
	U64 count = 0;
	Test_assert(t, "queryCount", File_queryFileObjectCountAll(&dir, true, &count, t->alloc, &t->err));
	// After operations: dir contains subdir (folder), file2 inside subdir, and renamedFile -> at least 3
	Test_assert(t, "countAtLeast3", count >= 3);

	//remove directory recursively
	Test_assert(t, "removeDir",  File_remove(&dir, 50 * MS, t->alloc, &t->err));
	Test_assert(t, "dirGone",    !File_has(&dir, t->alloc));

	goto clean;

clean:
	Buffer_free(&readBuf, t->alloc);
	FileInfo_free(&info, t->alloc);
	File_remove(&dir, 1 * SECOND, t->alloc, NULL);   // best-effort cleanup
}

// -- 4b. File - FileHandle direct API + ref-count leak check ------------------

static void Test_fileHandle(Test* t) {

	Test_setModule(t, "File/HandleDirect");

	RefPtrType fhType = FileHandle_makeType(t->alloc);

	CharString dir = CharString_createRefCStrConst("platform_test_handle_tmp");
	CharString filePath = CharString_createRefCStrConst("platform_test_handle_tmp/direct.bin");

	FileHandleRef *handle = NULL;
	FileHandleRef* roHandle = NULL;
	Buffer writeBuf = Buffer_createNull();
	Buffer readBuf = Buffer_createNull();

	//Clean up from any previous run
	File_remove(&dir, 1 * SECOND, t->alloc, NULL);

	//Create directory + file

	if (!Test_assert(t, "addDir", File_add(&dir, EFileType_Folder, false, t->alloc, &t->err)))
			goto clean;

	if (!Test_assert(t, "addFile", File_add(&filePath, EFileType_File, false, t->alloc, &t->err)))
		goto clean;

	//Write via handle
	U64 allocsBefore = Platform_getActiveAllocations(0);

	if (!Test_assert(t, "open_write", File_open(&filePath, 50 * MS, EFileOpenType_Write, false, &fhType, &handle, &t->err)))
		goto clean;

	//One allocation for the RefPtr should now be live
	Test_assert(t, "allocRises", Platform_getActiveAllocations(0) > allocsBefore);

	const C8 *payload = "DirectHandleTest";
	U64 payloadLen = 16;
	writeBuf = Buffer_createRefConst((const U8*)payload, payloadLen);

	Test_assert(t, "write", FileHandleRef_write(handle, 0, payloadLen, &writeBuf, &t->err));
	RefPtr_dec(&handle);

	//Write to read-only handle must fail

	Test_assert(t, "open_read", File_open(
		&filePath, 50 * MS, EFileOpenType_Read, false, &fhType, &roHandle, &t->err
	));

	Test_assert(t, "writeToROFails", !FileHandleRef_write(roHandle, 0, payloadLen, &writeBuf, NULL));
	RefPtr_dec(&roHandle);

	//Close the read handle
	RefPtr_dec(&handle);
	Test_assert(t, "handleNullAfterDec", handle == NULL);

	//After dec the allocation must have been released
	Test_assert(t, "allocRestored", Platform_getActiveAllocations(0) <= allocsBefore);

	//read via handle
	if (!Test_assert(t, "open_read2", File_open(&filePath, 5 * SECOND, EFileOpenType_Read, false, &fhType, &handle, &t->err)))
		goto clean;

	const FileHandle *fh = RefPtr_data(handle, FileHandle);
	U64 fileSize = FileHandle_fileSize(fh);
	Test_assert(t, "fileSizeMatch", fileSize == payloadLen);

	Test_assert(t, "createReadBuf", Buffer_createUninitializedBytes(fileSize, t->alloc, &readBuf, &t->err));

	Test_assert(t, "read", FileHandleRef_read(handle, 0, fileSize, &readBuf, &t->err));

	Test_assert(t, "contentMatch", Buffer_eq(readBuf, Buffer_createRefConst((const U8*)payload, payloadLen)));

	//Read past EOF must fail
	Buffer extra = Buffer_createNull();
	Buffer_createUninitializedBytes(1, t->alloc, &extra, &t->err);
	Test_assert(t, "readOOB", !FileHandleRef_read(handle, fileSize, 1, &extra, NULL));
	Buffer_free(&extra, t->alloc);

	RefPtr_dec(&handle);

	//ReadWrite handle
	FileHandleRef* rwHandle = NULL;
	if (!Test_assert(t, "open_rw", File_open(
		&filePath, 5 * SECOND, EFileOpenType_ReadWrite, false, &fhType, &rwHandle, &t->err
	)))
		goto clean;

	Test_assert(t, "rwIsRead", EFileHandle_isRead(RefPtr_data(rwHandle, FileHandle)));
	Test_assert(t, "rwIsWrite", EFileHandle_isWrite(RefPtr_data(rwHandle, FileHandle)));
	RefPtr_dec(&rwHandle);

	//Final leak check
	Test_assert(t, "noLeakAfterAll",
		Platform_getActiveAllocations(0) <= allocsBefore + 1);

clean:
	if (handle)   RefPtr_dec(&handle);
	if (roHandle) RefPtr_dec(&roHandle);
	Buffer_free(&writeBuf, t->alloc);
	Buffer_free(&readBuf, t->alloc);
	File_remove(&dir, 5 * SECOND, t->alloc, NULL);
}

// -- 4c. File - long path tests ------------------------------------------------
//Windows: verifies \\?\ long-path handling (paths > MAX_PATH = 260 chars).
//All platforms: verifies deeply nested paths and names near the 255-byte filename limit work correctly end-to-end
// through the platform abstraction.
 
//Build a CharString from a stack buffer without allocation.
//Only safe for string literals / compile-time-known content.
#define CS_REF(literal) CharString_createRefCStrConst(literal)
 
//Maximum single filename component we test (just under the 255-byte POSIX limit),
//platform_test_longpath/ + ^ > MAX_PATH (260)
#define LONG_NAME_LEN 240
 
static void Test_fileLongPath(Test *t) {
 
	Test_setModule(t, "File/LongPaths");
 
	Bool s_uccess = true;
	RefPtrType fhType = FileHandle_makeType(t->alloc);
 
	CharString root     = CharString_createNull();
	CharString deepDir  = CharString_createNull();
	CharString longFile = CharString_createNull();
	CharString deepFile = CharString_createNull();
	CharString longName = CharString_createNull();
	Buffer readBuf      = Buffer_createNull();
	FileInfo info       = (FileInfo){ 0 };
 
	//Root for all long-path tests
	gotoIfError3(clean, CharString_createCopy(CS_REF("platform_test_longpath"), t->alloc, &root, &t->err));
 
	File_remove(&root, 1 * SECOND, t->alloc, NULL);  //clean prior run
 
	// -- 1. Filename near the 255-byte limit -----------------------------------
	// Build:  platform_test_longpath/<240 'a' chars>.txt
 
	gotoIfError3(clean, CharString_create('a', LONG_NAME_LEN, t->alloc, &longName, NULL));
 
	gotoIfError3(clean, CharString_format(
		t->alloc, &longFile, NULL, "%.*s/%.*s.txt",
		(int) CharString_length(root),    root.ptr,
		(int) CharString_length(longName), longName.ptr
	));

	CharString_free(&longName, t->alloc);
 
	if (!Test_assert(t, "addRoot", File_add(&root, EFileType_Folder, false, t->alloc, &t->err)))
		goto clean;
 
	Test_assert(t, "addLongNameFile", File_add(&longFile, EFileType_File, false, t->alloc, &t->err));
	Test_assert(t, "hasLongNameFile", File_hasFile(&longFile, t->alloc));
 
	//getInfo on a long-named file

	Test_assert(t, "getInfoLong", File_getInfo(&longFile, &info, t->alloc, &t->err));
	Test_assert(t, "infoTypeLong", info.fileSize == 0);
	FileInfo_free(&info, t->alloc);
 
	//Write + read through the long-named file

	{
		const C8 *msg = "long-name payload";
		U64 msgLen    = 17;
		Buffer writeBuf = Buffer_createRefConst((const U8*)msg, msgLen);
		Test_assert(t, "writeLong", File_write(&writeBuf, &longFile, 0, 0, 50 * MS, false, &fhType, &t->err));
		Test_assert(t, "readLong", File_read(&longFile, 50 * MS, 0, 0, &fhType, &readBuf, &t->err));
		Test_assert(t, "longContentMatch", Buffer_eq(readBuf, writeBuf));
		Buffer_free(&readBuf, t->alloc);
	}
 
	//Rename the long-named file

	{
		CharString newName = CharString_createRefCStrConst("renamed_long.txt");
		Test_assert(t, "renameLong", File_rename(&longFile, &newName, 50 * MS, t->alloc, &t->err));
 
		CharString renamedPath = CharString_createNull();
		Test_assert(t, "format", CharString_format(t->alloc, &renamedPath, &t->err, "%.*s/renamed_long.txt",
			(int) CharString_length(root), root.ptr
		));

		Test_assert(t, "hasRenamed",  File_hasFile(&renamedPath, t->alloc));
		Test_assert(t, "longGone",    !File_hasFile(&longFile, t->alloc));
		CharString_free(&renamedPath, t->alloc);
	}

	CharString_free(&longFile, t->alloc);
 
	// -- 2. Deep directory tree (total path > 260 chars on Windows) ------------
	//Build a path that exceeds the legacy MAX_PATH of 260 characters
	// so that CharString_toLongPath's \\?\ prefix is exercised on Windows.
	//On POSIX systems this just tests a legitimately deep path.
	//
	//Structure:
	//   platform_test_longpath/
	//     level_01/level_02/.../level_32/   (each component 8 chars + '/')
	//
	//8 * 32 + 32 slashes = 288 chars just for levels, plus the 22-char root and a filename -> comfortably over 260.
 
	{
		//Build the deep dir path incrementally
		gotoIfError3(clean, CharString_createCopy(root, t->alloc, &deepDir, &t->err));
 
		for (U32 i = 1; i <= 32; ++i) {
			CharString component = CharString_createNull();
			gotoIfError3(clean, CharString_format(t->alloc, &component, &t->err, "/level_%02u", i));
			gotoIfError3(clean, CharString_appendString(&deepDir, &component, t->alloc, &t->err));
			CharString_free(&component, t->alloc);
		}
 
		//File_add with createParentOnly=false should create all missing ancestors
		Test_assert(t, "addDeepDir", File_add(&deepDir, EFileType_Folder, false, t->alloc, &t->err));
		Test_assert(t, "hasDeepDir", File_hasFolder(&deepDir, t->alloc));
 
		//Verify total path length is actually > 260 to confirm we're testing
		// the long-path code path on Windows
		#if _PLATFORM_TYPE == PLATFORM_WINDOWS
			Test_assert(t, "pathExceeds MAX_PATH", CharString_length(deepDir) > 260);
		#endif
 
		//Create, write, and read a file at the bottom of the deep tree
		gotoIfError3(clean, CharString_createCopy(deepDir, t->alloc, &deepFile, NULL));

		CharString deepPayloadBin = CS_REF("/deep_payload.bin");
		gotoIfError3(clean, CharString_appendString(&deepFile, &deepPayloadBin, t->alloc, NULL));
 
		Test_assert(t, "addDeepFile", File_add(&deepFile, EFileType_File, false, t->alloc, &t->err));
 
		const C8 *deepMsg = "deep path payload";
		U64 deepLen       = 17;
		Buffer deepWrite  = Buffer_createRefConst((const U8*)deepMsg, deepLen);
 
		Test_assert(t, "writeDeep", File_write(&deepWrite, &deepFile, 0, 0, 50 * MS, false, &fhType, &t->err));
 
		Test_assert(t, "readDeep", File_read(&deepFile, 50 * MS, 0, 0, &fhType, &readBuf, &t->err));
		Test_assert(t, "deepContentMatch", Buffer_length(readBuf) == deepLen && Buffer_eq(readBuf, deepWrite));
		Buffer_free(&readBuf, t->alloc);
 
		//getInfo at the deep file
		Test_assert(t, "getInfoDeep",  File_getInfo(&deepFile, &info, t->alloc, &t->err));
		Test_assert(t, "infoDeepFile", info.type == EFileType_File);
		Test_assert(t, "infoDeepSize", info.fileSize == deepLen);
		FileInfo_free(&info, t->alloc);
 
		//queryFileObjectCount from root, recursive, must find at least the
		//deep directory chain and the two files we created
		U64 count = 0;
		Test_assert(t, "queryDeepCount", File_queryFileObjectCountAll(&root, true, &count, t->alloc, &t->err));

		//32 level_XX dirs + renamed_long.txt + deep_payload.bin = 34
		Test_assert(t, "deepCountAtLeast22", count == 34);
	}
 
	// -- 3. Remove the whole tree recursively (long paths included) ------------

	Test_assert(t, "removeDeepTree", File_remove(&root, 50 * MS, t->alloc, &t->err));
	Test_assert(t, "rootGone", !File_has(&root, t->alloc));
 
clean:
	(void) s_uccess;

	FileInfo_free(&info,       t->alloc);
	Buffer_free(&readBuf,      t->alloc);
	CharString_free(&root,     t->alloc);
	CharString_free(&deepDir,  t->alloc);
	CharString_free(&longFile, t->alloc);
	CharString_free(&deepFile, t->alloc);
	CharString_free(&longName, t->alloc);

	CharString longPath = CharString_createRefCStrConst("platform_test_longpath");
	File_remove(&longPath, 1 * SECOND, t->alloc, NULL);
}

// -- 4d. File - Empty & Overlapping Ranges ------------------------------------

static void Test_fileEdgeCases(Test *t) {

	Test_setModule(t, "File/EdgeCases");

	RefPtrType fhType = FileHandle_makeType(t->alloc);
	CharString dir = CharString_createRefCStrConst("platform_test_edge");
	CharString filePath = CharString_createRefCStrConst("platform_test_edge/overlap.bin");
	Buffer readBuf = Buffer_createNull();

	File_remove(&dir, 50 * MS, t->alloc, NULL);
	Test_assert(t, "addDir", File_add(&dir, EFileType_Folder, false, t->alloc, &t->err));
	Test_assert(t, "addFile", File_add(&filePath, EFileType_File, false, t->alloc, &t->err));

	//1. Empty file read/write
	Test_assert(t, "writeEmpty", File_write(&readBuf, &filePath, 0, 0, 50 * MS, false, &fhType, &t->err));
	Test_assert(t, "readEmpty", File_read(&filePath, 50 * MS, 0, 0, &fhType, &readBuf, &t->err));
	Test_assert(t, "emptyLenZero", Buffer_length(readBuf) == 0);
	Buffer_free(&readBuf, t->alloc);

	//2. Overlapping read/write
	const C8 *payload = "0123456789";
	Buffer writeBuf = Buffer_createRefConst(payload, 10);
	Test_assert(t, "write10", File_write(&writeBuf, &filePath, 0, 0, 50 * MS, false, &fhType, &t->err));

	//Read 5 bytes starting at offset 3 ("34567")
	Test_assert(t, "readOverlap", File_read(&filePath, 50 * MS, 3, 5, &fhType, &readBuf, &t->err));
	Test_assert(t, "overlapLen", Buffer_length(readBuf) == 5);
	Test_assert(t, "overlapContent", Buffer_eq(readBuf, Buffer_createRefConst("34567", 5)));
	Buffer_free(&readBuf, t->alloc);

	//Cleanup
	File_remove(&dir, 50 * MS, t->alloc, NULL);
}

// -- 4e. File - Case Sensitivity & UTF-8 Paths --------------------------------

static void Test_fileCaseAndUtf8(Test *t) {

	Test_setModule(t, "File/CaseUtf8");

	CharString dir = CharString_createRefCStrConst("platform_test_caseutf8");
	File_remove(&dir, 50 * MS, t->alloc, NULL);
	Test_assert(t, "addDir", File_add(&dir, EFileType_Folder, false, t->alloc, &t->err));

	//1. Case sensitivity
	CharString upperFile = CharString_createRefCStrConst("platform_test_caseutf8/FOO.TXT");
	CharString lowerFile = CharString_createRefCStrConst("platform_test_caseutf8/foo.txt");
	Test_assert(t, "addUpper", File_add(&upperFile, EFileType_File, false, t->alloc, &t->err));

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS || _PLATFORM_TYPE == PLATFORM_OSX || _PLATFORM_TYPE == PLATFORM_IOS || \
		_PLATFORM_TYPE == PLATFORM_ANDROID
		//Windows/macOS: case-insensitive by default.
		//Android belongs here too even though its kernel is Linux: the working directory is externalDataPath
		//(see aplatform.c), and external storage is FUSE emulated, which preserves case but matches without it.
		Test_assert(t, "hasLower", File_hasFile(&lowerFile, t->alloc));
	#else
		//Linux: case-sensitive
		Test_assert(t, "notHasLower", !File_hasFile(&lowerFile, t->alloc));
	#endif

	//2. UTF-8 path (rocket emoji = 0xF0 0x9F 0x9A 0x80)
	//Constructed programmatically to avoid source-encoding issues
	CharString utf8File = CharString_createRefCStrConst(u8"platform_test_caseutf8/test_\U0001F680.txt");
	Test_assert(t, "addUtf8", File_add(&utf8File, EFileType_File, false, t->alloc, &t->err));
	Test_assert(t, "hasUtf8", File_hasFile(&utf8File, t->alloc));

	File_remove(&dir, 50 * MS, t->alloc, NULL);
}

// -- 4f. File - Repeated Open/Close Cycle -------------------------------------

static void Test_fileRepeatedOpenClose(Test *t) {

	Test_setModule(t, "File/RepeatedOpenClose");

	RefPtrType fhType = FileHandle_makeType(t->alloc);
	CharString dir = CharString_createRefCStrConst("platform_test_cycle");
	CharString filePath = CharString_createRefCStrConst("platform_test_cycle/cycle.bin");

	File_remove(&dir, 50 * MS, t->alloc, NULL);
	Test_assert(t, "addDir", File_add(&dir, EFileType_Folder, false, t->alloc, &t->err));

	//Helper to track active allocations across cycles
	U64 allocsBase = Platform_getActiveAllocations(0);

	for (U32 cycle = 0; cycle < 4; ++cycle) {

		U32 isWrite = (cycle % 2) == 0; // W, R, W, R

		const C8 *payload = isWrite ? "WRITE_CYCLE_42" : "READ_VERIFY";
		U64 payloadLen = isWrite ? 14 : 13;

		FileHandleRef *handle = NULL;
		Buffer buf = Buffer_createNull();

		//Open
		EFileOpenType openType = isWrite ? EFileOpenType_Write : EFileOpenType_Read;
		Test_assert(t, "open", File_open(&filePath, 50 * MS, openType, false, &fhType, &handle, &t->err));
		Test_assert(t, "handleValid", handle != NULL);

		if (isWrite) {
			buf = Buffer_createRefConst(payload, payloadLen);
			Test_assert(t, "write", FileHandleRef_write(handle, 0, payloadLen, &buf, &t->err));
		} else {
			const FileHandle *fh = RefPtr_data(handle, FileHandle);
			U64 fileSize = FileHandle_fileSize(fh);
			Test_assert(t, "createReadBuf", Buffer_createUninitializedBytes(fileSize, t->alloc, &buf, &t->err));
			Test_assert(t, "read", FileHandleRef_read(handle, 0, fileSize, &buf, &t->err));
			Test_assert(t, "readLen", Buffer_length(buf) == fileSize);

			//Verify content matches last write

			Test_assert(t, "contentMatch",
				fileSize == 14 && Buffer_eq(buf, Buffer_createRefConst("WRITE_CYCLE_42", 14)));
		}

		//Close & cleanup
		RefPtr_dec(&handle);
		Test_assert(t, "handleNulled", handle == NULL);
		Buffer_free(&buf, t->alloc);

		//Leak check mid-cycle
		Test_assert(t, "noLeakCycle", Platform_getActiveAllocations(0) <= allocsBase + 2);
	}

	//Final cleanup
	File_remove(&dir, 50 * MS, t->alloc, NULL);
}

// -- 5. File (virtual) ---------------------------------------------------------

static void Test_platformsFileVirtual(Test *t) {

	Test_setModule(t, "File/Virtual");

	Error err = Error_none();
	const Allocator *alloc = Platform_instance->alloc;

	CharString secPath   = CharString_createRefCStrConst(vSection);
	CharString helloPath = CharString_createRefCStrConst(vFileHello);
	CharString subPath   = CharString_createRefCStrConst(vFileSub);

	Buffer readBuf = Buffer_createNull();
	FileInfo info  = (FileInfo) { 0 };

	const RefPtrType memStreamType = MemoryStream_makeType(alloc);
	RefPtrType fhType = FileHandle_makeType(alloc);

	//The section is embedded into the binary by CMake; load it.
	//(MemoryStream and EncStream RefPtrTypes are needed; obtain from platform)
	//For the test we pass NULL encryption key (test data is unencrypted).
	Test_assert(t, "loadVirtual", File_loadVirtual(&secPath, &memStreamType, NULL, NULL, alloc, &err));

	Test_assert(t, "isLoaded", File_isVirtualLoaded(&secPath, alloc, &err));

	//has
	Test_assert(t, "hasHello", File_has(&helloPath, alloc));
	Test_assert(t, "hasSub",   File_has(&subPath,   alloc));

	//getInfo on a virtual file
	Test_assert(t, "getInfoHello", File_getInfo(&helloPath, &info, alloc, &err));
	Test_assert(t, "infoIsFile",   info.type == EFileType_File);
	Test_assert(t, "infoSizePos",  info.fileSize > 0);
	FileInfo_free(&info, alloc);

	//getInfo on virtual folder
	Test_assert(t, "getInfoSection", File_getInfo(&secPath, &info, alloc, &err));
	Test_assert(t, "infoIsFolder",   info.type == EFileType_Folder);
	FileInfo_free(&info, alloc);

	//read hello.txt
	
	//Virtual reads use File_read which dispatches to File_readVirtual internally
	Test_assert(t, "readHello", File_read(&helloPath, 50 * MS, 0, 0, &fhType, &readBuf, &err));
	Test_assert(t, "readNonEmpty", Buffer_length(readBuf) > 0);
	//Content must contain "Hello"
	CharString content = CharString_createRefSizedConst((const C8*)readBuf.ptr, Buffer_length(readBuf), false);

	const CharString hello = CharString_createRefCStrConst("Hello");
	Test_assert(t, "helloContent", CharString_containsStringSensitive(&content, &hello, 0, 0));
	Buffer_free(&readBuf, alloc);

	//foreach
	U64 count = 0;
	Test_assert(t, "queryVirtualCount", File_queryFileObjectCountAll(&secPath, true, &count, alloc, &err));
	Test_assert(t, "atLeast2", count >= 2);   // hello.txt + sub/world.txt

	//unload
	Test_assert(t, "unload", File_unloadVirtual(&secPath, alloc, &err));
	Test_assert(t, "notLoaded", !File_isVirtualLoaded(&secPath, alloc, NULL));

	goto clean;

clean:
	Buffer_free(&readBuf, alloc);
	FileInfo_free(&info, alloc);
	File_unloadVirtual(&secPath, alloc, NULL);
}

// -- 5b. Virtual - foreach hands back paths that are valid inputs ----------------------------------

//The contract documented above File_foreach: every path a callback receives is fully qualified and can be
// fed straight back into another File_* call.
//It is worth its own test because it broke once already, and it breaks silently: every in-tree consumer cuts
// a known prefix off the reported path, so a regression corrupts packaged archive names instead of erroring.

typedef struct ForeachRoundTrip {
	Test *t;
	U64 files, folders, badPrefix, unreadable, wrongSize;
} ForeachRoundTrip;

static Bool Test_foreachRoundTripCallback(const FileInfo *info, void *userData, const Allocator *alloc, Error *e_rr) {

	(void) e_rr;

	ForeachRoundTrip *ctx = (ForeachRoundTrip*) userData;

	if(info->type == EFileType_Folder)
		++ctx->folders;

	else ++ctx->files;

	//Fully qualified means // followed by the section for a virtual entry, and an absolute path otherwise.
	//A third slash would mean the section got joined onto an already prefixed path.
	//Either way the check has to keep going afterwards, since the read back below is the actual point.

	if(File_isVirtual(info->path)) {

		if(CharString_getAt(info->path, 2) == '/')
			++ctx->badPrefix;
	}

	else if(!CharString_length(info->path) || CharString_getAt(info->path, 0) == '.')
		++ctx->badPrefix;

	//The load bearing half: hand the reported path straight back to the file api.
	//getInfo has to agree it exists, and for a file the read has to produce exactly the size foreach reported.

	FileInfo again = (FileInfo) { 0 };

	if(!File_getInfo(&info->path, &again, alloc, NULL)) {
		++ctx->unreadable;
		return true;
	}

	if(again.type != info->type || again.fileSize != info->fileSize)
		++ctx->wrongSize;

	FileInfo_free(&again, alloc);

	if(info->type == EFileType_Folder)
		return true;

	Buffer buf = Buffer_createNull();
	RefPtrType fhType = FileHandle_makeType(alloc);

	if(!File_read(&info->path, 50 * MS, 0, 0, &fhType, &buf, NULL))
		++ctx->unreadable;

	else if(Buffer_length(buf) != info->fileSize)
		++ctx->wrongSize;

	Buffer_free(&buf, alloc);
	return true;
}

static void Test_virtualForeachRoundTrip(Test *t) {

	Test_setModule(t, "File/ForeachRoundTrip");

	const Allocator *alloc = Platform_instance->alloc;
	const RefPtrType memStreamType = MemoryStream_makeType(alloc);

	CharString secPath = CharString_createRefCStrConst(vSection);

	if(!Test_assert(t, "loadVirtual", File_loadVirtual(&secPath, &memStreamType, NULL, NULL, alloc, &t->err)))
		return;

	ForeachRoundTrip ctx = (ForeachRoundTrip) { .t = t };

	Test_assert(t, "foreachOk", File_foreach(
		&secPath, !Platform_instance->useWorkingDir, Test_foreachRoundTripCallback, &ctx, true, alloc, &t->err
	));

	Test_assert(t, "sawFiles",       ctx.files >= 2);            //hello.txt and sub/world.txt
	Test_assert(t, "allPrefixed",    !ctx.badPrefix);
	Test_assert(t, "allReadBack",    !ctx.unreadable);
	Test_assert(t, "sizesAgree",     !ctx.wrongSize);

	File_unloadVirtual(&secPath, alloc, NULL);

	//The same guarantee for physical paths, which report absolute rather than //-prefixed.
	//This builds its own two entry directory rather than walking the working directory,
	// since whatever happens to be lying there on a runner would make the read back assertions flaky.

	ForeachRoundTrip phys = (ForeachRoundTrip) { .t = t };

	CharString physDir  = CharString_createRefCStrConst("platform_foreach_tmp");
	CharString physFile = CharString_createRefCStrConst("platform_foreach_tmp/entry.bin");
	CharString physSub  = CharString_createRefCStrConst("platform_foreach_tmp/sub");

	RefPtrType fhType = FileHandle_makeType(alloc);
	Buffer payload = Buffer_createRefConst("foreach", 7);

	File_remove(&physDir, 50 * MS, alloc, NULL);

	if (
		Test_assert(t, "physAddDir",  File_add(&physDir,  EFileType_Folder, false, alloc, &t->err)) &&
		Test_assert(t, "physAddSub",  File_add(&physSub,  EFileType_Folder, false, alloc, &t->err)) &&
		Test_assert(t, "physAddFile", File_add(&physFile, EFileType_File,   false, alloc, &t->err)) &&
		Test_assert(t, "physWrite",   File_write(&payload, &physFile, 0, 0, 50 * MS, false, &fhType, &t->err))
	) {
		Test_assert(t, "physForeach", File_foreach(
			&physDir, !Platform_instance->useWorkingDir, Test_foreachRoundTripCallback, &phys, true, alloc, &t->err
		));

		Test_assert(t, "physSawBoth",       phys.files >= 1 && phys.folders >= 1);
		Test_assert(t, "physicalAbsolute",  !phys.badPrefix);
		Test_assert(t, "physicalReadBack",  !phys.unreadable);
		Test_assert(t, "physicalSizes",     !phys.wrongSize);
	}

	File_remove(&physDir, 50 * MS, alloc, NULL);
}

// -- 5a. Virtual - Double load, operations on root, library or section  ----------------------------

static void Test_virtualEdgeCases(Test *t) {

	Test_setModule(t, "Virtual/EdgeCases");

	const RefPtrType memStreamType = MemoryStream_makeType(t->alloc);
	CharString vRoot   = CharString_createRefCStrConst("//.");
	CharString libRoot = CharString_createRefCStrConst(vLibrary);
	CharString sec     = CharString_createRefCStrConst(vSection);
	CharString child   = CharString_createRefCStrConst(vFileHello);

	//1. Load at section level (baseline)
	Test_assert(t, "loadSec", File_loadVirtual(&sec, &memStreamType, NULL, NULL, t->alloc, &t->err));
	Test_assert(t, "secLoaded", File_isVirtualLoaded(&sec, t->alloc, NULL));
	Test_assert(t, "childExists", File_hasFile(&child, t->alloc));

	//2. Unload section, then load at library level
	Test_assert(t, "unloadSec", File_unloadVirtual(&sec, t->alloc, NULL));
	Test_assert(t, "secGone", !File_isVirtualLoaded(&sec, t->alloc, NULL));

	Test_assert(t, "loadLib", File_loadVirtual(&libRoot, &memStreamType, NULL, NULL, t->alloc, &t->err));
	Test_assert(t, "libLoaded", File_isVirtualLoaded(&libRoot, t->alloc, NULL));

	//Library load should expose all children
	Test_assert(t, "childViaLib", File_hasFile(&child, t->alloc));

	//3. Unload library level -> should cascade to children
	Test_assert(t, "unloadLib", File_unloadVirtual(&libRoot, t->alloc, NULL));
	Test_assert(t, "libGone", !File_isVirtualLoaded(&libRoot, t->alloc, NULL));
	Test_assert(t, "childGoneAfterLibUnload", !File_has(&child, t->alloc));
	Test_assert(t, "secGoneAfterLibUnload", !File_has(&sec, t->alloc));

	//4. Load at virtual root level (if supported)
	Test_assert(t, "loadRoot", File_loadVirtual(&vRoot, &memStreamType, NULL, NULL, t->alloc, &t->err));
	Test_assert(t, "rootLoaded", File_isVirtualLoaded(&vRoot, t->alloc, NULL));

	//Root load should expose all library namespaces
	Test_assert(t, "libVisibleFromRoot", File_hasFolder(&libRoot, t->alloc));
	Test_assert(t, "childFromRoot", File_hasFile(&child, t->alloc));

	//Unload root -> should clear everything

	Test_assert(t, "unloadRoot", File_unloadVirtual(&vRoot, t->alloc, NULL));
	Test_assert(t, "rootGone", !File_isVirtualLoaded(&vRoot, t->alloc, NULL));
	Test_assert(t, "libGoneAfterRootUnload", !File_has(&libRoot, t->alloc));
	Test_assert(t, "childGoneAfterRootUnload", !File_has(&child, t->alloc));

	//5. Final state: root always exists, nothing leaked
	Test_assert(t, "rootAlwaysExists", File_hasFolder(&vRoot, t->alloc));
	U64 count = 0;
	Test_assert(t, "rootEmpty", File_queryFileObjectCountAll(&vRoot, true, &count, t->alloc, &t->err) && count == 0);
}

// -- 6. DynamicLibrary ---------------------------------------------------------

static void Test_dynamicLibrary(Test *t) {

	(void) t;

#ifdef SUPPORTS_DYNAMIC_LINKING

	Test_setModule(t, "DynamicLibrary");

	Error err = Error_none();

	//isValidPath: empty string should be invalid
	CharString empty = CharString_createRefCStrConst("");
	Test_assert(t, "emptyInvalid", !DynamicLibrary_isValidPath(empty));

	//A totally bogus path
	CharString bogus = CharString_createRefCStrConst("not_a_real_library_xyz");
	Test_assert(t, "bogusInvalid", !DynamicLibrary_isValidPath(bogus));

	//Load the dummy library we've generated

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		CharString rtLib = CharString_createRefCStrConst("OxC3_platforms_interface_dylib_test.dll");
	#elif _PLATFORM_TYPE == PLATFORM_OSX
		CharString rtLib = CharString_createRefCStrConst("libOxC3_platforms_interface_dylib_test.dylib");
	#else
		CharString rtLib = CharString_createRefCStrConst("libOxC3_platforms_interface_dylib_test.so");
	#endif

	DynamicLibrary dl = NULL;

	//Android puts an apk's native libraries in a loader-managed directory, not the app data dir that
	//isAppDir resolves against, so a bare name (which the linker resolves against it) is the way in.

	#if _PLATFORM_TYPE == PLATFORM_ANDROID
		Test_assert(t, "load", DynamicLibrary_loadSystem(rtLib, &dl, &err));
	#else
		Test_assert(t, "load", DynamicLibrary_load(rtLib, true, &dl, &err));
	#endif

	Test_assert(t, "nonNull", dl != NULL);

	//Load a known symbol that exists in all C runtimes
	void *sym0 = NULL, *sym1 = NULL, *symFail = NULL;
	CharString symName0 = CharString_createRefCStrConst("Test_foo");
	CharString symName1 = CharString_createRefCStrConst("Test_bar");
	CharString symNameFail = CharString_createRefCStrConst("Test_foobar");

	Test_assert(t, "loadSymbol0", DynamicLibrary_loadSymbol(dl, symName0, &sym0, &err));
	Test_assert(t, "loadSymbol1", DynamicLibrary_loadSymbol(dl, symName1, &sym1, &err));
	Test_assert(t, "loadSymbolFail", !DynamicLibrary_loadSymbol(dl, symNameFail, &symFail, NULL));
	Test_assert(t, "symNonNull0", sym0 != NULL);
	Test_assert(t, "symNonNull1", sym1 != NULL);
	Test_assert(t, "symNullFail", symFail == NULL);

	DynamicLibrary_free(dl);

	//Loading a non-existent library should fail gracefully
	DynamicLibrary dl2 = NULL;
	CharString noLib = CharString_createRefCStrConst("does_not_exist_oxc3_test.dll");
	Test_assert(t, "loadFails", !DynamicLibrary_load(noLib, false, &dl2, NULL));
	Test_assert(t, "dl2Null", dl2 == NULL);

#endif
}

// -- 7. EResolution helpers ---------------------------------------------------

static void Test_resolution(Test *t) {

	Test_setModule(t, "EResolution");

	//get -> create round-trip for common named resolutions
	typedef struct { EResolution r; I32 w; I32 h; } Case;
	static const Case cases[] = {
		{ EResolution_HD,  1280, 720  },
		{ EResolution_FHD, 1920, 1080 },
		{ EResolution_QHD, 2560, 1440 },
		{ EResolution_UHD, 3840, 2160 },
	};

	for (U32 i = 0; i < 4; ++i) {
		I32x2 v = EResolution_get(cases[i].r);
		Test_assert(t, "getW", I32x2_x(v) == cases[i].w);
		Test_assert(t, "getH", I32x2_y(v) == cases[i].h);

		EResolution back = EResolution_create(v);
		Test_assert(t, "roundtrip", back == cases[i].r);
	}

	//Undefined round-trips to Undefined
	I32x2 zero = I32x2_zero;

	//0x0 is not a named resolution but create should not crash
	//(it may return Undefined or EResolution_create(0,0), just must not crash)
	(void) EResolution_create(zero);

	//Out-of-range (> U16_MAX) must return Undefined
	I32x2 huge = I32x2_create2(70000, 70000);
	Test_assert(t, "oobUndefined", EResolution_create(huge) == EResolution_Undefined);
}

// -- 8. Window null-guard + terminate edge cases ------------------------------

static void Test_windowNullguards(Test *t) {

	Test_setModule(t, "Window/NullGuards");

	//All helpers must return false/0 on NULL without crashing
	Test_assert(t, "isMinNull",         !Window_isMinimized(NULL));
	Test_assert(t, "isFocNull",         !Window_isFocussed(NULL));
	Test_assert(t, "isFSNull",          !Window_isFullScreen(NULL));
	Test_assert(t, "allowFSNull",       !Window_doesAllowFullScreen(NULL));
	Test_assert(t, "terminateNull",     !Window_terminate(NULL));

	//WindowManager_isAccessible on NULL
	Test_assert(t, "mgrNullFalse",      !WindowManager_isAccessible(NULL));

	//InputDevice helpers on NULL
	Test_assert(t, "idHandlesNull",     InputDevice_getHandles(NULL) == 0);
	Test_assert(t, "idIsAxisNull",      !InputDevice_isAxis(NULL, 0));
	Test_assert(t, "idIsButtonNull",    !InputDevice_isButton(NULL, 0));
	Test_assert(t, "idGetStateNull",    InputDevice_getState(NULL, 0) == EInputState_Up);
	Test_assert(t, "idGetAxisNull",     InputDevice_getCurrentAxis(NULL, 0) == 0.f);
	Test_assert(t, "idGetPrevAxisNull", InputDevice_getPreviousAxis(NULL, 0) == 0.f);
	Test_assert(t, "idDeadZoneNull",    InputDevice_getDeadZone(NULL, 0) == 0.f);
	Test_assert(t, "idHasFlagNull",     !InputDevice_hasFlag(NULL, 0));
	Test_assert(t, "idSetFlagNull",     !InputDevice_setFlag(NULL, 0));
	Test_assert(t, "idResetFlagNull",   !InputDevice_resetFlag(NULL, 0));
}

// -- 8. Platform allocator alignment ------------------------------------------

//OxC3's widest type, I32x4, is alignas(16) on every SIMD backend, so anything the platform allocator
//returns has to be able to hold one. malloc only promises alignof(max_align_t), which is 16 on the
//x64/arm64 ABIs but 8 on wasm: web therefore has to align explicitly (see platforms/unix/uplatform.c).
//Without this, every heap I32x4 is under-aligned and AES-GCM rejects its own target/AAD buffers.

static void Test_allocatorAlignment(Test *t) {

	Test_setModule(t, "Platform/Allocator");

	//Sizes that aren't multiples of 16 are the ones that expose a passthrough malloc

	static const U64 sizes[] = { 1, 3, 8, 13, 17, 24, 31, 33, 64, 100, 129, 1000 };

	Bool allAligned = true;
	Bool allAllocated = true;

	for(U64 i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {

		Buffer buf = Buffer_createNull();

		if(!Buffer_createUninitializedBytes(sizes[i], t->alloc, &buf, NULL)) {
			allAllocated = false;
			continue;
		}

		if((U64)(void*) buf.ptr & 15)
			allAligned = false;

		Buffer_free(&buf, t->alloc);
	}

	Test_assert(t, "allocSucceeds", allAllocated);
	Test_assert(t, "alignedTo16", allAligned);
}

// -- entry point ---------------------------------------------------------------

OXC3_TEST_ENTRY(platforms_interface) {

	Error err = Error_none();
	if (!Platform_create(Platform_argc, Platform_argv, Platform_getData(), NULL, true, &err)) {
		Test_printPlatformCreateFail(&err);        //Can't even set up the platform, hard fail (but say why first)
		Platform_return(1);
	}

	Test t = (Test) { .alloc = Platform_instance->alloc };

	U64 allocsBefore = Platform_getActiveAllocations(0);

	Test_platformsWindowManager(&t);
	Test_platformsWindowVirtual(&t);

	Test_platformsKeyboard(&t);
	Test_platformsMouse(&t);
	Test_keySerialization(&t);
	Test_mouseExtras(&t);

	Test_platformsFilePhysical(&t);
	Test_fileHandle(&t);
	Test_fileLongPath(&t);
	Test_fileEdgeCases(&t);
	Test_fileCaseAndUtf8(&t);
	Test_fileRepeatedOpenClose(&t);

	Test_platformsFileVirtual(&t);
	Test_virtualForeachRoundTrip(&t);
	Test_virtualEdgeCases(&t);

	Test_dynamicLibrary(&t);

	Test_resolution(&t);
	Test_windowNullguards(&t);
	Test_allocatorAlignment(&t);

	//We might have instantiated a list with some capacity, make sure we get rid of it so the counter doesn't false positive.

	for(U64 i = 0; i < Platform_instance->archives.length; ++i)
		CAFile_free(&Platform_instance->archives.ptrNonConst[i], Platform_instance->alloc);

	ListCAFile_free(&Platform_instance->archives, Platform_instance->alloc);

	U64 allocsAfter = Platform_getActiveAllocations(0);

	Test_setModule(&t, NULL);
	Test_assert(&t, "NoLeaks", allocsAfter <= allocsBefore);

	int result = Test_end(&t);
	Platform_cleanup();
	Platform_return(result);
}

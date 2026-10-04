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

//platforms/test/interface/test_platforms_hpp.cpp
//
//Type check for the C++ platforms layer.
//platforms/file.hpp and platforms/window.hpp are hand written and,
// until this file existed, no translation unit included either of them, so nothing had ever compiled them.
//This TU exists so both headers are built by every configuration the tests are,
// which is what stops them drifting from the C API they wrap.
//
//The type check below is deliberately never CALLED.
//It names each wrapper so the compiler has to instantiate and typecheck it against the current C headers;
// running it would touch the filesystem and open a real window, which the modules that do run already cover.
//A link time reference is enough to keep it honest.
//
//file.hpp is included first on purpose:
// window.hpp pulls it in itself, so listing it second would never prove file.hpp closes over its own dependencies.

#include "platforms/file.hpp"
#include "platforms/window.hpp"
#include "platforms/platform.hpp"

//The C test framework carries an extern "C" guard, so it is included inside oxc::c after the wrappers, as
// test_types_container_hpp.cpp does.

namespace oxc { namespace c {
	#include "types/test/test.h"
}}

//File_foreach's callback type is declared inside extern "C",
// so the callback handed to it is defined with C linkage rather than written as a captureless lambda.
//A lambda would convert in practice, but its function type carries C++ language linkage,
// which -fsanitize=function reads as a call through an incorrect function type,
// the same reason test_types_container_hpp.cpp keeps its comparator in C.

extern "C" {

	static oxc::c::Bool Test_platformsHppOnFile(
		const oxc::c::FileInfo*, void*, const oxc::c::Allocator*, oxc::c::Error*
	) {
		return true;
	}
}

//Member function form of the window callbacks, handling every callback the wrapper knows.
//That is what forces detail::Bind<T> to instantiate all nine thunks.

struct TypeCheckApp {

	oxc::c::U64 frames;

	oxc::c::Bool onCreate(oxc::win::Window, oxc::c::Error*) { return true; }
	oxc::c::Bool onResize(oxc::win::Window, oxc::c::Error*) { return true; }

	void onDestroy(oxc::win::Window) {}
	void onDraw(oxc::win::Window) { ++frames; }
	void onUpdate(oxc::win::Window, oxc::c::F64) {}
	void onCursorMove(oxc::win::Window) {}

	void onButton(oxc::win::Window, oxc::c::InputDevice*, oxc::c::InputHandle, oxc::c::Bool) {}
	void onAxis(oxc::win::Window, oxc::c::InputDevice*, oxc::c::InputHandle, oxc::c::F32) {}
	void onTypeChar(oxc::win::Window, oxc::c::CharString) {}
};

//An app that handles a single callback.
//The other eight detections have to compile to false rather than to an error,
// which is the half of the `if constexpr(requires ...)` chain the app above cannot reach.

struct TypeCheckMinimalApp {
	void onDraw(oxc::win::Window) {}
};

//Never invoked.
//See the file comment: this is a compile time check, not a test module.

extern "C" void Test_platformsHppTypeCheck(const oxc::c::Allocator *alloc, const oxc::c::C8 *path) {

	using namespace oxc;

	c::Error *e_rr = nullptr;

	//---------------------------------------------------------------- platforms/file.hpp

	//One Types owns the RefPtrTypes that every read, handle and stream below is made through,
	// and the Buffer receiving a read has to be built on the same allocator.

	const file::Types types(alloc);
	const StringView loc(path);

	Buffer data(*alloc);

	//Virtual sections, existence and metadata.

	(void) file::loadVirtual(loc, types, alloc, e_rr);
	(void) file::isVirtualLoaded(loc, alloc, e_rr);
	(void) file::has(loc, alloc);

	c::FileInfo info{};
	(void) file::getInfo(loc, info, alloc, e_rr);

	//Whole file IO, including both write overloads (owning Buffer and borrowed c::Buffer).

	(void) file::read(loc, types, data, 0, 0, c::U64_MAX, e_rr);
	(void) file::write(data, loc, types, 0, 0, c::U64_MAX, true, e_rr);
	(void) file::write(data.handle(), loc, types, 0, 0, c::U64_MAX, true, e_rr);

	//Directory queries.
	//The callback is the C typed one defined above.

	c::U64 fileCount = 0;
	(void) file::count(loc, c::EFileType_File, true, fileCount, alloc, e_rr);
	(void) file::countAll(loc, true, fileCount, alloc, e_rr);
	(void) file::foreach(loc, Test_platformsHppOnFile, nullptr, true, alloc, e_rr, false);

	//Mutating the filesystem.
	//rename takes a NAME, move takes the destination directory.

	(void) file::add(loc, c::EFileType_Folder, alloc, e_rr, false);
	(void) file::rename(loc, "renamed.txt", alloc, e_rr, c::U64_MAX);
	(void) file::move(loc, "destination", alloc, e_rr, c::U64_MAX);
	(void) file::remove(loc, alloc, e_rr, c::U64_MAX);
	(void) file::unloadVirtual(loc, alloc, e_rr);

	//Handles: the factory, both IO directions and every accessor.

	FileHandle handle;
	(void) FileHandle::open(loc, types, handle, c::EFileOpenType_ReadWrite, true, e_rr, c::U64_MAX);
	(void) handle.read(data, 0, 0, e_rr);
	(void) handle.write(data.handle(), 0, 0, e_rr);
	(void) handle.valid();
	(void) (bool) handle;
	(void) handle.handle();
	(void) handle.data();

	//Streams: the two factories, the second of which takes the handle over, and every accessor.

	FileStream stream;
	(void) FileStream::open(loc, types, stream, c::EFileOpenType_Read, false, e_rr, c::U64_MAX);
	(void) FileStream::adopt(handle, types, stream, e_rr);
	(void) stream.valid();
	(void) (bool) stream;
	(void) stream.handle();
	(void) stream.data();

	stream.release();
	handle.release();

	//---------------------------------------------------------------- platforms/window.hpp

	//Manager: both create forms, the userData slot behind the Registry, and the raw handle.

	win::Manager manager;
	(void) manager.create(e_rr);

	c::WindowManagerCallbacks managerCallbacks{};
	(void) manager.create(managerCallbacks, sizeof(c::U64), e_rr);
	(void) manager.data<c::U64>();
	(void) manager.handle();

	//Plain function form.
	//Captureless lambdas convert to the table's function pointers.

	win::Callbacks callbacks{};
	callbacks.onCreate = [](win::Window, c::Error*) -> c::Bool { return true; };
	callbacks.onResize = [](win::Window, c::Error*) -> c::Bool { return true; };
	callbacks.onDestroy = [](win::Window) {};
	callbacks.onDraw = [](win::Window) {};
	callbacks.onUpdate = [](win::Window, c::F64) {};
	callbacks.onCursorMove = [](win::Window) {};
	callbacks.onButton = [](win::Window, c::InputDevice*, c::InputHandle, c::Bool) {};
	callbacks.onAxis = [](win::Window, c::InputDevice*, c::InputHandle, c::F32) {};
	callbacks.onTypeChar = [](win::Window, c::CharString) {};

	const c::I32x2 size = c::I32x2_create2(1280, 720);

	(void) manager.createWindow(callbacks, size, "type check", sizeof(c::U32), e_rr);

	//Member function form, once with every callback and once with a single one.
	//The second one also names every remaining default argument.

	TypeCheckApp app{};
	TypeCheckMinimalApp minimal{};

	(void) manager.createWindow(app, size, "type check members", sizeof(c::U32), e_rr);

	(void) manager.createWindow(
		minimal, size, "type check minimal", 0, e_rr,
		c::EWindowType_Virtual, c::EWindowFormat_RGBA8, c::EWindowHint_None,
		c::I32x2_zero, c::I32x2_zero, c::I32x2_zero
	);

	//The dispatch state the manager keeps in its extendedData.
	//The wrapper fills this itself, but the types are public, so they get named here too.

	win::Registry registry{};
	registry.pending = win::Registry::maxWindows;

	win::Registry::Slot &slot = registry.slots[0];
	slot.plain = callbacks;

	const win::Table table = win::detail::tableFor(&slot.plain);
	slot.table = table;

	//Window view: every accessor, plus the per window state slot sized by createWindow's userData.

	win::Window window = manager.window(0);
	(void) (bool) window;
	(void) window.handle();
	(void) window.size();
	(void) window.width();
	(void) window.height();
	(void) window.cursor();
	(void) window.flags();
	(void) window.isFullScreen();
	(void) window.isMinimized();
	(void) window.isFocussed();
	(void) window.isDrawable();
	(void) window.toggleFullScreen(e_rr);
	(void) window.setTitle("type check", e_rr);
	(void) window.terminate();
	(void) window.data<c::U32>();

	//Input decodes.

	const c::InputDevice *device = nullptr;
	(void) win::isKeyboard(device);
	(void) win::isMouse(device);
	(void) win::key(c::EKey_W);
	(void) win::mouse(c::EMouseButton_Left);

	//Pumping events and teardown.

	(void) manager.wait(e_rr);
	manager.release();
}

//The environment layer is the exception: reading variables touches nothing, so it is run, not just compiled.
//Variables are set through the C suite's Test_setEnv, which writes them the way each OS stores them.

extern "C" oxc::c::Bool Test_setEnv(const oxc::c::Test *t, const oxc::c::C8 *name, const oxc::c::C8 *value);

extern "C" void Test_platformsEnvHpp(oxc::c::Test *t) {

	using namespace oxc;

	const c::Allocator &alloc = *t->alloc;
	const c::C8 *name = "OXC3_TEST_ENV_HPP";

	Test_setModule(t, "Environment (C++)");

	//get and has.

	String str(alloc);
	Test_setEnv(t, name, NULL);
	Test_assert(t, "unset gets nothing", env::get(name, str, alloc) && !str.handle().ptr && !env::has(name, alloc));

	Test_setEnv(t, name, "hello");
	Test_assert(
		t, "get",
		env::get(name, str, alloc) && str.handle().ptr && c::CharString_equalsCStringSensitive(&str.handle(), "hello")
	);

	Test_assert(t, "has", env::has(name, alloc));

	//readOr: the value, the default when unset, the default when it doesn't fit the type.

	struct Case { const c::C8 *value; c::Bool ok; };

	Test_setEnv(t, name, NULL);
	Test_assert(t, "unset reads the default", env::readOr<c::U32>(name, 7, alloc) == 7);

	Test_setEnv(t, name, "42");
	Test_assert(t, "u32", env::readOr<c::U32>(name, 7, alloc) == 42);

	Test_setEnv(t, name, "0x10");
	Test_assert(t, "u32 hex", env::readOr<c::U32>(name, 7, alloc) == 16);

	Test_setEnv(t, name, "4294967296");
	Test_assert(t, "u32 out of range", env::readOr<c::U32>(name, 7, alloc) == 7);

	Test_setEnv(t, name, "256");
	Test_assert(t, "u8 out of range", env::readOr<c::U8>(name, 7, alloc) == 7);

	Test_setEnv(t, name, "-1");
	Test_assert(t, "unsigned refuses a sign", env::readOr<c::U32>(name, 7, alloc) == 7);
	Test_assert(t, "i32", env::readOr<c::I32>(name, 7, alloc) == -1);

	Test_setEnv(t, name, "-3000000000");
	Test_assert(t, "i32 out of range", env::readOr<c::I32>(name, 7, alloc) == 7);
	Test_assert(t, "i64", env::readOr<c::I64>(name, 7, alloc) == -3000000000ll);

	Test_setEnv(t, name, "1.5");
	Test_assert(t, "f32", env::readOr<c::F32>(name, 7, alloc) == 1.5f);

	Test_setEnv(t, name, "1e39");
	Test_assert(t, "f32 out of range", env::readOr<c::F32>(name, 7, alloc) == 7);
	Test_assert(t, "f64", env::readOr<c::F64>(name, 7, alloc) == 1e39);

	Test_setEnv(t, name, "true");
	Test_assert(t, "bool", env::readOr<bool>(name, false, alloc));

	//read: a malformed value fails through e_rr and leaves the value alone.

	Test_setEnv(t, name, "12a");
	c::U32 u = 7;
	c::Error err = c::Error_none();
	Test_assert(t, "read refuses", !env::read(name, u, alloc, &err) && u == 7 && err.genericError);

	//Lists: exactly count values, or nothing written.

	c::F32 v[3] = { 9, 9, 9 };

	Test_setEnv(t, name, NULL);
	Test_assert(t, "unset list", env::readList(name, v, 3, alloc) && v[0] == 9 && !env::readListOr(name, v, 3, alloc));

	Test_setEnv(t, name, "1,-2.5,3e2");
	Test_assert(
		t, "list", env::readListOr(name, v, 3, alloc) && v[0] == 1 && v[1] == -2.5f && v[2] == 300
	);

	static const Case lists[] = { { "1,2", false }, { "1,2,3,4", false }, { "1,,3", false }, { "1,2,x", false } };
	c::Bool allRefused = true;

	for(const Case &l : lists) {
		c::F32 w[3] = { 9, 9, 9 };
		Test_setEnv(t, name, l.value);
		allRefused &= !env::readList(name, w, 3, alloc) && w[0] == 9 && w[1] == 9 && w[2] == 9;
	}

	Test_assert(t, "malformed lists refused whole", allRefused);

	Test_setEnv(t, name, NULL);
}

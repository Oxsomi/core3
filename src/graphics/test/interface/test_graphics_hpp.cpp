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

//graphics/test/interface/test_graphics_hpp.cpp
//
//Type check for the C++ graphics layer.
//graphics/graphics.hpp is hand written and, until this file existed, no translation unit included it, so
//nothing had ever compiled it. This TU exists so the header is built by every configuration the tests are,
//which is what stops it drifting from the C API it wraps.
//
//The body below is deliberately never CALLED. It names each wrapper so the compiler has to instantiate and
//typecheck it against the current C headers; running it would need a real device, which the modules that do
//run already provide. A link time reference is enough to keep it honest.

#include "graphics/graphics.hpp"

namespace oxc { namespace c {
	#include "test_graphics_shared.h"
}}

//A Bool directly before e_rr has to refuse the Error* that lands in it when the Bool is left out.
//Each pair names one such wrapper: the bool form has to compile, so the Error* form failing is down to BoolArg
// and not to the call being malformed in some other way.
//A requires expression only turns an ill-formed call into false inside a template, hence the parameter.

namespace {

	using namespace oxc;
	using namespace oxc::gfx;

	template<typename T>
	constexpr bool TABLE_SET = requires(DescriptorTable t, const c::Descriptor &d, T v) { t.set(0, 0, d, v); };

	template<typename T>
	constexpr bool TABLE_ALLOC = requires(DescriptorTable t, c::U64 &id, const c::Descriptor &d, T v) {
		t.alloc(0, id, d, v);
	};

	template<typename T>
	constexpr bool LIST_BEGIN = requires(CommandList l, T v) { l.begin(v); };

	template<typename T>
	constexpr bool CREATE_LIST = requires(Device d, CommandList &l, T v) { d.createCommandList(1, 1, 1, l, v); };

	template<typename T>
	constexpr bool CREATE_TLAS = requires(Device d, const c::TLASInstance *i, Tlas &t, T v) {
		d.createTlas(c::ERTASBuildFlags_DefaultTLAS, i, 1, "tlas", t, v);
	};

	template<typename T>
	constexpr bool CREATE_BITS = requires(Buffer b, T v) { b.createBits(8, v); };

	static_assert(TABLE_SET<bool> && !TABLE_SET<c::Error*>);
	static_assert(TABLE_ALLOC<bool> && !TABLE_ALLOC<c::Error*>);
	static_assert(LIST_BEGIN<bool> && !LIST_BEGIN<c::Error*>);
	static_assert(CREATE_LIST<bool> && !CREATE_LIST<c::Error*>);
	static_assert(CREATE_TLAS<bool> && !CREATE_TLAS<c::Error*>);
	static_assert(CREATE_BITS<bool> && !CREATE_BITS<c::Error*>);
}

//Never invoked. See the file comment: this is a compile time check, not a test module.

extern "C" void Test_graphicsHppTypeCheck(oxc::c::GraphicsDeviceRef *deviceRef, const oxc::c::SHFile *shFile) {

	using namespace oxc;
	using namespace oxc::gfx;

	c::Error *e_rr = nullptr;

	//Borrowing rather than adopting: the harness owns this ref and decs it itself.

	Device dev = Device::share(deviceRef);

	//Reflection derived layouts, including the push constant and push descriptor splits.

	c::DescriptorLayoutInfo layoutInfo{};
	c::DescriptorLayoutInfo pushInfo{};
	c::DescriptorBinding pushConstants{};

	(void) dev.detectLayout(*shFile, 0, layoutInfo, "constants", &pushConstants, { "params", "output" }, &pushInfo);

	DescriptorLayout layout;
	DescriptorLayout pushLayout;
	(void) dev.createDescriptorLayout(layoutInfo, "layout", layout, e_rr);
	(void) dev.createDescriptorLayout(pushInfo, "pushLayout", pushLayout, e_rr);

	//Heap, table and the two ways of filling one.

	const c::DescriptorHeapInfo heapInfo{};
	DescriptorHeap heap;
	(void) dev.createDescriptorHeap(heapInfo, "heap", heap, e_rr);

	DescriptorTable table;
	(void) heap.createTable(layout, "table", table, (c::EDescriptorTableFlags) 0, e_rr);

	const c::Descriptor descriptor{};
	(void) table.set(0, 0, descriptor, false, e_rr);
	(void) table.setByName("output", descriptor, 0, false, e_rr);
	(void) table.setRange(0, 0, { descriptor, descriptor }, false, e_rr);
	(void) table.setRangeByName("output", { descriptor, descriptor }, 0, false, e_rr);
	(void) table.unset(0, 0, 1, e_rr);
	(void) table.unsetByName("output", 0, 1, e_rr);

	//The allocation half, which picks the slot instead of taking one.

	c::U64 arrayId = 0;
	c::U16 bindId = 0;
	c::U8 bindlessTypeId = 0;

	(void) table.alloc(0, arrayId, descriptor, false, e_rr);
	(void) table.allocByName("output", arrayId, descriptor, false, e_rr);
	(void) table.allocBindless(
		c::EGfxRegisterType_ByteAddressBuffer, 0, bindId, bindlessTypeId, arrayId, descriptor, false, e_rr
	);

	(void) table.findBindlessRegister(
		c::EGfxRegisterType_ByteAddressBuffer, 0, bindId, bindlessTypeId, nullptr, 0, e_rr
	);

	(void) table.resolveRegisterName("output");

	//Pipeline layout carrying all three kinds at once.

	c::PipelineLayoutInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.bindings = layout.handle();
	pipelineLayoutInfo.pushDescriptors = pushLayout.handle();
	pipelineLayoutInfo.pushConstants = pushConstants;

	PipelineLayout pipelineLayout;
	(void) dev.createPipelineLayout(pipelineLayoutInfo, "pipelineLayout", pipelineLayout, e_rr);

	//Scope local bindful state.

	CommandList list;
	(void) dev.createCommandList(2 * c::KIBI, 32, 16, list, true, e_rr);

	struct PushData { c::U32 scale, bias, xorMask, offset; };
	const PushData constants{};

	{
		CommandScope scope(list.handle());

		(void) scope.bindDescriptorHeap(heap, e_rr);
		(void) scope.bindDescriptorTable(table, e_rr);
		(void) scope.setPushConstants(constants, e_rr);
		(void) scope.setPushDescriptors({ descriptor, descriptor }, e_rr);
		(void) scope.dispatch1D(1, e_rr);
	}
}

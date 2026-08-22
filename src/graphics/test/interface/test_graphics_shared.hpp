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

//graphics/test/interface/test_graphics_shared.hpp
//
//The shared test helpers in terms of the C++ layer's handle types, so a module written against
//graphics/graphics.hpp never has to reach for a raw *Ref pointer to call one.
//
//test_graphics_shared.h stays the C declaration of the same helpers, because the module ENTRY POINTS are C
//callable: the suite dispatches them by name. A module that has not been converted yet keeps calling the C
//forms, which forward to these, so the two can coexist while the conversion proceeds file by file.

#pragma once

#include "graphics/graphics.hpp"

namespace oxc { namespace c {
	#include "test_graphics_shared.h"
	#include "platforms/platform.h"
	#include "graphics/generic/device.h"
	#include "graphics/generic/instance.h"
	#include "graphics/generic/device_buffer.h"

	//RefPtr_ptr and everything built on it are macros, so they cannot be reached through a namespace
	//qualifier at all: c::GraphicsDeviceRef_ptr(x) expands to c::((GraphicsDevice*) ...).
	//The handle classes' own data() covers everything a converted module OWNS; these are for the few things
	//a module inspects without owning, which is the device the harness handed it, its instance, and the
	//device's own staging buffer.

	static inline GraphicsDevice *deviceOf(const RefPtr *ref) {
		return GraphicsDeviceRef_ptr((GraphicsDeviceRef*) ref);
	}

	static inline GraphicsInstance *instanceOf(const RefPtr *ref) {
		return GraphicsInstanceRef_ptr((GraphicsInstanceRef*) ref);
	}

	static inline DeviceBuffer *bufferOf(const RefPtr *ref) {
		return DeviceBufferRef_ptr((DeviceBufferRef*) ref);
	}
}}

namespace oxc { namespace gfxtest {

	//graphics.hpp already has the guard these were: OwnedList frees its list on every exit path, error
	//returns included, which is what lets a converted module drop its clean label entirely.
	//An SHFile and a DescriptorLayoutInfo are plain C structs with no handle of their own.

	using OwnedSHFile = gfx::OwnedList<c::SHFile, c::SHFile_free>;
	using OwnedLayoutInfo = gfx::OwnedList<c::DescriptorLayoutInfo, c::DescriptorLayoutInfo_free>;

	//A scope with a render pass already open in it, which is what the draw modules record into.
	//`render` is declared after `scope` so it destroys FIRST, which is the order the C API requires: a
	//render pass ends inside the scope that opened it.
	//A helper cannot hand these back any other way: both must outlive the call, and it cannot return a
	//plain bool either, since the scope has to stay open for the caller's draws.

	struct DrawPass {

		gfx::CommandScope scope;
		gfx::CommandRender render;
		c::Bool ok;

		[[nodiscard]] explicit operator c::Bool() const noexcept { return ok; }
	};

	//Loads one compiled oiSH out of the gtest section; false means the build had no shader compiler.

	[[nodiscard]] c::Bool loadFile(c::Test *t, const c::C8 *path, c::SHFile &file) noexcept;

	//Find one entry and record whether it was there, which is what every module does with the id.

	[[nodiscard]] c::U32 entry(c::Test *t, gfx::Device &dev, const c::SHFile &file, const c::C8 *name) noexcept;

	//A pipeline layout keeping the device's own bindless set and per frame globals, plus whatever push
	//constants the entry declares.

	[[nodiscard]] c::Bool pushConstantLayout(
		c::Test *t, gfx::Device &dev, const c::SHFile &file, c::U32 entryId, gfx::PipelineLayout &layout
	) noexcept;

	//"main" compute pipeline on the device's default layout.

	[[nodiscard]] c::Bool computePipeline(
		c::Test *t, gfx::Device &dev, const c::SHFile &file, gfx::Pipeline &pipeline
	) noexcept;

	//As above, but the shader reads push constants, so it gets a layout that declares them and hands it
	//back: the layout has to outlive the pipeline, so the caller holds it.

	[[nodiscard]] c::Bool computePipelinePush(
		c::Test *t, gfx::Device &dev, const c::SHFile &file, gfx::Pipeline &pipeline, gfx::PipelineLayout &layout
	) noexcept;

	[[nodiscard]] c::Bool submitAndWait(c::Test *t, gfx::Device &dev, const gfx::CommandList &commandList) noexcept;

	[[nodiscard]] c::Bool pullBuffer(
		c::Test *t, gfx::Device &dev, const gfx::CommandList &emptyList, const gfx::DeviceBuffer &buffer
	) noexcept;

	//Any of the three texture kinds; they are one type to the C API, so the handle crosses as the plain
	//RefPtr* that CommandScope::copyImage takes too.

	[[nodiscard]] c::Bool pullPixels(
		c::Test *t, gfx::Device &dev, const gfx::CommandList &emptyList, c::RefPtr *target,
		c::TestShaderPixels &pixels
	) noexcept;

	[[nodiscard]] c::Bool checkPixels(
		c::Test *t, gfx::Device &dev, const gfx::CommandList &emptyList, c::RefPtr *target, c::U32 expected
	) noexcept;

	//D3D12's GPU based validation instruments raytracing libs into invalid bytecode, so a module that
	//executes raytracing pipelines trades the suite's device for a dedicated one with only GPU based
	//validation off. Scoped: the swap lasts as long as the object, and its end checks that instance's own
	//counters.
	//
	//`dev` is rebound to the dedicated device when one was made, so the module keeps using the same object
	//either way. That rebind is why a caller taking the device by reference has to hand this a COPY it owns:
	//a swap that outlives the call leaves the next caller pointing at a released device.
	//Falsy means the workaround was needed but setting it up failed, and the module cannot run.

	class RtDedicatedDevice {

		c::Test *t;

		//The caller's handle, which the ctor rebound to the dedicated device and so holds a reference of its
		//own. It has to go back before the teardown below, or the dec in there drops 2->1 instead of
		//destroying the device: the validation counters would then be read while the device is still alive,
		//and its teardown messages would land after both asserts, on an instance about to be released.
		//It also keeps instanceType, which the instance's RefPtr points at, alive past the last reference.

		gfx::Device *bound = nullptr;

		c::GraphicsInstanceRef *ownInstance = nullptr;
		c::GraphicsDeviceRef *ownDevice = nullptr;
		c::RefPtrType instanceType{};
		c::Bool ok = false;

	public:

		RtDedicatedDevice(c::Test *test, gfx::Device &dev) noexcept;
		~RtDedicatedDevice() noexcept;

		RtDedicatedDevice(const RtDedicatedDevice&) = delete;
		RtDedicatedDevice &operator=(const RtDedicatedDevice&) = delete;

		[[nodiscard]] explicit operator c::Bool() const noexcept { return ok; }
	};
}}

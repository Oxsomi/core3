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

//graphics/graphics.hpp
//
//C++ layer over the generic graphics API.
//NOT generator territory:
// every graphics object is a *Ref_ handle (typedef RefPtr XRef),
// the exact shape src/tools/generate_hpp.py measures at ~43% of real consumer symbols and refuses ("needs
// oxc::RefPtr<T>, by hand").
//The enum/struct-only graphics headers need only inclusion into oxc::c, which happens here too,
// so nothing graphics goes in generate_hpp.json.
//
//Shape of the layer:
// - Every handle class is a thin, COPYABLE wrapper over oxc::RefPtr<c::X> (copying a refcounted
//   handle is what refcounts are for). Factories live on Device and adopt() the C factory result.
// - Command recording is SCOPED, mirroring the C API's own structure 1:1:
//
//       CommandScope scope = commandList.scope({ transitions }, id, { deps }, e_rr);
//       if(!scope)                        //refused: the C side assigned InvalidState, so this
//           return false;                 //recording is dead, report e_rr and abandon it
//       scope.setComputePipeline(p);
//       scope.dispatch2D(x, y);
//                                         //endScope runs on destruction
//
//   CommandRender nests inside a scope the same way for startRenderExt/endRenderExt.
// - Fallible calls follow the house idiom: [[nodiscard]] c::Bool + trailing c::Error *e_rr =
//   nullptr, except where an optional parameter was added later, which trails e_rr so existing
//   calls keep compiling. RAII destructors that must call an end*() swallow its error; call
//   end(e_rr) explicitly when it matters.
//
//NOT wrapped:
// the explicit-binding descriptor path (descriptor layouts/heaps/tables, pipeline layouts,
// bindless descriptor allocation), descriptor_table.h cannot enter a C++ namespace as it stands,
// see the include block below; MSAA attachments (both texture factories pin EMSAASamples_Off); render passes,
// which have no C creator either.

#pragma once

//Existing wrappers first:
// they pull their C headers into oxc::c with the right pre-include discipline (ref_ptr -> error/allocator/<atomic>,
// string -> buffer/CharString, vec4f -> SIMD intrinsics at global scope,
// texture_format -> the ETextureFormat enums that device_info.h forward-declares with `typedef enum X X;`,
// which is ill-formed C++ unless already defined).

#include "types/container/ref_ptr.hpp"
#include "types/container/string.hpp"
#include "types/container/buffer.hpp"
#include "types/container/texture_format.hpp"
#include "types/math/vec4f.hpp"

#include <stddef.h>            //swapchain.h / device_texture.h use it; must not first appear in a namespace
#include <initializer_list>

namespace oxc {

	//Include order is IMPORTANT:
	// C11 forward declarations (`typedef enum E E;`) are only legal C++ once the enum is DEFINED,
	// so every defining header must precede its forwarders,
	// sh_binaries (ESHBinaryType/ESHExtension) before device/pipeline, pipeline_structs (EMSAASamples) before texture,
	// resource (EResourceType) before device's allocator.
	//EWindowFormat's definition lives in the platforms layer, which this header does not pull in,
	// so it gets a C++ opaque declaration; layout-compatible with the C enum (int),
	// and the C definition never enters this TU.
	//(The platforms layer IS namespace-able, overturning the generator doc's stale closure claim,
	// but window.h needs one enum-cast fix and definitions-first ordering before a platforms .hpp exists;
	// until then this stays.)

	namespace c {
		#include "formats/oiSH/sh_file.h"
		#include "graphics/generic/pipeline_structs.h"
		#include "graphics/generic/device_info.h"
		#include "graphics/generic/instance.h"
		#include "graphics/generic/resource.h"
		#include "graphics/generic/device.h"
		#include "graphics/generic/device_buffer.h"
		#include "graphics/generic/texture.h"
		#include "graphics/generic/render_texture.h"

		//swapchain.h forward-declares EWindowFormat and takes a Window*, so the definition must be in scope.
		//A C++ opaque `enum EWindowFormat :
		// int;` cannot serve, an opaque declaration has to fix the underlying type and the C definition does not,
		// which is a hard mismatch.
		//The platforms layer namespaces cleanly now, so include the real header;
		// graphics already depends on it at the C level.

		#include "platforms/window.h"

		#include "graphics/generic/swapchain.h"
		#include "graphics/generic/pipeline.h"
		#include "graphics/generic/command_list.h"
		#include "graphics/generic/commands.h"

		//Bindful descriptors. Explicit rather than leaning on what command_list.h drags in, since the C++
		//classes below name every one of these types directly.

		#include "graphics/generic/descriptor_layout.h"
		#include "graphics/generic/descriptor_heap.h"
		#include "graphics/generic/descriptor_table.h"
		#include "graphics/generic/bindless_descriptor.h"
		#include "graphics/generic/pipeline_layout.h"
		#include "graphics/generic/blas.h"
		#include "graphics/generic/tlas.h"

		//opacity_micromap.h closes over acceleration_structure.h + device_buffer.h, both already in;
		// its re-typedef of OpacityMicromapRef after commands.h is a legal identical typedef.

		#include "graphics/generic/opacity_micromap.h"

		//device_texture.h leans on the global <stddef.h> pre-include (offsetof in a static_assert). depth_stencil.h
		// forwards EMSAASamples (pipeline_structs.h) and EDepthStencilFormat (texture_format.hpp),
		// both defined above. sampler.h only adds types/math/flp.h.

		#include "graphics/generic/device_texture.h"
		#include "graphics/generic/depth_stencil.h"
		#include "graphics/generic/sampler.h"

		//TODO:
		// NOT included:
		// descriptor_layout/heap/table, pipeline_layout, bindless_descriptor,
		// the explicit-binding path. descriptor_table.h cannot enter a C++ namespace as it stands:
		// TList(DescriptorTableBinding) expands _at/_atConst returning the element BY VALUE,
		// and DescriptorTableBinding embeds a SpinLock whose AtomicI64 is a std::atomic in C++, copy ctor deleted,
		// so those bodies are ill-formed even unused,
		// since they are concrete static inline functions rather than templates.
		//Upstream fix is one of:
		// guard _at/_atConst on std::is_copy_constructible in C++ mode,
		// make them templates so the body only instantiates when called,
		// or keep ListDescriptorTableBinding out of TList.
		//Bindless is the shipped path and needs none of this.
	}

	namespace gfx {

		//One-time backend load.
		//Must precede Instance::create.

		[[nodiscard]] inline c::Bool init(c::Error *e_rr = nullptr) noexcept {
			return c::GraphicsInterface_create(e_rr);
		}

		[[nodiscard]] inline c::CharString name(const c::C8 *s) noexcept {
			return c::CharString_createRefCStrConst(s);
		}

		//Device capability dump to the log; api only labels the output.

		inline void print(const c::GraphicsDeviceInfo &info, c::EGraphicsApi api, c::Bool capabilities = true) noexcept {
			c::GraphicsDeviceInfo_print(api, &info, capabilities);
		}

		//---------------------------------------------------------------- handles

		class Instance {

			RefPtr<c::GraphicsInstance> ref;
			c::RefPtrType type{};              //must outlive the instance (GraphicsInstance_create keeps the pointer)

		public:

			//Immovable on purpose:
			// create() hands the C API the ADDRESS of `type`,
			// and the instance's RefPtr keeps reading through it for its whole life.
			//Relocating the object (return by value, growing a vector) would leave the C side pointing at dead storage,
			// so the compiler forbids it rather than the caller discovering it at runtime.

			Instance() noexcept = default;
			Instance(const Instance&) = delete;
			Instance &operator=(const Instance&) = delete;
			Instance(Instance&&) = delete;
			Instance &operator=(Instance&&) = delete;

			//api = EGraphicsApi_Count picks the platform default.

			//alloc:
			// the platform allocator.
			//Include platforms/file.hpp to reach it as c::Platform_instance->alloc,
			// the platforms layer namespaces cleanly and that header already pulls platform.h into oxc::c;
			// do NOT include platform.h at global scope alongside this header,
			// since the two would then declare distinct types.
			//It must outlive the instance, as must this Instance object:
			// it owns the RefPtrType the C side keeps pointing at.

			[[nodiscard]] static c::Bool create(
				const c::GraphicsApplicationInfo &appInfo,
				c::EGraphicsApi api,
				c::EGraphicsInstanceFlags flags,
				const c::Allocator *alloc,
				Instance &result,
				c::Error *e_rr = nullptr
			) noexcept {

				result.release();
				result.type = c::GraphicsInstance_makeType(api, alloc);
				c::GraphicsInstanceRef *raw = nullptr;

				if(!c::GraphicsInstance_create(&appInfo, api, flags, alloc, &result.type, &raw, e_rr))
					return false;

				result.ref = RefPtr<c::GraphicsInstance>::adopt(raw);
				return true;
			}

			//True if init() ran and the backend for this api was compiled in and loads.
			//Static:
			// it asks the interface, no instance required yet.

			[[nodiscard]] static c::Bool supportsApi(c::EGraphicsApi api) noexcept {
				return c::GraphicsInterface_supportsApi(api);
			}

			//Every device this instance can see. infos is allocated with the instance's allocator;
			// the CALLER owns it and frees it with c::ListGraphicsDeviceInfo_free(&infos, alloc),
			// the same alloc create() was given.
			//GraphicsDeviceInfo itself holds no heap data.

			[[nodiscard]] c::Bool getDeviceInfos(c::ListGraphicsDeviceInfo &infos, c::Error *e_rr = nullptr) const noexcept {
				return c::GraphicsInstance_getDeviceInfos(ref.data(), &infos, e_rr);
			}

			//Validation messages the api's debug layers reported so far (0 when validation is off).
			//Only real errors/warnings count,
			// so CI can hard fail on non-zero (see GraphicsInstance's validation counters).

			[[nodiscard]] c::U64 getValidationErrors() const noexcept {
				return c::GraphicsInstance_getValidationErrors(ref.data());
			}

			[[nodiscard]] c::U64 getValidationWarnings() const noexcept {
				return c::GraphicsInstance_getValidationWarnings(ref.data());
			}

			//The mask defaults keep the old "any device" behavior;
			// pass others to filter by EGraphicsVendorId / EGraphicsDeviceType bit.

			[[nodiscard]] c::Bool getPreferredDevice(
				const c::GraphicsDeviceCapabilities &required,
				c::GraphicsDeviceInfo &deviceInfo,
				c::U64 vendorMask = c::GraphicsInstance_vendorMaskAll,
				c::U64 deviceTypeMask = c::GraphicsInstance_deviceTypeAll,
				c::Error *e_rr = nullptr
			) const noexcept {
				return c::GraphicsInstance_getPreferredDevice(
					ref.data(), &required, vendorMask, deviceTypeMask, &deviceInfo, e_rr
				);
			}

			[[nodiscard]] c::Bool valid() const noexcept { return ref.valid(); }
			[[nodiscard]] explicit operator bool() const noexcept { return ref.valid(); }
			[[nodiscard]] c::RefPtr *handle() const noexcept { return ref.handle(); }

			//Reset in place, the object cannot be reassigned from a temporary (see above),
			// so this is how a slot is emptied or refilled. create() rewrites type on the way back in.
			//
			//type is deliberately LEFT INTACT:
			// the RefPtr stores &type by pointer and dereferences it for the free callback when its count reaches zero,
			// and a Device holds a reference of its own (device.c RefPtr_inc(instanceRef)).
			//Zeroing here would leave a device that outlives this call decrementing through a blanked type,
			// no free callback, and a Buffer freed against a null allocator.
			//For the same reason an Instance must OUTLIVE every Device made from it; releasing it early is safe,
			// destroying it early is not.

			void release() noexcept { ref.release(); }
		};

		//Thin common shape for every device-owned handle:
		// copyable, adoptable, raw-handle interop.

		template<typename T>
		class Handle {

		protected:

			RefPtr<T> ref;

		public:

			Handle() noexcept = default;
			explicit Handle(RefPtr<T> &&r) noexcept : ref(static_cast<RefPtr<T>&&>(r)) {}

			[[nodiscard]] c::Bool valid() const noexcept { return ref.valid(); }
			[[nodiscard]] explicit operator bool() const noexcept { return ref.valid(); }
			[[nodiscard]] c::RefPtr *handle() const noexcept { return ref.handle(); }
			[[nodiscard]] T *data() const noexcept { return ref.data(); }
			void release() noexcept { ref.release(); }
		};

		class DeviceBuffer : public Handle<c::DeviceBuffer> {
		public:
			using Handle::Handle;

			[[nodiscard]] c::U32 readHandle() const noexcept { return data()->readHandle; }
			[[nodiscard]] c::U32 writeHandle() const noexcept { return data()->writeHandle; }

			[[nodiscard]] c::DeviceData region(c::U64 offset = 0, c::U64 len = 0) const noexcept {
				return c::DeviceData{ handle(), offset, len };
			}

			[[nodiscard]] c::Bool markDirty(c::U64 offset = 0, c::U64 count = 0, c::Error *e_rr = nullptr) noexcept {
				return c::DeviceBufferRef_markDirty(handle(), offset, count, e_rr);
			}

			//Readback into cpuData (requires CPUBacked).
			//The copy records after the next submit's lists so it sees that frame's results;
			// callback fires once the frame finished on the device (see DeviceBufferRef_pullRegion),
			// stall with Device::wait when it's needed now. len 0 = rest of the buffer.
			//The callback is the raw C pointer (the ResourceCallback typedef) on purpose:
			// callbacks crossing the C boundary keep builtin-only signatures (the SortCallable rationale in list.hpp),
			// so per-call state travels through context, never a capture.

			[[nodiscard]] c::Bool pullRegion(
				c::U64 offset, c::U64 len, c::DevicePullCallback callback, void *context, c::Error *e_rr = nullptr
			) noexcept {
				return c::DeviceBufferRef_pullRegion(handle(), offset, len, callback, context, e_rr);
			}
		};

		//Everything a TextureRef answers regardless of which kind of texture is behind it.
		//RenderTexture, DeviceTexture and Swapchain are all one to the C API, so they inherit this rather than
		// each spelling the same six calls out, or callers reaching past the wrapper for them.
		//CRTP because a Handle knows its own C type: the mixin needs handle(), which only the derived class has.

		template<typename Self>
		struct TextureHandleOps {

			[[nodiscard]] c::RefPtr *textureRef() const noexcept {
				return static_cast<const Self*>(this)->handle();
			}

			[[nodiscard]] c::U32 currReadHandle(c::U32 subResource = 0) const noexcept {
				return c::TextureRef_getCurrReadHandle(textureRef(), subResource);
			}

			[[nodiscard]] c::U32 currWriteHandle(c::U32 subResource = 0) const noexcept {
				return c::TextureRef_getCurrWriteHandle(textureRef(), subResource);
			}

			//imageId picks the image within the subresource; the curr* pair above resolves it for you and is
			//what a caller normally wants.

			[[nodiscard]] c::U32 readHandle(c::U32 subResource = 0, c::U8 imageId = 0) const noexcept {
				return c::TextureRef_getReadHandle(textureRef(), subResource, imageId);
			}

			[[nodiscard]] c::U32 writeHandle(c::U32 subResource = 0, c::U8 imageId = 0) const noexcept {
				return c::TextureRef_getWriteHandle(textureRef(), subResource, imageId);
			}

			//Size and format can change at resize time (a Swapchain especially), so this goes through the
			// locked TextureRef_getUnifiedTexture (see ETextureRangeType), never the *Fast path.
			//Pass a version to learn whether a recorded commandList survived a resize (ETextureRangeType's
			// layer/level fields).

			[[nodiscard]] c::UnifiedTexture unifiedTexture(c::DeviceResourceVersion *version = nullptr) const noexcept {
				return c::TextureRef_getUnifiedTexture(textureRef(), version);
			}

			[[nodiscard]] c::Bool isTexture() const noexcept { return c::TextureRef_isTexture(textureRef()); }

			[[nodiscard]] c::Bool isDepthStencil() const noexcept {
				return c::TextureRef_isDepthStencil(textureRef());
			}

			[[nodiscard]] c::Bool isRenderTargetWritable() const noexcept {
				return c::TextureRef_isRenderTargetWritable(textureRef());
			}

			//As I32x2 so it slots straight into scope.render()/setViewportAndScissor.
			//Built by hand rather than through xy(): that is declared further down, and vec2i.h's helpers are
			// not in this closure (resource.h only pulls vec2.h).
			//Don't use on a Swapchain if you don't know the size yet.

			[[nodiscard]] c::I32x2 size() const noexcept {
				const c::UnifiedTexture tex = unifiedTexture();
				return c::I32x2{ { (c::I32) tex.width, (c::I32) tex.height } };
			}

			[[nodiscard]] c::ETextureFormatId format() const noexcept {
				return (c::ETextureFormatId) unifiedTexture().textureFormatId;
			}
		};

		class RenderTexture : public Handle<c::RenderTexture>, public TextureHandleOps<RenderTexture> {
		public:
			using Handle::Handle;

			//Readback without a cpuData (see TextureRef_pullRegion):
			// the region arrives through the callback as a tight row buffer owned by the runtime,
			// move it out of *data to keep it,
			// leftovers are freed right after the callback returns (the ResourceCallback contract).
			//Timing as DeviceBuffer::pullRegion; zero w/h/l = rest of that axis; MSAA resolves first.
			//The plane argument stays internal:
			// a render texture is single-planar, only 0 is legal.
			//Raw C callback on purpose, see DeviceBuffer::pullRegion.

			[[nodiscard]] c::Bool pullRegion(
				c::U16 x, c::U16 y, c::U16 z, c::U16 w, c::U16 h, c::U16 l,
				c::TexturePullCallback callback, void *context, c::Error *e_rr = nullptr
			) noexcept {
				return c::TextureRef_pullRegion(handle(), x, y, z, w, h, l, 0, callback, context, e_rr);
			}
		};

		class DeviceTexture : public Handle<c::DeviceTexture>, public TextureHandleOps<DeviceTexture> {
		public:
			using Handle::Handle;

			//Zero w/h/l = rest of that axis, so the default marks the whole texture.
			//Regions merge like DeviceBuffer::markDirty:
			// call as little as possible (see DeviceTextureRef_markDirty).

			[[nodiscard]] c::Bool markDirty(
				c::U16 x = 0, c::U16 y = 0, c::U16 z = 0,
				c::U16 w = 0, c::U16 h = 0, c::U16 l = 0,
				c::Error *e_rr = nullptr
			) noexcept {
				return c::DeviceTextureRef_markDirty(handle(), x, y, z, w, h, l, e_rr);
			}

			//Readback into cpuData (requires CPUBacked, see DeviceTextureRef_pullRegion).
			//Timing and callback discipline as DeviceBuffer::pullRegion; region semantics as markDirty,
			// and compressed formats snap outward to whole blocks so slightly more than asked can be refreshed.

			[[nodiscard]] c::Bool pullRegion(
				c::U16 x, c::U16 y, c::U16 z, c::U16 w, c::U16 h, c::U16 l,
				c::DevicePullCallback callback, void *context, c::Error *e_rr = nullptr
			) noexcept {
				return c::DeviceTextureRef_pullRegion(handle(), x, y, z, w, h, l, callback, context, e_rr);
			}
		};

		class DepthStencil : public Handle<c::DepthStencil>, public TextureHandleOps<DepthStencil> {
		public:
			using Handle::Handle;

			//currReadHandle is only meaningful when created with allowShaderRead.

			//Depth-stencil readback is per plane, one pull each (see TextureRef_pullRegion's plane parameter):
			// plane 0 = depth (or the stencil of a stencil-only format),
			// plane 1 = the stencil of a combined format (1 byte per texel).
			//Data ownership and timing as RenderTexture::pullRegion.

			[[nodiscard]] c::Bool pullRegion(
				c::U16 x, c::U16 y, c::U16 z, c::U16 w, c::U16 h, c::U16 l, c::U8 plane,
				c::TexturePullCallback callback, void *context, c::Error *e_rr = nullptr
			) noexcept {
				return c::TextureRef_pullRegion(handle(), x, y, z, w, h, l, plane, callback, context, e_rr);
			}
		};

		class Swapchain : public Handle<c::Swapchain>, public TextureHandleOps<Swapchain> {
		public:
			using Handle::Handle;

			[[nodiscard]] c::Bool resize(c::Error *e_rr = nullptr) noexcept {
				return c::SwapchainRef_resize(handle(), e_rr);
			}
		};

		class Sampler : public Handle<c::Sampler> {
		public:
			using Handle::Handle;

			//The bindless handle shaders use to reach this sampler.
			[[nodiscard]] c::U32 bindlessHandle() const noexcept { return data()->samplerLocation; }
		};

		//Bindful descriptors.
		//A DescriptorLayout describes what a shader binds, a DescriptorHeap owns the storage and a
		//DescriptorTable is one layout's worth of descriptors allocated out of a heap.
		//Declared in that order because DescriptorHeap::createTable names the other two.

		//I32x2 by hand, for the same reason Texture::size() builds one by hand: vec2i.h's helpers are not in
		//this closure, and pulling it into the namespaced include block for two one liners would widen that
		//block for no gain. These exist so callers stop hand rolling them per file.

		[[nodiscard]] inline c::I32x2 xy(c::I32 x, c::I32 y) noexcept { return c::I32x2{ { x, y } }; }
		[[nodiscard]] inline c::I32x2 xy() noexcept { return c::I32x2{ { 0, 0 } }; }

		[[nodiscard]] inline c::Bool sameXy(c::I32x2 a, c::I32x2 b) noexcept {
			return a.v[0] == b.v[0] && a.v[1] == b.v[1];
		}

		class DescriptorLayout : public Handle<c::DescriptorLayout> { public: using Handle::Handle; };
		class PipelineLayout : public Handle<c::PipelineLayout> { public: using Handle::Handle; };

		class DescriptorTable : public Handle<c::DescriptorTable> {
		public:
			using Handle::Handle;

			//maintainRef makes the table hold a reference to the resource, so it outlives the caller's handle.

			[[nodiscard]] c::Bool set(
				c::U64 bindId, c::U64 arrayId, const c::Descriptor &d,
				c::Bool maintainRef = false, c::Error *e_rr = nullptr
			) noexcept {
				return c::DescriptorTableRef_setDescriptor(handle(), bindId, arrayId, maintainRef, &d, e_rr);
			}

			//By name is the reflection-driven form: it pairs with Device::detectLayout, which keeps the
			// register names, so nothing has to track which bindId a shader's register ended up as.

			[[nodiscard]] c::Bool setByName(
				const c::C8 *registerName, const c::Descriptor &d,
				c::U64 arrayId = 0, c::Bool maintainRef = false, c::Error *e_rr = nullptr
			) noexcept {
				const c::CharString n = name(registerName);
				return c::DescriptorTableRef_setDescriptorByName(handle(), &n, arrayId, maintainRef, &d, e_rr);
			}

			//Named setRange rather than an overload of set: a one element braced list would otherwise be
			//ambiguous against the single descriptor form above, which callers can only break by hoisting a
			//named initializer_list.

			[[nodiscard]] c::Bool setRange(
				c::U64 bindId, c::U64 arrayId, std::initializer_list<c::Descriptor> d,
				c::Bool maintainRef = false, c::Error *e_rr = nullptr
			) noexcept {

				c::ListDescriptor list{};

				if(d.size())
					(void) c::ListDescriptor_createRefConst(d.begin(), d.size(), &list, nullptr);

				return c::DescriptorTableRef_setDescriptors(handle(), bindId, arrayId, maintainRef, &list, e_rr);
			}

			[[nodiscard]] c::Bool unset(
				c::U64 bindId, c::U64 arrayId, c::U64 count, c::Error *e_rr = nullptr
			) noexcept {
				return c::DescriptorTableRef_unsetDescriptors(handle(), bindId, arrayId, count, e_rr);
			}

			//The plural and unset forms of the by-name path, which the singular setByName above only half covered.

			[[nodiscard]] c::Bool setRangeByName(
				const c::C8 *registerName, std::initializer_list<c::Descriptor> d,
				c::U64 arrayId = 0, c::Bool maintainRef = false, c::Error *e_rr = nullptr
			) noexcept {

				const c::CharString n = name(registerName);
				c::ListDescriptor list{};

				if(d.size())
					(void) c::ListDescriptor_createRefConst(d.begin(), d.size(), &list, nullptr);
					
				return c::DescriptorTableRef_setDescriptorsByName(handle(), &n, arrayId, maintainRef, &list, e_rr);
			}

			[[nodiscard]] c::Bool unsetByName(
				const c::C8 *registerName, c::U64 arrayId, c::U64 count, c::Error *e_rr = nullptr
			) noexcept {
				const c::CharString n = name(registerName);
				return c::DescriptorTableRef_unsetDescriptorsByName(handle(), &n, arrayId, count, e_rr);
			}

			//Allocation picks the arrayId for you instead of taking one, so it comes back by reference and the
			//Bool stays the success channel like everywhere else.

			[[nodiscard]] c::Bool alloc(
				c::U64 bindId, c::U64 &arrayId, const c::Descriptor &d,
				c::Bool maintainRef = false, c::Error *e_rr = nullptr
			) noexcept {
				return c::DescriptorTableRef_allocDescriptor(handle(), bindId, &arrayId, maintainRef, &d, e_rr);
			}

			[[nodiscard]] c::Bool allocByName(
				const c::C8 *registerName, c::U64 &arrayId, const c::Descriptor &d,
				c::Bool maintainRef = false, c::Error *e_rr = nullptr
			) noexcept {
				const c::CharString n = name(registerName);
				return c::DescriptorTableRef_allocDescriptorByName(handle(), &n, &arrayId, maintainRef, &d, e_rr);
			}

			//U64_MAX means the table's layout declares no such register.

			[[nodiscard]] c::U64 resolveRegisterName(const c::C8 *registerName) const noexcept {
				const c::CharString n = name(registerName);
				return c::DescriptorTableRef_resolveRegisterName(handle(), &n);
			}

			//The bindless pair. Rather than naming a register these SEARCH the layout for one that fits the
			//type and stride, so they hand back the bind id and bindless type id they landed on as well as the
			//array slot; all three come back by reference for the same reason alloc()'s arrayId does.
			//strideOrLength 0 means "don't care about size".

			[[nodiscard]] c::Bool allocBindless(
				c::ESHRegisterType type, c::U32 strideOrLength,
				c::U16 &bindId, c::U8 &bindlessTypeId, c::U64 &arrayId, const c::Descriptor &d,
				c::Bool maintainRef = false, c::Error *e_rr = nullptr
			) noexcept {
				return c::DescriptorTableRef_allocDescriptorBindless(
					handle(), type, strideOrLength, &bindId, &bindlessTypeId, &arrayId, maintainRef, &d, e_rr
				);
			}

			//Finds the register a resource WOULD go into without allocating one.
			//planeId picks a plane of a multi planar resource, which is how a depth buffer's stencil is read.

			[[nodiscard]] c::Bool findBindlessRegister(
				c::ESHRegisterType type, c::U32 strideOrLength,
				c::U16 &bindId, c::U8 &bindlessTypeId, c::RefPtr *resource,
				c::U8 planeId = 0, c::Error *e_rr = nullptr
			) const noexcept {
				return c::DescriptorTableRef_findBindlessRegister(
					handle(), type, strideOrLength, &bindId, &bindlessTypeId, resource, planeId, e_rr
				);
			}
		};

		class DescriptorHeap : public Handle<c::DescriptorHeap> {
		public:
			using Handle::Handle;

			[[nodiscard]] c::Bool createTable(
				const DescriptorLayout &layout, const c::C8 *debugName, DescriptorTable &result,
				c::EDescriptorTableFlags flags = (c::EDescriptorTableFlags) 0, c::Error *e_rr = nullptr
			) noexcept {

				const c::CharString n = name(debugName);
				c::DescriptorTableRef *raw = nullptr;

				if(!c::DescriptorHeapRef_createDescriptorTable(handle(), layout.handle(), flags, &n, &raw, e_rr))
					return false;

				result = DescriptorTable(RefPtr<c::DescriptorTable>::adopt(raw));
				return true;
			}
		};

		class Pipeline : public Handle<c::Pipeline> { public: using Handle::Handle; };
		class Blas : public Handle<c::BLAS> { public: using Handle::Handle; };

		class Tlas : public Handle<c::TLAS> {
		public:
			using Handle::Handle;

			//The bindless handle shaders use to reach this TLAS (tlasExt(i) / tlasExtUniform(i)).
			[[nodiscard]] c::U32 bindlessHandle() const noexcept { return data()->handle; }

			//Refit, not resize:
			// the count MUST match what the TLAS was created with (see TLAS::asConstructionType),
			// both APIs update an existing structure in place,
			// so a scene that gained or lost instances needs a new TLAS.
			//CPU-built TLASes that allow updates only; the next recorded updateTlas consumes the new instances.

			[[nodiscard]] c::Bool setInstances(
				const c::TLASInstance *instances, c::U64 instanceCount, c::Error *e_rr = nullptr
			) noexcept {

				c::ListTLASInstance list{};
				if(instanceCount && !c::ListTLASInstance_createRefConst(instances, instanceCount, &list, e_rr))
					return false;

				return c::TLASRef_setInstancesExt(handle(), &list, e_rr);
			}
		};

		class OpacityMicromap : public Handle<c::OpacityMicromap> { public: using Handle::Handle; };

		//---------------------------------------------------------------- scoped command recording

		class CommandRender {

			c::CommandListRef *list;

		public:

			explicit CommandRender(c::CommandListRef *l) noexcept : list(l) {}
			~CommandRender() noexcept { end(); }

			CommandRender(const CommandRender&) = delete;
			CommandRender &operator=(const CommandRender&) = delete;
			CommandRender(CommandRender &&other) noexcept : list(other.list) { other.list = nullptr; }
			CommandRender &operator=(CommandRender&&) = delete;

			[[nodiscard]] explicit operator bool() const noexcept { return list; }

			c::Bool end(c::Error *e_rr = nullptr) noexcept {
				if(!list) return true;
				c::CommandListRef *l = list;
				list = nullptr;
				return c::CommandListRef_endRenderExt(l, e_rr);
			}

			[[nodiscard]] c::Bool setGraphicsPipeline(const Pipeline &p, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_setGraphicsPipeline(list, p.handle(), e_rr);
			}

			[[nodiscard]] c::Bool setViewportAndScissor(c::I32x2 offset, c::I32x2 size, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_setViewportAndScissor(list, offset, size, e_rr);
			}

			[[nodiscard]] c::Bool setPrimitiveBuffers(const c::SetPrimitiveBuffersCmd &b, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_setPrimitiveBuffers(list, &b, e_rr);
			}

			[[nodiscard]] c::Bool drawIndexed(c::U32 indexCount, c::U32 instanceCount = 1, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_drawIndexed(list, indexCount, instanceCount, e_rr);
			}

			[[nodiscard]] c::Bool drawUnindexed(c::U32 vertexCount, c::U32 instanceCount = 1, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_drawUnindexed(list, vertexCount, instanceCount, e_rr);
			}

			[[nodiscard]] c::Bool draw(const c::DrawCmd &d, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_draw(list, &d, e_rr);
			}

			//Parameter order copied verbatim from the C API:
			// the indexed variant puts instanceOffset BEFORE vertexOffset (see CommandDrawIndirect),
			// the unindexed one after (see CommandDispatchIndirect).

			[[nodiscard]] c::Bool drawIndexedAdv(
				c::U32 indexCount, c::U32 instanceCount,
				c::U32 indexOffset, c::U32 instanceOffset, c::U32 vertexOffset,
				c::Error *e_rr = nullptr
			) noexcept {
				return c::CommandListRef_drawIndexedAdv(
					list, indexCount, instanceCount, indexOffset, instanceOffset, vertexOffset, e_rr
				);
			}

			[[nodiscard]] c::Bool drawUnindexedAdv(
				c::U32 vertexCount, c::U32 instanceCount,
				c::U32 vertexOffset, c::U32 instanceOffset,
				c::Error *e_rr = nullptr
			) noexcept {
				return c::CommandListRef_drawUnindexedAdv(
					list, vertexCount, instanceCount, vertexOffset, instanceOffset, e_rr
				);
			}

			//buffer holds drawCalls DrawCallIndexed/DrawCallUnindexed records (per `indexed`) at bufferOffset.

			[[nodiscard]] c::Bool drawIndirect(
				const DeviceBuffer &buffer, c::U64 bufferOffset, c::U32 drawCalls, c::Bool indexed,
				c::Error *e_rr = nullptr
			) noexcept {
				return c::CommandListRef_drawIndirect(list, buffer.handle(), bufferOffset, drawCalls, indexed, e_rr);
			}

			//GPU decides the draw count (one U32 in countBuffer at countOffset), capped at maxDrawCalls.

			[[nodiscard]] c::Bool drawIndirectCount(
				const DeviceBuffer &buffer, c::U64 bufferOffset,
				const DeviceBuffer &countBuffer, c::U64 countOffset,
				c::U32 maxDrawCalls, c::Bool indexed,
				c::Error *e_rr = nullptr
			) noexcept {
				return c::CommandListRef_drawIndirectCountExt(
					list, buffer.handle(), bufferOffset, countBuffer.handle(), countOffset, maxDrawCalls, indexed, e_rr
				);
			}

			//Escape hatch:
			// a command this layer has not wrapped yet must never force abandoning the RAII layer,
			// record it as c::CommandListRef_*(r.raw(), ...) while the pass is open.

			[[nodiscard]] c::CommandListRef *raw() const noexcept { return list; }
		};

		//Debug region sub-scope for GPU debuggers (DebugMarkers feature).
		//Nothing records "on" it, it only guarantees endRegionDebugExt across early returns;
		// keep recording through the owning scope while it is alive.
		//Made by CommandScope::regionDebug.

		class CommandDebugRegion {

			c::CommandListRef *list;

		public:

			explicit CommandDebugRegion(c::CommandListRef *l) noexcept : list(l) {}
			~CommandDebugRegion() noexcept { end(); }

			CommandDebugRegion(const CommandDebugRegion&) = delete;
			CommandDebugRegion &operator=(const CommandDebugRegion&) = delete;
			CommandDebugRegion(CommandDebugRegion &&other) noexcept : list(other.list) { other.list = nullptr; }
			CommandDebugRegion &operator=(CommandDebugRegion&&) = delete;

			[[nodiscard]] explicit operator bool() const noexcept { return list; }

			c::Bool end(c::Error *e_rr = nullptr) noexcept {
				if(!list) return true;
				c::CommandListRef *l = list;
				list = nullptr;
				return c::CommandListRef_endRegionDebugExt(l, e_rr);
			}
		};

		class CommandScope {

			c::CommandListRef *list;

		public:

			//Private-ish:
			// made by CommandList::scope.
			//A null list means startScope REFUSED, which is fatal to the recording rather than a benign skip:
			// the C side sets ECommandStateFlags_InvalidState (command_list.c, startScope's clean path),
			// and from there CommandListRef_end() hard-fails ("can't close a recording that was invalidated"),
			// while calling endScope anyway trips validateScope, no scope is open,
			// and marks the list permanently ECommandListState_Invalid.
			//So this object holds no list when refused (nothing to end), and the CALLER must treat falsy as an error:
			// report it and abandon the recording.
			//Testing it only to skip the body silently produces an empty frame,
			// which is how a black window at a great frame rate happens.
			//Worse for anyone tempted to continue:
			// the next scope inherits InvalidState|HasScope,
			// so ITS endScope rewinds everything it recorded and still reports success (CommandListRef_endScope's hide
			// branch), work vanishes with no error anywhere.
			//There is also no recovery:
			// begin() and clear() never touch tempStateFlags.

			explicit CommandScope(c::CommandListRef *l) noexcept : list(l) {}
			~CommandScope() noexcept { end(); }

			CommandScope(const CommandScope&) = delete;
			CommandScope &operator=(const CommandScope&) = delete;
			CommandScope(CommandScope &&other) noexcept : list(other.list) { other.list = nullptr; }
			CommandScope &operator=(CommandScope&&) = delete;

			[[nodiscard]] explicit operator bool() const noexcept { return list; }

			c::Bool end(c::Error *e_rr = nullptr) noexcept {
				if(!list) return true;
				c::CommandListRef *l = list;
				list = nullptr;
				return c::CommandListRef_endScope(l, e_rr);
			}

			//Render pass sub-scope (graphics pipelines only).

			[[nodiscard]] CommandRender render(
				c::I32x2 offset, c::I32x2 size,
				std::initializer_list<c::AttachmentInfo> colors,
				const c::DepthStencilAttachmentInfo *depthStencil = nullptr,
				c::Error *e_rr = nullptr
			) noexcept {

				c::ListAttachmentInfo colorList{};
				if(colors.size())
					(void) c::ListAttachmentInfo_createRefConst(colors.begin(), colors.size(), &colorList, nullptr);

				if(!c::CommandListRef_startRenderExt(list, offset, size, &colorList, depthStencil, e_rr))
					return CommandRender(nullptr);

				return CommandRender(list);
			}

			//Bindful: these only set state, and the work ops (draw/dispatch/dispatchRays) validate it against
			//the bound pipeline's layout, so bind order never matters. Scope end resets all of it.

			[[nodiscard]] c::Bool bindDescriptorHeap(const DescriptorHeap &heap, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_bindDescriptorHeap(list, heap.handle(), e_rr);
			}

			[[nodiscard]] c::Bool bindDescriptorTable(const DescriptorTable &table, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_bindDescriptorTable(list, table.handle(), e_rr);
			}

			//Push constants ride in the command stream rather than the heap. Templated on the payload so the
			//size comes from the type: the work op requires it to match the layout's declared size exactly,
			//and a hand-passed length is the one thing that can silently get that wrong.

			template<typename T>
			[[nodiscard]] c::Bool setPushConstants(const T &data, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_setPushConstants(
					list, c::Buffer_createRefConst(&data, sizeof(T)), e_rr
				);
			}

			//Every push descriptor the layout declares, in its binding order. All at once rather than one at
			//a time, because a partial set leaves the rest pointing at whatever the last pipeline bound.

			[[nodiscard]] c::Bool setPushDescriptors(
				std::initializer_list<c::Descriptor> descriptors, c::Error *e_rr = nullptr
			) noexcept {
				c::ListDescriptor d{};
				if(descriptors.size())
					(void) c::ListDescriptor_createRefConst(descriptors.begin(), descriptors.size(), &d, nullptr);
				return c::CommandListRef_setPushDescriptors(list, &d, e_rr);
			}

			[[nodiscard]] c::Bool setComputePipeline(const Pipeline &p, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_setComputePipeline(list, p.handle(), e_rr);
			}

			[[nodiscard]] c::Bool setRaytracingPipeline(const Pipeline &p, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_setRaytracingPipeline(list, p.handle(), e_rr);
			}

			[[nodiscard]] c::Bool dispatch1D(c::U32 x, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_dispatch1D(list, x, e_rr);
			}

			[[nodiscard]] c::Bool dispatch2D(c::U32 x, c::U32 y, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_dispatch2D(list, x, y, e_rr);
			}

			[[nodiscard]] c::Bool dispatch2DRays(c::U32 raygenLocalId, c::U32 x, c::U32 y, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_dispatch2DRaysExt(list, raygenLocalId, x, y, e_rr);
			}

			[[nodiscard]] c::Bool updateBlas(const Blas &b, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_updateBLASExt(list, b.handle(), e_rr);
			}

			[[nodiscard]] c::Bool updateTlas(const Tlas &t, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_updateTLASExt(list, t.handle(), e_rr);
			}

			[[nodiscard]] c::Bool copyImage(
				c::RefPtr *src, c::RefPtr *dst, c::CopyImageRegion region, c::Error *e_rr = nullptr
			) noexcept {
				return c::CommandListRef_copyImage(list, src, dst, region, e_rr);
			}

			//Dynamic graphics state.
			//The C API accepts these in ANY scope (they only bind against a pipeline at draw time),
			// so the singles live here; render() keeps the combined form too since that is where it is set in practice.

			[[nodiscard]] c::Bool setViewport(c::I32x2 offset, c::I32x2 size, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_setViewport(list, offset, size, e_rr);
			}

			[[nodiscard]] c::Bool setScissor(c::I32x2 offset, c::I32x2 size, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_setScissor(list, offset, size, e_rr);
			}

			[[nodiscard]] c::Bool setViewportAndScissor(c::I32x2 offset, c::I32x2 size, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_setViewportAndScissor(list, offset, size, e_rr);
			}

			[[nodiscard]] c::Bool setStencil(c::U8 stencilValue, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_setStencil(list, stencilValue, e_rr);
			}

			[[nodiscard]] c::Bool setBlendConstants(c::F32x4 blendConstants, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_setBlendConstants(list, blendConstants, e_rr);
			}

			//image is a Swapchain or RenderTexture (see CommandSetViewport); it crosses as the raw c::RefPtr*,
			// same as copyImage's src/dst, so either handle type fits without overloads.

			[[nodiscard]] c::Bool clearImagef(
				c::F32x4 color, c::ImageRange range, c::RefPtr *image, c::Error *e_rr = nullptr
			) noexcept {
				return c::CommandListRef_clearImagef(list, color, range, image, e_rr);
			}

			[[nodiscard]] c::Bool clearImagei(
				c::I32x4 color, c::ImageRange range, c::RefPtr *image, c::Error *e_rr = nullptr
			) noexcept {
				return c::CommandListRef_clearImagei(list, color, range, image, e_rr);
			}

			[[nodiscard]] c::Bool clearImageu(
				const c::U32 color[4], c::ImageRange range, c::RefPtr *image, c::Error *e_rr = nullptr
			) noexcept {
				return c::CommandListRef_clearImageu(list, color, range, image, e_rr);
			}

			//Unlike start*'s list arguments, clearImages takes its TList BY VALUE (see CommandSetStencil).

			[[nodiscard]] c::Bool clearImages(
				std::initializer_list<c::ClearImageCmd> images, c::Error *e_rr = nullptr
			) noexcept {

				c::ListClearImageCmd imageList{};
				if(images.size())
					(void) c::ListClearImageCmd_createRefConst(images.begin(), images.size(), &imageList, nullptr);

				return c::CommandListRef_clearImages(list, imageList, e_rr);
			}

			[[nodiscard]] c::Bool copyImageRegions(
				c::RefPtr *src, c::RefPtr *dst, std::initializer_list<c::CopyImageRegion> regions,
				c::Error *e_rr = nullptr
			) noexcept {

				c::ListCopyImageRegion regionList{};
				if(regions.size())
					(void) c::ListCopyImageRegion_createRefConst(regions.begin(), regions.size(), &regionList, nullptr);

				return c::CommandListRef_copyImageRegions(list, src, dst, regionList, e_rr);
			}

			[[nodiscard]] c::Bool dispatch(c::DispatchCmd groups, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_dispatch(list, groups, e_rr);
			}

			[[nodiscard]] c::Bool dispatch3D(c::U32 x, c::U32 y, c::U32 z, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_dispatch3D(list, x, y, z, e_rr);
			}

			//The GPU reads one Dispatch { x, y, z } record from buffer at offset.

			[[nodiscard]] c::Bool dispatchIndirect(
				const DeviceBuffer &buffer, c::U64 offset, c::Error *e_rr = nullptr
			) noexcept {
				return c::CommandListRef_dispatchIndirect(list, buffer.handle(), offset, e_rr);
			}

			//RayPipeline; names drop the C Ext suffix like dispatch2DRays above.

			[[nodiscard]] c::Bool dispatchRays(c::DispatchRaysExt rays, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_dispatchRaysExt(list, rays, e_rr);
			}

			[[nodiscard]] c::Bool dispatch1DRays(c::U32 raygenLocalId, c::U32 x, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_dispatch1DRaysExt(list, raygenLocalId, x, e_rr);
			}

			[[nodiscard]] c::Bool dispatch3DRays(
				c::U32 raygenLocalId, c::U32 x, c::U32 y, c::U32 z, c::Error *e_rr = nullptr
			) noexcept {
				return c::CommandListRef_dispatch3DRaysExt(list, raygenLocalId, x, y, z, e_rr);
			}

			//Must be recorded before any BLAS build that links it;
			// a completed micromap re-records as a no-op (see CommandDebugRegion),
			// so callers need not track completion.

			[[nodiscard]] c::Bool updateOmm(const OpacityMicromap &micromap, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_updateOmmExt(list, micromap.handle(), e_rr);
			}

			//DebugMarkers feature.

			[[nodiscard]] c::Bool addMarkerDebug(c::F32x4 color, const c::C8 *markerName, c::Error *e_rr = nullptr) noexcept {
				const c::CharString n = name(markerName);
				return c::CommandListRef_addMarkerDebugExt(list, color, &n, e_rr);
			}

			//Prefer regionDebug() below; the raw pair remains for regions that cannot nest lexically.

			[[nodiscard]] c::Bool startRegionDebug(
				c::F32x4 color, const c::C8 *regionName, c::Error *e_rr = nullptr
			) noexcept {
				const c::CharString n = name(regionName);
				return c::CommandListRef_startRegionDebugExt(list, color, &n, e_rr);
			}

			[[nodiscard]] c::Bool endRegionDebug(c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_endRegionDebugExt(list, e_rr);
			}

			//Debug region sub-scope, mirroring render():
			// a refused start returns a null region and endRegionDebugExt runs on destruction (or an explicit end()
			// when its error matters).

			[[nodiscard]] CommandDebugRegion regionDebug(
				c::F32x4 color, const c::C8 *regionName, c::Error *e_rr = nullptr
			) noexcept {

				const c::CharString n = name(regionName);
				if(!c::CommandListRef_startRegionDebugExt(list, color, &n, e_rr))
					return CommandDebugRegion(nullptr);

				return CommandDebugRegion(list);
			}

			//Escape hatch:
			// a command this layer has not wrapped yet must never force abandoning the RAII layer - record it as
			// c::CommandListRef_*(scope.raw(), ...) while the scope is open.

			[[nodiscard]] c::CommandListRef *raw() const noexcept { return list; }
		};

		class CommandList : public Handle<c::CommandList> {
		public:
			using Handle::Handle;

			[[nodiscard]] c::Bool begin(c::Bool doClear = true, c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_begin(handle(), doClear, c::U64_MAX, e_rr);
			}

			[[nodiscard]] c::Bool end(c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_end(handle(), e_rr);
			}

			//A refused scope invalidates the recording, see CommandScope for what that costs. e_rr carries the reason;
			// check the returned scope and stop recording if it is falsy.

			[[nodiscard]] CommandScope scope(
				std::initializer_list<c::Transition> transitions,
				c::U32 id = 0,
				std::initializer_list<c::CommandScopeDependency> deps = {},
				c::Error *e_rr = nullptr
			) noexcept {

				c::ListTransition transitionList{};
				c::ListCommandScopeDependency depList{};

				if(transitions.size())
					(void) c::ListTransition_createRefConst(transitions.begin(), transitions.size(), &transitionList, nullptr);

				if(deps.size())
					(void) c::ListCommandScopeDependency_createRefConst(deps.begin(), deps.size(), &depList, nullptr);

				if(!c::CommandListRef_startScope(handle(), &transitionList, id, &depList, e_rr))
					return CommandScope(nullptr);

				return CommandScope(handle());
			}

			//begin(doClear=true) already clears;
			// standalone clear() exists to drop the recorded commands AND the resource refs they keep alive without
			// opening the list for recording.

			[[nodiscard]] c::Bool clear(c::Error *e_rr = nullptr) noexcept {
				return c::CommandListRef_clear(handle(), e_rr);
			}
		};

		//---------------------------------------------------------------- device

		//Owning wrapper over a generated TList,
		// so the pipeline factories can size their stage/group arrays from the caller's entry counts instead of a fixed
		// cap.
		//Frees on every exit path, including the error returns,
		// which is why the factories can bail mid-build without leaking.

		template<typename T, void (*Free)(T*, const c::Allocator*)>
		struct OwnedList {

			T list{};
			const c::Allocator *alloc;

			explicit OwnedList(const c::Allocator *a) noexcept : alloc(a) {}
			~OwnedList() noexcept { Free(&list, alloc); }

			OwnedList(const OwnedList&) = delete;
			OwnedList &operator=(const OwnedList&) = delete;
		};

		//Everything GraphicsDeviceRef_getFirstShaderEntry can select a binary on, so compute,
		// graphics and raytracing pipelines all choose the same way instead of each exposing a different subset.
		// prefer/disallow narrow WHICH BINARY of an entry is taken:
		// an entry compiled for several extension sets has one binary each,
		// and first-fit would otherwise take whichever the packager emitted first. prefer is a preference,
		// not a requirement, the resolver retries without it,
		// so a device missing the extension still gets the baseline binary rather than failing to find the entry at
		// all. defines and uniforms are [key, value][] pairs selecting among variants compiled from the same source;
		// a uniform's value is compared against the binary's stringified value (GraphicsDeviceRef_getFirstShaderEntry's
		// defines/uniforms params).
		//BORROWED, like every other initializer_list here:
		// build it in the call expression, never store it.

		struct ShaderVariant {
			c::ESHExtension prefer = c::ESHExtension_None;
			c::ESHExtension disallow = c::ESHExtension_None;
			//No initializer on these two: MSVC rejects list initialization in a non-static data member
			// initializer (C2797), = {} included.
			//initializer_list default constructs to empty, so leaving it off means the same thing.

			std::initializer_list<const c::C8*> defines;
			std::initializer_list<const c::C8*> uniforms;
		};

		class Device {

			RefPtr<c::GraphicsDevice> ref;

		public:

			Device() noexcept = default;

			//Borrow a device someone else owns, which is how a test module wraps the raw GraphicsDeviceRef*
			//its harness hands it. share() increments, so the borrowed handle is released independently and
			//the owner's own dec still lands: adopting here would double free.

			[[nodiscard]] static Device share(c::GraphicsDeviceRef *raw) noexcept {
				Device d;
				d.ref = RefPtr<c::GraphicsDevice>::share(raw);
				return d;
			}

			//Straight off the device, so a caller does not reach through data() for either.
			//framesInFlight is how many submits can be outstanding, which is what sizes a per frame ring.

			[[nodiscard]] c::U8 framesInFlight() const noexcept { return ref.data()->framesInFlight; }

			//The api is the instance's, not the device info's; the device holds a ref on the instance for its
			// whole life, so reaching through it is safe.

			[[nodiscard]] c::EGraphicsApi api() const noexcept {

				//GraphicsInstanceRef_ptr is a macro, so it cannot be reached through c:: and is spelled out
				//here instead; RefPtr_data is the payload right after the RefPtr header.

				const c::RefPtr *instance = ref.data()->instance;
				return instance ? ((const c::GraphicsInstance*) (instance + 1))->api : c::EGraphicsApi_Count;
			}

		private:

			//Single entry-resolution path for every pipeline kind.
			//U32_MAX means no binary of that entry can run here, which each factory turns into its own error.

			[[nodiscard]] c::U32 resolveEntry(
				const c::SHFile &shFile, const c::C8 *entryName, const ShaderVariant &variant
			) noexcept {

				const c::CharString en = name(entryName);

				//Pairs, so an odd count means the caller mismatched a key with its value
				const c::U64 dn = variant.defines.size() &~ (c::U64)1;
				const c::U64 un = variant.uniforms.size() &~ (c::U64)1;

				c::CharString defineStr[32]{}, uniformStr[32]{};

				const c::U64 dc = dn < 32 ? dn : 32;
				const c::U64 uc = un < 32 ? un : 32;

				c::U64 k = 0;

				for(const c::C8 *v : variant.defines) {
					if(k >= dc) break;
					defineStr[k++] = name(v);
				}

				k = 0;

				for(const c::C8 *v : variant.uniforms) {
					if(k >= uc) break;
					uniformStr[k++] = name(v);
				}

				c::ListCharString defines{}, uniforms{};
				(void) c::ListCharString_createRefConst(defineStr, dc, &defines, nullptr);
				(void) c::ListCharString_createRefConst(uniformStr, uc, &uniforms, nullptr);

				const c::ListCharString *dp = dc ? &defines : nullptr;
				const c::ListCharString *up = uc ? &uniforms : nullptr;

				if(variant.prefer != c::ESHExtension_None) {

					const c::U32 id = c::GraphicsDeviceRef_getFirstShaderEntry(
						handle(), &shFile, &en, dp, up, variant.disallow, variant.prefer
					);

					if(id != c::U32_MAX)
						return id;
				}

				return c::GraphicsDeviceRef_getFirstShaderEntry(
					handle(), &shFile, &en, dp, up, variant.disallow, c::ESHExtension_None
				);
			}

		public:

			//bindlessLayout:
			// NULL = OxC3's default bindless layout (see GraphicsDeviceRef_create's bindlessLayout);
			// ignored without EGraphicsFeatures_Bindless or when EGraphicsDeviceFlags_DisableBindless is set.
			//The device COPIES it, so the caller keeps ownership.
			//The new params trail result so existing callers keep meaning the same thing.

			//Requires the //OxC3_graphics virtual section to be linked into the binary,
			// device creation loads image_copy.oiSH from it and fails with "loc not found" otherwise.
			//In CMake that is add_virtual_dependencies_external(TARGET x DEPENDENCIES OxC3) followed by
			// apply_dependencies(x); the DEPENDENCIES name is a find_program name, so it is the binary's exact case.

			[[nodiscard]] static c::Bool create(
				const Instance &instance,
				const c::GraphicsDeviceInfo &info,
				c::EGraphicsDeviceFlags flags,
				Device &result,
				c::EGraphicsBufferingMode bufferingMode = c::EGraphicsBufferingMode_Default,
				const c::DescriptorLayoutInfo *bindlessLayout = nullptr,
				c::Error *e_rr = nullptr
			) noexcept {

				result = Device();
				c::GraphicsDeviceRef *raw = nullptr;

				if(!c::GraphicsDeviceRef_create(instance.handle(), &info, flags, bufferingMode, bindlessLayout, &raw, e_rr))
					return false;

				result.ref = RefPtr<c::GraphicsDevice>::adopt(raw);
				return true;
			}

			[[nodiscard]] c::Bool valid() const noexcept { return ref.valid(); }
			[[nodiscard]] explicit operator bool() const noexcept { return ref.valid(); }
			[[nodiscard]] c::RefPtr *handle() const noexcept { return ref.handle(); }
			void release() noexcept { ref.release(); }
			[[nodiscard]] const c::GraphicsDeviceInfo &info() const noexcept { return ref.data()->info; }

			[[nodiscard]] c::Bool wait(c::Error *e_rr = nullptr) noexcept {
				return c::GraphicsDeviceRef_wait(handle(), e_rr);
			}

			//---- factories (each adopts the C factory's reference)

			//allowResize false pins the recording to bufferLen, which is what the fixed size tests need; the C
			//API refuses a command that would grow past it instead of reallocating.

			[[nodiscard]] c::Bool createCommandList(
				c::U64 bufferLen, c::U64 estCommands, c::U64 estResources,
				CommandList &result, c::Bool allowResize = true, c::Error *e_rr = nullptr
			) noexcept {

				c::CommandListRef *raw = nullptr;

				if(!c::GraphicsDeviceRef_createCommandList(
					handle(), bufferLen, estCommands, estResources, allowResize, &raw, e_rr
				))
					return false;

				result = CommandList(RefPtr<c::CommandList>::adopt(raw));
				return true;
			}

			//window is platforms-side and crosses as an opaque pointer (see file rationale).

			//presentModes:
			// optional priority list (ESwapchainPresentMode values), first supported wins;
			// empty keeps the platform default.
			//Fifo caps presentation at the refresh rate.

			[[nodiscard]] c::Bool createSwapchain(
				void *window, c::Bool allowComputeWrite, Swapchain &result,
				std::initializer_list<c::U8> presentModes = {}, c::Error *e_rr = nullptr
			) noexcept {

				c::SwapchainInfo info{};
				info.window = (c::Window*) window;

				c::U64 pm = 0;
				for(c::U8 mode : presentModes)
					if(pm < sizeof(info.presentModePriorities))
						info.presentModePriorities[pm++] = mode;
				c::SwapchainRef *raw = nullptr;

				if(!c::GraphicsDeviceRef_createSwapchain(handle(), info, allowComputeWrite, nullptr, &raw, e_rr))
					return false;

				result = Swapchain(RefPtr<c::Swapchain>::adopt(raw));
				return true;
			}

			//bindlessTable null routes the resource into the device's own bindless table, which is what the C
			//API's null means; pass one to route it into a table you own instead.

			[[nodiscard]] c::Bool createBuffer(
				c::EDeviceBufferUsage usage, c::EGraphicsResourceFlag flags,
				const c::C8 *debugName, c::U64 len, DeviceBuffer &result,
				const DescriptorTable *bindlessTable = nullptr, c::Error *e_rr = nullptr
			) noexcept {

				const c::CharString n = name(debugName);
				c::DeviceBufferRef *raw = nullptr;

				if(!c::GraphicsDeviceRef_createBuffer(
					handle(), usage, flags, bindlessTable ? bindlessTable->handle() : nullptr, &n, len, &raw, e_rr
				))
					return false;

				result = DeviceBuffer(RefPtr<c::DeviceBuffer>::adopt(raw));
				return true;
			}

			//dat is CONSUMED on success unless it is a ref (house rule of the C API).

			[[nodiscard]] c::Bool createBufferData(
				c::EDeviceBufferUsage usage, c::EGraphicsResourceFlag flags,
				const c::C8 *debugName, c::Buffer *dat, DeviceBuffer &result,
				const DescriptorTable *bindlessTable = nullptr, c::Error *e_rr = nullptr
			) noexcept {

				const c::CharString n = name(debugName);
				c::DeviceBufferRef *raw = nullptr;

				if(!c::GraphicsDeviceRef_createBufferData(
					handle(), usage, flags, bindlessTable ? bindlessTable->handle() : nullptr, &n, dat, &raw, e_rr
				))
					return false;

				result = DeviceBuffer(RefPtr<c::DeviceBuffer>::adopt(raw));
				return true;
			}

			[[nodiscard]] c::Bool createRenderTexture(
				c::U16 width, c::U16 height, c::ETextureFormatId format, c::EGraphicsResourceFlag flags,
				const c::C8 *debugName, RenderTexture &result,
				const DescriptorTable *bindlessTable = nullptr, c::Error *e_rr = nullptr
			) noexcept {

				const c::CharString n = name(debugName);
				c::RenderTextureRef *raw = nullptr;

				if(!c::GraphicsDeviceRef_createRenderTexture(
					handle(), c::ETextureType_2D, width, height, 1, format, flags,
					c::EMSAASamples_Off, bindlessTable ? bindlessTable->handle() : nullptr, &n, &raw, e_rr
				))
					return false;

				result = RenderTexture(RefPtr<c::RenderTexture>::adopt(raw));
				return true;
			}

			[[nodiscard]] c::Bool createBlas(
				const c::BLASCreateInfo &info, const c::C8 *debugName, Blas &result, c::Error *e_rr = nullptr
			) noexcept {

				const c::CharString n = name(debugName);
				c::BLASRef *raw = nullptr;

				if(!c::GraphicsDeviceRef_createBLASExt(handle(), &info, &n, &raw, e_rr))
					return false;

				result = Blas(RefPtr<c::BLAS>::adopt(raw));
				return true;
			}

			[[nodiscard]] c::Bool createTlas(
				c::ERTASBuildFlags buildFlags,
				const c::TLASInstance *instances, c::U64 instanceCount,
				const c::C8 *debugName, Tlas &result, c::Error *e_rr = nullptr
			) noexcept {

				const c::CharString n = name(debugName);
				c::ListTLASInstance list{};

				if(instanceCount && !c::ListTLASInstance_createRefConst(instances, instanceCount, &list, e_rr))
						return false;

				c::TLASRef *raw = nullptr;
				if(!c::GraphicsDeviceRef_createTLASExt(handle(), buildFlags, &list, false, nullptr, &n, &raw, e_rr))
					return false;

				result = Tlas(RefPtr<c::TLAS>::adopt(raw));
				return true;
			}

			//Entry selection + compute pipeline in one step; the common case for oiSH files.

			//layout null means OxC3's default bindless layout, which is what the C API's null means too.
			//A bindful pipeline passes the PipelineLayout it was built against; the work ops then validate the
			//bound table/push state against it.

			[[nodiscard]] c::Bool createComputePipeline(
				const c::SHFile &shFile, const c::C8 *entryName, const c::C8 *debugName,
				Pipeline &result, const ShaderVariant &variant = {}, const PipelineLayout *layout = nullptr,
				c::Error *e_rr = nullptr
			) noexcept {

				const c::U32 entryId = resolveEntry(shFile, entryName, variant);

				if(entryId == c::U32_MAX) {
					if(e_rr) *e_rr = c::Error_notFound(0, 0, "createComputePipeline() entry not found or unsupported");
					return false;
				}

				const c::CharString n = name(debugName);

				//SPIRV keeps the entry's ORIGINAL name (an oiSH entry "mainSingle" is "mainSingle" in the
				//module, not "main"), and the C API reads a null entryName as "main".
				//So the name this resolved the id from has to be forwarded, or a shader whose entry isn't
				//literally called main targets the wrong entrypoint on Vulkan. DXIL ignores it.

				const c::CharString entry = name(entryName);
				c::PipelineRef *raw = nullptr;

				if(!c::GraphicsDeviceRef_createPipelineCompute(
					handle(), &shFile, &n, entryId, &entry, c::EPipelineFlags_None,
					layout ? layout->handle() : nullptr, &raw, e_rr
				))
					return false;

				result = Pipeline(RefPtr<c::Pipeline>::adopt(raw));
				return true;
			}

			//Raytracing pipeline from one SHFile.
			//Stage order = [raygens..., miss, closestHit];
			// raygenLocalId for dispatch2DRays is the index into raygenEntries.
			//One triangle hit group pointing at the closest-hit stage; anyHit/intersection intentionally absent.

			//prefer:
			// extension set to REQUIRE per entry when a matching binary exists,
			// silently falling back to the baseline otherwise.
			//Exists because getFirstShaderEntry is FIRST-FIT over binary order:
			// an entry compiled in several extension combinations (e.g. RayTriPosition + bare) returns whichever combo
			// registered first,
			// so a device-supported binary can silently lose to a less capable one that happens to be earlier.
			//Without a preference, advertising an extension is no guarantee the binary using it is the one that runs.

			//One HIT GROUP per closesthit entry, in order:
			// a TLAS instance selects its group with sbtOffset24_flags8's low 24 bits,
			// so geometry that wants its own shading (an emitter,
			// a different BSDF) gets it without a branch inside one shared closesthit. anyHit and intersection stay
			// U32_MAX - triangle geometry needs neither, and adding them means deciding per group,
			// which is a signature this wrapper does not have a consumer for.

			[[nodiscard]] c::Bool createRaytracingPipeline(
				const c::SHFile &shFile,
				std::initializer_list<const c::C8*> raygenEntries,
				const c::C8 *missEntry, std::initializer_list<const c::C8*> closestHitEntries,
				const c::C8 *debugName, Pipeline &result,
				const ShaderVariant &variant = {},
				c::U8 maxRecursionDepth = 1,
				c::EPipelineRaytracingFlags flags = c::EPipelineRaytracingFlags_Default,
				c::Error *e_rr = nullptr
			) noexcept {

				if(!closestHitEntries.size()) {
					if(e_rr) *e_rr = c::Error_invalidParameter(3, 0, "at least one closesthit entry is required");
					return false;
				}

				const c::U64 stageCount = raygenEntries.size() + 1 + closestHitEntries.size();
				const c::Allocator *alloc = c::GraphicsDeviceRef_getAlloc(handle());

				//Sized from the caller's entries.
				//There is no arbitrary ceiling here:
				// an SBT wants one hit group per material that shades differently,
				// so a real scene passes far more than a fixed array would hold.
				//The runtime still bounds recursion depth and payload size.

				OwnedList<c::ListPipelineStage, c::ListPipelineStage_free> stages(alloc);
				OwnedList<c::ListPipelineRaytracingGroup, c::ListPipelineRaytracingGroup_free> groups(alloc);

				if(
					!c::ListPipelineStage_resize(&stages.list, stageCount, alloc, e_rr) ||
					!c::ListPipelineRaytracingGroup_resize(&groups.list, closestHitEntries.size(), alloc, e_rr)
				)
					return false;

				c::U64 n = 0;

				auto resolve = [&](const c::C8 *entry) noexcept -> c::Bool {

					const c::U32 id = resolveEntry(shFile, entry, variant);

					if(id == c::U32_MAX)
						return false;

					stages.list.ptrNonConst[n++].binaryId = id;
					return true;
				};

				for(const c::C8 *entry : raygenEntries)
					if(!resolve(entry)) {
						if(e_rr) *e_rr = c::Error_notFound(0, 0, "raygen entry not found or unsupported");
						return false;
					}

				if(!resolve(missEntry)) {
					if(e_rr) *e_rr = c::Error_notFound(1, 0, "miss entry not found");
					return false;
				}

				c::U64 groupCount = 0;

				for(const c::C8 *entry : closestHitEntries) {

					if(!resolve(entry)) {
						if(e_rr) *e_rr = c::Error_notFound(2, 0, "closesthit entry not found or unsupported");
						return false;
					}

					c::PipelineRaytracingGroup &group = groups.list.ptrNonConst[groupCount++];
					group.closestHit = (c::U32)(n - 1);
					group.anyHit = c::U32_MAX;
					group.intersection = c::U32_MAX;
				}

				c::ListSHFile fileList{};
				(void) c::ListSHFile_createRefConst(&shFile, 1, &fileList, nullptr);

				c::PipelineRaytracingInfo info{};

				//The C struct stores these as U8 and documents the enum it carries, so the narrowing is the
				// intent rather than an accident; MSVC warns (C4244) unless it is spelled out.

				info.flags = (c::U8) flags;
				info.maxRecursionDepth = maxRecursionDepth;

				const c::CharString nm = name(debugName);
				c::PipelineRef *raw = nullptr;

				if(!c::GraphicsDeviceRef_createPipelineRaytracingExt(
					handle(), &stages.list, &fileList, &groups.list, &info, &nm,
					c::EPipelineFlags_None, nullptr, &raw, e_rr
				))
					return false;

				result = Pipeline(RefPtr<c::Pipeline>::adopt(raw));
				return true;
			}

			//Whether a shader binary can run here at all:
			// features, data types, shader model, vendor,
			// and for binaries oiSH marks as bindless whether their registers match the device's bindless layout (see
			// GraphicsDeviceRef_checkShaderFeatures). getFirstShaderEntry applies the same test per entry;
			// call this directly to learn WHY a binary was refused.

			[[nodiscard]] c::Bool checkShaderFeatures(
				const c::SHBinaryInfo &binary, const c::SHEntry &entry, c::Error *e_rr = nullptr
			) const noexcept {
				return c::GraphicsDeviceRef_checkShaderFeatures(handle(), &binary, &entry, e_rr);
			}

			//Bytes in use; returns U64_MAX on error instead of Bool + e_rr (see GraphicsDeviceRef_getMemoryBudget).
			//On dGPU deviceLocal = VRAM and !deviceLocal = shared memory; iGPU/CPU report 0 for device local.

			[[nodiscard]] c::U64 getMemoryBudget(c::Bool isDeviceLocal) const noexcept {
				return c::GraphicsDeviceRef_getMemoryBudget(handle(), isDeviceLocal);
			}

			//The allocator every object of this device's instance was created with;
			// valid for as long as the device lives,
			// it holds a ref on the instance (the device holds a ref on its instance).

			[[nodiscard]] const c::Allocator *alloc() const noexcept {
				return c::GraphicsDeviceRef_getAlloc(handle());
			}

			//Size the pullRegion readback ring up front; zero reserves the minimum.
			//It otherwise grows at the first pull,
			// which on D3D12 can bring a whole new memory block in mid frame - reserving at load time moves that hitch
			// somewhere predictable (see GraphicsDeviceRef_wait).

			[[nodiscard]] c::Bool reserveReadback(c::U64 sizePerFrame, c::Error *e_rr = nullptr) noexcept {
				return c::GraphicsDeviceRef_reserveReadback(handle(), sizePerFrame, e_rr);
			}

			//dat is CONSUMED on success unless it is a ref (house rule of the C API).
			//NOTE the C argument order:
			// format/flag come BEFORE the extent (see DeviceTextureRef_create), the reverse of createRenderTexture,
			// preserved on the C call only.

			[[nodiscard]] c::Bool createTexture(
				c::ETextureType type, c::ETextureFormatId format, c::EGraphicsResourceFlag flags,
				c::U16 width, c::U16 height, c::U16 length,
				const c::C8 *debugName, c::Buffer *dat, DeviceTexture &result,
				const DescriptorTable *bindlessTable = nullptr, c::Error *e_rr = nullptr
			) noexcept {

				const c::CharString n = name(debugName);
				c::DeviceTextureRef *raw = nullptr;

				if(!c::GraphicsDeviceRef_createTexture(
					handle(), type, format, flags, width, height, length,
					bindlessTable ? bindlessTable->handle() : nullptr, &n, dat, &raw, e_rr
				))
					return false;

				result = DeviceTexture(RefPtr<c::DeviceTexture>::adopt(raw));
				return true;
			}

			[[nodiscard]] c::Bool createDepthStencil(
				c::U16 width, c::U16 height, c::EDepthStencilFormat format, c::Bool allowShaderRead,
				const c::C8 *debugName, DepthStencil &result, c::Error *e_rr = nullptr
			) noexcept {

				const c::CharString n = name(debugName);
				c::DepthStencilRef *raw = nullptr;

				if(!c::GraphicsDeviceRef_createDepthStencil(
					handle(), width, height, format, allowShaderRead, c::EMSAASamples_Off, nullptr, &n, &raw, e_rr
				))
					return false;

				result = DepthStencil(RefPtr<c::DepthStencil>::adopt(raw));
				return true;
			}

			//info crosses BY VALUE:
			// the C factory takes SamplerInfo directly (see SamplerInfo),
			// not through a pointer like every other create info.
			//Allocates into the device's default bindless table (disallowBindlessDescriptor = false),
			// like the other factories.

			[[nodiscard]] c::Bool createDescriptorHeap(
				const c::DescriptorHeapInfo &info, const c::C8 *debugName, DescriptorHeap &result,
				c::Error *e_rr = nullptr
			) noexcept {

				const c::CharString n = name(debugName);
				c::DescriptorHeapRef *raw = nullptr;

				if(!c::GraphicsDeviceRef_createDescriptorHeap(handle(), &info, &n, &raw, e_rr))
					return false;

				result = DescriptorHeap(RefPtr<c::DescriptorHeap>::adopt(raw));
				return true;
			}

			//info is MOVED on success (house rule of the C API), which is why it is not const here.

			[[nodiscard]] c::Bool createDescriptorLayout(
				c::DescriptorLayoutInfo &info, const c::C8 *debugName, DescriptorLayout &result,
				c::Error *e_rr = nullptr
			) noexcept {

				const c::CharString n = name(debugName);
				c::DescriptorLayoutRef *raw = nullptr;

				if(!c::GraphicsDeviceRef_createDescriptorLayout(handle(), &info, &n, &raw, e_rr))
					return false;

				result = DescriptorLayout(RefPtr<c::DescriptorLayout>::adopt(raw));
				return true;
			}

			[[nodiscard]] c::Bool createPipelineLayout(
				const c::PipelineLayoutInfo &info, const c::C8 *debugName, PipelineLayout &result,
				c::Error *e_rr = nullptr
			) noexcept {

				const c::CharString n = name(debugName);
				c::PipelineLayoutRef *raw = nullptr;

				if(!c::GraphicsDeviceRef_createPipelineLayout(handle(), &info, &n, &raw, e_rr))
					return false;

				result = PipelineLayout(RefPtr<c::PipelineLayout>::adopt(raw));
				return true;
			}

			//Reflection-derived layout for one entrypoint.
			//pushConstantName/pushDescriptorNames name the registers that are NOT ordinary bindings; the C
			// side requires the matching out parameter to be null when its list is absent, which is enforced
			// here by passing null through rather than by the caller remembering it.

			[[nodiscard]] c::Bool detectLayout(
				const c::SHFile &binary, c::U32 entrypoint,
				c::DescriptorLayoutInfo &info,
				const c::C8 *pushConstantName = nullptr, c::DescriptorBinding *pushConstantOut = nullptr,
				std::initializer_list<const c::C8*> pushDescriptorNames = {},
				c::DescriptorLayoutInfo *pushDescriptorInfo = nullptr,
				c::EDescriptorLayoutFlags flags = c::EDescriptorLayoutFlags_None,
				c::Error *e_rr = nullptr
			) noexcept {

				c::CharString names[OXC3_MAX_PUSH_DESCRIPTORS];
				c::ListCharString nameList{};

				const c::U64 count =
					pushDescriptorNames.size() < OXC3_MAX_PUSH_DESCRIPTORS ?
					pushDescriptorNames.size() : OXC3_MAX_PUSH_DESCRIPTORS;

				for(c::U64 i = 0; i < count; ++i)
					names[i] = name(pushDescriptorNames.begin()[i]);

				if(count)
					(void) c::ListCharString_createRefConst(names, count, &nameList, nullptr);

				const c::CharString pushName = pushConstantName ? name(pushConstantName) : c::CharString{};

				return c::GraphicsDeviceRef_detectLayoutFromEntry(
					handle(), &binary, entrypoint, flags, (c::EDetectDescriptorLayoutFlags) 0,
					count ? &nameList : nullptr,
					pushConstantName ? &pushName : nullptr,
					pushConstantOut,
					&info,
					count ? pushDescriptorInfo : nullptr,
					e_rr
				);
			}

			//disallowBindlessDescriptor keeps the sampler out of any bindless table, the one this names and the
			// device's default both, so it can only be reached through a bindful binding.

			[[nodiscard]] c::Bool createSampler(
				const c::SamplerInfo &info, const c::C8 *debugName, Sampler &result,
				const DescriptorTable *bindlessTable = nullptr, c::Bool disallowBindlessDescriptor = false,
				c::Error *e_rr = nullptr
			) noexcept {

				const c::CharString n = name(debugName);
				c::SamplerRef *raw = nullptr;

				if(!c::GraphicsDeviceRef_createSampler(
					handle(), info, disallowBindlessDescriptor,
					bindlessTable ? bindlessTable->handle() : nullptr, &n, &raw, e_rr
				))
					return false;

				result = Sampler(RefPtr<c::Sampler>::adopt(raw));
				return true;
			}

			//AABB BLAS (see BLASRef_create):
			// an intersection shader decides the hit,
			// so none of the position/index format business applies. aabbStride is 8 byte aligned;
			// buffer needs EDeviceBufferUsage_ASReadExt like every RTAS input,
			// and build recording stays manual (updateBlas in a scope), exactly like the triangle path.

			[[nodiscard]] c::Bool createBlasProcedural(
				c::ERTASBuildFlags buildFlags, c::EBLASFlag blasFlags,
				c::U32 aabbStride, c::U32 aabbOffset, c::DeviceData buffer,
				const c::C8 *debugName, Blas &result, c::Error *e_rr = nullptr
			) noexcept {

				const c::CharString n = name(debugName);
				c::BLASRef *raw = nullptr;

				if(!c::GraphicsDeviceRef_createBLASProceduralExt(
					handle(), buildFlags, blasFlags, aabbStride, aabbOffset, buffer, &n, &raw, e_rr
				))
					return false;

				result = Blas(RefPtr<c::BLAS>::adopt(raw));
				return true;
			}

			//TLAS whose instances already live on the GPU (see TLASRef_create):
			// instancesDevice points at TLASInstance[] in a DeviceBuffer sized for the instance count,
			// with blasDeviceAddress filled in rather than blasCpu.
			//Nothing is staged, unlike createTlas, the device reads the buffer in place,
			// so the CPU-side refit (setInstances) does not apply.

			[[nodiscard]] c::Bool createTlasDevice(
				c::ERTASBuildFlags buildFlags, const c::DeviceData &instancesDevice,
				const c::C8 *debugName, Tlas &result, c::Error *e_rr = nullptr
			) noexcept {

				const c::CharString n = name(debugName);
				c::TLASRef *raw = nullptr;

				if(!c::GraphicsDeviceRef_createTLASDeviceExt(
					handle(), buildFlags, &instancesDevice, false, nullptr, &n, &raw, e_rr
				))
					return false;

				result = Tlas(RefPtr<c::TLAS>::adopt(raw));
				return true;
			}

			//Passthrough like createBlas:
			// build info with c::OpacityMicromapCreateInfo_create or _uniform (usage counts + entries;
			// both buffers need EDeviceBufferUsage_ASReadExt). usages are borrowed for this call and copied (see
			// OpacityMicromapRef_create).
			//The micromap must be recorded before any BLAS build that links it, same submit or earlier.

			[[nodiscard]] c::Bool createOpacityMicromap(
				const c::OpacityMicromapCreateInfo &info, const c::C8 *debugName,
				OpacityMicromap &result, c::Error *e_rr = nullptr
			) noexcept {

				const c::CharString n = name(debugName);
				c::OpacityMicromapRef *raw = nullptr;

				if(!c::GraphicsDeviceRef_createOpacityMicromapExt(handle(), &info, &n, &raw, e_rr))
					return false;

				result = OpacityMicromap(RefPtr<c::OpacityMicromap>::adopt(raw));
				return true;
			}

			//Graphics pipeline from one SHFile;
			// stageEntries resolve like createRaytracingPipeline's (first binary compatible with the device per entry
			// name). info crosses as a passthrough (see PipelineGraphicsInfo);
			// a DIRECT-RENDERING pipeline (drawn inside CommandScope::render) MUST fill attachmentCountExt +
			// attachmentFormatsExt (ETextureFormatId each) and depthFormatExt to match render()'s attachments -
			// renderPass is ignored then.
			//Without DirectRendering, renderPass is required and the Ext fields are ignored; one, never both.

			[[nodiscard]] c::Bool createGraphicsPipeline(
				const c::PipelineGraphicsInfo &info,
				const c::SHFile &shFile,
				std::initializer_list<const c::C8*> stageEntries,
				const c::C8 *debugName, Pipeline &result,
				const ShaderVariant &variant = {}, c::Error *e_rr = nullptr
			) noexcept {

				const c::Allocator *alloc = c::GraphicsDeviceRef_getAlloc(handle());

				OwnedList<c::ListPipelineStage, c::ListPipelineStage_free> stages(alloc);

				if(!c::ListPipelineStage_resize(&stages.list, stageEntries.size(), alloc, e_rr))
					return false;

				c::U64 n = 0;

				for(const c::C8 *entry : stageEntries) {

					const c::U32 id = resolveEntry(shFile, entry, variant);

					if(id == c::U32_MAX) {
						if(e_rr) *e_rr = c::Error_notFound(0, 0, "createGraphicsPipeline() entry not found or unsupported");
						return false;
					}

					stages.list.ptrNonConst[n++].binaryId = id;
				}

				c::ListSHFile fileList{};
				(void) c::ListSHFile_createRefConst(&shFile, 1, &fileList, nullptr);

				const c::CharString nm = name(debugName);
				c::PipelineRef *raw = nullptr;

				//stages is MOVED:
				// the C side adopts an owned list and zeroes ours (GraphicsDeviceRef_createPipelineRaytracingExt takes
				// the same shape),
				// so OwnedList's free is a no-op afterwards and still correct on the early-exit paths above.

				if(!c::GraphicsDeviceRef_createPipelineGraphics(
					handle(), &fileList, &stages.list, &info, &nm, c::EPipelineFlags_None, nullptr, &raw, e_rr
				))
					return false;

				result = Pipeline(RefPtr<c::Pipeline>::adopt(raw));
				return true;
			}

			//Packed entryId | binaryId<<16, or U32_MAX when no binary in the file can run here.
			//Selection is FIRST-FIT over binary order,
			// so an entry compiled in several extension combinations returns whichever registered first,
			// pass require to steer it, the same prefer/disallow pair the pipeline factories take.

			[[nodiscard]] c::U32 getFirstShaderEntry(
				const c::SHFile &shFile, const c::C8 *entryName,
				c::ESHExtension disallow = c::ESHExtension_None,
				c::ESHExtension require = c::ESHExtension_None
			) const noexcept {
				const c::CharString en = name(entryName);
				return c::GraphicsDeviceRef_getFirstShaderEntry(
					handle(), &shFile, &en, nullptr, nullptr, disallow, require
				);
			}

			//---- submit (acquires swapchain images, uploads dirty resources, executes, presents)

			[[nodiscard]] c::Bool submit(
				std::initializer_list<const CommandList*> lists,
				std::initializer_list<const Swapchain*> swapchains,
				const void *appData, c::U64 appDataLen,
				c::F32 deltaTime = -1, c::F32 time = 0,
				c::Error *e_rr = nullptr
			) noexcept {

				c::CommandListRef *rawLists[16];
				c::SwapchainRef *rawChains[16];

				if(lists.size() > 16 || swapchains.size() > 16) {
					if(e_rr) *e_rr = c::Error_outOfBounds(0, lists.size(), 16, "submit() supports up to 16 lists/swapchains");
					return false;
				}

				c::U64 nl = 0, ns = 0;
				for(const CommandList *l : lists) rawLists[nl++] = l->handle();
				for(const Swapchain *s : swapchains) rawChains[ns++] = s->handle();

				c::ListCommandListRef listRefs{};
				c::ListSwapchainRef chainRefs{};

				if(nl) (void) c::ListCommandListRef_createRefConst(rawLists, nl, &listRefs, nullptr);
				if(ns) (void) c::ListSwapchainRef_createRefConst(rawChains, ns, &chainRefs, nullptr);

				const c::Buffer data = appDataLen ?
					c::Buffer_createRefConst(appData, appDataLen) : c::Buffer{};

				return c::GraphicsDeviceRef_submitCommands(handle(), &listRefs, &chainRefs, &data, deltaTime, time, e_rr);
			}
		};
	}
}

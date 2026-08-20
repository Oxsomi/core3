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

//platforms/window.hpp
//
//Hand-written C++ layer over the window manager, windows and their input devices.
//Hand territory for a different reason than graphics/graphics.hpp:
// nothing here is refcounted, but the C API is built on CALLBACK STRUCTS carrying raw Window*,
// and a consumer writing them by hand ends up re-deriving I32x2 accessors,
// extendedData casts and the manager/window split every time.
//
//Two ways to supply callbacks, both zero-allocation and neither using std::function or virtuals:
//
//  1. A Callbacks table of plain functions. CAPTURELESS LAMBDAS CONVERT, so they can be written
//     inline at the call site:
//         win::Callbacks cb{};
//         cb.onDraw = [](win::Window w) { draw(w); };
//
//  2. createWindow(app, ...) with an object whose MEMBER functions are the callbacks, state
//     lives in the object rather than in captures, which is what a capturing lambda would need
//     and what the C API (a bare function-pointer table) cannot hold:
//         struct App { Camera cam; void onDraw(win::Window w); c::Bool onResize(win::Window, c::Error*); };
//         App app; manager.createWindow(app, size, "title");
//     Members are optional and detected individually; the app object is BORROWED, so it must
//     outlive the window (a local in main around the wait() call, or a static).
//
//A capturing lambda per callback is the one thing deliberately not supported:
// it would mean owning type-erased storage with a destructor inside a C Buffer nothing calls back into.
//
//Include order below is load-bearing:
// window.h DEFINES EWindowHint/EWindowFormat/EWindowType,
// window_manager.h only forward-declares them with C11 `typedef enum E E;`,
// which is ill-formed C++ before the definition, the same hazard graphics.hpp documents for EGraphicsApi.

#pragma once

//file.hpp first:
// it pulls platform.h (and its SIMD closure) into oxc::c with the pre-include discipline this header would otherwise
// have to repeat, and every window app wants both anyway.

#include "platforms/file.hpp"

//Log::errorLn / Log::print are what a window app reports through;
// pulling them here means a consumer needs exactly one include to write a whole application.

#include "types/container/log.hpp"

namespace oxc {

	namespace c {
		#include "platforms/window.h"
		#include "platforms/window_manager.h"
		#include "platforms/monitor.h"
		#include "platforms/keyboard.h"
		#include "platforms/mouse.h"
	}

	namespace win {

		//---------------------------------------------------------------- window

		//Non-owning VIEW of a c::Window.
		//The manager owns the window; a view is what callbacks receive and what a consumer keeps.
		//Copying a view copies a pointer, nothing more.

		class Window {

			c::Window *self;

		public:

			explicit Window(c::Window *w = nullptr) noexcept : self(w) {}

			[[nodiscard]] explicit operator bool() const noexcept { return self; }
			[[nodiscard]] c::Window *handle() const noexcept { return self; }

			[[nodiscard]] c::I32x2 size() const noexcept { return self->size; }
			[[nodiscard]] c::U16 width() const noexcept { return (c::U16) c::I32x2_x(self->size); }
			[[nodiscard]] c::U16 height() const noexcept { return (c::U16) c::I32x2_y(self->size); }
			[[nodiscard]] c::I32x2 cursor() const noexcept { return self->cursor; }
			[[nodiscard]] c::EWindowFlags flags() const noexcept { return self->flags; }

			[[nodiscard]] c::Bool isFullScreen() const noexcept { return c::Window_isFullScreen(self); }
			[[nodiscard]] c::Bool isMinimized() const noexcept { return c::Window_isMinimized(self); }
			[[nodiscard]] c::Bool isFocussed() const noexcept { return c::Window_isFocussed(self); }

			//Zero size means minimized or mid-resize:
			// nothing may be rendered or presented then, which every draw path has to test, so it gets a name here.

			[[nodiscard]] c::Bool isDrawable() const noexcept {
				return self && c::I32x2_x(self->size) > 0 && c::I32x2_y(self->size) > 0;
			}

			[[nodiscard]] c::Bool toggleFullScreen(c::Error *e_rr = nullptr) noexcept {
				return c::Window_toggleFullScreen(self, e_rr);
			}

			[[nodiscard]] c::Bool setTitle(const c::C8 *title, c::Error *e_rr = nullptr) noexcept {
				return c::Window_updatePhysicalTitle(self, c::CharString_createRefCStrConst(title), e_rr);
			}

			c::Bool terminate() noexcept { return c::Window_terminate(self); }

			//The per-window state slot:
			// sized by createWindow's userData, zero-initialised by the C side.
			//Returns nullptr rather than reinterpreting nothing when none was requested.

			template<typename T>
			[[nodiscard]] T *data() const noexcept {
				return self && c::Buffer_length(self->extendedData) >= sizeof(T) ?
					(T*) self->extendedData.ptrNonConst : nullptr;
			}
		};

		//---------------------------------------------------------------- callbacks

		//Wrapper-typed mirror of c::WindowCallbacks.
		//Every member is optional; the thunks below translate the C signatures, so a consumer never names c::Window.

		//What the manager's extendedData holds:
		// the table,
		// plus the borrowed app object that the member-function form dispatches into (null for the plain-function
		// form).

		struct Callbacks;

		struct Table {
			void *app;
			c::Bool (*onCreate)(void*, Window, c::Error*);
			c::Bool (*onResize)(void*, Window, c::Error*);
			void (*onDestroy)(void*, Window);
			void (*onDraw)(void*, Window);
			void (*onUpdate)(void*, Window, c::F64);
			void (*onCursorMove)(void*, Window);
			void (*onButton)(void*, Window, c::InputDevice*, c::InputHandle, c::Bool);
			void (*onAxis)(void*, Window, c::InputDevice*, c::InputHandle, c::F32);
			void (*onTypeChar)(void*, Window, c::CharString);
		};

		struct Callbacks {

			c::Bool (*onCreate)(Window, c::Error*) = nullptr;
			c::Bool (*onResize)(Window, c::Error*) = nullptr;
			void (*onDestroy)(Window) = nullptr;
			void (*onDraw)(Window) = nullptr;
			void (*onUpdate)(Window, c::F64 dt) = nullptr;
			void (*onCursorMove)(Window) = nullptr;
			void (*onButton)(Window, c::InputDevice*, c::InputHandle, c::Bool isDown) = nullptr;
			void (*onAxis)(Window, c::InputDevice*, c::InputHandle, c::F32 value) = nullptr;
			void (*onTypeChar)(Window, c::CharString) = nullptr;
		};

		//Plain-function form adapters:
		// the Callbacks copy lives in the manager slot behind the Table, and app points at it.

		namespace detail {

			[[nodiscard]] inline Callbacks *plainOf(void *app) noexcept { return (Callbacks*) app; }

			inline c::Bool plainCreate(void *a, Window w, c::Error *e) noexcept { return plainOf(a)->onCreate(w, e); }
			inline c::Bool plainResize(void *a, Window w, c::Error *e) noexcept { return plainOf(a)->onResize(w, e); }
			inline void plainDestroy(void *a, Window w) noexcept { plainOf(a)->onDestroy(w); }
			inline void plainDraw(void *a, Window w) noexcept { plainOf(a)->onDraw(w); }
			inline void plainUpdate(void *a, Window w, c::F64 dt) noexcept { plainOf(a)->onUpdate(w, dt); }
			inline void plainCursor(void *a, Window w) noexcept { plainOf(a)->onCursorMove(w); }

			inline void plainButton(
				void *a, Window w, c::InputDevice *d, c::InputHandle h, c::Bool down
			) noexcept {
				plainOf(a)->onButton(w, d, h, down);
			}

			inline void plainAxis(
				void *a, Window w, c::InputDevice *d, c::InputHandle h, c::F32 v
			) noexcept {
				plainOf(a)->onAxis(w, d, h, v);
			}

			inline void plainChar(void *a, Window w, c::CharString s) noexcept { plainOf(a)->onTypeChar(w, s); }

			[[nodiscard]] inline Table tableFor(Callbacks *cb) noexcept {

				Table t{};
				t.app = cb;

				if(cb->onCreate)     t.onCreate = plainCreate;
				if(cb->onResize)     t.onResize = plainResize;
				if(cb->onDestroy)    t.onDestroy = plainDestroy;
				if(cb->onDraw)       t.onDraw = plainDraw;
				if(cb->onUpdate)     t.onUpdate = plainUpdate;
				if(cb->onCursorMove) t.onCursorMove = plainCursor;
				if(cb->onButton)     t.onButton = plainButton;
				if(cb->onAxis)       t.onAxis = plainAxis;
				if(cb->onTypeChar)   t.onTypeChar = plainChar;

				return t;
			}
		}

		//The C side has one callback struct per window and no user pointer beside it,
		// so the wrapper's dispatch state travels in the MANAGER's extendedData rather than the window's.
		//That is not a style choice:
		// onResize fires from inside WindowManager_createWindow (the window is finalized before it returns),
		// so anything keyed on the finished window arrives too late and the first resize,
		// the one that creates the swapchain, is silently dropped.
		//The manager exists before any window does, so its slot is always populated.
		//
		//Hence Registry:
		// one table PER WINDOW, plus a `pending` index naming the slot whose window is still inside createWindow.
		//Dispatch matches on the c::Window it is handed and falls back to the pending slot,
		// which is what makes that first resize land.
		//
		//The slot also OWNS the window.
		//WindowManager_createWindow hands back the only strong reference, the manager's own list borrows it (pushBack,
		// no inc) and OxC3's tests all RefPtr_dec it, so dropping it leaks the Window, its title and its extendedData.
		//Worse silently:
		// forgetWindow erases the list entry on close,
		// so the manager's own "window refs leaked" warning never fires either.

		struct Registry {

			static constexpr c::U32 maxWindows = 8;

			struct Slot {
				c::WindowRef *ref;         //owned; nullptr while the window is mid-creation
				c::Window *window;         //dispatch key, filled once createWindow returns
				Table table;
				Callbacks plain;           //storage for the plain-function form (table.app points here)
				c::Bool used;
				c::Bool destroyed;
			};

			Slot slots[maxWindows];
			c::U32 pending;                //index of the slot being created, or maxWindows for none
		};

		namespace detail {

			[[nodiscard]] inline Registry *registryOf(const c::WindowManager *m) noexcept {
				return m && c::Buffer_length(m->extendedData) >= sizeof(Registry) ?
					(Registry*) m->extendedData.ptrNonConst : nullptr;
			}

			//Matches the window to its slot; before createWindow returns there is no window to match,
			// so the slot being created answers instead.

			[[nodiscard]] inline Table *tableOf(c::Window *w) noexcept {

				if(!w)
					return nullptr;

				Registry *reg = registryOf(w->owner);

				if(!reg)
					return nullptr;

				for(c::U32 i = 0; i < Registry::maxWindows; ++i)
					if(reg->slots[i].used && reg->slots[i].window == w)
						return &reg->slots[i].table;

				return reg->pending < Registry::maxWindows ? &reg->slots[reg->pending].table : nullptr;
			}

			inline c::Bool onCreate(c::Window *w, c::Error *e_rr) noexcept {
				Table *t = tableOf(w);
				return t && t->onCreate ? t->onCreate(t->app, Window(w), e_rr) : true;
			}

			inline c::Bool onResize(c::Window *w, c::Error *e_rr) noexcept {
				Table *t = tableOf(w);
				return t && t->onResize ? t->onResize(t->app, Window(w), e_rr) : true;
			}

			//Marks the slot dead so createWindow can reclaim it later.
			//Deliberately does NOT RefPtr_dec here:
			// Window_free calls forgetWindow which calls this,
			// so decrementing would re-enter the free that is already running.

			inline void onDestroy(c::Window *w) noexcept {

				Table *t = tableOf(w);

				if(t && t->onDestroy)
					t->onDestroy(t->app, Window(w));

				if(Registry *reg = registryOf(w ? w->owner : nullptr))
					for(c::U32 i = 0; i < Registry::maxWindows; ++i)
						if(reg->slots[i].used && reg->slots[i].window == w)
							reg->slots[i].destroyed = true;
			}

			//The C API requires onDraw,
			// so the thunk is always installed even when the consumer supplies none (WindowManager_createWindow
			// requires it).

			inline void onDraw(c::Window *w) noexcept {
				Table *t = tableOf(w);
				if(t && t->onDraw) t->onDraw(t->app, Window(w));
			}

			inline void onUpdate(c::Window *w, c::F64 dt) noexcept {
				Table *t = tableOf(w);
				if(t && t->onUpdate) t->onUpdate(t->app, Window(w), dt);
			}

			inline void onCursorMove(c::Window *w) noexcept {
				Table *t = tableOf(w);
				if(t && t->onCursorMove) t->onCursorMove(t->app, Window(w));
			}

			inline void onButton(c::Window *w, c::InputDevice *d, c::InputHandle h, c::Bool down) noexcept {
				Table *t = tableOf(w);
				if(t && t->onButton) t->onButton(t->app, Window(w), d, h, down);
			}

			inline void onAxis(c::Window *w, c::InputDevice *d, c::InputHandle h, c::F32 v) noexcept {
				Table *t = tableOf(w);
				if(t && t->onAxis) t->onAxis(t->app, Window(w), d, h, v);
			}

			inline void onTypeChar(c::Window *w, c::CharString str) noexcept {
				Table *t = tableOf(w);
				if(t && t->onTypeChar) t->onTypeChar(t->app, Window(w), str);
			}

			//Member-function form:
			// one thunk per callback, generated for the app type and used only when that member exists,
			// so an app declares exactly what it handles.

			template<typename T>
			struct Bind {

				static c::Bool onCreate(void *a, Window w, c::Error *e_rr) noexcept {
					return ((T*) a)->onCreate(w, e_rr);
				}

				static c::Bool onResize(void *a, Window w, c::Error *e_rr) noexcept {
					return ((T*) a)->onResize(w, e_rr);
				}

				static void onDestroy(void *a, Window w) noexcept { ((T*) a)->onDestroy(w); }
				static void onDraw(void *a, Window w) noexcept { ((T*) a)->onDraw(w); }
				static void onUpdate(void *a, Window w, c::F64 dt) noexcept { ((T*) a)->onUpdate(w, dt); }
				static void onCursorMove(void *a, Window w) noexcept { ((T*) a)->onCursorMove(w); }

				static void onButton(
					void *a, Window w, c::InputDevice *d, c::InputHandle h, c::Bool down
				) noexcept {
					((T*) a)->onButton(w, d, h, down);
				}

				static void onAxis(
					void *a, Window w, c::InputDevice *d, c::InputHandle h, c::F32 v
				) noexcept {
					((T*) a)->onAxis(w, d, h, v);
				}

				static void onTypeChar(void *a, Window w, c::CharString str) noexcept {
					((T*) a)->onTypeChar(w, str);
				}

				[[nodiscard]] static Table make(T &app) noexcept {

					Table t{};
					t.app = &app;

					if constexpr(requires(T &x, Window w, c::Error *e) { x.onCreate(w, e); })
						t.onCreate = onCreate;

					if constexpr(requires(T &x, Window w, c::Error *e) { x.onResize(w, e); })
						t.onResize = onResize;

					if constexpr(requires(T &x, Window w) { x.onDestroy(w); })
						t.onDestroy = onDestroy;

					if constexpr(requires(T &x, Window w) { x.onDraw(w); })
						t.onDraw = onDraw;

					if constexpr(requires(T &x, Window w, c::F64 dt) { x.onUpdate(w, dt); })
						t.onUpdate = onUpdate;

					if constexpr(requires(T &x, Window w) { x.onCursorMove(w); })
						t.onCursorMove = onCursorMove;

					if constexpr(requires(T &x, Window w, c::InputDevice *d, c::InputHandle h) {
						x.onButton(w, d, h, true);
					})
						t.onButton = onButton;

					if constexpr(requires(T &x, Window w, c::InputDevice *d, c::InputHandle h) {
						x.onAxis(w, d, h, 0.f);
					})
						t.onAxis = onAxis;

					if constexpr(requires(T &x, Window w, c::CharString s) { x.onTypeChar(w, s); })
						t.onTypeChar = onTypeChar;

					return t;
				}
			};
		}

		//---------------------------------------------------------------- input

		//Buttons and axes arrive as an InputHandle whose meaning depends on the device;
		// these are the two decodes every consumer writes, so they live here rather than in each app.

		[[nodiscard]] inline c::Bool isKeyboard(const c::InputDevice *d) noexcept {
			return d && d->type == c::EInputDeviceType_Keyboard;
		}

		[[nodiscard]] inline c::Bool isMouse(const c::InputDevice *d) noexcept {
			return d && d->type == c::EInputDeviceType_Mouse;
		}

		[[nodiscard]] inline c::EKey key(c::InputHandle h) noexcept { return (c::EKey) h; }

		//Mouse buttons and axes share one enum, buttons starting at EMouseAxis_End.

		[[nodiscard]] inline c::EMouseActions mouse(c::InputHandle h) noexcept {
			return (c::EMouseActions) h;
		}

		//---------------------------------------------------------------- manager

		//Owns the c::WindowManager.
		//Move-only for the same reason gfx::Instance is immovable:
		// the C side keeps pointers into it (windows hold `owner`), so the object stays put.

		class Manager {

			c::WindowManager self{};
			c::Bool created = false;

		public:

			Manager() noexcept = default;
			~Manager() noexcept { release(); }

			Manager(const Manager&) = delete;
			Manager &operator=(const Manager&) = delete;
			Manager(Manager&&) = delete;
			Manager &operator=(Manager&&) = delete;

			//Drops our window references BEFORE freeing the manager,
			// so each Window runs its own free path and leaves manager->windows empty.
			//Leaving them would trip the C side's "shut down with window refs leaked ... could cause corruption"
			// force-dec loop.

			void release() noexcept {

				if(created) {
					reap(true);
					c::WindowManager_free(&self);
				}

				created = false;
				self = c::WindowManager{};
			}

			[[nodiscard]] c::Bool create(c::Error *e_rr = nullptr) noexcept {
				release();
				c::WindowManagerCallbacks cb{};
				created = c::WindowManager_create(cb, sizeof(Registry), &self, e_rr);
				initRegistry();
				return created;
			}

			//Manager-level callbacks stay raw:
			// they carry no wrapper type worth translating,
			// and an app that wants them is already writing C-shaped code around the manager itself.

			//Manager-level callbacks stay raw:
			// they carry no wrapper type worth translating.
			//The window table is reserved first, so userData starts behind it, reach it with Manager::data<T>().

			[[nodiscard]] c::Bool create(
				c::WindowManagerCallbacks cb, c::U64 userData, c::Error *e_rr = nullptr
			) noexcept {
				release();
				created = c::WindowManager_create(cb, sizeof(Registry) + userData, &self, e_rr);
				initRegistry();
				return created;
			}

			template<typename T>
			[[nodiscard]] T *data() noexcept {
				return c::Buffer_length(self.extendedData) >= sizeof(Registry) + sizeof(T) ?
					(T*)(self.extendedData.ptrNonConst + sizeof(Registry)) : nullptr;
			}

			//Live windows, in slot order.
			//Lets an app iterate what it opened without keeping its own list;
			// a slot whose window has closed is skipped.

			[[nodiscard]] Window window(c::U32 i) noexcept {
				Registry *reg = detail::registryOf(&self);
				return reg && i < Registry::maxWindows && reg->slots[i].used && !reg->slots[i].destroyed ?
					Window(reg->slots[i].window) : Window();
			}

		private:

			void initRegistry() noexcept {
				if(Registry *reg = detail::registryOf(&self))
					reg->pending = Registry::maxWindows;      //zeroed extendedData would mean slot 0
			}

			//Releases the references of windows that have closed.
			//Called at the top of every createWindow so slots recycle over a long session,
			// and with all=true from release().
			//The reference is taken OUT of the slot before decrementing:
			// Window_free re-enters detail::onDestroy, which must find nothing left to do.

			void reap(c::Bool all = false) noexcept {

				Registry *reg = detail::registryOf(&self);

				if(!reg)
					return;

				for(c::U32 i = 0; i < Registry::maxWindows; ++i) {

					Registry::Slot &slot = reg->slots[i];

					if(!slot.used || !(all || slot.destroyed))
						continue;

					c::WindowRef *ref = slot.ref;
					slot = Registry::Slot{};

					if(ref)
						c::RefPtr_dec(&ref);
				}
			}

			//Both createWindow forms claim their slot and fill its table BEFORE the C call:
			// it fires onCreate and onResize before returning,
			// so anything installed afterwards misses the first resize, the one that creates a swapchain.

			[[nodiscard]] Registry::Slot *claimSlot(c::Error *e_rr) noexcept {

				Registry *reg = detail::registryOf(&self);

				if(!reg) {
					if(e_rr)
						*e_rr = c::Error_invalidState(0, "createWindow() manager was not created by this wrapper");
					return nullptr;
				}

				reap();

				for(c::U32 i = 0; i < Registry::maxWindows; ++i)
					if(!reg->slots[i].used) {
						reg->slots[i] = Registry::Slot{};
						reg->slots[i].used = true;
						reg->pending = i;
						return &reg->slots[i];
					}

				if(e_rr)
					*e_rr = c::Error_outOfBounds(
						0, Registry::maxWindows, Registry::maxWindows,
						"createWindow() manager is already at Registry::maxWindows live windows"
					);

				return nullptr;
			}

			[[nodiscard]] c::Bool createWindowRaw(
				c::I32x2 size, const c::C8 *title, c::U64 userData, c::Error *e_rr,
				c::EWindowType type, c::EWindowFormat format, c::EWindowHint hint,
				c::I32x2 position, c::I32x2 minSize, c::I32x2 maxSize
			) noexcept {

				c::WindowCallbacks raw{};
				raw.onCreate = detail::onCreate;
				raw.onResize = detail::onResize;
				raw.onDestroy = detail::onDestroy;
				raw.onDraw = detail::onDraw;
				raw.onUpdate = detail::onUpdate;
				raw.onCursorMove = detail::onCursorMove;
				raw.onDeviceButton = detail::onButton;
				raw.onDeviceAxis = detail::onAxis;
				raw.onTypeChar = detail::onTypeChar;

				c::WindowRef *w = nullptr;

				const c::Bool ok = c::WindowManager_createWindow(
					&self, type, position, size, minSize, maxSize, hint,
					c::CharString_createRefCStrConst(title), raw, format, userData, &w, e_rr
				);

				Registry *reg = detail::registryOf(&self);
				const c::U32 i = reg ? reg->pending : Registry::maxWindows;

				if(reg)
					reg->pending = Registry::maxWindows;

				if(!ok) {
					if(reg && i < Registry::maxWindows)
						reg->slots[i] = Registry::Slot{};
					return false;
				}

				//The slot takes the strong reference over; from here release() is what frees it.

				reg->slots[i].ref = w;
				reg->slots[i].window = RefPtr_data(w, c::Window);
				return true;
			}

		public:

			//Member-function form:
			// app's onDraw/onResize/onButton/... become the callbacks, each detected independently,
			// so an app declares only what it handles. app is BORROWED - it must outlive the window.

			template<typename T>
			[[nodiscard]] c::Bool createWindow(
				T &app, c::I32x2 size, const c::C8 *title,
				c::U64 userData = 0, c::Error *e_rr = nullptr,
				c::EWindowType type = c::EWindowType_Physical,
				c::EWindowFormat format = c::EWindowFormat_AutoRGBA8,
				c::EWindowHint hint = c::EWindowHint_Default,
				c::I32x2 position = c::I32x2_zero,
				c::I32x2 minSize = c::I32x2_zero,
				c::I32x2 maxSize = c::I32x2_zero
			) noexcept {

				Registry::Slot *slot = claimSlot(e_rr);

				if(!slot)
					return false;

				slot->table = detail::Bind<T>::make(app);

				return createWindowRaw(
					size, title, userData, e_rr, type, format, hint, position, minSize, maxSize
				);
			}

			//userData sizes the window's own state slot,
			// reachable with Window::data<T>(). size/minSize/maxSize follow the C API:
			// zero means "platform default".

			[[nodiscard]] c::Bool createWindow(
				Callbacks callbacks, c::I32x2 size, const c::C8 *title,
				c::U64 userData = 0, c::Error *e_rr = nullptr,
				c::EWindowType type = c::EWindowType_Physical,
				c::EWindowFormat format = c::EWindowFormat_AutoRGBA8,
				c::EWindowHint hint = c::EWindowHint_Default,
				c::I32x2 position = c::I32x2_zero,
				c::I32x2 minSize = c::I32x2_zero,
				c::I32x2 maxSize = c::I32x2_zero
			) noexcept {

				Registry::Slot *slot = claimSlot(e_rr);

				if(!slot)
					return false;

				slot->plain = callbacks;                      //by value: the caller's copy may be a temporary
				slot->table = detail::tableFor(&slot->plain);

				return createWindowRaw(
					size, title, userData, e_rr, type, format, hint, position, minSize, maxSize
				);
			}

			//Pumps events until every window has closed.

			[[nodiscard]] c::Bool wait(c::Error *e_rr = nullptr) noexcept {
				return c::WindowManager_wait(&self, e_rr);
			}

			[[nodiscard]] c::WindowManager *handle() noexcept { return &self; }
		};
	}
}

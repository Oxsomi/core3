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

//types/container/job_queue.hpp

#pragma once
#include <memory>
#include <utility>
#include <type_traits>

//Pre-include system headers used by the C headers below at global scope;
//they must not be pulled in for the first time inside a namespace.

#include <atomic>
#include <stdalign.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

namespace oxc {

	namespace c {
		#include "types/container/job_queue.h"
		#include "types/base/error.h"
		#include "types/base/allocator.h"
		#include "types/base/buffer_base.h"
	}

	//RAII wrapper around the C JobQueue.
	//This is syntax sugar only; all real work goes through the C API, keeping the ABI stable.
	//On top of the raw C callbacks it allows pushing C++ callables (lambdas with captures),
	//which are stored via the queue's own Allocator and destroyed after they ran.
	//Callable signature: Bool(c::U64 threadId); returning false marks the queue as failed.

	class JobQueue {

		c::JobQueue queue;
		c::Bool initialized;

		//Storage for a pushed callable; freed by the trampoline after execution.

		template<typename F>
		struct JobStorage {

			c::Allocator alloc;
			F func;

			JobStorage(const c::Allocator &alloc, F &&f) noexcept : alloc(alloc), func(std::move(f)) {}

			static c::Bool run(void *data, c::U64 threadId, c::JobQueue*) {
				JobStorage *self = (JobStorage*) data;
				const c::Bool res = self->func(threadId);
				destroy(self);
				return res;
			}

			//Runs ~F and frees the storage. Used by run() after execution and, via JobDestructor,
			//by JobQueue_free for jobs discarded on shutdown (so captures don't leak).

			static void destroy(void *data) {
				JobStorage *self = (JobStorage*) data;
				const c::Allocator allocCopy = self->alloc;
				self->~JobStorage();
				allocCopy.free(allocCopy.ptr, c::Buffer_createManagedPtr(self, sizeof(JobStorage)));
			}
		};

		void release() noexcept {

			//JobQueue_free (C side) resets the struct to zero.
			//No C++ assignment here, since the struct contains std::atomic when compiled as C++.

			if(initialized)
				c::JobQueue_free(&queue);

			initialized = false;
		}

	public:

		JobQueue() noexcept : initialized(false) {

			//Zero state expected by JobQueue_create.
			//Byte-wise (not C++ assignment) since the struct contains std::atomic members when compiled as C++.

			(void) c::Buffer_unsetAllBits(c::Buffer_createRef(&queue, sizeof(queue)), nullptr);
		}
		
		~JobQueue() noexcept { release(); }

		JobQueue(const JobQueue&) = delete;
		JobQueue &operator=(const JobQueue&) = delete;
		JobQueue(JobQueue&&) = delete;                  //Address stable; workers point at the C struct
		JobQueue &operator=(JobQueue&&) = delete;

		//threadCount <= 1 = single threaded (inline) mode, see job_queue.h

		[[nodiscard]] c::Bool init(c::U64 threadCount, const c::Allocator &alloc, c::Error *e_rr = nullptr) noexcept {
			release();
			initialized = c::JobQueue_create(threadCount, &alloc, &queue, e_rr);
			return initialized;
		}

		//Raw C style push; data is owned by the caller

		[[nodiscard]] c::Bool push(c::JobCallback callback, void *data, c::Error *e_rr = nullptr) noexcept {
			return c::JobQueue_push(&queue, callback, data, e_rr);
		}

		//C++ style push; the callable (and its captures) is copied/moved into
		//allocator-owned storage and destroyed after it ran.

		template<typename F>
		[[nodiscard]] c::Bool push(F &&f, c::Error *e_rr = nullptr) noexcept {

			using Storage = JobStorage<typename std::decay<F>::type>;

			c::Buffer buf{};

			if(!queue.alloc || !queue.alloc->alloc || !queue.alloc->alloc(queue.alloc->ptr, sizeof(Storage), &buf, e_rr))
				return false;

			Storage *storage = std::construct_at((Storage*)buf.ptrNonConst, *queue.alloc, std::forward<F>(f));

			//Register destroy as the discard destructor so a queued-but-never-run job frees itself on shutdown.

			if (!c::JobQueue_pushDestructor(&queue, &Storage::run, storage, &Storage::destroy, e_rr)) {
				Storage::destroy(storage);
				return false;
			}

			return true;
		}

		[[nodiscard]] c::Bool wait(c::Error *e_rr = nullptr) noexcept { return c::JobQueue_wait(&queue, e_rr); }

		[[nodiscard]] c::Bool isSuccess() const noexcept { return c::JobQueue_isSuccess(&queue); }
		[[nodiscard]] c::U64 threadCount() const noexcept { return c::JobQueue_threadCount(&queue); }

		//Access to the underlying C queue for interop with C code

		[[nodiscard]] c::JobQueue *handle() noexcept { return &queue; }
	};
}

#include "platforms/window.h"
#include "platforms/platform.h"
#include "types/assert.h"

struct Window *Window_createVirtual(
	f32x4 size, struct WindowCallbacks callbacks, enum WindowFormat format
) {

	ocAssert("Can't init size to <0", !Vec_any(Vec_lt(size, Vec_zero())));

	AllocFunc alloc = Platform_instance.alloc.alloc;
	void *allocator = Platform_instance.alloc.ptr;

	struct Window *w = (struct Window*) alloc(allocator, sizeof(struct Window));

	ocAssert("Couldn't allocate window", w);

	void *nativeHandle;

	*w = (struct Window) {
		.size = size,
		.callbacks = callbacks,
		.format = format,
		.flags = WindowFlags_IsVirtual | WindowFlags_IsFocussed
	};

	usz size1D = Vec_x(size) * Vec_y(size);

	if (size1D > 0) {

		struct Buffer *b = (struct Buffer*) alloc(allocator, sizeof(struct Buffer));

		ocAssert("Couldn't allocate buffer", b);

		size1D *= formatSize;
		void *bufData = alloc(allocator, size1D);

		ocAssert("Couldn't allocate buffer data", bufData);

		*b = (struct Buffer) { .ptr = bufData, .siz = size1D };

		w->nativeHandle = b;
	}

	return w;
}

void Window_freeVirtual(struct Window **w) {

	if (!w || !*w)
		return;

	FreeFunc free = Platform_instance.alloc.free;
	void *allocator = Platform_instance.alloc.alloc;

	void *bufPtr = (*w)->nativeHandle;

	if(bufPtr) {

		void *datPtr = ((struct Buffer*)bufPtr)->ptr;

		if(datPtr)
			free(allocator, (struct Buffer){ .ptr = datPtr, .siz = ((struct Buffer*) bufPtr)->siz });

		free(allocator, (struct Buffer){ .ptr = bufPtr, .siz = sizeof(struct Buffer) });
	}

	free(allocator, (struct Buffer){ .ptr = *w, .siz = sizeof(struct Window) });
	*w = NULL;
}

void Window_presentVirtual(const struct Window *w, struct Buffer data, enum WindowFormat encodedFormat);

void Window_resizeVirtual(struct Window *w, bool copyData);

struct Buffer Window_getVirtualData(const struct Window *w) {

	if (!Window_isVirtual(w) || !w->nativeHandle)
		return (struct Buffer) { 0 };

	return *(const struct Buffer*) w->nativeHandle;
}
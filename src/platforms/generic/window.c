#include "platforms/window.h"
#include "platforms/platform.h"
#include "types/error.h"
#include "types/bit.h"
#include "formats/texture.h"

const usz WindowManager_maxVirtualWindowCount = 16;

struct Error Window_createVirtual(
	i32x2 size, struct WindowCallbacks callbacks, enum WindowFormat format,
	struct Window **w
) {

	if(!w)
		return (struct Error) { .genericError = GenericError_NullPointer, .paramId = 3 };

	if(*w)
		return (struct Error) { .genericError = GenericError_InvalidParameter, .paramId = 3 };

	switch (format) {

		case WindowFormat_rgba8:
		case WindowFormat_hdr10a2:
		case WindowFormat_rgba16f:
		case WindowFormat_rgba32f:
			break;

		default:
			return (struct Error) { .genericError = GenericError_InvalidParameter, .paramId = 2 };
	}

	if(i32x2_leq(size, i32x2_zero()))
		return (struct Error) { .genericError = GenericError_InvalidParameter };

	AllocFunc alloc = Platform_instance.alloc.alloc;
	FreeFunc free = Platform_instance.alloc.free;
	void *allocator = Platform_instance.alloc.ptr;

	struct Buffer buffer = (struct Buffer){ 0 };
	struct Error err = alloc(allocator, sizeof(struct Window), &buffer);

	if(err.genericError)
		return err;

	struct Window *win = (*w = (struct Window*) buffer.ptr);

	void *nativeHandle;

	*win = (struct Window) {
		.size = size,
		.callbacks = callbacks,
		.format = format,
		.flags = WindowFlags_IsVirtual | WindowFlags_IsFocussed
	};

	err = alloc(allocator, sizeof(struct Buffer), &buffer);

	if(err.genericError) {
		free(allocator, Bit_createRef(w, sizeof(struct Window)));
		return err;
	}

	struct Buffer *buf = (struct Buffer*) buffer.ptr;

	usz size1D = TextureFormat_getSize((enum TextureFormat) format, i32x2_x(size), i32x2_y(size));
	err = alloc(allocator, size1D, &buffer);

	if(err.genericError) {
		free(allocator, Bit_createRef(w, sizeof(struct Window)));
		free(allocator, Bit_createRef(buf, sizeof(struct Buffer)));
		return err;
	}

	*buf = buffer;
	win->nativeHandle = buf;

	return Error_none();
}

struct Error Window_freeVirtual(struct Window **w) {

	if (!w || !*w)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if(!Window_isVirtual(*w))
		return (struct Error) { .genericError = GenericError_InvalidParameter };

	FreeFunc free = Platform_instance.alloc.free;
	void *allocator = Platform_instance.alloc.alloc;

	struct Buffer *bufPtr = (struct Buffer*) (*w)->nativeHandle;

	struct Error err = Error_none(), errTemp;

	if(bufPtr) {

		if(bufPtr) {

			errTemp = free(allocator, *bufPtr);

			if(errTemp.genericError)
				err = errTemp;
		}

		errTemp = free(allocator, (struct Buffer){ .ptr = bufPtr, .siz = sizeof(struct Buffer) });

		if(errTemp.genericError)
			err = errTemp;
	}

	errTemp = free(allocator, (struct Buffer){ .ptr = *w, .siz = sizeof(struct Window) });

	if(errTemp.genericError)
		err = errTemp;

	*w = NULL;
	return err;
}

//TODO: Move this to texture class

struct Error Window_presentVirtual(const struct Window *w, struct Buffer data, enum WindowFormat encodedFormat);

struct Error Window_resizeVirtual(struct Window *w, bool copyData, i32x2 newSiz) {

	if (!w)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if(!Window_isVirtual(w))
		return (struct Error) { .genericError = GenericError_InvalidParameter };

	if(i32x4_eq2(w->size, newSiz))
		return Error_none();

	//Because we're resizing, we assume we will be resizing more often
	//To combat constantly reallocating, we will allocate 25% more memory than is needed.
	//And if our buffer should be 50% the size we downsize it to +25% of the real size.
	//Of course we will still have to resize sometimes

	struct Buffer old = *(struct Buffer*) w->nativeHandle;
	struct Buffer neo = old;

	AllocFunc alloc = Platform_instance.alloc.alloc;
	FreeFunc free = Platform_instance.alloc.free;
	void *allocator = Platform_instance.alloc.ptr;

	usz linSizOld = TextureFormat_getSize((enum TextureFormat) w->format, i32x2_x(w->size), i32x2_y(w->size));
	usz linSiz = TextureFormat_getSize((enum TextureFormat) w->format, i32x2_x(newSiz), i32x2_y(newSiz));

	//We need to grow; we're out of bounds

	bool resize = false;

	if (linSiz >= old.siz) {

		usz toAllocate = linSiz * 5 / 4;

		struct Error err = alloc(allocator, toAllocate, &neo);

		if(err.genericError)
			return err;

		resize = true;
	}

	//We need to shrink; we're way over allocated (>50%)

	else if (old.siz > linSiz * 3 / 2) {

		usz toAllocate = linSiz * 5 / 4;

		struct Error err = alloc(allocator, toAllocate, &neo);

		if(err.genericError)
			return err;

		resize = true;
	}

	//We need to preserve the data.
	//But stride might have changed, so we need to fill the gaps.

	if (copyData) {

		//Row remains in tact, no resize needed

		if (i32x2_x(w->siz) == i32x2_x(newSiz)) {

			//If we resized the buffer, we still have to copy the old data

			if(resize)
				Bit_copy(neo, Bit_createRef(old.ptr, (usz) u64_min(linSizOld, linSiz)));

			//If we added size, we need to clear those pixels

			if(i32x2_y(newSiz) > i32x2_y(w->siz)) {

				struct Error err = Bit_unsetAll(Bit_createRef(neo.ptr + linSizOld, linSiz - linSizOld));

				//Revert to old size

				if(err.genericError) {
				
					if(resize)
						free(allocator, neo);

					return err;
				}
			}
		}

		//Row changed; we have to unfortunately copy this over

		else {

			usz rowSiz = linSiz / i32x2_y(newSiz);
			usz rowSizOld = linSizOld / i32x2_y(w->siz);

			//Grab buffers for simple copies later

			struct Buffer src = (struct Buffer) { 0 };
			struct Error err = Bit_createSubset(old, 0, rowSizOld, &src);

			if(err.genericError) {

				if(resize)
					free(allocator, neo);

				return err;
			}

			struct Buffer dst = (struct Buffer) { 0 };
			err = Bit_createSubset(neo, 0, rowSiz, &src);

			if(err.genericError) {

				if(resize)
					free(allocator, neo);

				return err;
			}

			//First we ensure everything is copied to the right location

			i32 smallY = i32x2_min(i32x2_y(newSiz), i32x2_y(w->siz));

			//We're growing, so we want to copy from high to low
			//This is because it ends up at a higher address than source and 
			//we can safely read from lower addresses

			if(i32x2_x(newSiz) > i32x2_x(w->siz)) {

				//Reverse loop

				dst.ptr += linSiz - rowSiz;
				src.ptr += linSizOld - rowSizOld;

				usz sizDif = linSiz - linSizOld;

				for (i32 i = 0; i < smallY; ++i) {

					//Clear remainder of row

					struct Buffer toClear = (struct Buffer) { 0 };
					struct Error err = Bit_createSubset(dst, linSizOld, sizDif, &toClear);

					if (err.genericError) {

						if(resize)
							free(allocator, neo);

						return err;
					}

					Bit_unsetAll(toClear);

					//Copy part 

					Bit_revCopy(dst, src);		//Automatically truncates src

					//Jump backwards

					dst.ptr -= rowSiz;
					src.ptr -= rowSizOld;
				}
			}

			//We're shrinking, so we want to copy from low to high
			//This is because it ends up at a lower address than source and
			//we can safely read from higher addresses

			else {

				//

				for (i32 i = 0; i < smallY; ++i) {

					//We can skip copying the first if it's the same buffer

					if (!i && resize)
						continue;

					Bit_copy(dst, src);		//Automatically truncates src
					dst.ptr += rowSiz;
					src.ptr += rowSizOld;
				}
			}
		}

	}

	//Ensure it's all properly cleared

	else Bit_unsetAll(Bit_createRef(neo.ptr, linSiz));

	//Get rid of our old data

	if (resize) {

		//If this gives an error, we don't care because we already have the new buffer

		struct Error err = free(allocator, old);
		err;
	}

	*(struct Buffer*) w->nativeHandle = neo;
	w->size = newSiz;
	return Error_none();
}

struct Buffer Window_getVirtualData(const struct Window *w) {

	if (!Window_isVirtual(w) || !w->nativeHandle)
		return (struct Buffer) { 0 };

	struct Buffer *buf = (const struct Buffer*) w->nativeHandle;
	return buf ? *buf : (struct Buffer){ 0 };
}
#include "platforms/window.h"
#include "platforms/platform.h"
#include "file/file.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/string.h"
#include "formats/texture.h"
#include "formats/bmp.h"

const U8 WindowManager_maxTotalVirtualWindowCount = 16;

struct Error Window_createVirtual(
	I32x2 size, struct WindowCallbacks callbacks, enum WindowFormat format,
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

	if(I32x2_any(I32x2_leq(size, I32x2_zero())))
		return (struct Error) { .genericError = GenericError_InvalidParameter };

	struct Buffer buffer = Buffer_createNull();
	struct Error err = Buffer_createUninitializedBytes(sizeof(struct Window), Platform_instance.alloc, &buffer);

	if(err.genericError)
		return err;

	struct Window *win = (*w = (struct Window*) buffer.ptr);

	*win = (struct Window) {
		.size = size,
		.callbacks = callbacks,
		.format = format,
		.flags = WindowFlags_IsVirtual | WindowFlags_IsFocussed
	};

	U64 size1D = TextureFormat_getSize((enum TextureFormat) format, I32x2_x(size), I32x2_y(size));
	err = Buffer_createZeroBits(size1D, Platform_instance.alloc, &win->cpuVisibleBuffer);

	if(err.genericError) {
		buffer = Buffer_createRef(w, sizeof(struct Window));
		Buffer_free(&buffer, Platform_instance.alloc);
		return err;
	}

	return Error_none();
}

struct Error Window_freeVirtual(struct Window **w) {

	if (!w || !*w)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if(!Window_isVirtual(*w))
		return (struct Error) { .genericError = GenericError_InvalidParameter };

	struct Error err = Error_none(), errTemp;

	struct Buffer buf = (*w)->cpuVisibleBuffer;
	err = Buffer_free(&buf, Platform_instance.alloc);

	buf = Buffer_createRef(w, sizeof(struct Window));
	errTemp = Buffer_free(&buf, Platform_instance.alloc);

	if(errTemp.genericError)
		err = errTemp;

	*w = NULL;
	return err;
}

//TODO: Move this to texture class

struct Error Window_presentVirtual(const struct Window *w, struct Buffer data, enum WindowFormat encodedFormat);

struct Error Window_resizeVirtual(struct Window *w, Bool copyData, I32x2 newSiz) {

	if (!w)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if(!Window_isVirtual(w) && !(w->flags & WindowHint_ProvideCPUBuffer))
		return (struct Error) { .genericError = GenericError_InvalidParameter };

	if(I32x4_eq2(w->size, newSiz))
		return Error_none();

	//Because we're resizing, we assume we will be resizing more often
	//To combat constantly reallocating, we will allocate 25% more memory than is needed.
	//And if our buffer should be 50% the size we downsize it to +25% of the real size.
	//Of course we will still have to resize sometimes

	struct Buffer old = w->cpuVisibleBuffer;
	struct Buffer neo = old;

	U64 linSizOld = TextureFormat_getSize((enum TextureFormat) w->format, I32x2_x(w->size), I32x2_y(w->size));
	U64 linSiz = TextureFormat_getSize((enum TextureFormat) w->format, I32x2_x(newSiz), I32x2_y(newSiz));

	//We need to grow; we're out of bounds

	Bool resize = false;

	if (linSiz >= old.siz) {

		U64 toAllocate = linSiz * 5 / 4;

		struct Error err = Buffer_createUninitializedBytes(toAllocate, Platform_instance.alloc, &neo);

		if(err.genericError)
			return err;

		resize = true;
	}

	//We need to shrink; we're way over allocated (>50%)

	else if (old.siz > linSiz * 3 / 2) {

		U64 toAllocate = linSiz * 5 / 4;

		struct Error err = Buffer_createUninitializedBytes(toAllocate, Platform_instance.alloc, &neo);

		if(err.genericError)
			return err;

		resize = true;
	}

	//We need to preserve the data.
	//But stride might have changed, so we need to fill the gaps.

	if (copyData) {

		//Row remains in tact, no resize needed

		if (I32x2_x(w->size) == I32x2_x(newSiz)) {

			//If we resized the buffer, we still have to copy the old data

			if(resize)
				Buffer_copy(neo, Buffer_createRef(old.ptr, (U64) U64_min(linSizOld, linSiz)));

			//If we added size, we need to clear those pixels

			if(I32x2_y(newSiz) > I32x2_y(w->size)) {

				struct Error err = Buffer_unsetAllBits(Buffer_createRef(neo.ptr + linSizOld, linSiz - linSizOld));

				//Revert to old size

				if(err.genericError) {
				
					if(resize)
						Buffer_free(&neo, Platform_instance.alloc);

					return err;
				}
			}
		}

		//Row changed; we have to unfortunately copy this over

		else {

			U64 rowSiz = linSiz / I32x2_y(newSiz);
			U64 rowSizOld = linSizOld / I32x2_y(w->size);

			//Grab buffers for simple copies later

			struct Buffer src = Buffer_createNull();
			struct Error err = Buffer_createSubset(old, 0, rowSizOld, &src);

			if(err.genericError) {

				if(resize)
					Buffer_free(&neo, Platform_instance.alloc);

				return err;
			}

			struct Buffer dst = Buffer_createNull();
			err = Buffer_createSubset(neo, 0, rowSiz, &src);

			if(err.genericError) {

				if(resize)
					Buffer_free(&neo, Platform_instance.alloc);

				return err;
			}

			//First we ensure everything is copied to the right location

			I32 smallY = (I32) U64_min(I32x2_y(newSiz), I32x2_y(w->size));

			//We're growing, so we want to copy from high to low
			//This is because it ends up at a higher address than source and 
			//we can safely read from lower addresses

			if(I32x2_x(newSiz) > I32x2_x(w->size)) {

				//Reverse loop

				dst.ptr += linSiz - rowSiz;
				src.ptr += linSizOld - rowSizOld;

				U64 sizDif = linSiz - linSizOld;

				for (I32 i = 0; i < smallY; ++i) {

					//Clear remainder of row

					struct Buffer toClear = Buffer_createNull();
					struct Error err = Buffer_createSubset(dst, linSizOld, sizDif, &toClear);

					if (err.genericError) {

						if(resize)
							Buffer_free(&neo, Platform_instance.alloc);

						return err;
					}

					Buffer_unsetAllBits(toClear);

					//Copy part 

					Buffer_revCopy(dst, src);		//Automatically truncates src

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

				for (I32 i = 0; i < smallY; ++i) {

					//We can skip copying the first if it's the same buffer

					if (!i && !resize)
						continue;

					Buffer_copy(dst, src);		//Automatically truncates src
					dst.ptr += rowSiz;
					src.ptr += rowSizOld;
				}
			}
		}

	}

	//Ensure it's all properly cleared

	else Buffer_unsetAllBits(Buffer_createRef(neo.ptr, linSiz));

	//Get rid of our old data

	if (resize) {

		//If this gives an error, we don't care because we already have the new buffer

		struct Error err = Buffer_free(&old, Platform_instance.alloc);
		err;
	}

	w->cpuVisibleBuffer = neo;
	w->size = newSiz;
	return Error_none();
}

struct Error Window_storeCPUBufferToDisk(const struct Window *w, struct String filePath) {

	if (!w)
		return (struct Error) { .genericError = GenericError_NullPointer };

	struct Buffer buf = w->cpuVisibleBuffer;

	if(!buf.siz)
		return (struct Error) { .genericError = GenericError_InvalidOperation };

	if(w->format != WindowFormat_rgba8)
		return (struct Error) { .genericError = GenericError_UnsupportedOperation };		//TODO: Add support for other formats

	struct Buffer file = Buffer_createNull();

	struct Error err = BMP_writeRGBA(
		buf, I32x2_x(w->size), I32x2_y(w->size),
		false, Platform_instance.alloc, &file
	);

	if(err.genericError)
		return err;

	err = File_write(file, filePath);
	Buffer_free(&file, Platform_instance.alloc);

	return err;
}

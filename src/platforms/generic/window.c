/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "platforms/ext/listx_impl.h"
#include "platforms/window.h"
#include "platforms/platform.h"
#include "types/thread.h"
#include "platforms/file.h"
#include "types/string.h"
#include "types/time.h"
#include "formats/texture.h"
#include "formats/bmp.h"
#include "platforms/ext/bmpx.h"
#include "platforms/ext/bufferx.h"

TListImpl(InputDevice);
TListImpl(Monitor);

I32x2 EResolution_get(EResolution r) { return I32x2_create2(r >> 16, r & U16_MAX); }

EResolution EResolution_create(I32x2 v) {

	if(I32x2_neq2(I32x2_clamp(v, I32x2_zero(), I32x2_xx2(U16_MAX)), v))
		return EResolution_Undefined;

	return _RESOLUTION(I32x2_x(v), I32x2_y(v));
}

Bool Window_isMinimized(const Window *w) { return w && w->flags & EWindowFlags_IsMinimized; }
Bool Window_isFocussed(const Window *w) { return w && w->flags & EWindowFlags_IsFocussed; }
Bool Window_isFullScreen(const Window *w) { return w && w->flags & EWindowFlags_IsFullscreen; }

Bool Window_doesAllowFullScreen(const Window *w) { return w && w->hint & EWindowHint_AllowFullscreen; }

//TODO: Move this to texture class

Error Window_resizeCPUBuffer(Window *w, Bool copyData, I32x2 newSiz) {

	if (!w)
		return Error_nullPointer(0, "Window_resizeCPUBuffer()::w is required");

	if(w->type >= EWindowType_Extended)
		return Error_unsupportedOperation(0, "Window_resizeCPUBuffer() isn't supported for extended windows");

	if(w->type == EWindowType_Physical && !(w->hint & EWindowHint_ProvideCPUBuffer))
		return Error_invalidParameter(
			0, 0, "Window_resizeCPUBuffer()::w must be a virtual window or have EWindowHint_ProvideCPUBuffer"
		);

	if(I32x2_eq2(w->size, newSiz))
		return Error_none();

	if(I32x2_any(I32x2_leq(newSiz, I32x2_zero())))
		return Error_invalidParameter(2, 0, "Window_resizeCPUBuffer()::newSiz should be >0");

	//Because we're resizing, we assume we will be resizing more often
	//To combat constantly reallocating, we will allocate 25% more memory than is needed.
	//And if our buffer should be 50% the size we downsize it to +25% of the real size.
	//Of course we will still have to resize sometimes

	Buffer old = w->cpuVisibleBuffer;
	Buffer neo = old;

	U64 linSizOld = ETextureFormat_getSize((ETextureFormat) w->format, I32x2_x(w->size), I32x2_y(w->size), 1);
	U64 linSiz = ETextureFormat_getSize((ETextureFormat) w->format, I32x2_x(newSiz), I32x2_y(newSiz), 1);

	if(linSizOld * 5 < linSizOld)
		return Error_overflow(2, linSizOld * 5, U64_MAX, "Window_resizeCPUBuffer() overflow");

	//We need to grow; we're out of bounds

	Bool resize = false;

	U64 oldLen = Buffer_length(old);

	if (linSiz >= oldLen) {

		U64 toAllocate = linSiz * 5 / 4;

		Error err = Buffer_createUninitializedBytesx(toAllocate, &neo);

		if(err.genericError)
			return err;

		resize = true;
	}

	//We need to shrink; we're way over allocated (>50%)

	else if (oldLen > linSiz * 3 / 2) {

		U64 toAllocate = linSiz * 5 / 4;

		Error err = Buffer_createUninitializedBytesx(toAllocate, &neo);

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
				Buffer_copy(neo, Buffer_createRefConst(old.ptr, U64_min(linSizOld, linSiz)));

			//If we added size, we need to clear those pixels

			if(I32x2_y(newSiz) > I32x2_y(w->size)) {

				Error err = Buffer_unsetAllBits(Buffer_createRef((U8*)neo.ptr + linSizOld, linSiz - linSizOld));

				//Revert to old size

				if(err.genericError) {

					if(resize)
						Buffer_freex(&neo);

					return err;
				}
			}
		}

		//Row changed; we have to unfortunately copy this over

		else {

			U64 rowSiz = linSiz / I32x2_y(newSiz);
			U64 rowSizOld = linSizOld / I32x2_y(w->size);

			//Grab buffers for simple copies later

			Buffer src = Buffer_createNull();
			Error err = Buffer_createSubset(old, 0, rowSizOld, false, &src);

			if(err.genericError) {

				if(resize)
					Buffer_freex(&neo);

				return err;
			}

			Buffer dst = Buffer_createNull();
			err = Buffer_createSubset(neo, 0, rowSiz, false, &src);

			if(err.genericError) {

				if(resize)
					Buffer_freex(&neo);

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

					Buffer toClear = Buffer_createNull();
					err = Buffer_createSubset(dst, linSizOld, sizDif, false, &toClear);

					if (err.genericError) {

						if(resize)
							Buffer_freex(&neo);

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

	else Buffer_unsetAllBits(Buffer_createRef((U8*)neo.ptr, linSiz));

	//Get rid of our old data

	if (resize)
		Buffer_freex(&old);

	w->cpuVisibleBuffer = neo;
	w->size = newSiz;
	return Error_none();
}

Error Window_storeCPUBufferToDisk(const Window *w, CharString filePath, Ns maxTimeout) {

	if (!w)
		return Error_nullPointer(0, "Window_storeCPUBufferToDisk()::w is required");

	Buffer buf = w->cpuVisibleBuffer;

	if(!Buffer_length(buf))
		return Error_invalidOperation(
			0, "Window_storeCPUBufferToDisk()::w must be a virtual window or have EWindowHint_ProvideCPUBuffer"
		);

	if(w->format != EWindowFormat_BGRA8)
		return Error_unsupportedOperation(
			0, "Window_storeCPUBufferToDisk() is only supported for BGRA8 for now"		//TODO: Add support for other formats
		);

	Buffer file = Buffer_createNull();

	Error err = BMP_writex(
		buf,
		(BMPInfo) { .w = (U32) I32x2_x(w->size), .h = (U32) I32x2_y(w->size) },
		&file
	);

	if(err.genericError)
		return err;

	err = File_write(file, filePath, maxTimeout);
	Buffer_freex(&file);

	return err;
}

Bool Window_terminate(Window *w) {

	if(!w)
		return false;

	w->flags |= EWindowFlags_ShouldTerminate;		//Mark thread for destroy
	return true;
}

#include "platforms/window.h"
#include "platforms/platform.h"
#include "platforms/thread.h"
#include "file/file.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/string.h"
#include "types/timer.h"
#include "formats/texture.h"
#include "platforms/ext/bmpx.h"
#include "platforms/ext/bufferx.h"

Error Window_waitForExit(Window *w, Ns maxTimeout) {

	if(!w)
		return (Error) { .genericError = EGenericError_NullPointer };

	Ns start = Timer_now();

	//We lock to check window state
	//If there's no lock, then we've already been released

	if(w->lock.data && !Lock_isLockedForThread(w->lock) && !Lock_lock(&w->lock, maxTimeout))
		return (Error) { .genericError = EGenericError_InvalidOperation };

	//If our window isn't marked as active, then our window is gone
	//We've successfully waited

	if (!(w->flags & EWindowFlags_IsActive))
		return Error_none();

	//Now we have to make sure we still have time left to wait

	maxTimeout = U64_min(maxTimeout, I64_MAX);

	Ns left = (Ns) I64_max(0, maxTimeout - (DNs)(Timer_now() - start));

	//Release the lock, because otherwise our window can't resume itself

	if(!Lock_unlock(&w->lock))
		return (Error) { .genericError = EGenericError_InvalidOperation };

	//Keep checking until we run out of time

	while(left > 0) {

		//Wait to ensure we don't waste cycles
		//Virtual windows are allowed to run as fast as possible to produce the frames

		if(!Window_isVirtual(w))
			Thread_sleep(10 * ms);

		//Try to reacquire the lock

		if(w->lock.data && !Lock_isLockedForThread(w->lock) && !Lock_lock(&w->lock, left))
			return (Error) { .genericError = EGenericError_InvalidOperation };

		//Our window has been released!

		if (!(w->flags & EWindowFlags_IsActive))
			return Error_none();

		//Virtual windows can draw really quickly

		if(Window_isVirtual(w) && w->callbacks.onDraw)
			w->callbacks.onDraw(w);

		//Release the lock to check for the next time

		if(!Lock_unlock(&w->lock))
			return (Error) { .genericError = EGenericError_InvalidOperation };

		//

		left = (Ns) I64_max(0, maxTimeout - (DNs)(Timer_now() - start));
	}

	return (Error) { .genericError = EGenericError_TimedOut };
}

//TODO: Move this to texture class

Error Window_resizeCPUBuffer(Window *w, Bool copyData, I32x2 newSiz) {

	if (!w)
		return (Error) { .genericError = EGenericError_NullPointer };

	if(!Window_isVirtual(w) && !(w->hint & EWindowHint_ProvideCPUBuffer))
		return (Error) { .genericError = EGenericError_InvalidParameter };

	if(I32x2_eq2(w->size, newSiz))
		return Error_none();

	if(I32x2_any(I32x2_leq(newSiz, I32x2_zero())))
		return (Error) { .genericError = EGenericError_InvalidParameter, .paramId = 2 };

	//Because we're resizing, we assume we will be resizing more often
	//To combat constantly reallocating, we will allocate 25% more memory than is needed.
	//And if our buffer should be 50% the size we downsize it to +25% of the real size.
	//Of course we will still have to resize sometimes

	Buffer old = w->cpuVisibleBuffer;
	Buffer neo = old;

	U64 linSizOld = ETextureFormat_getSize((ETextureFormat) w->format, I32x2_x(w->size), I32x2_y(w->size));
	U64 linSiz = ETextureFormat_getSize((ETextureFormat) w->format, I32x2_x(newSiz), I32x2_y(newSiz));
	
	if(linSizOld * 5 < linSizOld)
		return (Error) { .genericError = EGenericError_Overflow };

	//We need to grow; we're out of bounds

	Bool resize = false;

	if (linSiz >= old.siz) {

		U64 toAllocate = linSiz * 5 / 4;

		Error err = Buffer_createUninitializedBytesx(toAllocate, &neo);

		if(err.genericError)
			return err;

		resize = true;
	}

	//We need to shrink; we're way over allocated (>50%)

	else if (old.siz > linSiz * 3 / 2) {

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
				Buffer_copy(neo, Buffer_createRef(old.ptr, U64_min(linSizOld, linSiz)));

			//If we added size, we need to clear those pixels

			if(I32x2_y(newSiz) > I32x2_y(w->size)) {

				Error err = Buffer_unsetAllBits(Buffer_createRef(neo.ptr + linSizOld, linSiz - linSizOld));

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
			Error err = Buffer_createSubset(old, 0, rowSizOld, &src);

			if(err.genericError) {

				if(resize)
					Buffer_freex(&neo);

				return err;
			}

			Buffer dst = Buffer_createNull();
			err = Buffer_createSubset(neo, 0, rowSiz, &src);

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
					err = Buffer_createSubset(dst, linSizOld, sizDif, &toClear);

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

	else Buffer_unsetAllBits(Buffer_createRef(neo.ptr, linSiz));

	//Get rid of our old data

	if (resize) {

		//If this gives an error, we don't care because we already have the new buffer

		Error err = Buffer_freex(&old);
		err;
	}

	w->cpuVisibleBuffer = neo;
	w->size = newSiz;
	return Error_none();
}

Error Window_storeCPUBufferToDisk(const Window *w, String filePath) {

	if (!w)
		return (Error) { .genericError = EGenericError_NullPointer };

	Buffer buf = w->cpuVisibleBuffer;

	if(!buf.siz)
		return (Error) { .genericError = EGenericError_InvalidOperation };

	if(w->format != EWindowFormat_rgba8)
		return (Error) { .genericError = EGenericError_UnsupportedOperation };		//TODO: Add support for other formats

	if(I32x2_any(I32x2_gt(w->size, I32x2_xx2(U16_MAX))))
		return (Error) { .genericError = EGenericError_InvalidOperation, .errorSubId = 1 };

	Buffer file = Buffer_createNull();

	Error err = BMP_writeRGBAx(
		buf, (U16) I32x2_x(w->size), (U16) I32x2_y(w->size),
		false, &file
	);

	if(err.genericError)
		return err;

	err = File_write(file, filePath);
	Buffer_freex(&file);

	return err;
}

Bool Window_terminateVirtual(Window *w) {

	if(!w || !Window_isVirtual(w))
		return false;

	if(!Lock_isLockedForThread(w->lock) || !Lock_isLockedForThread(Platform_instance.windowManager.lock))
		return false;

	w->flags &= ~EWindowFlags_IsActive;
	return !WindowManager_freeVirtual(&Platform_instance.windowManager, &w).genericError;
}

/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "audio/openal_soft/openal_soft.h"
#include "audio/device.h"
#include "types/base/allocator.h"
#include "types/base/error.h"

U32 AudioDevice_sizeExt = sizeof(ALAudioDevice);

void alListener3fv(ALenum e, F32x4 v) {
	alListener3f(e, F32x4_x(v), F32x4_y(v), F32x4_z(v));
}

void alListener3f2v(ALenum e, F32x4 a, F32x4 b) {

	F32 v[6] = {
		F32x4_x(a), F32x4_y(a), F32x4_z(a),
		F32x4_x(b), F32x4_y(b), F32x4_z(b)
	};

	alListenerfv(e, v);
}

Bool AudioDevice_updateListenerTransformExt(AudioDevice *dev, Error *e_rr) {

	Bool s_uccess = true;

	ALAudioDevice *devExt = AudioDevice_ext(dev, AL);
	
	U8 dirtyMask = dev->pendingDirtyMask;

	if(dirtyMask & 15)
		AL_PROCESS_ERROR(devExt->device, alcMakeContextCurrent(devExt->context))

	if(dirtyMask & 1)
		AL_PROCESS_ERROR(devExt->device, alListener3fv(AL_POSITION, dev->info.position))

	if(dirtyMask & 6)
		AL_PROCESS_ERROR(devExt->device, alListener3f2v(AL_ORIENTATION, dev->info.forward, dev->info.up))

	if(dirtyMask & 8)
		AL_PROCESS_ERROR(devExt->device, alListener3fv(AL_VELOCITY, dev->info.velocity))

	dev->pendingDirtyMask = 0;

clean:
	return s_uccess;
}

Bool AudioDevice_createExt(Bool isDebug, AudioDevice *dev, Error *e_rr) { 

	Bool s_uccess = true;
	ALAudioDevice *devExt = AudioDevice_ext(dev, AL);

	AL_PROCESS_ERROR(NULL, devExt->device = alcOpenDevice(dev->info.name))

	ALCint ints[] = { ALC_CONTEXT_FLAGS_EXT, isDebug ? ALC_CONTEXT_DEBUG_BIT_EXT : 0, 0, 0 };

	AL_PROCESS_ERROR(devExt->device, devExt->context = alcCreateContext(devExt->device, ints))
	dev->pendingDirtyMask = 0xF;

clean:
	return s_uccess;
}

Bool AudioDevice_freeExt(AudioDevice *dev, Allocator alloc) {

	(void) alloc;

	ALAudioDevice *devExt = AudioDevice_ext(dev, AL);

	if(devExt->context) {

		if(alcGetCurrentContext() == devExt->context)
			alcMakeContextCurrent(NULL);

		alcDestroyContext(devExt->context);
	}

	if(devExt->device)
		alcCloseDevice(devExt->device);

	return true;
}

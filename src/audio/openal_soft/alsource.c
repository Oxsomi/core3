/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "audio/source.h"
#include "audio/device.h"
#include "audio/stream.h"
#include "audio/openal_soft/openal_soft.h"
#include "types/container/ref_ptr.h"
#include "types/base/error.h"

U32 AudioSource_sizeExt = (U32) sizeof(ALAudioSource);

Bool AudioSource_createExt(AudioSource *source, Allocator alloc, Error *e_rr) {

	(void) alloc;

	Bool s_uccess = true;
	ALAudioDevice *deviceExt = AudioDevice_ext(AudioDeviceRef_ptr(source->device), AL);
	ALAudioSource *sourceExt = AudioSource_ext(source, AL);

	AL_PROCESS_ERROR(deviceExt->device, alcMakeContextCurrent(deviceExt->context))
	AL_PROCESS_ERROR(deviceExt->device, alGenSources(1, &sourceExt->source))
	AL_PROCESS_ERROR(deviceExt->device, alSourcei(sourceExt->source, AL_LOOPING, AL_FALSE))
	sourceExt->isInitialized = true;

clean:
	return s_uccess;
};

Bool AudioSource_freeExt(AudioSource *source, Allocator alloc) {
	
	(void) alloc;

	ALAudioDevice *deviceExt = AudioDevice_ext(AudioDeviceRef_ptr(source->device), AL);
	ALAudioSource *sourceExt = AudioSource_ext(source, AL);
	Error *e_rr = NULL;
	Bool s_uccess = true;

	if(sourceExt->isInitialized) {
		AL_PROCESS_ERROR(deviceExt->device, alcMakeContextCurrent(deviceExt->context))
		AL_PROCESS_ERROR(deviceExt->device, alDeleteSources(1, &sourceExt->source))
		sourceExt->isInitialized = false;
	}

clean:
	return s_uccess;
}

void alSource3fv(ALuint id, ALenum e, F32x4 v) {
	alSource3f(id, e, F32x4_x(v), F32x4_y(v), F32x4_z(v));
}

Bool AudioSource_update(AudioSource *source, Error *e_rr) {

	Bool s_uccess = true;

	ALAudioDevice *deviceExt = AudioDevice_ext(AudioDeviceRef_ptr(source->device), AL);
	ALAudioSource *sourceExt = AudioSource_ext(source, AL);

	if((source->dirtyMask & 3) || (source->spatialAudio && (source->dirtyMask & 12)))
		AL_PROCESS_ERROR(deviceExt->device, alcMakeContextCurrent(deviceExt->context))

	if(source->dirtyMask & 1)
		AL_PROCESS_ERROR(deviceExt->device, alSourcef(sourceExt->source, AL_GAIN, source->modifier.gain))

	if(source->dirtyMask & 2)
		AL_PROCESS_ERROR(deviceExt->device, alSourcef(sourceExt->source, AL_PITCH, source->modifier.pitch))

	if (source->spatialAudio) {

		if(source->dirtyMask & 4)
			AL_PROCESS_ERROR(deviceExt->device, alSource3fv(sourceExt->source, AL_POSITION, source->point.pos))

		if(source->dirtyMask & 8)
			AL_PROCESS_ERROR(deviceExt->device, alSource3fv(sourceExt->source, AL_VELOCITY, source->point.velocity))
	}

	source->dirtyMask = 0;

clean:
	return s_uccess;
}

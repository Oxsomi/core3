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

#pragma once
#include "types/base/types.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

typedef struct ALAudioInterface {
	U8 padding;					//Not useful yet, might be for loading functions of DLL for example
} ALAudioInterface;

typedef struct ALAudioDevice {
	ALCdevice *device;
	ALCcontext *context;		//Unlike OpenGL, context is thread safe, so no need for multiple
} ALAudioDevice;

#define ALAudioStream_bufferCount (4)
#define ALAudioStream_bufferSize (65536)

typedef struct ALAudioStream {

	Buffer tmp;		//Buffer with temporary stream data
	Buffer tmpCvt;	//Buffer with temporary convert data (likely bigger than tmp if fallback format)

	//4 separate streams to ensure we always have one that isn't in use
	ALuint buffer[ALAudioStream_bufferCount];

	Bool initializedBuffers;	//Since buffer handles of 0 are still valid, unlike OpenGL.
	Bool filledStream;
	Bool initializedSource;
	U8 padding;

	U32 format;

	ALuint source;				//Since otherwise we can't track progress of a stream

} ALAudioStream;

typedef struct ALAudioSource {
	ALuint source;
	Bool isInitialized; U8 padding[3];
} ALAudioSource;

Bool alProcessError(ALenum error, Error *e_rr);

#define AL_PROCESS_ERROR(device, ...) { __VA_ARGS__; gotoIfError3(clean, alProcessError(alcGetError(device), e_rr)) }

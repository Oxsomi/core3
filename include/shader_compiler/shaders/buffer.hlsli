R"(
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

//shader_compiler/shaders/buffer.hlsli
//
//Typed reads and writes through the bindless byte address buffers, plus their sizes.

#pragma once
#include "@resources.hlsli"

U32 bufferBytesUniform(U32 resourceId) {
	U32 bytes;
	bufferUniform(resourceId).GetDimensions(bytes);
	return bytes;
}

U32 bufferBytesUniformRW(U32 resourceId) {
	U32 bytes;
	rwBufferUniform(resourceId).GetDimensions(bytes);
	return bytes;
}

U32 bufferBytes(U32 resourceId) {
	U32 bytes;
	buffer(resourceId).GetDimensions(bytes);
	return bytes;
}

U32 bufferBytesRW(U32 resourceId) {
	U32 bytes;
	rwBuffer(resourceId).GetDimensions(bytes);
	return bytes;
}

template<typename T>
T getAtUniform(U32 resourceId, U32 id) {
	return bufferUniform(resourceId).Load<T>(id);
}

template<typename T>
T getAt(U32 resourceId, U32 id) {
	return buffer(resourceId).Load<T>(id);
}

template<typename T>
void setAtUniform(U32 resourceId, U32 id, T t) {
	rwBufferUniform(resourceId).Store<T>(id, t);
}

template<typename T>
void setAt(U32 resourceId, U32 id, T t) {
	rwBuffer(resourceId).Store<T>(id, t);
}

)"

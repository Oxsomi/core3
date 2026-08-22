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

//shader_compiler/shaders/appdata.hlsli
//
//Per frame data the application pushes: swapchain ids and the app data block.
//Offsets are in U32s, not bytes, and an aligned fetch returns 0 unless the whole vector fits.

#pragma once
#include "@resources.hlsli"

//Fetch per frame data from the application.
//If possible, please make sure the offset is const so it's as fast as possible.
//Offset is in U32s (4-byte), not in bytes!
//Aligned fetches only return non 0 if all of the vector can be fetched!

U32 getReadSwapchain(U32 offset) { return offset & 1 ? _swapchains[offset >> 1].z : _swapchains[offset >> 1].x; }
U32 getWriteSwapchain(U32 offset) { return offset & 1 ? _swapchains[offset >> 1].w : _swapchains[offset >> 1].y; }

//Fetch 1 element from user data

U32 getAppData1u(U32 offset) { return offset >= 92 ? 0 : _appData[offset >> 2][offset & 3]; }
I32 getAppData1i(U32 offset) { return (I32) getAppData1u(offset); }
F32 getAppData1f(U32 offset) { return F32_fromU32(getAppData1u(offset)); }

//Fetch 2 element vector from user data
//Use unaligned only if necessary, otherwise please align offset to 8-byte!

U32x2 getAppData2u(U32 offset) {
	return offset >= 92 ? 0.xx : (
		(offset & 2) == 0 ? _appData[offset >> 2].xy : _appData[offset >> 2].zw
	);
}

I32x2 getAppData2i(U32 offset) { return (I32x2) getAppData2u(offset); }
F32x2 getAppData2f(U32 offset) { return F32x2_fromU32x2(getAppData2u(offset)); }

U32x2 getAppData2uUnaligned(U32 offset) { return U32x2(getAppData1u(offset), getAppData1u(offset + 1)); }
I32x2 getAppData2iUnaligned(U32 offset) { return (I32x2) getAppData2uUnaligned(offset); }
F32x2 getAppData2fUnaligned(U32 offset) { return F32x2_fromU32x2(getAppData2uUnaligned(offset)); }

//Fetch 4 element vector from user data
//Use unaligned only if necessary, otherwise please align offset to 8-byte!

U32x4 getAppData4u(U32 offset) { return offset >= 92 ? 0 : _appData[offset >> 2]; }
I32x4 getAppData4i(U32 offset) { return (I32x4) getAppData4u(offset); }
F32x4 getAppData4f(U32 offset) { return F32x4_fromU32x4(getAppData4u(offset)); }

U32x4 getAppData4uUnaligned(U32 offset) { return U32x4(getAppData2u(offset), getAppData2u(offset + 1)); }
I32x4 getAppData4iUnaligned(U32 offset) { return (I32x4) getAppData4uUnaligned(offset); }
F32x4 getAppData4fUnaligned(U32 offset) { return F32x4_fromU32x4(getAppData4uUnaligned(offset)); }

//Fetch 3 element vector from user data
//Use unaligned only if necessary, otherwise please align offset to 16-byte!

U32x3 getAppData3u(U32 offset) { return getAppData4u(offset).xyz; }
I32x3 getAppData3i(U32 offset) { return (I32x3) getAppData3u(offset); }
F32x3 getAppData3f(U32 offset) { return F32x3_fromU32x3(getAppData3u(offset)); }

U32x3 getAppData3uUnaligned(U32 offset) { return getAppData4uUnaligned(offset).xyz; }
I32x3 getAppData3iUnaligned(U32 offset) { return (I32x3) getAppData3uUnaligned(offset); }
F32x3 getAppData3fUnaligned(U32 offset) { return F32x3_fromU32x3(getAppData3uUnaligned(offset)); }

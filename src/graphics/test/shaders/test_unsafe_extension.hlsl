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

//The unsafe extension gate: an ordinary compute shader marked UnsafeExtensionExecution, so only a device created
// with EGraphicsDeviceFlags_AllowUnsafeExtensionExecution takes it. What is tested is the marker, not an extension
// OxC3 doesn't recognize, which no device the suite runs on would have to support (the compiler tests cover those).
//One entrypoint per file, as in the other test shaders. Both backends, as the extension is.

#include "@resources.hlsli"

struct ExtensionPush {
	U32 output;          //Bindless write handle of the output buffer
	U32 padding0, padding1, padding2;
};

PUSH_CONSTANT ExtensionPush _push;

[[oxc::extension("UnsafeExtensionExecution")]]
[shader("compute")]
[numthreads(64, 1, 1)]
void main(U32x3 id : SV_DispatchThreadID) {
	rwBuffer(_push.output).Store(id.x * 4, id.x * 5 + 1);
}

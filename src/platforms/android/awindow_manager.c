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

#include "platforms/window_manager.h"
#include "types/base/error.h"
#include "platforms/platform.h"

#include <android_native_app_glue.h>

void *Platform_getDataImpl(void *ptr) { (void) ptr; return (struct android_app*) Platform_instance->data; }

Bool WindowManager_createNative(WindowManager *w, Error *e_rr) {
	(void) w; (void) e_rr;
	return true;
}

Bool WindowManager_freeNative(WindowManager *w) {
	(void) w;
	return true;
}

void WindowManager_updateExt(WindowManager *manager) {

	(void) manager;
	
	int ident, events;
	struct android_poll_source *source;
	
	while((ident = ALooper_pollOnce(0, NULL, &events, (void**)&source)) >= 0) {
		if(source)
			source->process(((struct android_app*) Platform_instance->data), source);
	}
}

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

#pragma once
#include "types/string.h"
#include "types/allocator.h"
#include "types/archive.h"
#include "platforms/window_manager.h"

typedef enum EPlatform {

	EPlatform_Uninitialized,
	EPlatform_Windows,
	EPlatform_Linux,
	EPlatform_Android,
	EPlatform_Web,
	EPlatform_iOS,
	EPlatform_OSX

} EPlatform;

typedef struct VirtualSection {

	String path;
	void *dataExt;			//Information about how to load the virtual file
	Archive loadedData;		//If the data is in memory, this will be used
	Bool loaded;

} VirtualSection;

typedef struct Platform {

	EPlatform platformType;

	StringList args;
	String workingDirectory;		//Contains a trailing slash to make file stuff easier

	Allocator alloc;
	WindowManager windowManager;

	List virtualSections;			//<VirtualSection>

	void *data, *dataExt;

} Platform;

extern Platform Platform_instance;
user_impl extern const Bool Platform_useWorkingDirectory;			//If false, the app directory will be used instead.

Error Platform_create(
	int cmdArgc,
	const C8 *cmdArgs[], 
	void *data,
	FreeFunc free,
	AllocFunc alloc,
	void *allocator
);

impl void Platform_cleanupExt(Platform *platform);
impl Error Platform_initExt(Platform *platform, String currentAppDir);

void Platform_cleanup();

user_impl extern int Program_run();
user_impl extern void Program_exit();

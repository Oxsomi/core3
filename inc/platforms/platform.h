/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
*/

#pragma once
#include "types/string.h"
#include "types/allocator.h"
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

typedef struct Platform {

	EPlatform platformType;

	StringList args;
	String workingDirectory;		//Contains a trailing slash to make file stuff easier

	Allocator alloc;
	WindowManager windowManager;

	void *data, *dataExt;

} Platform;

extern Platform Platform_instance;

Error Platform_create(
	int cmdArgc, const C8 *cmdArgs[], 
	void *data,
	FreeFunc free, AllocFunc alloc, void *allocator
);

impl void Platform_cleanupExt(Platform *platform);
impl Error Platform_initExt(Platform *platform, String currentAppDir);

void Platform_cleanup();

user_impl extern int Program_run();
user_impl extern void Program_exit();

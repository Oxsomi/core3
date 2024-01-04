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
#include "types/list.h"
#include "types/vec.h"

typedef struct Window Window;

TListNamed(Window*, ListWindowPtr);

typedef struct CharString CharString;
typedef struct WindowCallbacks WindowCallbacks;
typedef enum EWindowHint EWindowHint;
typedef enum EWindowFormat EWindowFormat;
typedef enum EWindowType EWindowType;

extern const U32 WindowManager_magic;

typedef struct WindowManager WindowManager;

typedef void (*WindowManagerCallback)(WindowManager*);
typedef void (*WindowManagerUpdateCallback)(WindowManager*, F64);

typedef struct WindowManagerCallbacks {
	WindowManagerCallback onCreate, onDestroy, onDraw;
	WindowManagerUpdateCallback onUpdate;
} WindowManagerCallbacks;

typedef struct WindowManager {

	U32 owningThread;		//Only one thread can own a window manager at a time
	U32 isActive;			//WindowManager_magic if active

	ListWindowPtr windows;

	WindowManagerCallbacks callbacks;

	Buffer extendedData;

	Ns lastUpdate;

	Buffer platformData;

} WindowManager;

Error WindowManager_create(WindowManagerCallbacks callbacks, U64 extendedData, WindowManager *manager);
Bool WindowManager_isAccessible(const WindowManager *manager);
Bool WindowManager_free(WindowManager *manager);

Error WindowManager_wait(WindowManager *manager);

impl Bool WindowManager_supportsFormat(const WindowManager *manager, EWindowFormat format);

Error WindowManager_createWindow(

	WindowManager *manager, 

	EWindowType type,

	I32x2 position, 
	I32x2 size, 
	I32x2 minSize, 
	I32x2 maxSize,

	EWindowHint hint, 
	CharString title, 
	WindowCallbacks callbacks, 
	EWindowFormat format,
	U64 extendedData,
	Window **w
);

Bool WindowManager_freeWindow(WindowManager *manager, Window **w);

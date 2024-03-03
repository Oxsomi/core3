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

#include "platforms/ext/listx_impl.h"
#include "platforms/platform.h"
#include "platforms/log.h"
#include "types/atomic.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/archivex.h"

#define UNICODE
#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <Windows.h>

#include <stdlib.h>

CharString Error_formatPlatformError(Allocator alloc, Error err) {

	if(err.genericError != EGenericError_PlatformError)
		return CharString_createNull();

	if(!FAILED(err.paramValue0))
		return CharString_createNull();

	wchar_t *lpBuffer = NULL;

	DWORD f = FormatMessageW(

		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,

		NULL,
		(DWORD) err.paramValue0,

		MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),

		(LPTSTR)&lpBuffer,
		0,

		NULL
	);

	//Unfortunately we have to copy, because we can't tell CharString to use LocalFree instead of free

	if(!f)
		return CharString_createNull();

	CharString res;
	if((err = CharString_createFromUTF16((const U16*)lpBuffer, U64_MAX, alloc, &res)).genericError) {
		LocalFree(lpBuffer);
		return CharString_createNull();
	}

	LocalFree(lpBuffer);
	return res;
}

void *Platform_allocate(void *allocator, U64 length) { (void)allocator; return malloc(length); }
void Platform_free(void *allocator, void *ptr, U64 length) { (void) allocator; (void)length; free(ptr); }

WORD oldColor = 0;

I32 main(I32 argc, const C8 *argv[]) {

	Error err = Platform_create(argc, argv, GetModuleHandleW(NULL), NULL);

	if(err.genericError)
		return -1;

	I32 res = Program_run();
	Program_exit();
	Platform_cleanup();

	return res;
}

void Platform_cleanupExt() {
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), oldColor);
}

typedef struct EnumerateFiles {
	ListVirtualSection *sections;
	Bool b;
} EnumerateFiles;

BOOL enumerateFiles(HMODULE mod, LPWSTR unused, LPWSTR name, EnumerateFiles *sections) {

	mod; unused;

	CharString str = CharString_createNull();
	Error err = CharString_createFromUTF16x((const U16*)name, U64_MAX, &str);
	_gotoIfError(clean, err);

	if(CharString_countAllSensitive(str, '/') != 1)
		Log_warnLnx("Executable contained unrecognized RCDATA. Ignoring it...");

	else {

		VirtualSection section = (VirtualSection) { .path = str };
		_gotoIfError(clean, ListVirtualSection_pushBackx(sections->sections, section));
	}

clean:

	if(err.genericError) {
		sections->b = true;			//Signal that we failed
		CharString_freex(&str);
	}

	return !err.genericError;
}

Error Platform_initExt() {

	Error err = Error_none();

	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
	oldColor = info.wAttributes;

	SetConsoleOutputCP(CP_UTF8);		//Set UTF8 codepage

	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	Platform_instance.threads = systemInfo.dwNumberOfProcessors;

	if(Platform_useWorkingDirectory) {

		CharString_freex(&Platform_instance.workingDirectory);

		//Init working dir

		wchar_t buff[MAX_PATH + 1];
		DWORD chars = GetCurrentDirectoryW(MAX_PATH + 1, buff);

		if(!chars)
			_gotoIfError(clean, Error_platformError(
				0, GetLastError(), "Platform_initExt() GetCurrentDirectory failed"
			));

		_gotoIfError(clean, CharString_createFromUTF16x((const U16*)buff, chars, &Platform_instance.workingDirectory));

		CharString_replaceAllSensitive(&Platform_instance.workingDirectory, '\\', '/');

		_gotoIfError(clean, CharString_appendx(&Platform_instance.workingDirectory, '/'));
	}

	//Init virtual files

	EnumerateFiles files = (EnumerateFiles) { .sections = &Platform_instance.virtualSections };

	if (!EnumResourceNamesW(
		NULL, RT_RCDATA,
		(ENUMRESNAMEPROCW)enumerateFiles,
		(LONG_PTR)&files
	)) {

		//Enum resource names also fails if we don't have any resources.
		//To counter this, enumerateFiles sets stride to 0 if the reason it returned false was because of the function.

		if(files.b)
			_gotoIfError(clean, Error_invalidState(1, "Platform_initExt() EnumResourceNames failed"));
	}

clean:
	return err;
}

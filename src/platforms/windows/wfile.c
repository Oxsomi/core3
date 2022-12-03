#include "types/error.h"
#include "platforms/file.h"
#include "platforms/ext/stringx.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

Error File_foreach(String loc, FileCallback callback, void *userData, Bool isRecursive) {

	String resolved = String_createNull();
	Error err = Error_none();
	HANDLE file = NULL;

	if(!callback) 
		_gotoIfError(clean, Error_nullPointer(1, 0));

	if(!String_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0, 0));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_foreachVirtual(loc, callback, userData, isRecursive));
		return Error_none();
	}

	_gotoIfError(clean, File_resolve(loc, &isVirtual, &resolved));

	if(isVirtual)
		_gotoIfError(clean, Error_invalidOperation(0));

	//Append /*\0 (replace last \0)

	resolved.ptr[resolved.length - 1] = '/';

	_gotoIfError(clean, String_appendStringx(&resolved, String_createConstRefSized("*", 2)));

	if(resolved.length > MAX_PATH)
		_gotoIfError(clean, Error_outOfBounds(0, 0, resolved.length, MAX_PATH));

	//Skip .

	WIN32_FIND_DATA dat;
	file = FindFirstFileA(resolved.ptr, &dat);

	if(file == INVALID_HANDLE_VALUE)
		_gotoIfError(clean, Error_notFound(0, 0, 0));

	if(!FindNextFileA(file, &dat))
		_gotoIfError(clean, Error_notFound(0, 0, 0));

	//Loop through real files (while instead of do while because we wanna skip ..)

	while(FindNextFileA(file, &dat)) {

		//Folders can also have timestamps, so parse timestamp here

		LARGE_INTEGER time;
		time.LowPart = dat.ftLastWriteTime.dwLowDateTime;
		time.HighPart = dat.ftLastWriteTime.dwHighDateTime;

		//Before 1970, unsupported. Default to 1970

		Ns timestamp = 0;
		static const U64 UNIX_START_WIN = (Ns)11644473600 * (1'000'000'000 / 100);	//ns to 100s of ns

		if ((U64)time.QuadPart >= UNIX_START_WIN)
			timestamp = (time.QuadPart - UNIX_START_WIN) * 100;	//Convert to Oxsomi time

		//Folder parsing

		if(dat.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {

			FileInfo info = (FileInfo) {
				.path = String_createConstRef(dat.cFileName, MAX_PATH),
				.timestamp = timestamp,
				.access = dat.dwFileAttributes & FILE_ATTRIBUTE_READONLY ? FileAccess_Read : FileAccess_ReadWrite,
				.type = FileType_Folder
			};

			_gotoIfError(clean, callback(info, userData));

			if(isRecursive)
				_gotoIfError(clean, File_foreach(info.path, callback, userData, true));

			continue;
		}

		LARGE_INTEGER size;
		size.LowPart = dat.nFileSizeLow;
		size.HighPart = dat.nFileSizeHigh;

		//File parsing

		FileInfo info = (FileInfo) {
			.path = String_createConstRef(dat.cFileName, MAX_PATH),
			.timestamp = timestamp,
			.access = dat.dwFileAttributes & FILE_ATTRIBUTE_READONLY ? FileAccess_Read : FileAccess_ReadWrite,
			.type = FileType_File,
			.fileSize = size.QuadPart
		};

		_gotoIfError(clean, callback(info, userData));
	}

	DWORD hr = GetLastError();

	if(hr != ERROR_NO_MORE_FILES)
		_gotoIfError(clean, Error_platformError(0, hr));

clean:

	if(file)
		FindClose(file);

	String_freex(&resolved);
	return err;
}

//TODO:!

Error File_removeVirtual(String loc, Ns maxTimeout) { loc; maxTimeout; return Error_unimplemented(0); }
Error File_addVirtual(String loc, FileType type, Ns maxTimeout) { loc; type; maxTimeout; return Error_unimplemented(0); }

Error File_renameVirtual(String loc, String newFileName, Ns maxTimeout) { 
	loc; newFileName; maxTimeout; return Error_unimplemented(0); 
}

Error File_moveVirtual(String loc, String directoryName, Ns maxTimeout) { 
	loc; directoryName; maxTimeout; return Error_unimplemented(0); 
}

Error File_writeVirtual(Buffer buf, String loc, Ns maxTimeout) { maxTimeout; buf; loc; return Error_unimplemented(0); }

Error File_readVirtual(String loc, Buffer *output, Ns maxTimeout) { 
	maxTimeout; loc; output; return Error_unimplemented(0); 
}

Error File_getInfoVirtual(String loc, FileInfo *info) { loc; info; return Error_unimplemented(0); }

Error File_foreachVirtual(String loc, FileCallback callback, void *userData, Bool isRecursive) { 
	loc; callback; userData; isRecursive;
	return Error_unimplemented(0); 
}

Error File_queryFileObjectCountVirtual(String loc, FileType type, Bool isRecursive, U64 *res) { 
	loc; type; isRecursive; res;
	return Error_unimplemented(0); 
}

Error File_queryFileObjectCountAllVirtual(String loc, Bool isRecursive, U64 *res) { 
	loc; isRecursive; res;
	return Error_unimplemented(0); 
}

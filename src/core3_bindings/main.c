/* OxC3_bindings (Oxsomi core 3 / bindings), for generating OxC3 bindings for different languages (such as C#).
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
*  along with this program. If not, see https://github.com/Oxsomi/rt_core/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

#include "platforms/platform.h"
#include "platforms/file.h"
#include "platforms/log.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/errorx.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/string.h"
#include "core3_bindings/parser.h"

const Bool Platform_useWorkingDirectory = true;

Error Program_parseFile(FileInfo info, void *dummy) {

	dummy;

	Error err = Error_none();
	Buffer buf = Buffer_createNull();

	_gotoIfError(clean, File_read(info.path, U64_MAX, &buf));

	CharString file = CharString_createConstRefSized((const C8*) buf.ptr, Buffer_length(buf), false);

	Parser parser = (Parser) { 0 };
	_gotoIfError(clean, Parser_create(file, &parser));

clean:
	Parser_free(&parser);
	Buffer_freex(&buf);
	return Error_none();
}

int Program_run() {

	//Error err = File_foreach(CharString_createConstRefCStr("core3"), Parse_file, NULL, true);

	FileInfo info = (FileInfo) { 0 };
	Error err = Error_none();
	_gotoIfError(clean, File_getInfo(CharString_createConstRefCStr("core3/inc/types/types.h"), &info));
	_gotoIfError(clean, Program_parseFile(info, NULL));

clean:
	FileInfo_freex(&info);
	Error_printx(err, ELogLevel_Error, ELogOptions_Default);
	return 0;
}

void Program_exit() { }

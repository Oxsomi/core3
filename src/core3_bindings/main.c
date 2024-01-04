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
#include "platforms/ext/stringx.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/string.h"
#include "core3_bindings/lexer.h"
#include "core3_bindings/parser.h"

const Bool Platform_useWorkingDirectory = true;

void Program_handleToken(CharString str, const Parser *p, U64 index, Token t) {

	LexerToken lt = p->lexer->tokens.ptr[t.naiveTokenId];
	CharString tokenRaw = Token_asString(t, p);

	Log_debugLnx(

		"T%llu(%llu,%llu[%llu]: L#%llu:%llu)\t\t%.*s", 

		index, 

		t.naiveTokenId, 
		t.lexerTokenSubId, 
		t.tokenSize, 

		lt.lineId, 
		lt.charId + t.lexerTokenSubId,

		CharString_length(tokenRaw), 
		tokenRaw.ptr
	);
}

typedef enum EBindingLanguage {

	EBindingLanguage_CPP,
	//EBindingLanguage_CSharp,
	//EBindingLanguage_Python,
	//EBindingLanguage_Lua,
	//EBindingLanguage_JS,

	EBindingLanguage_Count

} EBindingLanguage;

Error Program_generateBindings(CharString str, Parser parser, CharString *result, EBindingLanguage lang) {

	if(lang != EBindingLanguage_CPP)
		return Error_unimplemented(0, "Program_generateBindings()::lang required to be CPP, no others are supported yet");

	Error err = Error_none();
	Bool hasOxC3 = false;

	for (U64 i = 0; i < parser.tokens.length; ++i)
		Program_handleToken(str, &parser, i, parser.tokens.ptr[i]);

	goto clean;

clean:
	return err;
}

#define Program_userDefine(x, y) { CharString_createConstRefCStr(x), CharString_createConstRefCStr(y) }
#define Program_stringify(x) Program_userDefine(#x, values[x])

Error Program_parseFile(FileInfo info, void *dummy) {

	dummy;

	Error err = Error_none();
	Buffer buf = Buffer_createNull();
	CharString newFile = CharString_createNull();

	_gotoIfError(clean, File_read(info.path, U64_MAX, &buf));

	CharString file = CharString_createConstRefSized((const C8*) buf.ptr, Buffer_length(buf), false);

	typedef struct ProgramUserDefine {

		const C8 *name;
		const C8 *value;

	} ProgramUserDefine;

	const C8 *values[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };

	UserDefine defines[] = {
		Program_stringify(_ARCH),
		Program_stringify(_SIMD),
		Program_stringify(_RELAX_FLOAT),
		Program_stringify(_FORCE_FLOAT_FALLBACK),
		Program_stringify(_PLATFORM_TYPE),
		Program_userDefine("FLT_EVAL_METHOD", values[0])
	};

	ListUserDefine userDefines = { 0 };
	_gotoIfError(clean, ListUserDefine_createRefConst(defines, sizeof(defines) / sizeof(UserDefine), &userDefines));

	Lexer lexer = (Lexer) { 0 };
	Parser parser = (Parser) { 0 };
	_gotoIfError(clean, Lexer_create(file, &lexer));
	_gotoIfError(clean, Parser_create(&lexer, &parser, userDefines));
	_gotoIfError(clean, Program_generateBindings(file, parser, &newFile, EBindingLanguage_CPP));

clean:
	Parser_free(&parser);
	Lexer_free(&lexer);
	CharString_freex(&newFile);
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

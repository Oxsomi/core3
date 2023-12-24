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

void Lexer_handleExpression(CharString str, const Lexer *p, U64 index, LexerExpression e) {

	CharString expressionRaw = LexerExpression_asString(e, *p);

	Bool printTokens = true;

	switch (e.type) {

		case ELexerExpressionType_Comment:
			Log_debugLnx("E%llu\tComment:\t%.*s", index, CharString_length(expressionRaw), expressionRaw.ptr);
			break;

		case ELexerExpressionType_MultiLineComment:
			Log_debugLnx("E%llu\tMulti line comment:\t%.*s", index, CharString_length(expressionRaw), expressionRaw.ptr);
			break;

		case ELexerExpressionType_Preprocessor:
			Log_debugLnx("E%llu\tPreprocessor:\t%.*s", index, CharString_length(expressionRaw), expressionRaw.ptr);
			break;

		default:
			Log_debugLnx("E%llu\tGeneric expression:\t%.*s", index, CharString_length(expressionRaw), expressionRaw.ptr);
			break;
	}

	if(printTokens)
		for (U64 i = 0; i < e.tokenCount; ++i) {

			U64 j = e.tokenOffset + i;
			LexerToken t = *(const LexerToken*) List_ptrConst(p->tokens, j);
			const C8 *cStart = LexerToken_getTokenStart(t, *p);
			const C8 *cEnd = LexerToken_getTokenEnd(t, *p);

			U64 tline = t.lineId, tchar = t.charId;
			U64 toff = (U64) (t.offsetType << 2 >> 2);
			U64 tlen = (U64) t.length;

			const C8 *fmt = "\tT%llu(%llu,%llu: L#%llu:%llu)\t%s:\t%.*s";

			switch(t.offsetType >> 30) {

				case ELexerTokenType_Symbols:
					Log_debugLnx(fmt, j, toff, tlen, tline, tchar, "Symbols", cEnd - cStart, cStart);
					break;

				case ELexerTokenType_Double:
					Log_debugLnx(fmt, j, toff, tlen, tline, tchar, "Float", cEnd - cStart, cStart);
					break;

				case ELexerTokenType_Integer:
					Log_debugLnx(fmt, j, toff, tlen, tline, tchar, "Integer", cEnd - cStart, cStart);
					break;

				default:
					Log_debugLnx(fmt, j, toff, tlen, tline, tchar, "Identifier", cEnd - cStart, cStart);
					break;
			}
		}
}

typedef enum EBindingLanguage {

	EBindingLanguage_CPP,
	//EBindingLanguage_CSharp,
	//EBindingLanguage_Python,
	//EBindingLanguage_Lua,
	//EBindingLanguage_JS,

	EBindingLanguage_Count

} EBindingLanguage;

Error Program_generateBindings(CharString str, Lexer lexer, CharString *result, EBindingLanguage lang) {

	if(lang != EBindingLanguage_CPP)
		return Error_unimplemented(0, "Program_generateBindings()::lang required to be CPP, no others are supported yet");

	Error err = Error_none();
	Bool hasOxC3 = false;

	for (U64 i = 0; i < lexer.expressions.length; ++i) {

		LexerExpression e = *(const LexerExpression*) List_ptrConst(lexer.expressions, i);
		Lexer_handleExpression(str, &lexer, i, e);
		continue;

		if(e.type == ELexerExpressionType_Comment || e.type == ELexerExpressionType_MultiLineComment) {

			//Detect license

			if (!i && e.tokenCount >= 2) {

				LexerToken t = *(const LexerToken*) List_ptrConst(lexer.tokens, 1);

				hasOxC3 = CharString_equalsStringSensitive(
					LexerToken_asString(t, lexer), CharString_createConstRefCStr("OxC3")
				);

				if(hasOxC3)
					continue;
			}

			//Maintain comments

			U64 lastLine = U64_MAX;

			for (U64 i = 0; i < e.tokenCount; ++i) {

				LexerToken t = *(const LexerToken*) List_ptrConst(lexer.tokens, e.tokenOffset + i);

				if(lastLine != t.lineId) {
					_gotoIfError(clean, CharString_appendx(result, '\n'));		//Maintain consistent namespace indenting
					_gotoIfError(clean, CharString_appendx(result, '\t'));
					lastLine = t.lineId;
				}

				else {		//Maintain same whitespace

				}

				_gotoIfError(clean, CharString_appendStringx(result, LexerToken_asString(t, lexer)));
			}
		}

		//Parse real info

		else {

		}
	}

	//Add header and footer

	_gotoIfError(clean, CharString_prependStringx(result, CharString_createConstRefCStr(
		"\n#pragma once\n\nnamespace oxc {\n"
	)));

	if (hasOxC3) {
		LexerExpression e = *(const LexerExpression*) List_ptrConst(lexer.expressions, 0);
		CharString expressionRaw = LexerExpression_asString(e, lexer);
		_gotoIfError(clean, CharString_prependStringx(result, expressionRaw));
	}

	_gotoIfError(clean, CharString_appendStringx(result, CharString_createConstRefCStr("\n}\n")));

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

	List userDefines = (List) { 0 };
	_gotoIfError(clean, List_createConstRef(
		(const U8*)defines, sizeof(defines) / sizeof(UserDefine), sizeof(UserDefine), &userDefines
	));

	Lexer lexer = (Lexer) { 0 };
	Parser parser = (Parser) { 0 };
	_gotoIfError(clean, Lexer_create(file, &lexer));
	_gotoIfError(clean, Parser_create(&lexer, &parser, userDefines));
	_gotoIfError(clean, Program_generateBindings(file, lexer, &newFile, EBindingLanguage_CPP));

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

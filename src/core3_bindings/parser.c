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

#include "platforms/log.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "types/error.h"
#include "types/buffer.h"

void Parser_handlePreprocess(CharString str, U64 i) {
	Log_debugLnx("%llu\tPreprocessor:\t%s", i, str.ptr);
	int dbg = 0;
	dbg;
}

void Parser_handleComment(CharString str, Bool multiline, U64 i) {
	Log_debugLnx("%llu\t%s:\t%s", i, multiline ? "Multiline comment" : "Comment", str.ptr);
	int dbg = 0;
	dbg;
}

void Parser_handleExpression(CharString str, U64 i) {
	Log_debugLnx("%llu\tExpression:\t%s", i, str.ptr);
	int dbg = 0;
	dbg;
}

Error Parser_exec(CharString str) {
	
	Error err = Error_none();
	CharString pending = CharString_createNull();
	_gotoIfError(clean, CharString_reservex(&pending, 128));

	Bool isPreprocessorOp = false;
	Bool isEscaped = false, hadAnyChar = false;
	Bool isFirstChar = true;
	Bool isComment = false, isMultiLineComment = false;

	U64 counter = 0;

	for (U64 i = 0; i < CharString_length(str); ++i) {

		C8 c = CharString_getAt(str, i);
		C8 next = CharString_getAt(str, i + 1);

		//Escaping end of line

		if (c == '\\') {

			if(isEscaped)
				isEscaped = false;

			else isEscaped = true;
		}

		//Newline; end define

		if (C8_isNewLine(c)) {

			Bool isMultiNewLine = false;

			if(c == '\r' && next == '\n') {		//Consume \r\n as one token

				if (!isEscaped && isMultiLineComment) {
					_gotoIfError(clean, CharString_appendx(&pending, '\r'));
					_gotoIfError(clean, CharString_appendx(&pending, '\n'));
					isMultiNewLine = true;
				}

				++i;
			}

			else if(!isEscaped && isMultiLineComment)
				_gotoIfError(clean, CharString_appendx(&pending, '\n'));

			if(!isEscaped) {

				if (isComment) {
					Parser_handleComment(pending, false, counter++);
					CharString_clear(&pending);
					hadAnyChar = false;
				}

				else if(isPreprocessorOp) {
					Parser_handlePreprocess(pending, counter++);
					CharString_clear(&pending);
					hadAnyChar = false;
				}

				if(isPreprocessorOp || isComment || isMultiLineComment)
					hadAnyChar = false;

				isPreprocessorOp = false;
				isFirstChar = true;
				isComment = false;
			}

			if(!isComment && !isMultiLineComment && !isPreprocessorOp) {

				if(hadAnyChar) {

					if(isMultiNewLine)
						_gotoIfError(clean, CharString_appendx(&pending, '\r'));

					_gotoIfError(clean, CharString_appendx(&pending, '\n'));
				}

				else CharString_clear(&pending);

				continue;
			}
		}

		isEscaped = false;

		//Preprocessor

		if(c == '#' && isFirstChar)
			isPreprocessorOp = true;

		//First non whitespace char

		if(!C8_isWhitespace(c)) {
			hadAnyChar = true;
			isFirstChar = false;
		}

		//Handle starting comment

		if(c == '/' && next == '/' && !isMultiLineComment) {
			_gotoIfError(clean, CharString_appendx(&pending, '/'));
			_gotoIfError(clean, CharString_appendx(&pending, '/'));
			isComment = true;
			++i;
			continue;
		}

		//Handle starting multi line comment

		if(c == '/' && next == '*' && !isMultiLineComment) {
			_gotoIfError(clean, CharString_appendx(&pending, '/'));
			_gotoIfError(clean, CharString_appendx(&pending, '*'));
			isMultiLineComment = true;
			++i;
			continue;
		}

		//Deal with closing multi line comment

		if (isMultiLineComment) {

			if (c == '*' && next == '/') {

				_gotoIfError(clean, CharString_appendx(&pending, '*'));
				_gotoIfError(clean, CharString_appendx(&pending, '/'));

				++i;
				isMultiLineComment = false;
				Parser_handleComment(pending, true, counter++);
				CharString_clear(&pending);
				hadAnyChar = false;
				continue;
			}
		}

		//Ignore leading whitespace

		if(C8_isWhitespace(c) && !(isComment || isMultiLineComment || isPreprocessorOp) && isFirstChar)
			continue;

		_gotoIfError(clean, CharString_appendx(&pending, c));

		//Parse ; as end of expression

		if(c == ';' && !isPreprocessorOp && !isComment && !isMultiLineComment) {
			Parser_handleExpression(pending, counter++);
			CharString_clear(&pending);
			isFirstChar = true;
			hadAnyChar = false;
		}
	}

clean:
	CharString_freex(&pending);
	return Error_none();
}
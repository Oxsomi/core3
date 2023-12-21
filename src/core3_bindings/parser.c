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
#include "platforms/ext/listx.h"
#include "types/error.h"
#include "types/buffer.h"
#include "core3_bindings/parser.h"

const C8 *NaiveToken_getTokenEnd(NaiveToken tok, CharString c) {

	if((tok.offsetType << 2 >> 2) + tok.length > CharString_length(c))
		return NULL;

	return c.ptr + (tok.offsetType << 2 >> 2) + tok.length;
}

const C8 *NaiveToken_getTokenStart(NaiveToken tok, CharString c) {

	if((tok.offsetType << 2 >> 2) > CharString_length(c))
		return NULL;

	return c.ptr + (tok.offsetType << 2 >> 2);
}

CharString NaiveToken_asString(NaiveToken tok, CharString c) {
	const C8 *ptr = NaiveToken_getTokenStart(tok, c);
	return CharString_createConstRefSized(ptr, tok.length, false);
}

CharString Expression_asString(Expression e, Parser p, CharString str) {

	NaiveToken tStart = *(const NaiveToken*) List_ptrConst(p.tokens, e.tokenOffset);
	NaiveToken tEnd = *(const NaiveToken*) List_ptrConst(p.tokens, e.tokenOffset + e.tokenCount - 1);

	const C8 *cStart = NaiveToken_getTokenStart(tStart, str);
	const C8 *cEnd = NaiveToken_getTokenEnd(tEnd, str);

	return CharString_createConstRefSized(cStart, cEnd - cStart, false);
}

Error Parser_endExpression(U64 *lastTokenPtr, List *expressions, U64 tokenCount, EExpressionType *expressionType) {

	Error err = Error_none();

	U64 lastToken = *lastTokenPtr;

	if(lastToken == tokenCount)
		return err;

	if(lastToken >> 32)
		_gotoIfError(clean, Error_outOfBounds(
			0, lastToken, U32_MAX, "Parser_create() Expression offset is limited to U32_MAX tokens"
		));

	if((tokenCount - lastToken) >> 16)
		_gotoIfError(clean, Error_outOfBounds(
			0, tokenCount - lastToken, U16_MAX, "Parser_create() Expression is limited to U16_MAX tokens"
		));

	Expression exp = (Expression) { 
		.type = *expressionType,
		.tokenOffset = (U32) lastToken,
		.tokenCount = (U16) (tokenCount - lastToken)
	};

	_gotoIfError(clean, List_pushBackx(expressions, Buffer_createConstRef(&exp, sizeof(exp))));
	*lastTokenPtr = tokenCount;
	*expressionType = EExpressionType_None;

clean:
	return err;
}

Error Parser_create(CharString str, Parser *parser) {
	
	if(!parser)
		return Error_nullPointer(!str.ptr ? 0 : 1, "Parser_create()::str and parser are required");
	
	if(parser->tokens.ptr)
		return Error_invalidParameter(1, 0, "Parser_create()::parser wasn't empty, might indicate memleak");

	if(CharString_length(str) >> 30)
		return Error_outOfBounds(0, GIBI, GIBI, "Parser_create()::str is limited to a GiB");

	Error err = Error_none();
	List tokens = List_createEmpty(sizeof(NaiveToken));
	List expressions = List_createEmpty(sizeof(Expression));
	_gotoIfError(clean, List_reservex(&expressions, 64 + CharString_length(str) / 64));
	_gotoIfError(clean, List_reservex(&tokens, 64 + CharString_length(str) / 6));

	const C8 *prevIt = NULL;
	C8 prev = 0;
	EExpressionType expressionType = EExpressionType_None;
	ENaiveTokenType tokenType = ENaiveTokenType_Count;

	U64 lineCounter = 1, lastLineStart = 0;
	U64 lastToken = 0;

	for (U64 i = 0; i < CharString_length(str); ++i) {

		C8 c = CharString_getAt(str, i);

		if(!C8_isValidAscii(c))
			_gotoIfError(clean, Error_invalidParameter(0, 0, "Parser_create()::str has to be ascii"));

		C8 next = CharString_getAt(str, i + 1);
		C8 prevTemp = prev;
		prev = c;

		Bool multiChar = false;
		Bool endExpression = false;
		Bool endToken = false;

		//Newline; end last expression in some cases

		if(c == '\r' && next == '\n')		//Consume \r\n as one char
			multiChar = true;

		if(C8_isNewLine(c)) {

			++lineCounter;
			lastLineStart = i + multiChar + 1;

			if (lineCounter >> 16)
				_gotoIfError(clean, Error_outOfBounds(0, U16_MAX, U16_MAX, "Parser_create() line count is limited to 64Ki"));
		}

		if (prevTemp != '\\' && C8_isNewLine(c)) {

			if(expressionType == EExpressionType_Comment || expressionType == EExpressionType_Preprocessor)
				endExpression = true;
		}

		//Preprocessor

		if(c == '#' && !expressionType) {
			expressionType = EExpressionType_Preprocessor;
			prevIt = str.ptr + i;
		}

		//Handle starting comment

		if(c == '/' && next == '/' && expressionType != EExpressionType_MultiLineComment) {
			_gotoIfError(clean, Parser_endExpression(&lastToken, &expressions, tokens.length, &expressionType));
			expressionType = EExpressionType_Comment;
			multiChar = true;
			endToken = true;
		}

		//Handle starting multi line comment

		if(c == '/' && next == '*' && !expressionType) {
			_gotoIfError(clean, Parser_endExpression(&lastToken, &expressions, tokens.length, &expressionType));
			expressionType = EExpressionType_MultiLineComment;
			multiChar = true;
			endToken = true;
		}

		//Deal with closing multi line comment

		else if (expressionType == EExpressionType_MultiLineComment) {

			if (c == '*' && next == '/') {
				multiChar = true;
				endExpression = true;
			}
		}

		//Parse ; as end of expression

		if(c == ';' && expressionType == EExpressionType_Generic)
			endExpression = true;

		//First non whitespace char

		ENaiveTokenType current = ENaiveTokenType_Count;

		if(!C8_isWhitespace(c)) {

			if(!expressionType)
				expressionType = EExpressionType_Generic;

			if(!prevIt)
				prevIt = str.ptr + i;

			/* TODO: If . merge with prev int to form float if no whitespace */

			tokenType = 
				tokenType != ENaiveTokenType_Identifier && C8_isDec(c) ? ENaiveTokenType_Integer : (
					C8_isNyto(c) ? ENaiveTokenType_Identifier : ENaiveTokenType_Symbols
				);

			ENaiveTokenType nextType = 
				tokenType != ENaiveTokenType_Identifier && C8_isDec(next) ? ENaiveTokenType_Integer : (
					C8_isNyto(next) ? ENaiveTokenType_Identifier : (
						C8_isWhitespace(next) ? ENaiveTokenType_Count : ENaiveTokenType_Symbols
					)
				);

			if(nextType != tokenType)
				endToken = true;
		}

		else if (prevIt) endToken = true;
		
		if(endToken || (prevIt && endExpression)) {

			U64 len = (str.ptr + i + 1 + multiChar) - prevIt;

			if(len >> 8)
				_gotoIfError(clean, Error_outOfBounds(0, 256, 256, "Parser_create() tokens are limited to 256 C8s"));

			if((i + 2  - lastLineStart) >> 7)
				_gotoIfError(clean, Error_outOfBounds(0, 128, 128, "Parser_create() lines are limited to 128 C8s"));

			NaiveToken tok = (NaiveToken) { 
				.lineId = (U16) lineCounter, 
				.charId = (U8) (i + 1 - len - lastLineStart) + 1,
				.length = (U8) len, 
				.offsetType = (i + 1 + multiChar - len) | ((U64)tokenType << 30)
			};

			_gotoIfError(clean, List_pushBackx(&tokens, Buffer_createConstRef(&tok, sizeof(tok))));
			prevIt = NULL;
			tokenType = ENaiveTokenType_Count;
		}

		//Push it as an expression

		if (endExpression)
			_gotoIfError(clean, Parser_endExpression(&lastToken, &expressions, tokens.length, &expressionType));

		if(multiChar)
			++i;
	}

clean:

	if(err.genericError) {
		List_freex(&tokens);
		List_freex(&expressions);
	}
	else *parser = (Parser) { .expressions = expressions, .tokens = tokens };

	return err;
}

Bool Parser_free(Parser *parser) {

	if(!parser)
		return true;

	List_freex(&parser->expressions);
	List_freex(&parser->tokens);
	return true;
}

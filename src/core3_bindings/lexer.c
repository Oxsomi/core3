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

#include "platforms/ext/listx_impl.h"
#include "platforms/log.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "core3_bindings/lexer.h"

TListImpl(LexerToken);
TListImpl(LexerExpression);

const C8 *LexerToken_getTokenEnd(LexerToken tok, Lexer l) {

	if((tok.offsetType << 2 >> 2) + tok.length > CharString_length(l.source))
		return NULL;

	return l.source.ptr + (tok.offsetType << 2 >> 2) + tok.length;
}

const C8 *LexerToken_getTokenStart(LexerToken tok, Lexer l) {

	if((tok.offsetType << 2 >> 2) > CharString_length(l.source))
		return NULL;

	return l.source.ptr + (tok.offsetType << 2 >> 2);
}

CharString LexerToken_asString(LexerToken tok, Lexer l) {
	const C8 *ptr = LexerToken_getTokenStart(tok, l);
	return CharString_createRefSizedConst(ptr, tok.length, false);
}

CharString LexerExpression_asString(LexerExpression e, Lexer l) {

	LexerToken tStart = l.tokens.ptr[e.tokenOffset];
	LexerToken tEnd = l.tokens.ptr[e.tokenOffset + e.tokenCount - 1];

	const C8 *cStart = LexerToken_getTokenStart(tStart, l);
	const C8 *cEnd = LexerToken_getTokenEnd(tEnd, l);

	return CharString_createRefSizedConst(cStart, cEnd - cStart, false);
}

Error Lexer_endExpression(
	U64 *lastTokenPtr, 
	ListLexerExpression *expressions, 
	U64 tokenCount, 
	ELexerExpressionType *expressionType
) {

	Error err = Error_none();

	U64 lastToken = *lastTokenPtr;

	if(lastToken == tokenCount)
		return err;

	if(lastToken >> 32)
		_gotoIfError(clean, Error_outOfBounds(
			0, lastToken, U32_MAX, "Lexer_create() LexerExpression offset is limited to U32_MAX tokens"
		));

	if((tokenCount - lastToken) >> 16)
		_gotoIfError(clean, Error_outOfBounds(
			0, tokenCount - lastToken, U16_MAX, "Lexer_create() LexerExpression is limited to U16_MAX tokens"
		));

	LexerExpression exp = (LexerExpression) { 
		.type = *expressionType,
		.tokenOffset = (U32) lastToken,
		.tokenCount = (U16) (tokenCount - lastToken)
	};

	_gotoIfError(clean, ListLexerExpression_pushBackx(expressions, exp));
	*lastTokenPtr = tokenCount;
	*expressionType = ELexerExpressionType_None;

clean:
	return err;
}

Error Lexer_create(CharString str, Lexer *lexer) {
	
	if(!lexer)
		return Error_nullPointer(!str.ptr ? 0 : 1, "Lexer_create()::str and lexer are required");
	
	if(lexer->tokens.ptr)
		return Error_invalidParameter(1, 0, "Lexer_create()::lexer wasn't empty, might indicate memleak");

	if(CharString_length(str) >> 30)
		return Error_outOfBounds(0, GIBI, GIBI, "Lexer_create()::str is limited to a GiB");

	Error err = Error_none();
	ListLexerToken tokens = { 0 };
	ListLexerExpression expressions = { 0 };
	_gotoIfError(clean, ListLexerExpression_reservex(&expressions, 64 + CharString_length(str) / 64));
	_gotoIfError(clean, ListLexerToken_reservex(&tokens, 64 + CharString_length(str) / 6));

	const C8 *prevIt = NULL;
	C8 prev = 0;
	ELexerExpressionType expressionType = ELexerExpressionType_None;
	ELexerTokenType tokenType = ELexerTokenType_Count;

	U64 lineCounter = 1, lastLineStart = 0;
	U64 lastToken = 0;

	for (U32 i = 0; i < (U32) CharString_length(str); ++i) {

		C8 c = CharString_getAt(str, i);

		if(!C8_isValidAscii(c))
			_gotoIfError(clean, Error_invalidParameter(0, 0, "Lexer_create()::str has to be ascii"));

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
				_gotoIfError(clean, Error_outOfBounds(0, U16_MAX, U16_MAX, "Lexer_create() line count is limited to 64Ki"));
		}

		if (prevTemp != '\\' && C8_isNewLine(c)) {

			if(expressionType == ELexerExpressionType_Comment || expressionType == ELexerExpressionType_Preprocessor)
				endExpression = true;
		}

		//Preprocessor

		if(c == '#' && !expressionType) {
			expressionType = ELexerExpressionType_Preprocessor;
			prevIt = str.ptr + i;
		}

		//Handle starting comment

		if(c == '/' && next == '/' && expressionType != ELexerExpressionType_MultiLineComment) {
			_gotoIfError(clean, Lexer_endExpression(&lastToken, &expressions, tokens.length, &expressionType));
			expressionType = ELexerExpressionType_Comment;
			multiChar = true;
			endToken = true;
		}

		//Handle starting multi line comment

		if(c == '/' && next == '*' && !expressionType) {
			_gotoIfError(clean, Lexer_endExpression(&lastToken, &expressions, tokens.length, &expressionType));
			expressionType = ELexerExpressionType_MultiLineComment;
			multiChar = true;
			endToken = true;
		}

		//Deal with closing multi line comment

		else if (expressionType == ELexerExpressionType_MultiLineComment) {

			if (c == '*' && next == '/') {
				multiChar = true;
				endExpression = true;
			}
		}

		//Parse ; as end of expression

		if(c == ';' && expressionType == ELexerExpressionType_Generic)
			endExpression = true;

		//First non whitespace char

		ELexerTokenType current = ELexerTokenType_Count;

		if(!C8_isWhitespace(c)) {

			if(!expressionType)
				expressionType = ELexerExpressionType_Generic;

			if(!prevIt)
				prevIt = str.ptr + i;

			/* TODO: If . merge with prev int to form float if no whitespace */

			tokenType = 
				tokenType != ELexerTokenType_Identifier && C8_isDec(c) ? ELexerTokenType_Integer : (
					C8_isNyto(c) ? ELexerTokenType_Identifier : ELexerTokenType_Symbols
				);

			ELexerTokenType nextType = 
				tokenType != ELexerTokenType_Identifier && C8_isDec(next) ? ELexerTokenType_Integer : (
					C8_isNyto(next) ? ELexerTokenType_Identifier : (
						C8_isWhitespace(next) ? ELexerTokenType_Count : ELexerTokenType_Symbols
					)
				);

			if(nextType != tokenType)
				endToken = true;
		}

		else if (prevIt) 
			endToken = true;
		
		if(endToken || (prevIt && endExpression)) {

			U64 len = (str.ptr + i + 1 + multiChar) - prevIt;

			if(len >> 8)
				_gotoIfError(clean, Error_outOfBounds(0, 256, 256, "Lexer_create() tokens are limited to 256 C8s"));

			if((i + 2  - lastLineStart) >> 7)
				_gotoIfError(clean, Error_outOfBounds(0, 128, 128, "Lexer_create() lines are limited to 128 C8s"));

			LexerToken tok = (LexerToken) { 
				.lineId = (U16) lineCounter, 
				.charId = (U8) (i + 1 - len - lastLineStart) + 1,
				.length = (U8) len, 
				.offsetType = (U32)((i + 1 + multiChar - len) | (tokenType << 30))
			};

			_gotoIfError(clean, ListLexerToken_pushBackx(&tokens, tok));
			prevIt = NULL;
			tokenType = ELexerTokenType_Count;
		}

		//Push it as an expression

		if (endExpression)
			_gotoIfError(clean, Lexer_endExpression(&lastToken, &expressions, tokens.length, &expressionType));

		if(multiChar)
			++i;
	}

	if (lastToken != tokens.length)
		_gotoIfError(clean, Lexer_endExpression(&lastToken, &expressions, tokens.length, &expressionType));

clean:

	if(err.genericError) {
		ListLexerToken_freex(&tokens);
		ListLexerExpression_freex(&expressions);
	}
	else *lexer = (Lexer) { 
		.source = CharString_createRefSizedConst(str.ptr, CharString_length(str), false),
		.expressions = expressions, 
		.tokens = tokens 
	};

	return err;
}

Bool Lexer_free(Lexer *parser) {

	if(!parser)
		return true;

	ListLexerExpression_freex(&parser->expressions);
	ListLexerToken_freex(&parser->tokens);
	return true;
}

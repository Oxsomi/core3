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
#include "types/string.h"
#include "platforms/ext/stringx.h"
#include "core3_bindings/parser.h"
#include "core3_bindings/lexer.h"

TListImpl(Token);
TListImpl(Symbol);
TListImpl(Define);
TListImpl(UserDefine);

CharString Token_asString(Token t, const Parser *p) {

	if(!p || t.naiveTokenId >= p->lexer->tokens.length)
		return CharString_createNull();

	CharString lextStr = LexerToken_asString(p->lexer->tokens.ptr[t.naiveTokenId], *p->lexer);

	if((U32)t.lexerTokenSubId + t.tokenSize > CharString_length(lextStr))
		return CharString_createNull();

	return CharString_createRefSizedConst(lextStr.ptr + t.lexerTokenSubId, t.tokenSize, false);
}

//Helpers for handling defines

//Returns U64_MAX if not found
//Returns [ defines.length, defines.length + userDefines.length > if found in a user define

U64 Parser_findDefine(ListDefine defines, ListUserDefine userDefines, CharString str, const Lexer *lexer) {

	//TODO: Map

	for (U64 i = 0; i < defines.length; ++i) {

		Define d = defines.ptr[i];
		CharString ds = LexerToken_asString(lexer->tokens.ptr[d.nameTokenId], *lexer);

		if (CharString_equalsStringSensitive(str, ds))
			return i;
	}

	for (U64 i = 0; i < userDefines.length; ++i) {

		UserDefine ud = userDefines.ptr[i];

		if (CharString_equalsStringSensitive(str, ud.name))
			return i + defines.length;
	}

	return U64_MAX;
}

Error Parser_eraseDefine(ListDefine *defines, ListUserDefine *userDefines, U64 i) {

	if(i == U64_MAX)
		return Error_none();

	if(i < defines->length)
		return ListDefine_erase(defines, i);

	i -= defines->length;

	if(i < userDefines->length)
		return ListUserDefine_erase(userDefines, i);

	return Error_outOfBounds(
		2, i, userDefines->length, "Parser_eraseDefine()::i exceeded userDefines->length + defines->length"
	);
}

//Define without value

Error Parser_addDefine(ListDefine *defines, U32 nameLexerTokenId) {
	Define define = (Define) { .defineType = EDefineType_ValueEmpty, .nameTokenId = nameLexerTokenId };
	return ListDefine_pushBackx(defines, define);
}

//Define with value

Error Parser_addDefineWithValue(ListDefine *defines, U32 nameLexerTokenId, U32 childLexerTokenId, U16 valueTokenCount) {

	Define define = (Define) { 
		.defineType = EDefineType_ValueFull, 
		.valueTokens = valueTokenCount,
		.nameTokenId = nameLexerTokenId,
		.valueStartId = childLexerTokenId
	};

	return ListDefine_pushBackx(defines, define);
}

//Macro with(out) value

Error Parser_addMacro(
	ListDefine *defines, 
	U32 nameLexerTokenId, 
	U32 childLexerTokenId, 
	U8 childCount,
	U32 valueLexerTokenId,
	U16 valueTokenCount
) {

	Define define = (Define) { 
		.defineType = !valueTokenCount ? EDefineType_MacroEmpty : EDefineType_MacroFull, 
		.children = childCount,
		.valueTokens = valueTokenCount,
		.nameTokenId = nameLexerTokenId,
		.childStartTokenId = childLexerTokenId,
		.valueStartId = valueLexerTokenId
	};

	return ListDefine_pushBackx(defines, define);
}

//Parse token type

ETokenType Parser_getTokenType(CharString str, U64 *subTokenOffset) {

	if(!subTokenOffset || *subTokenOffset >= CharString_length(str))
		return ETokenType_Count;

	U64 off = *subTokenOffset;
	C8 start = str.ptr[off];
	U64 count = 0;

	ETokenType tokenType = ETokenType_Count;

	C8 next  = off + 1 < CharString_length(str) ? str.ptr[off + 1] : 0;
	C8 next2 = off + 2 < CharString_length(str) ? str.ptr[off + 2] : 0;

	switch (start) {

		case '!':		// !, !=
			count = 1 + (next == '=');
			tokenType = count == 2 ? ETokenType_Neq : ETokenType_BooleanNot;
			break;

		case '~':		// ~
			count = 1;
			tokenType = ETokenType_Not;
			break;

		case '+':		// +, ++, +=
			tokenType = next == '=' ? ETokenType_AddAsg : (next == start ? ETokenType_Inc : ETokenType_Add);
			count = tokenType == ETokenType_Add ? 1 : 2;
			break;

		case '-':		// -, --, -=
			tokenType = next == '=' ? ETokenType_SubAsg : (next == start ? ETokenType_Dec : ETokenType_Sub);
			count = tokenType == ETokenType_Sub ? 1 : 2;
			break;

		case '/':		// /, /=
			count = 1 + (next == '=');
			tokenType = count == 2 ? ETokenType_DivAsg : ETokenType_Div;
			break;

		case '*':		// *, *=
			count = 1 + (next == '=');
			tokenType = count == 2 ? ETokenType_MulAsg : ETokenType_Mul;
			break;

		case '%':		// %, %=
			count = 1 + (next == '=');
			tokenType = count == 2 ? ETokenType_ModAsg : ETokenType_Mod;
			break;

		case '=':		// =, ==
			count = 1 + (next == '=');
			tokenType = count == 2 ? ETokenType_Eq : ETokenType_Asg;
			break;

		case '&':		// &, &&, &=
			tokenType = next == '=' ? ETokenType_AndAsg : (next == start ? ETokenType_BooleanAnd : ETokenType_And);
			count = tokenType == ETokenType_And ? 1 : 2;
			break;

		case '|':		// |, ||, |=
			tokenType = next == '=' ? ETokenType_OrAsg : (next == start ? ETokenType_BooleanOr : ETokenType_Or);
			count = tokenType == ETokenType_Or ? 1 : 2;
			break;

		case '^':		// ^, ^=
			count = 1 + (next == '=');
			tokenType = count == 2 ? ETokenType_XorAsg : ETokenType_Xor;
			break;

		case '<':		//<, <=, <<, <<=

			count = 1 + (next == '=' || next == start) + (next == start && next2 == '=');
			
			switch (count) {
				case 1:		tokenType = ETokenType_Lt;										break;
				case 3:		tokenType = ETokenType_LshAsg;									break;
				default:	tokenType = next == start ? ETokenType_Lsh : ETokenType_Leq;	break;
			}

			break;

		case '>':		//>, >=, >>, >>=

			count = 1 + (next == '=' || next == start) + (next == start && next2 == '=');
			
			switch (count) {
				case 1:		tokenType = ETokenType_Gt;										break;
				case 3:		tokenType = ETokenType_RshAsg;									break;
				default:	tokenType = next == start ? ETokenType_Rsh : ETokenType_Geq;	break;
			}

			break;

		//()[]{}:;,.

		case ')':	count = 1;	tokenType = ETokenType_RoundParenthesisStart;	break;
		case '(':	count = 1;	tokenType = ETokenType_RoundParenthesisEnd;		break;
		case '[':	count = 1;	tokenType = ETokenType_SquareBracketStart;		break;
		case ']':	count = 1;	tokenType = ETokenType_SquareBracketEnd;		break;
		case '{':	count = 1;	tokenType = ETokenType_CurlyBraceStart;			break;
		case '}':	count = 1;	tokenType = ETokenType_CurlyBraceEnd;			break;
		case ':':	count = 1;	tokenType = ETokenType_Colon;					break;
		case ';':	count = 1;	tokenType = ETokenType_Semicolon;				break;
		case ',':	count = 1;	tokenType = ETokenType_Comma;					break;
		case '.':	count = 1;	tokenType = ETokenType_Period;					break;
	}

	*subTokenOffset += count;
	return tokenType;
}

//Parse the lexed file

typedef struct PreprocessorIfStack {

	//I64 valueStack[14];
	//
	//U16 valueStackIsActive;
	//U8 valueStackCount;

	U8 padding[3];
	U8 stackIndex;			//Indicates how deep the stack is with #ifs

	U16 activeStack;		//Indicates if the current stack is active (>> (stackIndex - 1)) & 1
	U16 hasProcessedIf;		//If a scope has already been entered

} PreprocessorIfStack;

Error Parser_preprocessContents(
	const Lexer *lexer, 
	ListDefine defines, 
	ListUserDefine userDefines, 
	U64 tokenOffset, 
	U64 totalTokenCount,
	CharString *replaced
) {

	CharString temp = CharString_createNull();
	Error err = Error_none();

	U64 i = tokenOffset;

	while(i < tokenOffset + totalTokenCount) {
	
		LexerToken lext = lexer->tokens.ptr[i];
		CharString lextStr = LexerToken_asString(lext, *lexer);

		++i;
		U64 tokenCount = totalTokenCount - (i - tokenOffset);

		switch(lext.offsetType >> 30) {

			//Checking a define or defined()

			case ELexerTokenType_Identifier: {

				if(CharString_length(*replaced))
					_gotoIfError(clean, CharString_appendx(replaced, ' '));

				//If keyword is "defined" then match defined(identifier)

				if (CharString_equalsStringSensitive(lextStr, CharString_createRefCStrConst("defined"))) {

					if(tokenCount < 3)
						_gotoIfError(clean, Error_invalidState(2, "Parser_handleIfCondition() expected defined(identifier)"));

					lextStr = LexerToken_asString(lexer->tokens.ptr[tokenOffset], *lexer);

					if(!CharString_equalsSensitive(lextStr, '('))
						_gotoIfError(clean, Error_invalidState(
							3, "Parser_handleIfCondition() expected '(' in defined(identifier)"
						));

					LexerToken identifier = lexer->tokens.ptr[tokenOffset + 1];

					if((identifier.offsetType >> 30) != ELexerTokenType_Identifier)
						_gotoIfError(clean, Error_invalidState(
							4, "Parser_handleIfCondition() expected identifier in defined(identifier)"
						));

					lextStr = LexerToken_asString(lexer->tokens.ptr[tokenOffset + 2], *lexer);

					if (!CharString_equalsSensitive(lextStr, ')'))
						_gotoIfError(clean, Error_invalidState(
							5, "Parser_handleIfCondition() expected ')' in defined(identifier)"
						));

					lextStr = LexerToken_asString(identifier, *lexer);

					U64 defineId = Parser_findDefine(defines, userDefines, lextStr, lexer);

					_gotoIfError(clean, CharString_appendx(replaced, defineId != U64_MAX ? '1' : '0'));

					i += 3;
					break;
				}

				//Else check define value and return it

				U64 defineId = Parser_findDefine(defines, userDefines, lextStr, lexer);

				if (defineId == U64_MAX)
					_gotoIfError(clean, Error_invalidState(6, "Parser_handleIfCondition() expected define as identifier"));

				if (defineId >= defines.length) {		//User defined (must be int)

					UserDefine ud = userDefines.ptr[defineId - defines.length];

					if(!CharString_length(ud.value))
						_gotoIfError(clean, Error_invalidState(
							12, "Parser_handleIfCondition() expected value of user define"
						));

					U64 tmp = 0;
					if(!CharString_parseU64(ud.value, &tmp) || (tmp >> 63))
						_gotoIfError(clean, Error_invalidState(
							13, "Parser_handleIfCondition() expected user define as int (<I64_MAX)"
						));

					_gotoIfError(clean, CharString_appendStringx(replaced, ud.value));
					break;
				}

				//Macro

				Define def = defines.ptr[defineId];

				if (def.defineType & EDefineType_IsMacro) {
					//TODO: Support macros
					_gotoIfError(clean, Error_unimplemented(0, "Parser_handleIfCondition() #if value not supported yet"));
				}

				//Regular define (visit define)

				if(def.valueStartId != U32_MAX) {

					LexerToken start = lexer->tokens.ptr[def.valueStartId];
					LexerToken end = lexer->tokens.ptr[def.valueStartId + def.valueTokens - 1];

					const C8 *tokenStart = LexerToken_getTokenStart(start, *lexer);
					const C8 *tokenEnd = LexerToken_getTokenEnd(end, *lexer);

					_gotoIfError(clean, CharString_appendStringx(replaced, CharString_createRefSizedConst(
						tokenStart, tokenEnd - tokenStart, false
					)));
				}

				break;
			}

			//Unsupported in C

			case ELexerTokenType_Double:
				_gotoIfError(clean, Error_invalidState(
					7, "Parser_handleIfCondition() #if statements only accept integers and defines"
				));

			//Store value

			case ELexerTokenType_Integer:
			
				U64 tmp = 0;
				if(!CharString_parseU64(lextStr, &tmp) || tmp >> 63)
					_gotoIfError(clean, Error_invalidState(
						9, "Parser_handleIfCondition() #if statements expected integer (< I64_MAX)"
					));

				_gotoIfError(clean, CharString_appendStringx(replaced, lextStr));
				break;

			//Operators

			case ELexerTokenType_Symbols: 
				_gotoIfError(clean, CharString_appendStringx(replaced, lextStr));
				break;
		}
	}

clean:
	CharString_freex(&temp);
	return err;
}

Error Parser_handleIfCondition(
	PreprocessorIfStack *stack,
	const Lexer *lexer,
	ListDefine defines,
	ListUserDefine userDefines,
	U64 tokenOffset,
	U64 tokenCount,
	Bool *value
) {

	if(!tokenCount)
		return Error_invalidState(0, "Parser_handleIfCondition() expected token but got none");

	//First step is to resolve "identifiers" until there's none left (so resolving macros and defines).

	Error err = Error_none();
	CharString replaced = CharString_createNull();
	Lexer tempLexer = (Lexer) { 0 };
	Parser tempParser = (Parser) { 0 };
	Bool hasAnyIdentifier = false;
	U64 loopRecursions = 0;

	do {

		_gotoIfError(clean, Parser_preprocessContents(lexer, defines, userDefines, tokenOffset, tokenCount, &replaced));

		Lexer_free(&tempLexer);

		_gotoIfError(clean, Lexer_create(replaced, &tempLexer));

		hasAnyIdentifier = false;

		for(U64 i = 0; i < tempLexer.tokens.length; ++i)
			if ((tempLexer.tokens.ptr[i].offsetType >> 30) == ETokenType_Identifier) {
				hasAnyIdentifier = true;
				break;
			}

		++loopRecursions;

		if(loopRecursions > 32)
			_gotoIfError(clean, Error_invalidState(
				0, 
				"Parser_handleIfCondition() tried to parse #if condition. "
				"But it contained a keyword it didn't recognize. This possibly resulted in an infinite loop (>32 recursions)"
			));
	}
	while(hasAnyIdentifier);		//Lexer is freed if there's no more work

	//First, we have to transform the lexer into a parser so we have simplified tokens
	//Everything except tokens are cleared.
	//Tokens can be modified to evaluate the expression

	_gotoIfError(clean, Parser_create(&tempLexer, &tempParser, (ListUserDefine) { 0 }));

	_gotoIfError(clean, ListSymbol_clear(&tempParser.symbols));
	_gotoIfError(clean, ListDefine_clear(&tempParser.defines));

	//Then we have to scan for tokens in order of operator precedence and replace the affected tokens by a value

	if(tempParser.tokens.length >= 1) {

		const Token *firstToken = tempParser.tokens.ptr;
		ETokenType first = firstToken->tokenType;

		//We can expand this to complex operations by finding combos in order of precedence and replacing it with the value.

		if (tempParser.tokens.length == 1) {				//x

			if(first != ETokenType_Integer)
				_gotoIfError(clean, Error_invalidOperation(
					3, "Parser_handleIfCondition() couldn't parse #if statement correctly. Expected !x, -x, +x or ~x"
				));

			*value = firstToken->value;
		}

		else if(tempParser.tokens.length == 2) {			//!x, ~x, -x, +x

			const Token *secondToken = tempParser.tokens.ptr + 1;
			ETokenType second = secondToken->tokenType;

			if(second != ETokenType_Integer)
				_gotoIfError(clean, Error_invalidOperation(
					0, "Parser_handleIfCondition() couldn't parse #if statement correctly. Expected !x, -x, +x or ~x"
				));

			I64 result = secondToken->value;

			switch (first) {

				case ETokenType_BooleanNot:		result = !result;		break;
				case ETokenType_Not:			result = ~result;		break;
				case ETokenType_Sub:			result = -result;		break;
				case ETokenType_Add:			result = +result;		break;

				default:
					_gotoIfError(clean, Error_invalidOperation(
						1, "Parser_handleIfCondition() couldn't parse #if statement correctly. Unsupported token type (!+-~)"
					));
			}

			*value = result;
		}

		else if (tempParser.tokens.length == 3) {		//a >= b, <=, >, <, !=, ==, *, /, %, +, -, &&, ||, &, |, ^, <<, >>

			const Token *thirdToken = tempParser.tokens.ptr + 2;
			ETokenType third = thirdToken->tokenType;

			if(first != ETokenType_Integer || third != ETokenType_Integer)
				_gotoIfError(clean, Error_invalidOperation(
					4, 
					"Parser_handleIfCondition() couldn't parse #if statement correctly. "
					"Expected integer operator integer"
				));

			I64 result = firstToken->value;
			I64 thirdValue = thirdToken->value;

			Bool nullCheck = false;

			switch(tempParser.tokens.ptr[1].tokenType) {
			
				case ETokenType_Add:			result += thirdValue;										break;
				case ETokenType_Sub:			result -= thirdValue;										break;
				case ETokenType_Mul:			result *= thirdValue;										break;
				case ETokenType_Mod:			result %= thirdValue ? thirdValue : 1;	nullCheck = true;	break;
				case ETokenType_Div:			result /= thirdValue ? thirdValue : 1;	nullCheck = true;	break;
				
				case ETokenType_Or:				result |= thirdValue;										break;
				case ETokenType_And:			result &= thirdValue;										break;
				case ETokenType_Xor:			result ^= thirdValue;										break;

				case ETokenType_Lsh:			*(U64*)&result <<= thirdValue;								break;
				case ETokenType_Rsh:			*(U64*)&result >>= thirdValue;								break;

				case ETokenType_BooleanOr:		result = result || thirdValue;								break;
				case ETokenType_BooleanAnd:		result = result && thirdValue;								break;

				case ETokenType_Lt:				result = result < thirdValue;								break;
				case ETokenType_Leq:			result = result <= thirdValue;								break;
				case ETokenType_Gt:				result = result > thirdValue;								break;
				case ETokenType_Geq:			result = result >= thirdValue;								break;
				case ETokenType_Eq:				result = result == thirdValue;								break;
				case ETokenType_Neq:			result = result != thirdValue;								break;

				default:
					_gotoIfError(clean, Error_invalidOperation(
						4, 
						"Parser_handleIfCondition() couldn't parse #if statement correctly. "
						"Expected (a op b) where op is <=, >=, <, >, ==, !=, *, /, %, +, -, &&, ||, &, |, ^, << or >>"
					));
			}

			if(nullCheck && !thirdValue)
				_gotoIfError(clean, Error_unimplemented(
					0, "Parser_handleIfCondition() #if statement contained division by zero"
				))

			*value = result;
		}

		else {		//TODO: Complex expressions aren't supported yet
			_gotoIfError(clean, Error_unimplemented(
				0, "Parser_handleIfCondition() can't handle complex expressions yet (only operators with 1 or 2 integers"
			))
		}
	}

	else _gotoIfError(clean, Error_invalidOperation(
		2, "Parser_handleIfCondition() couldn't parse #if statement correctly. Missing token"
	));

clean:
	Parser_free(&tempParser);
	Lexer_free(&tempLexer);
	CharString_freex(&replaced);
	return err;
}

typedef struct ParserContext {
	ListUserDefine userDefines;
	ListDefine defines;
	ListSymbol symbols;
	ListToken tokens;
	PreprocessorIfStack stack;
	const Lexer *lexer;
} ParserContext;

Error ParserContext_visit(ParserContext *context, U32 lexerTokenId, U32 lexerTokenCount) {

	Error err = Error_none();
	const Lexer *lexer = context->lexer;

	//Ensure all previous scopes are active

	U16 fullStack = (U16)((1 << context->stack.stackIndex) - 1);
	Bool isPreprocessorScopeActive = !context->stack.stackIndex || (context->stack.activeStack & fullStack) == fullStack;

	//Handle preprocessor

	LexerToken lext = lexer->tokens.ptr[lexerTokenId];

	if (lext.length >= 1 && lexer->source.ptr[lext.offsetType << 2 >> 2] == '#') {

		if(lexerTokenCount < 2)
			_gotoIfError(clean, Error_invalidParameter(0, 0, "ParserContext_visit() source was invalid. Expected #<token>"));

		if(lext.length != 1)
			_gotoIfError(clean, Error_invalidParameter(
				0, 0, "ParserContext_visit() source was invalid. Expected # for preprocessor"
			));

		lext = lexer->tokens.ptr[lexerTokenId + 1];

		if(lext.length < 2)
			_gotoIfError(clean, Error_invalidParameter(
				0, 0, "ParserContext_visit() source was invalid. #<token> requires at least 2 chars"
			));

		CharString lextStr = LexerToken_asString(lext, *lexer);

		//Since C doesn't have switch(string), we'll have to make it ourselves

		U16 v16 = *(const U16*) (lexer->source.ptr + (lext.offsetType << 2 >> 2));

		switch (v16) {

			case C8x2('p', 'r'): {		//#pragma once

				if(!isPreprocessorScopeActive)
					goto clean;

				const C8 *pragmaOnce[] = { "pragma", "once" };
				CharString pragma = CharString_createRefCStrConst(pragmaOnce[0]);
				CharString once = CharString_createRefCStrConst(pragmaOnce[1]);

				if(!CharString_equalsStringSensitive(pragma, lextStr) || lexerTokenCount != 3)
					_gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #pragma once if #pr is detected"
					));

				lext = lexer->tokens.ptr[lexerTokenId + 2];
				lextStr = LexerToken_asString(lext, *lexer);

				if(!CharString_equalsStringSensitive(once, lextStr))
					_gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #pragma once if #pr is detected"
					));

				break;
			}

			case C8x2('i', 'n'):		//#include

				if(!isPreprocessorScopeActive)
					goto clean;

				if (!CharString_equalsStringSensitive(CharString_createRefCStrConst("include"), lextStr))
					_gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #include if #in is detected"
					));

				//TODO:

				break;

			case C8x2('d', 'e'): {		//#define

				if(!isPreprocessorScopeActive)
					goto clean;

				if (!CharString_equalsStringSensitive(CharString_createRefCStrConst("define"), lextStr))
					_gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #define if #de is detected"
					));

				if(lexerTokenCount < 3)
					_gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #define <define>..."
					));

				lext = lexer->tokens.ptr[lexerTokenId + 2];
				lextStr = LexerToken_asString(lext, *lexer);

				U64 i = Parser_findDefine(context->defines, context->userDefines, lextStr, lexer);
				_gotoIfError(clean, Parser_eraseDefine(&context->defines, &context->userDefines, i));

				//"defined" is not allowed

				if(CharString_equalsStringSensitive(lextStr, CharString_createRefCStrConst("defined")))
					_gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. #define defined is illegal"
					));

				//Name only, no value (#define MY_VALUE)

				if(lexerTokenCount == 3)
					_gotoIfError(clean, Parser_addDefine(&context->defines, lexerTokenId + 2))

				//#define MY_VALUE(x, ...) ... or #define MY_VALUE 1 ...

				else {

					//If next char is ( without space, then it's a macro

					Bool isMacro = false;

					if (lexerTokenCount >= 4) {
						LexerToken tok = lexer->tokens.ptr[lexerTokenId + 3];
						isMacro = tok.length == 1 && lexer->source.ptr[tok.offsetType << 2 >> 2] == '(';
					}

					if(isMacro) {

						if (lexerTokenCount < 6)
							_gotoIfError(clean, Error_invalidParameter(
								0, 0, "ParserContext_visit() source was invalid. Expected #define macro(...) ..."
							));

						//Require (a), (a, b, ...), (...), etc.

						U32 j = lexerTokenId + 3;

						lext = lexer->tokens.ptr[j];
						lextStr = LexerToken_asString(lext, *lexer);

						if(!CharString_equalsSensitive(lextStr, '('))
							_gotoIfError(clean, Error_invalidParameter(
								0, 0, "ParserContext_visit() source was invalid. Expected ( in #define macro(...)"
							));

						++j;

						for (; j < lexerTokenId + lexerTokenCount; j += 2) {

							lext = lexer->tokens.ptr[j];
							lextStr = LexerToken_asString(lext, *lexer);

							if (CharString_equalsStringSensitive(lextStr, CharString_createRefCStrConst("..."))) {
								++j;
								break;
							}

							if((lext.offsetType >> 30) != ELexerTokenType_Identifier)
								_gotoIfError(clean, Error_invalidParameter(
									0, 0, "ParserContext_visit() source was invalid. Expected identifier in macro"
								));

							if (lexerTokenCount <= j - lexerTokenId + 1)
								_gotoIfError(clean, Error_invalidParameter(
									0, 0, "ParserContext_visit() source was invalid. Expected token in macro"
								));

							lext = lexer->tokens.ptr[j + 1];
							lextStr = LexerToken_asString(lext, *lexer);

							if(CharString_equalsSensitive(lextStr, ')')) {
								++j;
								break;
							}

							if(!CharString_equalsSensitive(lextStr, ','))
								_gotoIfError(clean, Error_invalidParameter(
									0, 0, "ParserContext_visit() source was invalid. Expected , separator in macro"
								));
						}

						lext = lexer->tokens.ptr[j];
						lextStr = LexerToken_asString(lext, *lexer);

						if(!CharString_equalsSensitive(lextStr, ')'))
							_gotoIfError(clean, Error_invalidParameter(
								0, 0, "ParserContext_visit() source was invalid. Expected ) in #define macro(...)"
							));

						if(((j - (lexerTokenId + 3) + 1) >> 1) >> 8)
							_gotoIfError(clean, Error_invalidParameter(
								0, 0, "ParserContext_visit() source was invalid. Too many macro children"
							));

						if((lexerTokenId + lexerTokenCount - (j + 1)) >> 16)
							_gotoIfError(clean, Error_invalidParameter(
								0, 0, "ParserContext_visit() source was invalid. Too many value children"
							));

						_gotoIfError(clean, Parser_addMacro(
							&context->defines, 
							lexerTokenId + 2,									//Name
							lexerTokenId + 3,									//Children
							(U8)((j - (lexerTokenId + 3) + 1) >> 1),			//Child count
							j + 1,												//Value start
							(U16)(lexerTokenId + lexerTokenCount - (j + 1))		//Value count
						));
					}

					//Otherwise we have a value

					else _gotoIfError(clean, Parser_addDefineWithValue(
						&context->defines, lexerTokenId + 2, lexerTokenId + 3, lexerTokenCount - 3
					));

				}

				break;
			}

			case C8x2('i', 'f'):		//#if, #ifdef, #ifndef

				if(context->stack.stackIndex == 16)
					_gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. #if stack can only be 16 deep"
					));

				++context->stack.stackIndex;

				U16 mask = (U16)(1 << (context->stack.stackIndex - 1));

				if (lext.length == 5 || lext.length == 6) {			//#ifdef, #ifndef

					if(lext.length == 5 && !CharString_equalsStringSensitive(			//#ifdef
						CharString_createRefCStrConst("ifdef"), lextStr
					))
						_gotoIfError(clean, Error_invalidParameter(
							0, 0, "ParserContext_visit() source was invalid. Expected #ifdef"
						));

					if(lext.length == 6 && !CharString_equalsStringSensitive(			//#ifndef
						CharString_createRefCStrConst("ifndef"), lextStr
					))
						_gotoIfError(clean, Error_invalidParameter(
							0, 0, "ParserContext_visit() source was invalid. Expected #ifdef"
						));

					Bool isIfdef = lext.length == 5;

					if(lexerTokenCount < 3)
						_gotoIfError(clean, Error_invalidParameter(
							0, 0, "ParserContext_visit() source was invalid. Expected #if(n)def <define>"
						));

					lext = lexer->tokens.ptr[lexerTokenId + 2];
					lextStr = LexerToken_asString(lext, *lexer);

					U64 i = Parser_findDefine(context->defines, context->userDefines, lextStr, lexer);

					Bool enable = (i != U64_MAX) == isIfdef;

					if (enable) {
						context->stack.hasProcessedIf |= mask;
						context->stack.activeStack |= mask;
					}

					else {
						context->stack.hasProcessedIf &=~ mask;
						context->stack.activeStack &=~ mask;		//Ensure we mark the scope as inactive
					}
				}

				else if(lext.length != 2)
					_gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #if, #ifndef or #ifdef"
					))

				else {						//#if

					Bool value = false;
					_gotoIfError(clean, Parser_handleIfCondition(
						&context->stack, lexer, context->defines, context->userDefines,
						lexerTokenId + 2, lexerTokenCount - 2,
						&value
					));

					if (value) {
						context->stack.hasProcessedIf |= mask;
						context->stack.activeStack |= mask;
					}

					else {
						context->stack.hasProcessedIf &=~ mask;
						context->stack.activeStack &=~ mask;		//Ensure we mark the scope as inactive
					}
				}

				break;

			case C8x2('e', 'l'): {		//#else, #elif

				if(lext.length != 4)
					_gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #elif or #else"
					));

				if(!context->stack.stackIndex)
					_gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. #else/#elif didn't have a matching #if/#ifdef/#ifndef"
					));

				U16 mask = (U16)(1 << (context->stack.stackIndex - 1));

				v16 = *(const U16*) (lexer->source.ptr + (lext.offsetType << 2 >> 2) + 2);

				if(v16 == C8x2('s', 'e')) {			//#else

					//#else only works if none of the predecessors have been processed yet

					if (!(context->stack.hasProcessedIf & mask)) {
						context->stack.hasProcessedIf |= mask;
						context->stack.activeStack |= mask;
					}

					else context->stack.activeStack &=~ mask;		//Ensure we mark the scope as inactive
				}

				else if(v16 == C8x2('i', 'f')) {		//#elif

					//#elif only works if none of the predecessors have been processed yet

					Bool value = false;
					_gotoIfError(clean, Parser_handleIfCondition(
						&context->stack, lexer, context->defines, context->userDefines,
						lexerTokenId + 2, lexerTokenCount - 2,
						&value
					));

					if (!(context->stack.hasProcessedIf & mask) && value) {
						context->stack.hasProcessedIf |= mask;
						context->stack.activeStack |= mask;
					}

					else context->stack.activeStack &=~ mask;		//Ensure we mark the scope as inactive
				}

				else _gotoIfError(clean, Error_invalidParameter(
					0, 0, "ParserContext_visit() source was invalid. Expected #elif or #else"
				));

				break;
			}

			case C8x2('e', 'n'):		//#endif

				if (!CharString_equalsStringSensitive(CharString_createRefCStrConst("endif"), lextStr))
					_gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #endif if #en is detected"
					));

				if(!context->stack.stackIndex)
					_gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. #endif didn't have a matching #if/#ifdef/#ifndef"
					));

				--context->stack.stackIndex;
				break;

			case C8x2('e', 'r'): {		//#error

				if(!isPreprocessorScopeActive)
					goto clean;

				if (!CharString_equalsStringSensitive(CharString_createRefCStrConst("error"), lextStr))
					_gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #error if #er is detected"
					));

				const C8 *startPtr = lexer->source.ptr + (lext.offsetType << 2 >> 2);
				lext = lexer->tokens.ptr[lexerTokenId + lexerTokenCount - 1];

				const C8 *endPtr = lexer->source.ptr + (lext.offsetType << 2 >> 2) + lext.length - 1;

				endPtr;		//TODO: Put this into the Error msg once it supports CharStrings instead of const C8*
				startPtr;

				_gotoIfError(clean, Error_invalidState(0, "ParserContext_visit() source triggered #error message"));

				break;
			}

			case C8x2('u', 'n'):		//#undef

				if(!isPreprocessorScopeActive)
					goto clean;

				if (
					!CharString_equalsStringSensitive(CharString_createRefCStrConst("undef"), lextStr) ||
					lexerTokenCount != 3
				)
					_gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #undef <value> if #un is detected"
					));

				lext = lexer->tokens.ptr[lexerTokenId + 2];
				lextStr = LexerToken_asString(lext, *lexer);

				U64 i = Parser_findDefine(context->defines, context->userDefines, lextStr, lexer);
				_gotoIfError(clean, Parser_eraseDefine(&context->defines, &context->userDefines, i));

				break;

			default:					//Unsupported preprocessor statement

				_gotoIfError(clean, Error_unsupportedOperation(
					0, "ParserContext_visit() source was invalid. Expected preprocessor directive is unsupported"
				));

				break;
		}
	}

	//Handle real expressions

	else {

		if(!isPreprocessorScopeActive)
			goto clean;

		for(U32 i = 0; i < lexerTokenCount; ++i) {

			LexerToken lext = lexer->tokens.ptr[lexerTokenId + i];
			CharString lextStr = LexerToken_asString(lext, *lexer);
			ELexerTokenType lextType = (ELexerTokenType)(lext.offsetType >> 30);

			if(CharString_length(lextStr) >> 8)
				_gotoIfError(clean, Error_invalidParameter(0, 0, "ParserContext_visit() token max size is 256"));

			switch(lextType) {

				//Handle define/macro and normal keyword

				case ELexerTokenType_Identifier: {

					//Visit define

					U64 defineId = Parser_findDefine(context->defines, context->userDefines, lextStr, lexer);

					if (defineId != U64_MAX) {

						if (defineId >= context->defines.length) {		//User defined (must be int)

							UserDefine ud = context->userDefines.ptr[defineId - context->defines.length];
							ud;

							//TODO: Support user define as identifier

							_gotoIfError(clean, Error_unimplemented(0, "ParserContext_visit() user define is unsupported"));
						}

						//Macro

						Define def = context->defines.ptr[defineId];

						if (def.defineType & EDefineType_IsMacro) {
							//TODO: Support macros
							_gotoIfError(clean, Error_unimplemented(1, "ParserContext_visit() macro is unsupported"));
						}

						//Value

						if(def.valueStartId != U32_MAX)
							_gotoIfError(clean, ParserContext_visit(context, def.valueStartId, def.valueTokens));
					}

					//Otherwise it's probably a type of a keyword

					else {

						Token tok = (Token) { 
							.naiveTokenId = lexerTokenId + i, 
							.tokenType = ETokenType_Identifier,
							.tokenSize = (U8) CharString_length(lextStr)
						};

						_gotoIfError(clean, ListToken_pushBackx(&context->tokens, tok));
					}

					break;
				}

				//Append entire lexer token

				case ELexerTokenType_Double: {

					F64 tmp = 0;
					if(!CharString_parseDouble(lextStr, &tmp))
						_gotoIfError(clean, Error_invalidOperation(
							2, "ParserContext_visit() expected double but couldn't parse it"
						))

					Token tok = (Token) {
						.naiveTokenId = lexerTokenId + i,
						.tokenType = ETokenType_Double,
						.value = (I64) (const U64*) &tmp,
						.tokenSize = (U8) CharString_length(lextStr)
					};

					_gotoIfError(clean, ListToken_pushBackx(&context->tokens, tok));
					break;
				}

				case ELexerTokenType_Integer: {

					U64 tmp = 0;
					if(!CharString_parseU64(lextStr, &tmp) || (tmp >> 63))
						_gotoIfError(clean, Error_invalidOperation(
							3, "ParserContext_visit() expected integer, but couldn't parse it"
						))

					Token tok = (Token) {
						.naiveTokenId = lexerTokenId + i,
						.tokenType = ETokenType_Integer,
						.value = (I64) tmp,
						.tokenSize = (U8) CharString_length(lextStr)
					};

					_gotoIfError(clean, ListToken_pushBackx(&context->tokens, tok));
					break;
				}

				//Symbols need to be split into multiple tokens, because someone can use &=~ for example
				//(two operators in one lexer token)

				default:

					U64 subTokenOffset = 0;

					while(subTokenOffset < CharString_length(lextStr)) {

						U64 prev = subTokenOffset;
						ETokenType tokenType = Parser_getTokenType(lextStr, &subTokenOffset);

						if(tokenType == ETokenType_Count)
							_gotoIfError(clean, Error_unsupportedOperation(1, "ParserContext_visit() invalid token"));

						Token tok = (Token) {
							.naiveTokenId = lexerTokenId + i,
							.tokenType = tokenType,
							.lexerTokenSubId = (U8) prev,
							.tokenSize = (U8)(subTokenOffset - prev)
						};

						_gotoIfError(clean, ListToken_pushBackx(&context->tokens, tok));
					}
			}
		}
	}

clean:
	return err;
}

Error Parser_create(const Lexer *lexer, Parser *parser, ListUserDefine userDefinesOuter) {

	if(!lexer || !parser)
		return Error_nullPointer(!lexer ? 0 : 1, "Parser_create()::parser and lexer are required");

	for (U64 i = 0; i < userDefinesOuter.length; ++i) {

		UserDefine ud = userDefinesOuter.ptr[i];

		if (!CharString_length(ud.name))
			return Error_nullPointer(2, "Parser_create()::userDefines[i].name is required");
	}

	Error err = Error_none();
	ParserContext context = (ParserContext) { .lexer = lexer };

	_gotoIfError(clean, ListSymbol_reservex(&context.symbols, 64));
	_gotoIfError(clean, ListDefine_reservex(&context.defines, 64));
	_gotoIfError(clean, ListToken_reservex(&context.tokens, lexer->tokens.length * 3 / 2));
	_gotoIfError(clean, ListUserDefine_createCopyx(userDefinesOuter, &context.userDefines));

	for (U64 i = 0; i < lexer->expressions.length; ++i) {

		LexerExpression e = lexer->expressions.ptr[i];

		if(e.type == ELexerExpressionType_MultiLineComment || e.type == ELexerExpressionType_Comment)
			continue;

		_gotoIfError(clean, ParserContext_visit(&context, e.tokenOffset, e.tokenCount));
	}

	//TODO: Combine tokens into symbols

clean:

	if(err.genericError) {
		ListToken_freex(&context.tokens);
		ListSymbol_freex(&context.symbols);
		ListDefine_freex(&context.defines);
	}

	else *parser = (Parser) { 
		.symbols = context.symbols, 
		.tokens = context.tokens, 
		.defines = context.defines,
		.lexer = lexer
	};

	ListUserDefine_freex(&context.userDefines);
	return err;
}

Bool Parser_free(Parser *parser) {

	if(!parser)
		return true;

	ListSymbol_freex(&parser->symbols);
	ListToken_freex(&parser->tokens);
	ListDefine_freex(&parser->defines);
	return true;
}

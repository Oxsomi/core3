/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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

#include "types/list_impl.h"
#include "types/string.h"
#include "types/parser.h"
#include "types/lexer.h"

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

Error Parser_addDefine(ListDefine *defines, U32 nameLexerTokenId, Allocator alloc) {
	Define define = (Define) { .defineType = EDefineType_ValueEmpty, .nameTokenId = nameLexerTokenId };
	return ListDefine_pushBack(defines, define, alloc);
}

//Define with value

Error Parser_addDefineWithValue(
	ListDefine *defines, U32 nameLexerTokenId, U32 childLexerTokenId, U16 valueTokenCount, Allocator alloc
) {

	Define define = (Define) { 
		.defineType = EDefineType_ValueFull, 
		.valueTokens = valueTokenCount,
		.nameTokenId = nameLexerTokenId,
		.valueStartId = childLexerTokenId
	};

	return ListDefine_pushBack(defines, define, alloc);
}

//Macro with(out) value

Error Parser_addMacro(
	ListDefine *defines, 
	U32 nameLexerTokenId, 
	U32 childLexerTokenId, 
	U8 childCount,
	U32 valueLexerTokenId,
	U16 valueTokenCount,
	Allocator alloc
) {

	Define define = (Define) { 
		.defineType = !valueTokenCount ? EDefineType_MacroEmpty : EDefineType_MacroFull, 
		.children = childCount,
		.valueTokens = valueTokenCount,
		.nameTokenId = nameLexerTokenId,
		.childStartTokenId = childLexerTokenId,
		.valueStartId = valueLexerTokenId
	};

	return ListDefine_pushBack(defines, define, alloc);
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

		case '?':		// ~
			count = 1;
			tokenType = ETokenType_Ternary;
			break;

		case '+':		// +, ++, +=
			tokenType = next == '=' ? ETokenType_AddAsg : (next == start ? ETokenType_Inc : ETokenType_Add);
			count = tokenType == ETokenType_Add ? 1 : 2;
			break;

		case '-':		// -, ->, --, -=

			tokenType = 
				next == '=' ? ETokenType_SubAsg : (
					next == start ? ETokenType_Dec : (next == '>' ? ETokenType_Arrow : ETokenType_Sub)
				);

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

		case '<':		//<, <=, <<, <<=, <=>

			count = 1 + (next == '=' || next == start) + (next == start && next2 == '=') + (next2 == '>');
			
			switch (count) {
				case 1:		tokenType = ETokenType_Lt;												break;
				case 3:		tokenType = next2 == '>' ? ETokenType_Flagship : ETokenType_LshAsg;		break;
				default:	tokenType = next == start ? ETokenType_Lsh : ETokenType_Leq;			break;
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

		case '(':	count = 1;	tokenType = ETokenType_RoundParenthesisStart;	break;
		case ')':	count = 1;	tokenType = ETokenType_RoundParenthesisEnd;		break;
		case '{':	count = 1;	tokenType = ETokenType_CurlyBraceStart;			break;
		case '}':	count = 1;	tokenType = ETokenType_CurlyBraceEnd;			break;
		case ';':	count = 1;	tokenType = ETokenType_Semicolon;				break;
		case ',':	count = 1;	tokenType = ETokenType_Comma;					break;
		case '.':	count = 1;	tokenType = ETokenType_Period;					break;

		//: and ::
		//[ and [[
		//] and ]]

		case ':':
			count = next == ':' ? 2 : 1;
			tokenType = count == 2 ? ETokenType_Colon2 : ETokenType_Colon;
			break;

		case '[':
			count = next == '[' ? 2 : 1;
			tokenType = count == 2 ? ETokenType_SquareBracketStart2 : ETokenType_SquareBracketStart;
			break;

		case ']':
			count = next == ']' ? 2 : 1;
			tokenType = count == 2 ? ETokenType_SquareBracketEnd2 : ETokenType_SquareBracketEnd;
			break;
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
	CharString *replaced,
	Allocator alloc
) {

	CharString temp = CharString_createNull();
	Error err = Error_none();

	U64 i = tokenOffset;

	while(i < tokenOffset + totalTokenCount) {
	
		LexerToken lext = lexer->tokens.ptr[i];
		CharString lextStr = LexerToken_asString(lext, *lexer);

		++i;
		U64 tokenCount = totalTokenCount - (i - tokenOffset);

		switch(LexerToken_getType(lext)) {

			//Checking a define or defined()

			case ELexerTokenType_Identifier: {

				if(CharString_length(*replaced))
					gotoIfError(clean, CharString_append(replaced, ' ', alloc))

				//If keyword is "defined" then match defined(identifier)

				if (CharString_equalsStringSensitive(lextStr, CharString_createRefCStrConst("defined"))) {

					if(tokenCount < 3)
						gotoIfError(clean, Error_invalidState(2, "Parser_handleIfCondition() expected defined(identifier)"))

					lextStr = LexerToken_asString(lexer->tokens.ptr[tokenOffset], *lexer);

					if(!CharString_equalsSensitive(lextStr, '('))
						gotoIfError(clean, Error_invalidState(
							3, "Parser_handleIfCondition() expected '(' in defined(identifier)"
						))

					LexerToken identifier = lexer->tokens.ptr[tokenOffset + 1];

					if(LexerToken_getType(identifier) != ELexerTokenType_Identifier)
						gotoIfError(clean, Error_invalidState(
							4, "Parser_handleIfCondition() expected identifier in defined(identifier)"
						))

					lextStr = LexerToken_asString(lexer->tokens.ptr[tokenOffset + 2], *lexer);

					if (!CharString_equalsSensitive(lextStr, ')'))
						gotoIfError(clean, Error_invalidState(
							5, "Parser_handleIfCondition() expected ')' in defined(identifier)"
						))

					lextStr = LexerToken_asString(identifier, *lexer);

					U64 defineId = Parser_findDefine(defines, userDefines, lextStr, lexer);

					gotoIfError(clean, CharString_append(replaced, defineId != U64_MAX ? '1' : '0', alloc))

					i += 3;
					break;
				}

				//Else check define value and return it

				U64 defineId = Parser_findDefine(defines, userDefines, lextStr, lexer);

				if (defineId == U64_MAX)
					gotoIfError(clean, Error_invalidState(6, "Parser_handleIfCondition() expected define as identifier"))

				if (defineId >= defines.length) {		//User defined (must be int)

					UserDefine ud = userDefines.ptr[defineId - defines.length];

					if(!CharString_length(ud.value))
						gotoIfError(clean, Error_invalidState(
							12, "Parser_handleIfCondition() expected value of user define"
						))

					U64 tmp = 0;
					if(!CharString_parseU64(ud.value, &tmp) || (tmp >> 63))
						gotoIfError(clean, Error_invalidState(
							13, "Parser_handleIfCondition() expected user define as int (<I64_MAX)"
						))

					gotoIfError(clean, CharString_appendString(replaced, ud.value, alloc))
					break;
				}

				//Macro

				Define def = defines.ptr[defineId];

				if (def.defineType & EDefineType_IsMacro) {
					//TODO: Support macros
					gotoIfError(clean, Error_unimplemented(0, "Parser_handleIfCondition() #if value not supported yet"))
				}

				//Regular define (visit define)

				if(def.valueStartId != U32_MAX) {

					LexerToken start = lexer->tokens.ptr[def.valueStartId];
					LexerToken end = lexer->tokens.ptr[def.valueStartId + def.valueTokens - 1];

					const C8 *tokenStart = LexerToken_getTokenStart(start, *lexer);
					const C8 *tokenEnd = LexerToken_getTokenEnd(end, *lexer);

					gotoIfError(clean, CharString_appendString(
						replaced,
						CharString_createRefSizedConst(tokenStart, tokenEnd - tokenStart, false),
						alloc
					))
				}

				break;
			}

			//Unsupported in C

			case ELexerTokenType_Float:
			case ELexerTokenType_Double:
				gotoIfError(clean, Error_invalidState(
					7, "Parser_handleIfCondition() #if statements only accept integers and defines"
				))

			//Store value

			case ELexerTokenType_IntegerBinary:
			case ELexerTokenType_IntegerHex:
			case ELexerTokenType_IntegerOctal:
			case ELexerTokenType_IntegerDec:
			case ELexerTokenType_IntegerNyto: {
			
				U64 tmp = 0;
				if(!CharString_parseU64(lextStr, &tmp) || tmp >> 63)
					gotoIfError(clean, Error_invalidState(
						9, "Parser_handleIfCondition() #if statements expected integer (< I64_MAX)"
					))

				gotoIfError(clean, CharString_appendString(replaced, lextStr, alloc))
				break;
			}

			//Operators

			case ELexerTokenType_Symbols: 
				gotoIfError(clean, CharString_appendString(replaced, lextStr, alloc))
				break;
		}
	}

clean:
	CharString_free(&temp, alloc);
	return err;
}

Error Parser_handleIfCondition(
	//PreprocessorIfStack *stack,
	const Lexer *lexer,
	ListDefine defines,
	ListUserDefine userDefines,
	U64 tokenOffset,
	U64 tokenCount,
	Bool *value,
	Allocator alloc
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

		gotoIfError(clean, Parser_preprocessContents(lexer, defines, userDefines, tokenOffset, tokenCount, &replaced, alloc))

		Lexer_free(&tempLexer, alloc);

		gotoIfError(clean, Lexer_create(replaced, alloc, &tempLexer))

		hasAnyIdentifier = false;

		for(U64 i = 0; i < tempLexer.tokens.length; ++i)
			if (LexerToken_getType(tempLexer.tokens.ptr[i]) == ETokenType_Identifier) {
				hasAnyIdentifier = true;
				break;
			}

		++loopRecursions;

		if(loopRecursions > 32)
			gotoIfError(clean, Error_invalidState(
				0, 
				"Parser_handleIfCondition() tried to parse #if condition. "
				"But it contained a keyword it didn't recognize. This possibly resulted in an infinite loop (>32 recursions)"
			))
	}
	while(hasAnyIdentifier);		//Lexer is freed if there's no more work

	//First, we have to transform the lexer into a parser so we have simplified tokens
	//Everything except tokens are cleared.
	//Tokens can be modified to evaluate the expression

	gotoIfError(clean, Parser_create(&tempLexer, &tempParser, (ListUserDefine) { 0 }, alloc))

	gotoIfError(clean, ListSymbol_clear(&tempParser.symbols))
	gotoIfError(clean, ListDefine_clear(&tempParser.defines))

	//Then we have to scan for tokens in order of operator precedence and replace the affected tokens by a value

	if(tempParser.tokens.length >= 1) {

		const Token *firstToken = tempParser.tokens.ptr;
		ETokenType first = firstToken->tokenType;

		//We can expand this to complex operations by finding combos in order of precedence and replacing it with the value.

		if (tempParser.tokens.length == 1) {				//x

			if(first != ETokenType_Integer)
				gotoIfError(clean, Error_invalidOperation(
					3, "Parser_handleIfCondition() couldn't parse #if statement correctly. Expected !x, -x, +x or ~x"
				))

			*value = firstToken->valuei;
		}

		else if(tempParser.tokens.length == 2) {			//!x, ~x, -x, +x

			const Token *secondToken = tempParser.tokens.ptr + 1;
			ETokenType second = secondToken->tokenType;

			if(second != ETokenType_Integer)
				gotoIfError(clean, Error_invalidOperation(
					0, "Parser_handleIfCondition() couldn't parse #if statement correctly. Expected !x, -x, +x or ~x"
				))

			I64 result = secondToken->valuei;

			switch (first) {

				case ETokenType_BooleanNot:		result = !result;		break;
				case ETokenType_Not:			result = ~result;		break;
				case ETokenType_Sub:			result = -result;		break;
				case ETokenType_Add:			result = +result;		break;

				default:
					gotoIfError(clean, Error_invalidOperation(
						1, "Parser_handleIfCondition() couldn't parse #if statement correctly. Unsupported token type (!+-~)"
					))
			}

			*value = result;
		}

		else if (tempParser.tokens.length == 3) {		//a >= b, <=, >, <, !=, ==, *, /, %, +, -, &&, ||, &, |, ^, <<, >>

			const Token *thirdToken = tempParser.tokens.ptr + 2;
			ETokenType third = thirdToken->tokenType;

			if(first != ETokenType_Integer || third != ETokenType_Integer)
				gotoIfError(clean, Error_invalidOperation(
					4, 
					"Parser_handleIfCondition() couldn't parse #if statement correctly. "
					"Expected integer operator integer"
				))

			I64 result = firstToken->valuei;
			I64 thirdValue = thirdToken->valuei;

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
					gotoIfError(clean, Error_invalidOperation(
						4, 
						"Parser_handleIfCondition() couldn't parse #if statement correctly. "
						"Expected (a op b) where op is <=, >=, <, >, ==, !=, *, /, %, +, -, &&, ||, &, |, ^, << or >>"
					))
			}

			if(nullCheck && !thirdValue)
				gotoIfError(clean, Error_unimplemented(
					0, "Parser_handleIfCondition() #if statement contained division by zero"
				))

			*value = result;
		}

		else {		//TODO: Complex expressions aren't supported yet
			gotoIfError(clean, Error_unimplemented(
				0, "Parser_handleIfCondition() can't handle complex expressions yet (only operators with 1 or 2 integers"
			))
		}
	}

	else gotoIfError(clean, Error_invalidOperation(
		2, "Parser_handleIfCondition() couldn't parse #if statement correctly. Missing token"
	))

clean:
	Parser_free(&tempParser, alloc);
	Lexer_free(&tempLexer, alloc);
	CharString_free(&replaced, alloc);
	return err;
}

typedef struct ParserContext {

	ListUserDefine userDefines;
	ListDefine defines;
	ListSymbol symbols;
	ListToken tokens;
	ListCharString stringLiterals;

	PreprocessorIfStack stack;
	const Lexer *lexer;

	//Information for handling errors

	CharString currentFile;
	U64 currentLine;

} ParserContext;

Error ParserContext_visit(ParserContext *context, U32 lexerTokenId, U32 lexerTokenCount, Allocator alloc) {

	Error err = Error_none();
	const Lexer *lexer = context->lexer;
	CharString temp = CharString_createNull();		//For parsing strings

	//Ensure all previous scopes are active

	U16 fullStack = (U16)((1 << context->stack.stackIndex) - 1);
	Bool isPreprocessorScopeActive = !context->stack.stackIndex || (context->stack.activeStack & fullStack) == fullStack;

	//Handle preprocessor

	LexerToken lext = lexer->tokens.ptr[lexerTokenId];

	if (lext.length >= 1 && lexer->source.ptr[LexerToken_getOffset(lext)] == '#') {

		if(lexerTokenCount < 2)
			gotoIfError(clean, Error_invalidParameter(0, 0, "ParserContext_visit() source was invalid. Expected #<token>"))

		if(lext.length != 1)
			gotoIfError(clean, Error_invalidParameter(
				0, 0, "ParserContext_visit() source was invalid. Expected # for preprocessor"
			))

		lext = lexer->tokens.ptr[lexerTokenId + 1];

		if(lext.length < 2)
			gotoIfError(clean, Error_invalidParameter(
				0, 0, "ParserContext_visit() source was invalid. #<token> requires at least 2 chars"
			))

		CharString lextStr = LexerToken_asString(lext, *lexer);

		//Since C doesn't have switch(string), we'll have to make it ourselves

		U16 v16 = *(const U16*) (lexer->source.ptr + LexerToken_getOffset(lext));

		switch (v16) {

			case C8x2('p', 'r'): {		//#pragma once

				if(!isPreprocessorScopeActive)
					goto clean;

				const C8 *pragmaOnce[] = { "pragma", "once" };
				CharString pragma = CharString_createRefCStrConst(pragmaOnce[0]);
				CharString once = CharString_createRefCStrConst(pragmaOnce[1]);

				if(!CharString_equalsStringSensitive(pragma, lextStr) || lexerTokenCount != 3)
					gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #pragma once if #pr is detected"
					))

				lext = lexer->tokens.ptr[lexerTokenId + 2];
				lextStr = LexerToken_asString(lext, *lexer);

				if(!CharString_equalsStringSensitive(once, lextStr))
					gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #pragma once if #pr is detected"
					))

				break;
			}

			case C8x2('i', 'n'):		//#include

				if(!isPreprocessorScopeActive)
					goto clean;

				if (!CharString_equalsStringSensitive(CharString_createRefCStrConst("include"), lextStr))
					gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #include if #in is detected"
					))

				//TODO:

				break;

			case C8x2('d', 'e'): {		//#define

				if(!isPreprocessorScopeActive)
					goto clean;

				if (!CharString_equalsStringSensitive(CharString_createRefCStrConst("define"), lextStr))
					gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #define if #de is detected"
					))

				if(lexerTokenCount < 3)
					gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #define <define>..."
					))

				lext = lexer->tokens.ptr[lexerTokenId + 2];
				lextStr = LexerToken_asString(lext, *lexer);

				U64 i = Parser_findDefine(context->defines, context->userDefines, lextStr, lexer);
				gotoIfError(clean, Parser_eraseDefine(&context->defines, &context->userDefines, i))

				//"defined" is not allowed

				if(CharString_equalsStringSensitive(lextStr, CharString_createRefCStrConst("defined")))
					gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. #define defined is illegal"
					))

				//Name only, no value (#define MY_VALUE)

				if(lexerTokenCount == 3)
					gotoIfError(clean, Parser_addDefine(&context->defines, lexerTokenId + 2, alloc))

				//#define MY_VALUE(x, ...) ... or #define MY_VALUE 1 ...

				else {

					//If next char is ( without space, then it's a macro

					Bool isMacro = false;

					if (lexerTokenCount >= 4) {
						LexerToken tok = lexer->tokens.ptr[lexerTokenId + 3];
						isMacro = tok.length == 1 && lexer->source.ptr[LexerToken_getOffset(tok)] == '(';
					}

					if(isMacro) {

						if (lexerTokenCount < 6)
							gotoIfError(clean, Error_invalidParameter(
								0, 0, "ParserContext_visit() source was invalid. Expected #define macro(...) ..."
							))

						//Require (a), (a, b, ...), (...), etc.

						U32 j = lexerTokenId + 3;

						lext = lexer->tokens.ptr[j];
						lextStr = LexerToken_asString(lext, *lexer);

						if(!CharString_equalsSensitive(lextStr, '('))
							gotoIfError(clean, Error_invalidParameter(
								0, 0, "ParserContext_visit() source was invalid. Expected ( in #define macro(...)"
							))

						++j;

						for (; j < lexerTokenId + lexerTokenCount; j += 2) {

							lext = lexer->tokens.ptr[j];
							lextStr = LexerToken_asString(lext, *lexer);

							if (CharString_equalsStringSensitive(lextStr, CharString_createRefCStrConst("..."))) {
								++j;
								break;
							}

							if(LexerToken_getType(lext) != ELexerTokenType_Identifier)
								gotoIfError(clean, Error_invalidParameter(
									0, 0, "ParserContext_visit() source was invalid. Expected identifier in macro"
								))

							if (lexerTokenCount <= j - lexerTokenId + 1)
								gotoIfError(clean, Error_invalidParameter(
									0, 0, "ParserContext_visit() source was invalid. Expected token in macro"
								))

							lext = lexer->tokens.ptr[j + 1];
							lextStr = LexerToken_asString(lext, *lexer);

							if(CharString_equalsSensitive(lextStr, ')')) {
								++j;
								break;
							}

							if(!CharString_equalsSensitive(lextStr, ','))
								gotoIfError(clean, Error_invalidParameter(
									0, 0, "ParserContext_visit() source was invalid. Expected , separator in macro"
								))
						}

						lext = lexer->tokens.ptr[j];
						lextStr = LexerToken_asString(lext, *lexer);

						if(!CharString_equalsSensitive(lextStr, ')'))
							gotoIfError(clean, Error_invalidParameter(
								0, 0, "ParserContext_visit() source was invalid. Expected ) in #define macro(...)"
							))

						if(((j - (lexerTokenId + 3) + 1) >> 1) >> 8)
							gotoIfError(clean, Error_invalidParameter(
								0, 0, "ParserContext_visit() source was invalid. Too many macro children"
							))

						if((lexerTokenId + lexerTokenCount - (j + 1)) >> 16)
							gotoIfError(clean, Error_invalidParameter(
								0, 0, "ParserContext_visit() source was invalid. Too many value children"
							))

						gotoIfError(clean, Parser_addMacro(
							&context->defines, 
							lexerTokenId + 2,									//Name
							lexerTokenId + 3,									//Children
							(U8)((j - (lexerTokenId + 3) + 1) >> 1),			//Child count
							j + 1,												//Value start
							(U16)(lexerTokenId + lexerTokenCount - (j + 1)),	//Value count
							alloc
						))
					}

					//Otherwise we have a value

					else {

						if((lexerTokenCount - 3) >> 16)
							gotoIfError(clean, Error_invalidParameter(
								0, 0, "ParserContext_visit() length of lexerToken is invalid; max of 64KiB"
							))

						gotoIfError(clean, Parser_addDefineWithValue(
							&context->defines, lexerTokenId + 2, lexerTokenId + 3, (U16)(lexerTokenCount - 3), alloc
						))
					}

				}

				break;
			}

			case C8x2('i', 'f'): {		//#if, #ifdef, #ifndef

				if(context->stack.stackIndex == 16)
					gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. #if stack can only be 16 deep"
					))

				++context->stack.stackIndex;

				U16 mask = (U16)(1 << (context->stack.stackIndex - 1));

				if (lext.length == 5 || lext.length == 6) {			//#ifdef, #ifndef

					if(lext.length == 5 && !CharString_equalsStringSensitive(			//#ifdef
						CharString_createRefCStrConst("ifdef"), lextStr
					))
						gotoIfError(clean, Error_invalidParameter(
							0, 0, "ParserContext_visit() source was invalid. Expected #ifdef"
						))

					if(lext.length == 6 && !CharString_equalsStringSensitive(			//#ifndef
						CharString_createRefCStrConst("ifndef"), lextStr
					))
						gotoIfError(clean, Error_invalidParameter(
							0, 0, "ParserContext_visit() source was invalid. Expected #ifdef"
						))

					Bool isIfdef = lext.length == 5;

					if(lexerTokenCount < 3)
						gotoIfError(clean, Error_invalidParameter(
							0, 0, "ParserContext_visit() source was invalid. Expected #if(n)def <define>"
						))

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
					gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #if, #ifndef or #ifdef"
					))

				else {						//#if

					Bool value = false;
					gotoIfError(clean, Parser_handleIfCondition(
						/*&context->stack,*/ lexer, context->defines, context->userDefines,
						lexerTokenId + 2, lexerTokenCount - 2,
						&value,
						alloc
					))

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
			}

			case C8x2('e', 'l'): {		//#else, #elif

				if(lext.length != 4)
					gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #elif or #else"
					))

				if(!context->stack.stackIndex)
					gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. #else/#elif didn't have a matching #if/#ifdef/#ifndef"
					))

				U16 mask = (U16)(1 << (context->stack.stackIndex - 1));

				v16 = *(const U16*) (lexer->source.ptr + LexerToken_getOffset(lext) + 2);

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
					gotoIfError(clean, Parser_handleIfCondition(
						/*&context->stack,*/ lexer, context->defines, context->userDefines,
						lexerTokenId + 2, lexerTokenCount - 2,
						&value,
						alloc
					))

					if (!(context->stack.hasProcessedIf & mask) && value) {
						context->stack.hasProcessedIf |= mask;
						context->stack.activeStack |= mask;
					}

					else context->stack.activeStack &=~ mask;		//Ensure we mark the scope as inactive
				}

				else gotoIfError(clean, Error_invalidParameter(
					0, 0, "ParserContext_visit() source was invalid. Expected #elif or #else"
				))

				break;
			}

			case C8x2('e', 'n'):		//#endif

				if (!CharString_equalsStringSensitive(CharString_createRefCStrConst("endif"), lextStr))
					gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #endif if #en is detected"
					))

				if(!context->stack.stackIndex)
					gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. #endif didn't have a matching #if/#ifdef/#ifndef"
					))

				--context->stack.stackIndex;
				break;

			case C8x2('e', 'r'): {		//#error

				if(!isPreprocessorScopeActive)
					goto clean;

				if (!CharString_equalsStringSensitive(CharString_createRefCStrConst("error"), lextStr))
					gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #error if #er is detected"
					))

				const C8 *startPtr = lexer->source.ptr + LexerToken_getOffset(lext);
				lext = lexer->tokens.ptr[lexerTokenId + lexerTokenCount - 1];

				const C8 *endPtr = lexer->source.ptr + LexerToken_getOffset(lext) + lext.length - 1;

				endPtr;		//TODO: Put this into the Error msg once it supports CharStrings instead of const C8*
				startPtr;

				gotoIfError(clean, Error_invalidState(0, "ParserContext_visit() source triggered #error message"))

				break;
			}

			case C8x2('u', 'n'):		//#undef

				if(!isPreprocessorScopeActive)
					goto clean;

				if (
					!CharString_equalsStringSensitive(CharString_createRefCStrConst("undef"), lextStr) ||
					lexerTokenCount != 3
				)
					gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #undef <value> if #un is detected"
					))

				lext = lexer->tokens.ptr[lexerTokenId + 2];
				lextStr = LexerToken_asString(lext, *lexer);

				U64 i = Parser_findDefine(context->defines, context->userDefines, lextStr, lexer);
				gotoIfError(clean, Parser_eraseDefine(&context->defines, &context->userDefines, i))

				break;

			case C8x2('l', 'i'):		//#line

				if(!isPreprocessorScopeActive)
					goto clean;

				if (
					!CharString_equalsStringSensitive(CharString_createRefCStrConst("line"), lextStr) ||
					lexerTokenCount < 3
				)
					gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #line when #li is encountered"
					))

				//#line N
				
				lext = lexer->tokens.ptr[lexerTokenId + 2];
				lextStr = LexerToken_asString(lext, *lexer);

				if(!CharString_parseU64(lextStr, &context->currentLine))
					gotoIfError(clean, Error_invalidParameter(
						0, 0, "ParserContext_visit() source was invalid. Expected #line N or #line 1 N \"file\""
					))

				//#line N "x"

				if (lexerTokenCount > 3) {

					lext = lexer->tokens.ptr[lexerTokenId + 3];

					CharString_clear(&context->currentFile);

					Bool isEscaped = false;
					Bool prevSlash = false;
					Bool leadingSlashSlash = false;

					for (U64 k = LexerToken_getOffset(lext), j = k; j < k + lext.length; ++j) {

						C8 cj = lexer->source.ptr[j];

						if (cj == '\\') {

							if(isEscaped) {
								gotoIfError(clean, CharString_append(&context->currentFile, cj, alloc))
								isEscaped = false;
								continue;
							}

							isEscaped = true;
							continue;
						}

						if(prevSlash && cj == '/') {						//Allow //
							CharString_clear(&context->currentFile);
							leadingSlashSlash = true;
						}

						prevSlash = cj == '/';

						gotoIfError(clean, CharString_append(&context->currentFile, cj, alloc))
						isEscaped = false;
					}

					if(isEscaped)
						gotoIfError(clean, Error_invalidParameter(
							0, 0, "ParserContext_visit() source was invalid. String can't end with an escaped quote!"
						))

					//Fix path

					if(leadingSlashSlash)
						gotoIfError(clean, CharString_insert(&context->currentFile, '/', 0, alloc))
				}

				break;

			default:					//Unsupported preprocessor statement

				gotoIfError(clean, Error_unsupportedOperation(
					0, "ParserContext_visit() source was invalid. Expected preprocessor directive is unsupported"
				))

				break;
		}
	}

	//Handle real expressions

	else {

		if(!isPreprocessorScopeActive)
			goto clean;

		for(U32 i = 0; i < lexerTokenCount; ++i) {

			lext = lexer->tokens.ptr[lexerTokenId + i];
			CharString lextStr = LexerToken_asString(lext, *lexer);
			ELexerTokenType lextType = LexerToken_getType(lext);

			if(CharString_length(lextStr) >> 8)
				gotoIfError(clean, Error_invalidParameter(0, 0, "ParserContext_visit() token max size is 256"))

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

							gotoIfError(clean, Error_unimplemented(0, "ParserContext_visit() user define is unsupported"))
						}

						//Macro

						Define def = context->defines.ptr[defineId];

						if (def.defineType & EDefineType_IsMacro) {
							//TODO: Support macros
							gotoIfError(clean, Error_unimplemented(1, "ParserContext_visit() macro is unsupported"))
						}

						//Value

						if(def.valueStartId != U32_MAX)
							gotoIfError(clean, ParserContext_visit(context, def.valueStartId, def.valueTokens, alloc))
					}

					//Otherwise it's probably a type of a keyword

					else {

						Token tok = (Token) { 
							.naiveTokenId = lexerTokenId + i, 
							.tokenType = ETokenType_Identifier,
							.tokenSize = (U8) CharString_length(lextStr)
						};

						gotoIfError(clean, ListToken_pushBack(&context->tokens, tok, alloc))
					}

					break;
				}

				//Append entire lexer token
				
				case ELexerTokenType_Float:
				case ELexerTokenType_Double: {

					F64 tmp = 0;
					if(!CharString_parseDouble(lextStr, &tmp))
						gotoIfError(clean, Error_invalidOperation(
							2, "ParserContext_visit() expected double but couldn't parse it"
						))

					Token tok = (Token) {
						.naiveTokenId = lexerTokenId + i,
						.tokenType = lextType == ELexerTokenType_Float ? ETokenType_Float : ETokenType_Double,
						.valuef = tmp,
						.tokenSize = (U8) CharString_length(lextStr)
					};

					gotoIfError(clean, ListToken_pushBack(&context->tokens, tok, alloc))
					break;
				}

				case ELexerTokenType_IntegerHex:
				case ELexerTokenType_IntegerBinary:
				case ELexerTokenType_IntegerDec:
				case ELexerTokenType_IntegerNyto:
				case ELexerTokenType_IntegerOctal: {

					C8 first = CharString_getAt(lextStr, 0);
					Bool negate = first == '-';
					Bool movedChar = false;

					if (negate || first == '+') {
						++lextStr.ptr;
						--lextStr.lenAndNullTerminated;
						movedChar = true;
					}

					U64 tmp = 0;
					if(!CharString_parseU64(lextStr, &tmp) || (tmp >> 63))
						gotoIfError(clean, Error_invalidOperation(
							3, "ParserContext_visit() expected integer, but couldn't parse it"
						))

					if(negate && (tmp >> 63))
						gotoIfError(clean, Error_invalidOperation(
							4, "ParserContext_visit() expected I64, but got U64 (too big)"
						))

					Token tok = (Token) {
						.naiveTokenId = lexerTokenId + i,
						.tokenType = negate ? ETokenType_SignedInteger : ETokenType_Integer,
						.valuei = ((I64) tmp) * (negate ? -1 : 1),
						.tokenSize = (U8) CharString_length(lextStr) + movedChar
					};

					gotoIfError(clean, ListToken_pushBack(&context->tokens, tok, alloc))
					break;
				}

				case ELexerTokenType_String: {

					//Escaped strings need to be resolved

					Bool isEscaped = false;

					for (U64 j = 0; j < CharString_length(lextStr); ++j) {

						C8 c = lextStr.ptr[j];

						if (isEscaped || c != '\\') {
							gotoIfError(clean, CharString_append(&temp, c, alloc))
							isEscaped = false;
							continue;
						}

						isEscaped = true;
					}

					gotoIfError(clean, ListCharString_pushBack(&context->stringLiterals, temp, alloc))
					temp = CharString_createNull();

					Token tok = (Token) {
						.naiveTokenId = lexerTokenId + i,
						.tokenType = ETokenType_String,
						.valueu = context->stringLiterals.length - 1,
						.tokenSize = (U8) CharString_length(lextStr)
					};

					gotoIfError(clean, ListToken_pushBack(&context->tokens, tok, alloc))
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
							gotoIfError(clean, Error_unsupportedOperation(1, "ParserContext_visit() invalid token"))

						Token tok = (Token) {
							.naiveTokenId = lexerTokenId + i,
							.tokenType = tokenType,
							.lexerTokenSubId = (U8) prev,
							.tokenSize = (U8)(subTokenOffset - prev)
						};

						gotoIfError(clean, ListToken_pushBack(&context->tokens, tok, alloc))
					}
			}
		}
	}

clean:
	CharString_free(&temp, alloc);
	return err;
}

Error Parser_create(const Lexer *lexer, Parser *parser, ListUserDefine userDefinesOuter, Allocator alloc) {

	if(!lexer || !parser)
		return Error_nullPointer(!lexer ? 0 : 1, "Parser_create()::parser and lexer are required");

	for (U64 i = 0; i < userDefinesOuter.length; ++i) {

		UserDefine ud = userDefinesOuter.ptr[i];

		if (!CharString_length(ud.name))
			return Error_nullPointer(2, "Parser_create()::userDefines[i].name is required");
	}

	Error err = Error_none();
	ParserContext context = (ParserContext) { .lexer = lexer };

	gotoIfError(clean, ListSymbol_reserve(&context.symbols, 64, alloc))
	gotoIfError(clean, ListDefine_reserve(&context.defines, 64, alloc))
	gotoIfError(clean, ListCharString_reserve(&context.stringLiterals, 64, alloc))
	gotoIfError(clean, ListToken_reserve(&context.tokens, lexer->tokens.length * 3 / 2, alloc))
	gotoIfError(clean, ListUserDefine_createCopy(userDefinesOuter, alloc, &context.userDefines))

	for (U64 i = 0; i < lexer->expressions.length; ++i) {

		LexerExpression e = lexer->expressions.ptr[i];

		if(e.type == ELexerExpressionType_MultiLineComment || e.type == ELexerExpressionType_Comment)
			continue;

		gotoIfError(clean, ParserContext_visit(&context, e.tokenOffset, e.tokenCount, alloc))
	}

	//TODO: Combine tokens into symbols

clean:

	if(err.genericError) {
		ListToken_free(&context.tokens, alloc);
		ListSymbol_free(&context.symbols, alloc);
		ListDefine_free(&context.defines, alloc);
		ListCharString_freeUnderlying(&context.stringLiterals, alloc);
	}

	else *parser = (Parser) { 
		.symbols = context.symbols, 
		.tokens = context.tokens, 
		.defines = context.defines,
		.parsedLiterals = context.stringLiterals,
		.lexer = lexer
	};

	CharString_free(&context.currentFile, alloc);
	ListUserDefine_free(&context.userDefines, alloc);
	return err;
}

Bool Parser_free(Parser *parser, Allocator alloc) {

	if(!parser)
		return true;

	ListSymbol_free(&parser->symbols, alloc);
	ListToken_free(&parser->tokens, alloc);
	ListDefine_free(&parser->defines, alloc);
	ListCharString_freeUnderlying(&parser->parsedLiterals, alloc);
	return true;
}

void Parser_print(Parser parser, Allocator alloc) {

	if(!parser.tokens.length)
		Log_debugLn(alloc, "Parser: Empty");

	else Log_debugLn(alloc, "Parser:");

	//Fetch important data

	for (U64 i = 0; i < parser.tokens.length; ++i) {

		Token t = parser.tokens.ptr[i];

		LexerToken lt = parser.lexer->tokens.ptr[t.naiveTokenId];
		CharString tokenRaw = Token_asString(t, &parser);

		const C8 *tokenType[] = {

			"Identifier",
			"Integer",
			"Signed integer",
			"Double",
			"Float",
			"String",

			"Increment",
			"Decrement",

			"Bitwise not",
			"Boolean not",

			"Multiply",
			"Divide",
			"Modulo",
			"Add",
			"Subtract",

			"Multiply assign",
			"Divide assign",
			"Modulo assign",
			"Add assign",
			"Subtract assign",

			"Boolean and",
			"Boolean or",

			"Bitwise and",
			"Bitwise or",
			"Bitwise xor",

			"Bitwise and assign",
			"Bitwise or assign",
			"Bitwise xor assign",

			"Left shift",
			"Right shift",

			"Left shift assign",
			"Right shift assign",

			"Less than",
			"Greater than",
			"Less equal",
			"Greater equal",

			"Assign",
			"Equals",
			"Not equals",

			"Round parenthesis start",
			"Round parenthesis end",

			"Square bracket start",
			"Square bracket end",

			"Curly brace start",
			"Curly brace end",

			"Colon",
			"Double colon",
			"Semicolon",

			"Comma",
			"Period",

			"Ternary",

			"Double square bracket start",
			"Double square bracket end",

			"Arrow",
			"Flagship operator"
		};

		#define BASE_FORMAT "T%05"PRIu64"(%05"PRIu64",%03"PRIu64"[%03"PRIu64"]: L#%04"PRIu64":%03"PRIu64")\t% 32s: %.*s"
		const C8 *formatS = BASE_FORMAT;
		const C8 *formatD = BASE_FORMAT " (parsed as F64: %f)";
		const C8 *formatF = BASE_FORMAT " (parsed as F32: %f)";
		const C8 *formatI = BASE_FORMAT " (parsed as I64: %" PRIi64 ")";
		const C8 *formatU = BASE_FORMAT " (parsed as U64: %" PRIu64 ")";

		const C8 *format = t.tokenType == ETokenType_Double ? formatD : (
			t.tokenType == ETokenType_Float ? formatF : (
				t.tokenType == ETokenType_SignedInteger ? formatI : (
					t.tokenType == ETokenType_Integer ? formatU : formatS
				)
			)
		);

		Log_debugLn(

			alloc,

			format, 

			i, 

			(U64)t.naiveTokenId, 
			(U64)t.lexerTokenSubId, 
			(U64)t.tokenSize, 

			(U64)lt.lineId, 
			(U64)(lt.charId + t.lexerTokenSubId),

			tokenType[t.tokenType],
			CharString_length(tokenRaw), 
			tokenRaw.ptr,
			t.valuef,
			t.valuei,
			t.valueu
		);
	}
}

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

#include "types/string.h"
#include "types/error.h"
#include "types/buffer.h"
#include "platforms/ext/listx.h"
#include "core3_bindings/parser.h"
#include "core3_bindings/lexer.h"

//Helpers for handling defines

//Returns U64_MAX if not found
//Returns [ defines.length, defines.length + userDefines.length > if found in a user define

U64 Parser_findDefine(List defines, List userDefines, CharString str, const Lexer *lexer) {

	//TODO: Map

	for (U64 i = 0; i < defines.length; ++i) {

		Define d = *List_ptrT(Define, defines, i);
		CharString ds = LexerToken_asString(*List_ptrT(LexerToken, lexer->tokens, d.nameTokenId), *lexer);

		if (CharString_equalsStringSensitive(str, ds))
			return i;
	}

	for (U64 i = 0; i < userDefines.length; ++i) {

		UserDefine ud = *List_ptrT(UserDefine, userDefines, i);

		if (CharString_equalsStringSensitive(str, ud.name))
			return i + defines.length;
	}

	return U64_MAX;
}

Error Parser_eraseDefine(List *defines, List *userDefines, U64 i) {

	if(i == U64_MAX)
		return Error_none();

	if(i < defines->length)
		return List_erase(defines, i);

	i -= defines->length;

	if(i < userDefines->length)
		return List_erase(userDefines, i);

	return Error_outOfBounds(
		2, i, userDefines->length, "Parser_eraseDefine()::i exceeded userDefines->length + defines->length"
	);
}

//Define without value

Error Parser_addDefine(List *defines, U32 nameLexerTokenId) {
	Define define = (Define) { .defineType = EDefineType_ValueEmpty, .nameTokenId = nameLexerTokenId };
	return List_pushBackx(defines, Buffer_createConstRef(&define, sizeof(define)));
}

//Define with value

Error Parser_addDefineWithValue(List *defines, U32 nameLexerTokenId, U32 childLexerTokenId, U16 valueTokenCount) {

	Define define = (Define) { 
		.defineType = EDefineType_ValueFull, 
		.valueTokens = valueTokenCount,
		.nameTokenId = nameLexerTokenId,
		.valueStartId = childLexerTokenId
	};

	return List_pushBackx(defines, Buffer_createConstRef(&define, sizeof(define)));
}

//Macro with(out) value

Error Parser_addMacro(
	List *defines, 
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

	return List_pushBackx(defines, Buffer_createConstRef(&define, sizeof(define)));
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

		//()[]{}:;

		case ')':	count = 1;	tokenType = ETokenType_RoundParenthesisStart;	break;
		case '(':	count = 1;	tokenType = ETokenType_RoundParenthesisEnd;		break;
		case '[':	count = 1;	tokenType = ETokenType_SquareBracketStart;		break;
		case ']':	count = 1;	tokenType = ETokenType_SquareBracketEnd;		break;
		case '{':	count = 1;	tokenType = ETokenType_CurlyBraceStart;			break;
		case '}':	count = 1;	tokenType = ETokenType_CurlyBraceEnd;			break;
		case ':':	count = 1;	tokenType = ETokenType_Colon;					break;
		case ';':	count = 1;	tokenType = ETokenType_Semicolon;				break;
	}

	*subTokenOffset += count;
	return tokenType;
}

//Parse the lexed file

typedef struct PreprocessorIfStack {

	I64 valueStack[14];

	U16 valueStackIsActive;
	U8 valueStackCount;
	U8 stackIndex;			//Indicates how deep the stack is with #ifs

	U16 activeStack;		//Indicates if the current stack is active (>> (stackIndex - 1)) & 1
	U16 hasProcessedIf;		//If a scope has already been entered

} PreprocessorIfStack;

Error Parser_handleIfCondition(
	PreprocessorIfStack *stack,
	const Lexer *lexer,
	List defines,
	List userDefines,
	U64 tokenOffset,
	U64 tokenCount,
	U64 *subTokenOffset,
	U64 *expressionSize
) {

	if(!tokenCount)
		return Error_invalidState(0, "Parser_handleIfCondition() expected token but got none");

	LexerToken lext = ((const LexerToken*) lexer->tokens.ptr)[tokenOffset];
	CharString lextStr = LexerToken_asString(lext, *lexer);

	if(*subTokenOffset >= CharString_length(lextStr))
		return Error_invalidState(1, "Parser_handleIfCondition() token out of bounds");

	I64 *value = &stack->valueStack[stack->valueStackCount];
	U64 parsedCounter = 0;

	switch(lext.offsetType >> 30) {

		//Checking a define or defined()

		case ELexerTokenType_Identifier: {

			//If keyword is "defined" then match defined(identifier)

			if (CharString_equalsStringSensitive(lextStr, CharString_createConstRefCStr("defined"))) {

				if(tokenCount < 4)
					return Error_invalidState(2, "Parser_handleIfCondition() expected defined(identifier)");

				lextStr = LexerToken_asString(((const LexerToken*) lexer->tokens.ptr)[tokenOffset + 1], *lexer);

				if(!CharString_equalsSensitive(lextStr, '('))
					return Error_invalidState(3, "Parser_handleIfCondition() expected '(' in defined(identifier)");

				LexerToken identifier = ((const LexerToken*) lexer->tokens.ptr)[tokenOffset + 2];

				if((identifier.offsetType >> 30) != ELexerTokenType_Identifier)
					return Error_invalidState(4, "Parser_handleIfCondition() expected identifier in defined(identifier)");

				lextStr = LexerToken_asString(((const LexerToken*) lexer->tokens.ptr)[tokenOffset + 3], *lexer);

				if (!CharString_equalsSensitive(lextStr, ')'))
					return Error_invalidState(5, "Parser_handleIfCondition() expected ')' in defined(identifier)");

				if ((stack->valueStackIsActive >> stack->valueStackCount) & 1)
					return Error_invalidState(
						16, "Parser_handleIfCondition() #if defined statement expected an operator in between ints"
					);

				lextStr = LexerToken_asString(identifier, *lexer);

				U64 i = Parser_findDefine(defines, userDefines, lextStr, lexer);
				*value = i != U64_MAX;
				stack->valueStackIsActive |= 1u << stack->valueStackCount;

				parsedCounter += 4;
				break;
			}

			//Else check define value and return it

			U64 i = Parser_findDefine(defines, userDefines, lextStr, lexer);

			if (i == U64_MAX)
				return Error_invalidState(6, "Parser_handleIfCondition() expected define as identifier");

			if (i >= defines.length) {		//User defined (must be int)

				UserDefine ud = *List_ptrT(UserDefine, userDefines, i - defines.length);

				if(!CharString_length(ud.value))
					return Error_invalidState(12, "Parser_handleIfCondition() expected value of user define");

				U64 tmp = 0;
				if(!CharString_parseU64(ud.value, &tmp))
					return Error_invalidState(13, "Parser_handleIfCondition() expected user define as int");

				if(tmp >> 63)
					return Error_invalidState(14, "Parser_handleIfCondition() user define was out of bounds (I64_MAX)");

				if ((stack->valueStackIsActive >> stack->valueStackCount) & 1)
					return Error_invalidState(
						15, "Parser_handleIfCondition() #if value expected an operator in between ints"
					);

				stack->valueStackIsActive |= 1u << stack->valueStackCount;
				*value = (I64) tmp;
				++parsedCounter;
				break;
			}

			//Macro

			Define def = *List_ptrT(Define, defines, i);

			if (def.defineType & EDefineType_IsMacro) {
				//TODO: Support macros
				return Error_unimplemented(0, "Parser_handleIfCondition() #if value not supported yet");
			}

			//Regular define (visit define)

			U64 subTokenOffset2 = 0;
			U64 nextExpressionSize = 0;

			Error err = Parser_handleIfCondition(
				stack,
				lexer,
				defines,
				userDefines,
				def.valueStartId,		//Visit define as if it was inline
				def.valueTokens,
				&subTokenOffset2,
				&nextExpressionSize
			);

			if(err.genericError)
				return err;

			++parsedCounter;
			break;
		}

		//Unsupported in C

		case ELexerTokenType_Float:
			return Error_invalidState(7, "Parser_handleIfCondition() #if statements only accept integers and defines");

		//Store value

		case ELexerTokenType_Integer:
			
			if ((stack->valueStackIsActive >> stack->valueStackCount) & 1)
				return Error_invalidState(
					8, "Parser_handleIfCondition() #if statements only accept ints if there's an operator in between"
				);

			U64 tempValue = 0;
			if(!CharString_parseU64(lextStr, &tempValue) || tempValue >> 63)
				return Error_invalidState(
					9, "Parser_handleIfCondition() #if statements expected integer (< I64_MAX)"
				);

			*value = (I64) tempValue;
			stack->valueStackIsActive |= 1u << stack->valueStackCount;
			++parsedCounter;
			break;

		//Operators

		case ELexerTokenType_Symbols: {
		
			//All operators to be supported (lvalue): (//TODO:)
			//	!, ~, &&, &, |, ^, ||, !=, >, <, <=, >=, ==, <<, >>, %, *, /, +, -, ()

			ETokenType token = Parser_getTokenType(lextStr, subTokenOffset);

			if(token == ETokenType_Count)
				return Error_invalidState(
					10, "Parser_handleIfCondition() #if couldn't parse operator type"
				);

			//Check if we support this type of operator.
			//Currently we only support:
			//Unary: !, ~, - and +

			Bool supported = false;

			switch(token) {

				case ETokenType_Not:	case ETokenType_BooleanNot:
				case ETokenType_Add:	case ETokenType_Sub:

				case ETokenType_Gt:		case ETokenType_Geq:
				case ETokenType_Lt:		case ETokenType_Leq:
				case ETokenType_Neq:	case ETokenType_Eq:

					supported = true;
					break;
			}

			if(!supported)
				return Error_unimplemented(0, "Parser_handleIfCondition() #if had operator type that isn't supported yet");

			U64 subTokenOffset2 = *subTokenOffset;
			U64 nextExpressionSize = 0;
			Error err = Error_none();

			//Push 

			++stack->valueStackCount;

			if(stack->valueStackCount == 14)
				return Error_invalidState(17, "Parser_handleIfCondition() #if value stack only has depth of 14");

			stack->valueStackIsActive &=~ (1u << stack->valueStackCount);

			if(*subTokenOffset == CharString_length(lextStr)) {

				++parsedCounter;
				subTokenOffset2 = 0;

				err = Parser_handleIfCondition(
					stack,
					lexer,
					defines,
					userDefines,
					tokenOffset + 1,
					tokenCount - 1,
					&subTokenOffset2,
					&nextExpressionSize
				);
			}

			//Chained tokens

			else err = Parser_handleIfCondition(
				stack,
				lexer,
				defines,
				userDefines,
				tokenOffset,
				tokenCount,
				&subTokenOffset2,
				&nextExpressionSize
			);

			if(err.genericError)
				return err;

			if (!((stack->valueStackIsActive >> stack->valueStackCount) & 1))
				return Error_invalidState(11, "Parser_handleIfCondition() #if statement didn't return a value or result!");

			I64 nestedValue = stack->valueStack[stack->valueStackCount];
			stack->valueStackIsActive &=~ (1u << stack->valueStackCount);
			--stack->valueStackCount;

			parsedCounter += nextExpressionSize;
			*subTokenOffset = subTokenOffset2;

			Bool makeValue = false;
				
			switch(token) {

				case ETokenType_Not:			*value = ~nestedValue;	makeValue = true;	break;
				case ETokenType_BooleanNot:		*value = !nestedValue;	makeValue = true;	break;
				case ETokenType_Add:			*value = +nestedValue;	makeValue = true;	break;
				case ETokenType_Sub:			*value = -nestedValue;	makeValue = true;	break;

				case ETokenType_Gt:				*value = *value > nestedValue;		break;
				case ETokenType_Geq:			*value = *value >= nestedValue;		break;
				case ETokenType_Lt:				*value = *value < nestedValue;		break;
				case ETokenType_Leq:			*value = *value <= nestedValue;		break;
				case ETokenType_Neq:			*value = *value != nestedValue;		break;
				case ETokenType_Eq:				*value = *value == nestedValue;		break;
			}

			if(makeValue)
				stack->valueStackIsActive |= 1u << stack->valueStackCount;

			break;
		}
	}

	if (!((stack->valueStackIsActive >> stack->valueStackCount) & 1))
		return Error_invalidState(11, "Parser_handleIfCondition() #if statement didn't return a value or result!");

	*expressionSize = parsedCounter;
	return Error_none();
}

Error Parser_create(const Lexer *lexer, Parser *parser, List userDefinesOuter) {

	if(!lexer || !parser)
		return Error_nullPointer(!lexer ? 0 : 1, "Parser_create()::parser and lexer are required");

	if(userDefinesOuter.stride && userDefinesOuter.stride != sizeof(UserDefine))
		return Error_invalidParameter(2, 0, "Parser_create()::userDefines needs to be UserDefine[]");

	for (U64 i = 0; i < userDefinesOuter.length; ++i) {

		UserDefine ud = *List_ptrConstT(UserDefine, userDefinesOuter, i);

		if (!CharString_length(ud.name))
			return Error_nullPointer(2, "Parser_create()::userDefines[i].name is required");
	}

	Error err = Error_none();
	List symbols = List_createEmpty(sizeof(Symbol));
	List tokens = List_createEmpty(sizeof(Token));
	List defines = List_createEmpty(sizeof(Define));
	List userDefines = (List) { 0 };

	//TODO: Copy user defines to local so we can safely erase them if needed

	_gotoIfError(clean, List_reservex(&symbols, 64));
	_gotoIfError(clean, List_reservex(&defines, 64));
	_gotoIfError(clean, List_reservex(&tokens, lexer->tokens.length * 3 / 2));
	_gotoIfError(clean, List_createCopyx(userDefinesOuter, &userDefines));

	PreprocessorIfStack stack = (PreprocessorIfStack) { 0 };

	for (U64 i = 0; i < lexer->expressions.length; ++i) {

		LexerExpression e = ((const LexerExpression*) lexer->expressions.ptr)[i];

		//Ensure all previous scopes are active

		U16 fullStack = (U16)((1 << stack.stackIndex) - 1);
		Bool isPreprocessorScopeActive = !stack.stackIndex || (stack.activeStack & fullStack) == fullStack;

		//Handle preprocessor

		if (e.type == ELexerExpressionType_Preprocessor) {

			if(e.tokenCount < 2)
				_gotoIfError(clean, Error_invalidParameter(0, 0, "Parser_create() source was invalid. Expected #<token>"));

			LexerToken lext = ((const LexerToken*) lexer->tokens.ptr)[e.tokenOffset];

			if(lext.length != 1 || lexer->source.ptr[lext.offsetType << 2 >> 2] != '#')
				_gotoIfError(clean, Error_invalidParameter(
					0, 0, "Parser_create() source was invalid. Expected # for preprocessor"
				));

			lext = ((const LexerToken*) lexer->tokens.ptr)[e.tokenOffset + 1];

			if(lext.length < 2)
				_gotoIfError(clean, Error_invalidParameter(
					0, 0, "Parser_create() source was invalid. #<token> requires at least 2 chars"
				));

			CharString lextStr = LexerToken_asString(lext, *lexer);

			//Since C doesn't have switch(string), we'll have to make it ourselves

			U16 v16 = *(const U16*) (lexer->source.ptr + (lext.offsetType << 2 >> 2));

			switch (v16) {

				case C8x2('p', 'r'): {		//#pragma once

					if(!isPreprocessorScopeActive)
						continue;

					const C8 *pragmaOnce[] = { "pragma", "once" };
					CharString pragma = CharString_createConstRefCStr(pragmaOnce[0]);
					CharString once = CharString_createConstRefCStr(pragmaOnce[1]);

					if(!CharString_equalsStringSensitive(pragma, lextStr) || e.tokenCount != 3)
						_gotoIfError(clean, Error_invalidParameter(
							0, 0, "Parser_create() source was invalid. Expected #pragma once if #pr is detected"
						));

					lext = ((const LexerToken*) lexer->tokens.ptr)[e.tokenOffset + 2];
					lextStr = LexerToken_asString(lext, *lexer);

					if(!CharString_equalsStringSensitive(once, lextStr))
						_gotoIfError(clean, Error_invalidParameter(
							0, 0, "Parser_create() source was invalid. Expected #pragma once if #pr is detected"
						));

					break;
				}

				case C8x2('i', 'n'):		//#include

					if(!isPreprocessorScopeActive)
						continue;

					if (!CharString_equalsStringSensitive(CharString_createConstRefCStr("include"), lextStr))
						_gotoIfError(clean, Error_invalidParameter(
							0, 0, "Parser_create() source was invalid. Expected #include if #in is detected"
						));

					//TODO:

					break;

				case C8x2('d', 'e'): {		//#define

					if(!isPreprocessorScopeActive)
						continue;

					if (!CharString_equalsStringSensitive(CharString_createConstRefCStr("define"), lextStr))
						_gotoIfError(clean, Error_invalidParameter(
							0, 0, "Parser_create() source was invalid. Expected #define if #de is detected"
						));

					if(e.tokenCount < 3)
						_gotoIfError(clean, Error_invalidParameter(
							0, 0, "Parser_create() source was invalid. Expected #define <define>..."
						));

					lext = ((const LexerToken*) lexer->tokens.ptr)[e.tokenOffset + 2];
					lextStr = LexerToken_asString(lext, *lexer);

					U64 i = Parser_findDefine(defines, userDefines, lextStr, lexer);
					_gotoIfError(clean, Parser_eraseDefine(&defines, &userDefines, i));

					//"defined" is not allowed

					if(CharString_equalsStringSensitive(lextStr, CharString_createConstRefCStr("defined")))
						_gotoIfError(clean, Error_invalidParameter(
							0, 0, "Parser_create() source was invalid. #define defined is illegal"
						));

					//Name only, no value (#define MY_VALUE)

					if(e.tokenCount == 3)
						_gotoIfError(clean, Parser_addDefine(&defines, e.tokenOffset + 2))

					//#define MY_VALUE(x, ...) ... or #define MY_VALUE 1 ...

					else {

						//If next char is ( without space, then it's a macro

						Bool isMacro = false;

						if (e.tokenCount >= 4) {
							LexerToken tok = ((const LexerToken*) lexer->tokens.ptr)[e.tokenOffset + 3];
							isMacro = tok.length == 1 && lexer->source.ptr[tok.offsetType << 2 >> 2] == '(';
						}

						if(isMacro) {

							if (e.tokenCount < 6)
								_gotoIfError(clean, Error_invalidParameter(
									0, 0, "Parser_create() source was invalid. Expected #define macro(...) ..."
								));

							//Require (a), (a, b, ...), (...), etc.

							U32 i = e.tokenOffset + 3;

							lext = ((const LexerToken*) lexer->tokens.ptr)[i];
							lextStr = LexerToken_asString(lext, *lexer);

							if(!CharString_equalsSensitive(lextStr, '('))
								_gotoIfError(clean, Error_invalidParameter(
									0, 0, "Parser_create() source was invalid. Expected ( in #define macro(...)"
								));

							++i;

							for (; i < e.tokenOffset + e.tokenCount; i += 2) {

								lext = ((const LexerToken*) lexer->tokens.ptr)[i];
								lextStr = LexerToken_asString(lext, *lexer);

								if (CharString_equalsStringSensitive(lextStr, CharString_createConstRefCStr("..."))) {
									++i;
									break;
								}

								if((lext.offsetType >> 30) != ELexerTokenType_Identifier)
									_gotoIfError(clean, Error_invalidParameter(
										0, 0, "Parser_create() source was invalid. Expected identifier in macro"
									));

								if (e.tokenCount <= i - e.tokenOffset + 1)
									_gotoIfError(clean, Error_invalidParameter(
										0, 0, "Parser_create() source was invalid. Expected token in macro"
									));

								lext = ((const LexerToken*) lexer->tokens.ptr)[i + 1];
								lextStr = LexerToken_asString(lext, *lexer);

								if(CharString_equalsSensitive(lextStr, ')')) {
									++i;
									break;
								}

								if(!CharString_equalsSensitive(lextStr, ','))
									_gotoIfError(clean, Error_invalidParameter(
										0, 0, "Parser_create() source was invalid. Expected , separator in macro"
									));
							}

							lext = ((const LexerToken*) lexer->tokens.ptr)[i];
							lextStr = LexerToken_asString(lext, *lexer);

							if(!CharString_equalsSensitive(lextStr, ')'))
								_gotoIfError(clean, Error_invalidParameter(
									0, 0, "Parser_create() source was invalid. Expected ) in #define macro(...)"
								));

							if(((i - (e.tokenOffset + 3) + 1) >> 1) >> 8)
								_gotoIfError(clean, Error_invalidParameter(
									0, 0, "Parser_create() source was invalid. Too many macro children"
								));

							if((e.tokenOffset + e.tokenCount - (i + 1)) >> 16)
								_gotoIfError(clean, Error_invalidParameter(
									0, 0, "Parser_create() source was invalid. Too many value children"
								));

							_gotoIfError(clean, Parser_addMacro(
								&defines, 
								e.tokenOffset + 2,								//Name
								e.tokenOffset + 3,								//Children
								(U8)((i - (e.tokenOffset + 3) + 1) >> 1),		//Child count
								i + 1,											//Value start
								(U16)(e.tokenOffset + e.tokenCount - (i + 1))	//Value count
							));
						}

						//Otherwise we have a value

						else _gotoIfError(clean, Parser_addDefineWithValue(
							&defines, e.tokenOffset + 2, e.tokenOffset + 3, e.tokenCount - 3
						));

					}

					break;
				}

				case C8x2('i', 'f'):		//#if, #ifdef, #ifndef

					if(stack.stackIndex == 16)
						_gotoIfError(clean, Error_invalidParameter(
							0, 0, "Parser_create() source was invalid. #if stack can only be 16 deep"
						));

					++stack.stackIndex;

					U16 mask = (U16)(1 << (stack.stackIndex - 1));

					if (lext.length == 5 || lext.length == 6) {			//#ifdef, #ifndef

						if(lext.length == 5 && !CharString_equalsStringSensitive(			//#ifdef
							CharString_createConstRefCStr("ifdef"), lextStr
						))
							_gotoIfError(clean, Error_invalidParameter(
								0, 0, "Parser_create() source was invalid. Expected #ifdef"
							));

						if(lext.length == 6 && !CharString_equalsStringSensitive(			//#ifndef
							CharString_createConstRefCStr("ifndef"), lextStr
						))
							_gotoIfError(clean, Error_invalidParameter(
								0, 0, "Parser_create() source was invalid. Expected #ifdef"
							));

						Bool isIfdef = lext.length == 5;

						if(e.tokenCount < 3)
							_gotoIfError(clean, Error_invalidParameter(
								0, 0, "Parser_create() source was invalid. Expected #if(n)def <define>"
							));

						lext = ((const LexerToken*) lexer->tokens.ptr)[e.tokenOffset + 2];
						lextStr = LexerToken_asString(lext, *lexer);

						U64 i = Parser_findDefine(defines, userDefines, lextStr, lexer);

						Bool enable = (i != U64_MAX) == isIfdef;

						if (enable) {
							stack.hasProcessedIf |= mask;
							stack.activeStack |= mask;
						}

						else {
							stack.hasProcessedIf &=~ mask;
							stack.activeStack &=~ mask;		//Ensure we mark the scope as inactive
						}
					}

					else if(lext.length != 2)
						_gotoIfError(clean, Error_invalidParameter(
							0, 0, "Parser_create() source was invalid. Expected #if, #ifndef or #ifdef"
						))

					else {						//#if

						U64 expressionSize = 0;
						U64 subTokenOffset = 0;

						U64 offset = e.tokenOffset + 2;
						U64 count = e.tokenCount - 2;

						while(count) {

							_gotoIfError(clean, Parser_handleIfCondition(
								&stack, lexer, defines, userDefines,
								offset, count,
								&subTokenOffset,
								&expressionSize
							));

							offset += expressionSize;
							count -= expressionSize;
						}

						if (stack.valueStack[0]) {
							stack.hasProcessedIf |= mask;
							stack.activeStack |= mask;
						}

						else {
							stack.hasProcessedIf &=~ mask;
							stack.activeStack &=~ mask;		//Ensure we mark the scope as inactive
						}

						stack.valueStackIsActive &=~ (1u << stack.valueStackCount);
					}

					break;

				case C8x2('e', 'l'): {		//#else, #elif

					if(lext.length != 4)
						_gotoIfError(clean, Error_invalidParameter(
							0, 0, "Parser_create() source was invalid. Expected #elif or #else"
						));

					if(!stack.stackIndex)
						_gotoIfError(clean, Error_invalidParameter(
							0, 0, "Parser_create() source was invalid. #else/#elif didn't have a matching #if/#ifdef/#ifndef"
						));

					U16 mask = (U16)(1 << (stack.stackIndex - 1));

					v16 = *(const U16*) (lexer->source.ptr + (lext.offsetType << 2 >> 2) + 2);

					if(v16 == C8x2('s', 'e')) {			//#else

						//#else only works if none of the predecessors have been processed yet

						if (!(stack.hasProcessedIf & mask)) {
							stack.hasProcessedIf |= mask;
							stack.activeStack |= mask;
						}

						else stack.activeStack &=~ mask;		//Ensure we mark the scope as inactive
					}

					else if(v16 == C8x2('i', 'f')) {		//#elif

						//#elif only works if none of the predecessors have been processed yet

						U64 expressionSize = 0;
						U64 subTokenOffset = 0;

						_gotoIfError(clean, Parser_handleIfCondition(
							&stack, lexer, defines, userDefines,
							e.tokenOffset + 2, e.tokenCount - 2,
							&subTokenOffset,
							&expressionSize
						));

						if(expressionSize != e.tokenCount - 2)
							_gotoIfError(clean, Error_invalidParameter(
								0, 0, "Parser_create() source was invalid. Couldn't parse #elif correctly"
							))

						if (!(stack.hasProcessedIf & mask) && stack.valueStack[0]) {
							stack.hasProcessedIf |= mask;
							stack.activeStack |= mask;
						}

						else stack.activeStack &=~ mask;		//Ensure we mark the scope as inactive

						stack.valueStackIsActive &=~ (1u << stack.valueStackCount);
					}

					else _gotoIfError(clean, Error_invalidParameter(
						0, 0, "Parser_create() source was invalid. Expected #elif or #else"
					));

					break;
				}

				case C8x2('e', 'n'):		//#endif

					if (!CharString_equalsStringSensitive(CharString_createConstRefCStr("endif"), lextStr))
						_gotoIfError(clean, Error_invalidParameter(
							0, 0, "Parser_create() source was invalid. Expected #endif if #en is detected"
						));

					if(!stack.stackIndex)
						_gotoIfError(clean, Error_invalidParameter(
							0, 0, "Parser_create() source was invalid. #endif didn't have a matching #if/#ifdef/#ifndef"
						));

					--stack.stackIndex;
					break;

				case C8x2('e', 'r'): {		//#error

					if(!isPreprocessorScopeActive)
						continue;

					if (!CharString_equalsStringSensitive(CharString_createConstRefCStr("error"), lextStr))
						_gotoIfError(clean, Error_invalidParameter(
							0, 0, "Parser_create() source was invalid. Expected #error if #er is detected"
						));

					const C8 *startPtr = lexer->source.ptr + (lext.offsetType << 2 >> 2);
					lext = ((const LexerToken*) lexer->tokens.ptr)[e.tokenOffset + e.tokenCount - 1];

					const C8 *endPtr = lexer->source.ptr + (lext.offsetType << 2 >> 2) + lext.length - 1;

					endPtr;		//TODO: Put this into the Error msg once it supports CharStrings instead of const C8*
					startPtr;

					_gotoIfError(clean, Error_invalidState(0, "Parser_create() source triggered #error message"));

					break;
				}

				case C8x2('u', 'n'):		//#undef

					if(!isPreprocessorScopeActive)
						continue;

					if (
						!CharString_equalsStringSensitive(CharString_createConstRefCStr("undef"), lextStr) ||
						e.tokenCount != 3
					)
						_gotoIfError(clean, Error_invalidParameter(
							0, 0, "Parser_create() source was invalid. Expected #undef <value> if #un is detected"
						));

					lext = ((const LexerToken*) lexer->tokens.ptr)[e.tokenOffset + 2];
					lextStr = LexerToken_asString(lext, *lexer);

					U64 i = Parser_findDefine(defines, userDefines, lextStr, lexer);
					_gotoIfError(clean, Parser_eraseDefine(&defines, &userDefines, i));

					break;

				default:					//Unsupported preprocessor statement

					_gotoIfError(clean, Error_unsupportedOperation(
						0, "Parser_create() source was invalid. Expected preprocessor directive is unsupported"
					));

					break;
			}
		}

		//Handle real expressions

		else if (e.type == ELexerExpressionType_Generic) {

			if(!isPreprocessorScopeActive)
				continue;

			//TODO:
		}
	}

clean:

	if(err.genericError) {
		List_freex(&tokens);
		List_freex(&symbols);
		List_freex(&defines);
	}
	else *parser = (Parser) { .symbols = symbols, .tokens = tokens, .defines = defines };

	List_freex(&userDefines);
	return err;
}

Bool Parser_free(Parser *parser) {

	if(!parser)
		return true;

	List_freex(&parser->symbols);
	List_freex(&parser->tokens);
	List_freex(&parser->defines);
	return true;
}

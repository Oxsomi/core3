/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/big_int.h"

TListImpl(Token);
TListImpl(Symbol);

CharString Token_asString(Token t, const Parser *p) {

	if(!p || t.naiveTokenId >= p->lexer->tokens.length)
		return CharString_createNull();

	CharString lextStr = LexerToken_asString(p->lexer->tokens.ptr[t.naiveTokenId], *p->lexer);

	if((U32)t.lexerTokenSubId + t.tokenSize > CharString_length(lextStr))
		return CharString_createNull();

	return CharString_createRefSizedConst(lextStr.ptr + t.lexerTokenSubId, t.tokenSize, false);
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

		//(){}:;,.

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

typedef struct ParserContext {

	ListSymbol symbols;
	ListToken tokens;
	ListCharString stringLiterals;

	const Lexer *lexer;

} ParserContext;

Error ParserContext_visit(ParserContext *context, U32 lexerTokenId, U32 lexerTokenCount, Allocator alloc) {

	Error err = Error_none();
	const Lexer *lexer = context->lexer;
	CharString temp = CharString_createNull();		//For parsing strings

	for(U32 i = 0; i < lexerTokenCount; ++i) {

		LexerToken lext = lexer->tokens.ptr[lexerTokenId + i];
		CharString lextStr = LexerToken_asString(lext, *lexer);
		ELexerTokenType lextType = LexerToken_getType(lext);

		if(CharString_length(lextStr) >> 8)
			gotoIfError(clean, Error_invalidParameter(0, 0, "ParserContext_visit() token max size is 256"))

		switch(lextType) {

			//Handle define/macro and normal keyword

			case ELexerTokenType_Identifier: {

				Token tok = (Token) {
					.naiveTokenId = lexerTokenId + i,
					.tokenType = ETokenType_Identifier,
					.tokenSize = (U8) CharString_length(lextStr)
				};

				gotoIfError(clean, ListToken_pushBack(&context->tokens, tok, alloc))
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

clean:
	CharString_free(&temp, alloc);
	return err;
}

Error Parser_create(const Lexer *lexer, Parser *parser, Allocator alloc) {

	if(!lexer || !parser)
		return Error_nullPointer(!lexer ? 0 : 1, "Parser_create()::parser and lexer are required");

	Error err = Error_none();
	ParserContext context = (ParserContext) { .lexer = lexer };

	gotoIfError(clean, ListCharString_reserve(&context.stringLiterals, 64, alloc))
	gotoIfError(clean, ListToken_reserve(&context.tokens, lexer->tokens.length * 3 / 2, alloc))

	for (U64 i = 0; i < lexer->expressions.length; ++i) {

		LexerExpression e = lexer->expressions.ptr[i];

		if(e.type == ELexerExpressionType_MultiLineComment || e.type == ELexerExpressionType_Comment)
			continue;

		gotoIfError(clean, ParserContext_visit(&context, e.tokenOffset, e.tokenCount, alloc))
	}

clean:

	if(err.genericError) {
		ListToken_free(&context.tokens, alloc);
		ListCharString_freeUnderlying(&context.stringLiterals, alloc);
	}

	else *parser = (Parser) {
		.tokens = context.tokens,
		.parsedLiterals = context.stringLiterals,
		.lexer = lexer
	};

	return err;
}

//Helpers for classification

Error Parser_assert(Parser *parser, U64 *i, ETokenType type) {

	if (*i >= parser->tokens.length)
		return Error_outOfBounds(1, *i, parser->tokens.length, "Parser_assertAndSkip() out of bounds");

	if (parser->tokens.ptr[*i].tokenType != type)
		return Error_invalidParameter(1, 0, "Parser_assertAndSkip() unexpected token");

	return Error_none();
}

Error Parser_assertSymbol(Parser *parser, U64 *i) {

	if (*i >= parser->tokens.length)
		return Error_outOfBounds(1, *i, parser->tokens.length, "Parser_assertAndSkip() out of bounds");

	switch (parser->tokens.ptr[*i].tokenType) {

		case ETokenType_Identifier:
		case ETokenType_Integer:
		case ETokenType_SignedInteger:
		case ETokenType_Double:
		case ETokenType_Float:
		case ETokenType_String:
			return Error_invalidParameter(1, 0, "Parser_assertSymbol() unexpected token");

		default:
			return Error_none();
	}
}

Error Parser_assertAndSkip(Parser *parser, U64 *i, ETokenType type) {

	Error err = Parser_assert(parser, i, type);

	if (err.genericError)
		return err;

	++*i;
	return Error_none();
}

Bool Parser_next(Parser *parser, U64 *i, ETokenType type) {
	return *i < parser->tokens.length && parser->tokens.ptr[*i].tokenType == type;
}

Bool Parser_eof(Parser *parser, U64 *i) {
	return *i >= parser->tokens.length;
}

Bool Parser_skipIfNext(Parser *parser, U64 *i, ETokenType type) {

	if (Parser_next(parser, i, type)) {
		++*i;
		return true;
	}

	return false;
}

//Classifying anything (in global scope, structs or namespaces for example)
//If it's not detected as processed, it means it might be an expression

Error Parser_classifyBase(Parser* parser, U64 *i, Allocator alloc);

Error Parser_classifyClass(Parser *parser, U64 *i, Allocator alloc, U64 *classId);
Error Parser_classifyStruct(Parser *parser, U64 *i, Allocator alloc, U64 *structId);
Error Parser_classifyUnion(Parser *parser, U64 *i, Allocator alloc, U64 *unionId);
Error Parser_classifyInterface(Parser *parser, U64 *i, Allocator alloc, U64 *interfaceId);
Error Parser_classifyEnum(Parser *parser, U64 *i, Allocator alloc, U64 *enumId);

//Classifying type

Error Parser_classifyType(Parser *parser, U64 *i, Allocator alloc, U64 *typeId) {

	Error err = Error_none();
	gotoIfError(clean, Parser_assert(parser, i, ETokenType_Identifier))

	Token tok = parser->tokens.ptr[*i];
	CharString tokStr = Token_asString(tok, parser);
	U64 len = tok.tokenSize;

	Bool consumed = false;
	U32 c8x4 = len < 4 ? 0 : *(const U32*)tokStr.ptr;

	//struct
	//union
	//enum
	//class
	//interface
	//^

	*typeId = U64_MAX;

	switch (c8x4) {

		case C8x4('s', 't', 'r', 'u'):		//struct

			if (len == 6 && *(const U16*)&tokStr.ptr[4] == C8x2('c', 't')) {
				consumed = true;
				gotoIfError(clean, Parser_classifyStruct(parser, i, alloc, typeId))
				break;
			}

			break;

		case C8x4('u', 'n', 'i', 'o'):		//union

			if (len == 5 && tokStr.ptr[4] == 'n') {
				consumed = true;
				gotoIfError(clean, Parser_classifyUnion(parser, i, alloc, typeId))
				break;
			}

			break;

		case C8x4('e', 'n', 'u', 'm'):		//enum

			if (len == 4) {
				consumed = true;
				gotoIfError(clean, Parser_classifyEnum(parser, i, alloc, typeId))
				break;
			}

			break;

		case C8x4('c', 'l', 'a', 's'):		//class

			if (len == 5 && tokStr.ptr[4] == 's') {
				consumed = true;
				gotoIfError(clean, Parser_classifyClass(parser, i, alloc, typeId))
				break;
			}

			break;

		case C8x4('i', 'n', 't', 'e'):		//interface

			if (len == 9 && *(const U32*)&tokStr.ptr[4] == C8x4('r', 'f', 'a', 'c') && tokStr.ptr[8] == 'e') {
				consumed = true;
				gotoIfError(clean, Parser_classifyInterface(parser, i, alloc, typeId))
				break;
			}

			break;
	}

	//T<>
	//T
	//^

	if (!consumed) {

		CharString typedeffed = Token_asString(parser->tokens.ptr[*i], parser);
		typedeffed;
		++*i;

		if (Parser_next(parser, i, ETokenType_Lt))
			gotoIfError(clean, Error_unimplemented(0, "Parser_classifyTypedef() can't handle templates yet"))		//TODO:
	}

clean:
	return err;
}

//Classifying classes, structs, interfaces and unions

typedef enum EClassType {
	EClassType_Class,
	EClassType_Struct,
	EClassType_Interface,
	EClassType_Union
} EClassType;

Error Parser_classifyClassBase(Parser *parser, U64 *i, Allocator alloc, U64 *classId, EClassType classType) {

	(void)classType; (void)classId;

	Error err = Error_none();

	//class T
	//class {}
	//class T {}
	//struct T
	//struct {}
	//struct T {}
	//interface T
	//interface {}
	//interface T {}
	//^

	++*i;

	//class     T
	//struct    T
	//interface T
	//          ^

	Bool enteredOne = false;
	CharString className = CharString_createNull();

	if (Parser_next(parser, i, ETokenType_Identifier)) {
		className = Token_asString(parser->tokens.ptr[*i], parser);
		++*i;
		enteredOne = true;
	}

	//class T     { }
	//class       { }
	//struct T    { }
	//struct      { }
	//interface T { }
	//interface   { }
	//            ^

	if (Parser_skipIfNext(parser, i, ETokenType_CurlyBraceStart)) {

		enteredOne = true;

		//class T     { }
		//class       { }
		//struct T    { }
		//struct      { }
		//interface T { }
		//interface   { }
		//             ^

		while(!Parser_next(parser, i, ETokenType_CurlyBraceEnd))
			gotoIfError(clean, Parser_classifyBase(parser, i, alloc))

		//class T     { }
		//class       { }
		//struct T    { }
		//struct      { }
		//interface T { }
		//interface   { }
		//              ^

		gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_CurlyBraceEnd))
	}

	if(!enteredOne)
		gotoIfError(clean, Error_invalidState(0, "Parser_classifyClassBase() can't define class without identifier or {}"))

clean:
	return err;
}

Error Parser_classifyInitializer(Parser *parser, U64 *i, ETokenType endToken, ETokenType endToken2) {

	Error err = Error_none();

	//U32 a = 0xDEADBEEF,
	//U32 b = (0x12 << 8) | 0x34,
	//      ^

	gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_Asg))

	//U32 a = 0xDEADBEEF,
	//U32 b = (0x12 << 8) | 0x34,
	//        ^

	if(Parser_next(parser, i, endToken) || Parser_eof(parser, i))
		gotoIfError(clean, Error_invalidParameter(
			0, 0, "Parser_classifyInitializer() default value expected"
		))

	typedef enum EBracketType {
		EBracketType_Curly,
		EBracketType_Square,
		EBracketType_Round,
		EBracketType_Count
	} EBracketType;

	U64 tokenCounter = 0;	//U2[32] of EBracketType
	U8 bracketCounter = 0;

	for (; !Parser_eof(parser, i); ++*i) {

		EBracketType type = EBracketType_Count;
		Bool isOpen = true;

		//U32 a = 0xDEADBEEF,
		//                  ^
		//U32 b = (0x12 << 8) | 0x34,
		//                          ^
		//U32 c = 123;
		//           ^

		if(!bracketCounter && (Parser_next(parser, i, endToken) || Parser_next(parser, i, endToken2)))
			goto clean;

		//U32 b = (0x12 << 8) | 0x34,
		//        ^         ^

		switch (parser->tokens.ptr[*i].tokenType) {

			case ETokenType_RoundParenthesisStart:	type = EBracketType_Round;		break;
			case ETokenType_SquareBracketStart:		type = EBracketType_Square;		break;
			case ETokenType_CurlyBraceStart:		type = EBracketType_Curly;		break;

			case ETokenType_RoundParenthesisEnd:	type = EBracketType_Round;		isOpen = false;		break;
			case ETokenType_SquareBracketEnd:		type = EBracketType_Square;		isOpen = false;		break;
			case ETokenType_CurlyBraceEnd:			type = EBracketType_Curly;		isOpen = false;		break;
		}

		//Open and close bracket
		//U32 b = (0x12 << 8) | 0x34,
		//        ^         ^

		if(type != EBracketType_Count) {

			//Close bracket

			if (!isOpen) {

				if(!bracketCounter)
					gotoIfError(clean, Error_invalidParameter(
						0, 0, "Parser_classifyInitializer() found one of }]) but had no opening brackets before it"
					))

				EBracketType type2 = (tokenCounter >> ((bracketCounter - 1) << 1)) & 3;

				if(type2 != type)
					gotoIfError(clean, Error_invalidParameter(
						0, 0, "Parser_classifyInitializer() found one of mismatching }]) with one of {[("
					))

				--bracketCounter;
			}

			//Open bracket

			else {

				if(bracketCounter >= 32)
					gotoIfError(clean, Error_outOfBounds(
						0, 32, 32, "Parser_classifyInitializer() only allows {[( of 32 levels deep, but it was exceeded"
					))

				tokenCounter &= ~((U64)3 << (bracketCounter << 1));
				tokenCounter |= (U64)type << (bracketCounter << 1);

				++bracketCounter;
			}
		}
	}

	gotoIfError(clean, Error_invalidParameter(0, 0, "Parser_classifyInitializer() didn't find end of statement"))

clean:
	return err;
}

Error Parser_classifyFunction(Parser *parser, U64 *i, Allocator alloc) {

	Error err = Error_none();

	while (!Parser_next(parser, i, ETokenType_RoundParenthesisEnd)) {

		//static F32 test(F32 a, F32 b);
		//                ^      ^

		U64 typeId;
		gotoIfError(clean, Parser_classifyType(parser, i, alloc, &typeId))

		//static F32 test(F32 a, F32 b);
		//                    ^      ^

		CharString paramStr = Token_asString(parser->tokens.ptr[*i], parser);
		paramStr;
		++*i;

		//static F32 test(F32 a[3], F32 b[3][3]);
		//                     ^         ^  ^

		while (Parser_skipIfNext(parser, i, ETokenType_SquareBracketStart)) {

			//static F32 test(F32 a[3], F32 b[3][3]);
			//                      ^         ^  ^

			gotoIfError(clean, Parser_assert(parser, i, ETokenType_Integer))
			U64 paramArrayCount = parser->tokens.ptr[*i].valueu;
			paramArrayCount;
			++*i;

			//static F32 test(F32 a[3], F32 b[3][3]);
			//                       ^         ^  ^

			gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_SquareBracketEnd))
		}

		//F32 a : A,
		//      ^

		if (Parser_skipIfNext(parser, i, ETokenType_Colon)) {

			gotoIfError(clean, Parser_assert(parser, i, ETokenType_Identifier))

			CharString variableSemanticStr = Token_asString(parser->tokens.ptr[*i], parser);
			++*i;

			variableSemanticStr;
		}

		//U32 a = 0xDEADBEEF,
		//      ^

		if(Parser_next(parser, i, ETokenType_Asg))
			gotoIfError(clean, Parser_classifyInitializer(parser, i, ETokenType_Comma, ETokenType_RoundParenthesisEnd))

		if(Parser_next(parser, i, ETokenType_RoundParenthesisEnd))
			break;

		//F32 a : A,
		//         ^

		gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_Comma))
	}

	//static F32 test(F32 a, F32 b);
	//                            ^

	gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_RoundParenthesisEnd))

	//static F32 test(F32 a, F32 b);
	//                             ^

	if(Parser_skipIfNext(parser, i, ETokenType_Semicolon))
		goto clean;

	//static F32 test(F32 a, F32 b) {
	//                              ^

	gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_CurlyBraceStart))

	//static F32 test(F32 a, F32 b) { }
	//                               ^  (Skip everything in body for now)

	U64 bracketCounter = 1;

	for (; *i < parser->tokens.length; ++*i) {

		if(Parser_next(parser, i, ETokenType_CurlyBraceStart))
			++bracketCounter;

		else if(Parser_next(parser, i, ETokenType_CurlyBraceEnd)) {

			--bracketCounter;

			if(!bracketCounter)
				break;
		}
	}

	//static F32 test(F32 a, F32 b) { }
	//                                ^

	gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_CurlyBraceEnd))

clean:
	return err;
}

Error Parser_classifyFunctionOrVariable(Parser *parser, U64 *i, Allocator alloc, ESymbolAccess *scopeAccess) {

	//Detect all modifiers

	ESymbolFlag flag = ESymbolFlag_None;
	Error err = Error_none();

	if(*scopeAccess == ESymbolAccess_Private)		flag |= ESymbolFlag_IsPrivate;
	else if(*scopeAccess == ESymbolAccess_Public)	flag |= ESymbolFlag_IsPublic;

	//extern static const F32 x[5], y, z, w;
	//^      ^      ^

	//static F32 test(F32 a, F32 b);
	//^

	while (*i < parser->tokens.length) {

		Token tok = parser->tokens.ptr[*i];

		if(tok.tokenType != ETokenType_Identifier)
			break;

		CharString tokStr = Token_asString(tok, parser);
		U64 len = tok.tokenSize;
		U32 c8x4 = len < 4 ? 0 : *(const U32*)tokStr.ptr;
		Bool consumed = false;

		if (len == 2 && *(const U16*)tokStr.ptr == C8x2('i', 'n')) {								//in
			consumed = true;
			flag |= ESymbolFlag_IsIn;
		}

		else if(len == 3 && *(const U16*)tokStr.ptr == C8x2('o', 'u') && tokStr.ptr[2] == 't') {	//out
			consumed = true;
			flag |= ESymbolFlag_IsOut;
		}

		else switch (c8x4) {

			case C8x4('i', 'n', 'o', 'u'):		//inout

				if (len == 5 && tokStr.ptr[4] == 't') {
					consumed = true;
					flag |= ESymbolFlag_IsIn | ESymbolFlag_IsOut;
				}

				break;

			case C8x4('f', 'l', 'a', 't'):		//flat

				if (len == 4) {
					consumed = true;
					flag |= ESymbolFlag_NoInterpolation;
				}

				break;

			case C8x4('n', 'o', 'i', 'n'):		//nointerpolation

				if(
					len == 15 &&
					*(const U64*)&tokStr.ptr[4] == C8x8('t', 'e', 'r', 'p', 'o', 'l', 'a', 't') &&
					*(const U16*)&tokStr.ptr[12] == C8x2('i', 'o') &&
					tokStr.ptr[14] == 'n'
				) {
					consumed = true;
					flag |= ESymbolFlag_NoInterpolation;
				}

				break;

			case C8x4('l', 'i', 'n', 'e'):		//linear

				if (len == 6 && *(const U16*)&tokStr.ptr[4] == C8x2('a', 'r')) {
					consumed = true;
					flag |= ESymbolFlag_Linear;
				}

				break;

			case C8x4('s', 'm', 'o', 'o'):		//smooth

				if (len == 6 && *(const U16*)&tokStr.ptr[4] == C8x2('t', 'h')) {
					consumed = true;
					flag |= ESymbolFlag_Linear;
				}

				break;

			case C8x4('s', 'a', 'm', 'p'):		//sample

				if (len == 6 && *(const U16*)&tokStr.ptr[4] == C8x2('l', 'e')) {
					consumed = true;
					flag |= ESymbolFlag_Sample;
				}

				break;

			case C8x4('c', 'e', 'n', 't'):		//centroid

				if (len == 8 && *(const U32*)&tokStr.ptr[4] == C8x4('r', 'o', 'i', 'd')) {
					consumed = true;
					flag |= ESymbolFlag_Centroid;
				}

				break;

			case C8x4('n', 'o', 'p', 'e'):		//noperspective

				if(
					len == 13 &&
					*(const U64*)&tokStr.ptr[4] == C8x8('r', 's', 'p', 'e', 'c', 't', 'i', 'v') &&
					tokStr.ptr[12] == 'e'
				) {
					consumed = true;
					flag |= ESymbolFlag_NoPerspective;
				}

				break;

			case C8x4('s', 'n', 'o', 'r'):		//snorm
			case C8x4('u', 'n', 'o', 'r'):		//unorm

				if (len == 5 && tokStr.ptr[4] == 'm') {
					consumed = true;
					flag |= (c8x4 == C8x4('s', 'n', 'o', 'r') ? ESymbolFlag_IsSnorm : ESymbolFlag_IsUnorm);
				}

				break;

			case C8x4('c', 'o', 'n', 's'):		//const, constexpr

				if (len == 5 && tokStr.ptr[4] == 't') {
					consumed = true;
					flag |= ESymbolFlag_IsConst;
				}

				else if (len == 9 && *(const U32 *)&tokStr.ptr[4] == C8x4('t', 'e', 'x', 'p') && tokStr.ptr[8] == 'r') {
					consumed = true;
					flag |= ESymbolFlag_IsConstexpr;
				}

				break;

			case C8x4('p', 'u', 'b', 'l'):		//public

				if (len == 6 && *(const U16*)&tokStr.ptr[4] == C8x2('i', 'c')) {

					U64 temp = *i + 1;

					if(Parser_next(parser, &temp, ETokenType_Colon)) {
						++*i;
						*scopeAccess = ESymbolAccess_Public;
					}

					consumed = true;
					flag &= ~ESymbolFlag_IsPrivate;
					flag |= ESymbolFlag_IsPublic;
				}

				break;

			case C8x4('p', 'r', 'i', 'v'):		//private

				if (len == 7 && *(const U16*)&tokStr.ptr[4] == C8x2('a', 't') && tokStr.ptr[6] == 'e') {

					U64 temp = *i + 1;

					if(Parser_next(parser, &temp, ETokenType_Colon)) {
						++*i;
						*scopeAccess = ESymbolAccess_Private;
					}

					consumed = true;
					flag &= ~ESymbolFlag_IsPublic;
					flag |= ESymbolFlag_IsPrivate;
				}

				break;

			case C8x4('p', 'r', 'o', 't'):		//protected

				if (len == 9 && *(const U32*)&tokStr.ptr[4] == C8x4('e', 'c', 't', 'e') && tokStr.ptr[8] == 'd') {

					U64 temp = *i + 1;

					if(Parser_next(parser, &temp, ETokenType_Colon)) {
						++*i;
						*scopeAccess = ESymbolAccess_Protected;
					}

					consumed = true;
					flag &= ~ESymbolFlag_Access;
				}

				break;

			case C8x4('e', 'x', 't', 'e'):		//extern

				if (len == 6 && *(const U16*)&tokStr.ptr[4] == C8x2('r', 'n')) {
					consumed = true;
					flag |= ESymbolFlag_IsExtern;
				}

				break;

			case C8x4('s', 't', 'a', 't'):		//static

				if (len == 6 && *(const U16*)&tokStr.ptr[4] == C8x2('i', 'c')) {
					consumed = true;
					flag |= ESymbolFlag_IsStatic;
				}

				break;

			case C8x4('i', 'm', 'p', 'l'):		//impl

				if (len == 4) {
					consumed = true;
					flag |= ESymbolFlag_HasImpl;
				}

				break;

			case C8x4('u', 's', 'e', 'r'):		//user_impl

				if (len == 9 && *(const U32*)&tokStr.ptr[4] == C8x4('_', 'i', 'm', 'p') && tokStr.ptr[8] == 'l') {
					consumed = true;
					flag |= ESymbolFlag_HasUserImpl;
				}

				break;
		}

		if(!consumed)
			break;

		++*i;
	}

	//extern static const F32 x[5], y, z, w;
	//                    ^

	//static F32 test(F32 a, F32 b);
	//       ^

	U64 typeId;
	gotoIfError(clean, Parser_classifyType(parser, i, alloc, &typeId))

	//extern static const F32 x[5], y, z, w;
	//                        ^

	//static F32 test(F32 a, F32 b);
	//           ^

	gotoIfError(clean, Parser_assert(parser, i, ETokenType_Identifier))

	//Continue until we find the end

	while (*i < parser->tokens.length) {

		Token tok = parser->tokens.ptr[*i];

		if(tok.tokenType != ETokenType_Identifier)
			break;

		CharString nameStr = Token_asString(tok, parser);
		nameStr;
		++*i;

		//F32 operator+(
		//    ^

		if(tok.tokenSize == 8 && *(const U64*)nameStr.ptr == C8x8('o', 'p', 'e', 'r', 'a', 't', 'o', 'r')) {

			flag |= ESymbolFlag_IsOperator;

			//F32 operator+(
			//            ^

			gotoIfError(clean, Parser_assertSymbol(parser, i))
			ETokenType tokType = parser->tokens.ptr[*i].tokenType;
			nameStr = Token_asString(parser->tokens.ptr[*i], parser);
			++*i;

			switch (tokType) {

				case ETokenType_Period:

				case ETokenType_Ternary:

				case ETokenType_RoundParenthesisEnd:
				case ETokenType_SquareBracketEnd:

				case ETokenType_CurlyBraceStart:
				case ETokenType_CurlyBraceEnd:

				case ETokenType_SquareBracketStart2:
				case ETokenType_SquareBracketEnd2:

				case ETokenType_Colon:
				case ETokenType_Colon2:
				case ETokenType_Semicolon:
					gotoIfError(clean, Error_invalidState(
						0,
						"Parser_classifyFunctionOrVariable() operator is disallowed with one of the following: "
						".?]){}:; or [[, ]], ::"
					))
			}

			//F32 operator[](
			//            ^

			//TODO: operator[] and operator() are called [ and ( due to quirks

			if (tokType == ETokenType_SquareBracketStart || tokType == ETokenType_RoundParenthesisStart) {

				ETokenType expected =
					tokType == ETokenType_SquareBracketStart ? ETokenType_SquareBracketEnd : ETokenType_RoundParenthesisEnd;

				gotoIfError(clean, Parser_assertAndSkip(parser, i, expected))
			}

			//F32 operator()(
			//              ^

			gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_RoundParenthesisStart))
		}

		//static F32 test(F32 a, F32 b);
		//               ^

		if (Parser_skipIfNext(parser, i, ETokenType_RoundParenthesisStart)) {
			gotoIfError(clean, Parser_classifyFunction(parser, i, alloc))
			goto clean;
		}

		//TODO: (test)[5] is legal and ((test))[5] is as well and ((test)[5]), etc.

		//extern static const F32 x[5], y[3][3], z, w;
		//                         ^     ^  ^

		while (Parser_skipIfNext(parser, i, ETokenType_SquareBracketStart)) {

			//extern static const F32 x[5], y[3][3], z, w;
			//                          ^     ^  ^

			gotoIfError(clean, Parser_assert(parser, i, ETokenType_Integer))
			U64 arrayCount = parser->tokens.ptr[*i].valueu;
			arrayCount;
			++*i;

			//extern static const F32 x[5], y[3][3], z, w;
			//                           ^     ^  ^

			gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_SquareBracketEnd))
		}

		//F32 a : A;
		//      ^

		if (Parser_skipIfNext(parser, i, ETokenType_Colon)) {

			gotoIfError(clean, Parser_assert(parser, i, ETokenType_Identifier))

			CharString semanticStr = Token_asString(tok, parser);
			++*i;

			semanticStr;
		}

		//static constexpr U32 a = 0xDEADBEEF;
		//                       ^

		if(Parser_next(parser, i, ETokenType_Asg))
			gotoIfError(clean, Parser_classifyInitializer(parser, i, ETokenType_Semicolon, ETokenType_Count))

		//extern static const F32 x[5], y[3][3], z, w;
		//                            ^        ^  ^  ^

		//extern static const F32 x[5], y[3][3], z, w;
		//                                           ^

		if(Parser_next(parser, i, ETokenType_Semicolon))
			break;

		gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_Comma))
	}

	gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon))

clean:
	return err;
}

Error Parser_classifyEnumBody(Parser *parser, U64 *i, Allocator alloc) {

	(void) alloc;

	Error err = Error_none();

	//X = (1 << 3),
	//^
	//Y = 123,
	//Z,
	//W

	while(!Parser_next(parser, i, ETokenType_CurlyBraceEnd)) {

		//X = (1 << 3),
		//Y = 123,
		//Z,
		//W
		//^

		gotoIfError(clean, Parser_assert(parser, i, ETokenType_Identifier))
		CharString enumName = Token_asString(parser->tokens.ptr[*i], parser);
		++*i;

		(void) enumName;

		//Z,
		// ^

		if(Parser_skipIfNext(parser, i, ETokenType_Comma))
			continue;

		//X = (1 << 3),
		//Y = 123,
		//  ^

		Parser_assertAndSkip(parser, i, ETokenType_Eq);

		//X = (1 << 3),
		//Y = 123,
		//    ^

		U64 bracketCounter = 0;

		for (; *i < parser->tokens.length; ++*i) {

			//X = (1 << 3),
			//    ^

			if(Parser_next(parser, i, ETokenType_RoundParenthesisStart))
				++bracketCounter;

			//X = (1 << 3),
			//           ^

			else if(Parser_next(parser, i, ETokenType_RoundParenthesisEnd)) {

				if(!bracketCounter)
					gotoIfError(clean, Error_invalidState(
						0, "Parser_classifyEnumBody() ')' is found, but isn't matched to '('"
					))

				--bracketCounter;
			}

			//X = (1 << 3),
			//Y = 123     ,
			//            ^

			else if(Parser_next(parser, i, ETokenType_Comma) && !bracketCounter)
				break;
		}

		//X = (1 << 3),
		//Y = 123     ,
		//            ^

		gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_Comma))
	}

clean:
	return err;
}

Error Parser_classifyEnum(Parser *parser, U64 *i, Allocator alloc, U64 *enumId) {

	(void)enumId;

	Error err = Error_none();

	//enum T
	//enum
	//^

	++*i;

	//enum T
	//enum class
	//     ^

	Bool enteredOne = false;
	CharString className = CharString_createNull();

	Bool enumClass = false;

	CharString enumTypeStr = CharString_createNull();
	U64 enumType = U64_MAX;

	if (Parser_next(parser, i, ETokenType_Identifier)) {

		className = Token_asString(parser->tokens.ptr[*i], parser);
		++*i;

		//enum class
		//     ^

		if (
			CharString_length(className) == 5 &&
			*(const U32*)&className.ptr == C8x4('c', 'l', 'a', 's') &&
			className.ptr[4] == 's'
		) {

			enumClass = true;

			gotoIfError(clean, Parser_assert(parser, i, ETokenType_Identifier))
			className = Token_asString(parser->tokens.ptr[*i], parser);
			++*i;

			//enum class T : U8
			//             ^

			if (Parser_skipIfNext(parser, i, ETokenType_Colon)) {

				gotoIfError(clean, Parser_assert(parser, i, ETokenType_Identifier))
				enumTypeStr = Token_asString(parser->tokens.ptr[*i], parser);
				++*i;

				if (Parser_next(parser, i, ETokenType_Lt))
					gotoIfError(clean, Error_unimplemented(0, "Parser_classifyEnum() can't handle templates yet"))		//TODO:

				(void)enumType;

				//TODO: fetch enumType
			}
		}

		enteredOne = true;
	}

	//enum T            { }
	//enum              { }
	//enum class T      { }
	//enum class T : U8 { }
	//                  ^

	if (Parser_skipIfNext(parser, i, ETokenType_CurlyBraceStart)) {

		enteredOne = true;

		//enum T            { }
		//enum              { }
		//enum class T      { }
		//enum class T : U8 { }
		//                   ^

		gotoIfError(clean, Parser_classifyEnumBody(parser, i, alloc))

		//enum T            { }
		//enum              { }
		//enum class T      { }
		//enum class T : U8 { }
		//                    ^

		gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_CurlyBraceEnd))
	}

	if(!enteredOne)
		gotoIfError(clean, Error_invalidState(0, "Parser_classifyEnum() can't define enum without identifier or {}"))

clean:
	return err;
}

Error Parser_classifyClass(Parser *parser, U64 *i, Allocator alloc, U64 *classId) {
	return Parser_classifyClassBase(parser, i, alloc, classId, EClassType_Class);
}

Error Parser_classifyStruct(Parser *parser, U64 *i, Allocator alloc, U64 *structId) {
	return Parser_classifyClassBase(parser, i, alloc, structId, EClassType_Struct);
}

Error Parser_classifyUnion(Parser *parser, U64 *i, Allocator alloc, U64 *unionId) {
	return Parser_classifyClassBase(parser, i, alloc, unionId, EClassType_Union);
}

Error Parser_classifyInterface(Parser *parser, U64 *i, Allocator alloc, U64 *interfaceId) {
	return Parser_classifyClassBase(parser, i, alloc, interfaceId, EClassType_Interface);
}

Error Parser_classifyUsing(Parser *parser, U64 *i, Allocator alloc) {

	Error err = Error_none();

	//using T2 = T;
	//^

	++*i;

	//using T2 = T;
	//      ^

	gotoIfError(clean, Parser_assert(parser, i, ETokenType_Identifier))
	CharString typeName = Token_asString(parser->tokens.ptr[*i], parser);

	typeName;

	//using T2 = T;
	//         ^

	gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_Asg))

	//using T2 = T;
	//           ^

	U64 typeId;
	gotoIfError(clean, Parser_classifyType(parser, i, alloc, &typeId))

	//TODO: Insert symbol here

clean:
	return err;
}

Error Parser_classifyTypedef(Parser *parser, U64 *i, Allocator alloc) {

	Error err = Error_none();

	//typedef T<x, y> T2;
	//^

	++*i;

	//typedef T<x, y> T2;
	//        ^

	U64 typeId;
	gotoIfError(clean, Parser_classifyType(parser, i, alloc, &typeId))

	//typedef struct {} T;
	//typedef T2        T;
	//                  ^

	gotoIfError(clean, Parser_assert(parser, i, ETokenType_Identifier))
	CharString typeName = Token_asString(parser->tokens.ptr[*i], parser);
	++*i;

	typeName;

	//typedef struct {} T;
	//                   ^

	gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon))

	//TODO: Insert symbol here

clean:
	return err;
}

Error Parser_classifyBase(Parser *parser, U64 *i, Allocator alloc) {

	Token tok = parser->tokens.ptr[*i];
	CharString tokStr = Token_asString(tok, parser);
	Error err = Error_none();
	Bool consumed = false;

	//Most expressions

	if (tok.tokenType == ETokenType_Identifier) {

		U64 len = tok.tokenSize;
		U32 c8x4 = len < 4 ? 0 : *(const U32*)tokStr.ptr;

		switch (c8x4) {

			case C8x4('t', 'e', 'm', 'p'):		//template

				//TODO:
				/*if (len == 8 && *(const U32*)&tokStr.ptr[4] == C8x4('l', 'a', 't', 'e')) {
					consumed = true;
					gotoIfError(clean, Parser_classifyTemplate(parser, &i, alloc))
					gotoIfError(clean, Parser_assertAndSkip(parser, &i, ETokenType_Semicolon))
					break;
				}*/

				break;

			case C8x4('t', 'y', 'p', 'e'):		//typedef

				if (len == 7 && *(const U16*)&tokStr.ptr[4] == C8x2('d', 'e') && tokStr.ptr[6] == 'f') {
					consumed = true;
					gotoIfError(clean, Parser_classifyTypedef(parser, i, alloc))
					break;
				}

				break;

			case C8x4('u', 's', 'i' ,'n'):		//using

				if (len == 5 && tokStr.ptr[4] == 'g') {
					consumed = true;
					gotoIfError(clean, Parser_classifyUsing(parser, i, alloc))
					gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon))
					break;
				}

				break;

			case C8x4('c', 'l', 'a', 's'):		//class

				if (len == 5 && tokStr.ptr[4] == 's') {
					consumed = true;
					gotoIfError(clean, Parser_classifyClass(parser, i, alloc, NULL))
					gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon))
					break;
				}

				break;

			case C8x4('s', 't', 'r', 'u'):		//struct

				if (len == 6 && *(const U16*)&tokStr.ptr[4] == C8x2('c', 't')) {
					consumed = true;
					gotoIfError(clean, Parser_classifyStruct(parser, i, alloc, NULL))
					gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon))
					break;
				}

				break;

			case C8x4('u', 'n', 'i', 'o'):		//union

				if (len == 5 && tokStr.ptr[4] == 'n') {
					consumed = true;
					gotoIfError(clean, Parser_classifyUnion(parser, i, alloc, NULL))
					gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon))
					break;
				}

				break;

			case C8x4('e', 'n', 'u', 'm'):		//enum

				if (len == 4) {
					consumed = true;
					gotoIfError(clean, Parser_classifyEnum(parser, i, alloc, NULL))
					gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon))
					break;
				}

				break;

			case C8x4('i', 'n', 't', 'e'):		//interface

				if (len == 9 && *(const U32*)&tokStr.ptr[4] == C8x4('r', 'f', 'a', 'c') && tokStr.ptr[8] == 'e') {
					consumed = true;
					gotoIfError(clean, Parser_classifyInterface(parser, i, alloc, NULL))
					gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon))
					break;
				}

				break;

			case C8x4('n', 'a', 'm', 'e'):		//namespace

				if (len == 9 && *(const U32*)&tokStr.ptr[4] == C8x4('s', 'p', 'a', 'c') && tokStr.ptr[8] == 'e') {

					consumed = true;

					++*i;
					gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_CurlyBraceStart))
					gotoIfError(clean, Parser_classifyBase(parser, i, alloc))
					gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_CurlyBraceEnd))
					break;
				}

				break;

			default:
				break;
		}

		//If it's not a reserved keyword, then it's probably a variable or function
		//Or it's an annotation [likeThis]
		//Or it's an expression, which we won't handle here.

		if(!consumed) {
			ESymbolAccess symbolAccess = ESymbolAccess_Protected;		//TODO:
			gotoIfError(clean, Parser_classifyFunctionOrVariable(parser, i, alloc, &symbolAccess))
		}
	}

	//Annotations

	else {

		//[shader("test")]
		//^

		if (Parser_skipIfNext(parser, i, ETokenType_SquareBracketStart)) {

			//[shader("test")]
			// ^

			U64 counter = 1;

			for (; *i < parser->tokens.length; ++*i) {

				switch (parser->tokens.ptr[*i].tokenType) {
					case ETokenType_SquareBracketStart:	++counter;	break;
					case ETokenType_SquareBracketEnd:	--counter;	break;
				}

				if(!counter)
					break;
			}

			//[shader("test")]
			//               ^

			gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_SquareBracketEnd))
		}

		//[[vk::binding(1, 0)]]
		//^

		else if (Parser_skipIfNext(parser, i, ETokenType_SquareBracketStart2)) {

			//[[vk::binding(1, 0)]]
			//  ^

			for (; *i < parser->tokens.length; ++*i)
				if (Parser_next(parser, i, ETokenType_SquareBracketEnd2))
					break;

			//[[vk::binding(1, 0)]]
			//                   ^

			gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_SquareBracketEnd2))
		}

		//Unrecognized

		else gotoIfError(clean, Error_invalidParameter(0, 0, "Parser_classify() couldn't classify element"))
	}

clean:
	return err;
}

Error Parser_classify(Parser *parser, Allocator alloc) {

	if (!parser || !parser->lexer)
		return Error_nullPointer(0, "Parser_classify()::parser is required");

	//Classify tokens

	Error err = Error_none();
	gotoIfError(clean, ListSymbol_reserve(&parser->symbols, 64, alloc))

	U64 i = 0;

	for (; i < parser->tokens.length; )
		gotoIfError(clean, Parser_classifyBase(parser, &i, alloc))

clean:
	return err;
}

Bool Parser_free(Parser *parser, Allocator alloc) {

	if(!parser)
		return true;

	ListSymbol_free(&parser->symbols, alloc);
	ListToken_free(&parser->tokens, alloc);
	ListCharString_freeUnderlying(&parser->parsedLiterals, alloc);
	return true;
}

void Parser_printTokens(Parser parser, Allocator alloc) {

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

void Parser_printSymbols(Parser parser, Allocator alloc) {
	(void)parser;
	Log_debugLn(alloc, "Print symbols not implemented yet");
}

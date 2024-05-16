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

Bool Parser_visit(Parser *parser, U32 lexerTokenId, U32 lexerTokenCount, Allocator alloc, Error *e_rr) {

	const Lexer *lexer = parser->lexer;
	CharString temp = CharString_createNull();		//For parsing strings
	Bool s_uccess = true;

	for(U32 i = 0; i < lexerTokenCount; ++i) {

		LexerToken lext = lexer->tokens.ptr[lexerTokenId + i];
		CharString lextStr = LexerToken_asString(lext, *lexer);
		ELexerTokenType lextType = LexerToken_getType(lext);

		if(CharString_length(lextStr) >> 8)
			retError(clean, Error_invalidParameter(0, 0, "Parser_visit() token max size is 256"))

		switch(lextType) {

			//Handle define/macro and normal keyword

			case ELexerTokenType_Identifier: {

				Token tok = (Token) {
					.naiveTokenId = lexerTokenId + i,
					.tokenType = ETokenType_Identifier,
					.tokenSize = (U8) CharString_length(lextStr)
				};

				gotoIfError2(clean, ListToken_pushBack(&parser->tokens, tok, alloc))
				break;
			}

			//Append entire lexer token

			case ELexerTokenType_Float:
			case ELexerTokenType_Double: {

				F64 tmp = 0;
				if(!CharString_parseDouble(lextStr, &tmp))
					retError(clean, Error_invalidOperation(2, "Parser_visit() expected double but couldn't parse it"))

				Token tok = (Token) {
					.naiveTokenId = lexerTokenId + i,
					.tokenType = lextType == ELexerTokenType_Float ? ETokenType_Float : ETokenType_Double,
					.valuef = tmp,
					.tokenSize = (U8) CharString_length(lextStr)
				};

				gotoIfError2(clean, ListToken_pushBack(&parser->tokens, tok, alloc))
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
					retError(clean, Error_invalidOperation(3, "Parser_visit() expected integer, but couldn't parse it"))

				if(negate && (tmp >> 63))
					retError(clean, Error_invalidOperation(4, "Parser_visit() expected I64, but got U64 (too big)"))

				Token tok = (Token) {
					.naiveTokenId = lexerTokenId + i,
					.tokenType = negate ? ETokenType_SignedInteger : ETokenType_Integer,
					.valuei = ((I64) tmp) * (negate ? -1 : 1),
					.tokenSize = (U8) CharString_length(lextStr) + movedChar
				};

				gotoIfError2(clean, ListToken_pushBack(&parser->tokens, tok, alloc))
				break;
			}

			case ELexerTokenType_String: {

				//Escaped strings need to be resolved

				Bool isEscaped = false;

				for (U64 j = 0; j < CharString_length(lextStr); ++j) {

					C8 c = lextStr.ptr[j];

					if (isEscaped || c != '\\') {
						gotoIfError2(clean, CharString_append(&temp, c, alloc))
						isEscaped = false;
						continue;
					}

					isEscaped = true;
				}

				gotoIfError2(clean, ListCharString_pushBack(&parser->parsedLiterals, temp, alloc))
				temp = CharString_createNull();

				Token tok = (Token) {
					.naiveTokenId = lexerTokenId + i,
					.tokenType = ETokenType_String,
					.valueu = parser->parsedLiterals.length - 1,
					.tokenSize = (U8) CharString_length(lextStr)
				};

				gotoIfError2(clean, ListToken_pushBack(&parser->tokens, tok, alloc))
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
						retError(clean, Error_unsupportedOperation(1, "Parser_visit() invalid token"))

					Token tok = (Token) {
						.naiveTokenId = lexerTokenId + i,
						.tokenType = tokenType,
						.lexerTokenSubId = (U8) prev,
						.tokenSize = (U8)(subTokenOffset - prev)
					};

					gotoIfError2(clean, ListToken_pushBack(&parser->tokens, tok, alloc))
				}
		}
	}

clean:
	CharString_free(&temp, alloc);
	return s_uccess;
}

Bool Parser_create(const Lexer *lexer, Parser *parser, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!lexer || !parser)
		retError(clean, Error_nullPointer(!lexer ? 0 : 1, "Parser_create()::parser and lexer are required"))

	if(parser->symbols.ptr)
		retError(clean, Error_alreadyDefined(1, "Parser_create()::parser must be empty"))

	parser->lexer = lexer;
	gotoIfError2(clean, ListCharString_reserve(&parser->parsedLiterals, 64, alloc))
	gotoIfError2(clean, ListToken_reserve(&parser->tokens, lexer->tokens.length * 3 / 2, alloc))

	for (U64 i = 0; i < lexer->expressions.length; ++i) {

		LexerExpression e = lexer->expressions.ptr[i];

		if(e.type == ELexerExpressionType_MultiLineComment || e.type == ELexerExpressionType_Comment)
			continue;

		gotoIfError3(clean, Parser_visit(parser, e.tokenOffset, e.tokenCount, alloc, e_rr))
	}

clean:

	if(!s_uccess)
		Parser_free(parser, alloc);

	return s_uccess;
}

void Parser_free(Parser *parser, Allocator alloc) {

	if(!parser)
		return;

	ListSymbol_free(&parser->symbols, alloc);
	ListToken_free(&parser->tokens, alloc);
	ListCharString_freeUnderlying(&parser->parsedLiterals, alloc);
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

Bool Symbol_create(

	ESymbolType type,
	ESymbolFlag flags,
	U8 annotations,

	U32 name,
	U32 baseType,
	U32 parent,
	U32 localId,		//U20
	U16 children,		//U10

	Error *e_rr,
	Symbol *symbol
) {

	Bool s_uccess = true;

	if(!symbol)
		retError(clean, Error_nullPointer(9, "Symbol_create()::symbol is required"))

	if(annotations >> 5)
		retError(clean, Error_outOfBounds(2, annotations, (1 << 5) - 1, "Symbol_create()::annotations out of bounds"))

	if(type >> 5)
		retError(clean, Error_outOfBounds(2, type, (1 << 5) - 1, "Symbol_create()::type out of bounds"))

	if(children >> 10)
		retError(clean, Error_outOfBounds(1, children, (1 << 10) - 1, "Symbol_create()::children out of bounds"))

	if(flags >> 20)
		retError(clean, Error_outOfBounds(1, flags, (1 << 20) - 1, "Symbol_create()::flags out of bounds"))

	if(localId >> 20)
		retError(clean, Error_outOfBounds(1, localId, (1 << 20) - 1, "Symbol_create()::localId out of bounds"))

	*symbol = (Symbol) {
		.symbolFlagTypeAnnotations = type | (flags << 5) | ((U32)annotations << 25),
		.name = name,
		.baseType = baseType,
		.parent = parent,
		.localIdChildren = localId | ((U32)children << 20)
	};

clean:
	return s_uccess;
}

U32 Symbol_getLocalId(Symbol s) {
	return s.localIdChildren & ((1 << 20) - 1);
}

U32 Symbol_getChildren(Symbol s) {
	return s.localIdChildren >> 20;
}

ESymbolType Symbol_getType(Symbol s) {
	return (ESymbolType) (s.symbolFlagTypeAnnotations & 31);
}

ESymbolFlag Symbol_getFlags(Symbol s) {
	return (s.symbolFlagTypeAnnotations >> 5) & ((1 << 20) - 1);
}

U8 Symbol_getAnnotations(Symbol s) {
	return s.symbolFlagTypeAnnotations >> 25;
}

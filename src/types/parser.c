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

Bool Parser_skipIfNext(Parser *parser, U64 *i, ETokenType type) {

	if (Parser_next(parser, i, type)) {
		++*i;
		return true;
	}

	return false;
}

//Handling 

//TODO: namespaces
//TODO: operator overloading
//TODO: Bitfields and semantics

//gotoIfError(clean, Parser_classifyBody(parser, i, alloc))

//Classifying anything (in global scope, structs or namespaces for example)
//If it's not detected as processed, it means it might be an expression

Error Parser_classifyBase(Parser* parser, U64 *i, Allocator alloc, Bool *processed);

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

		++*i;
		CharString typedeffed = Token_asString(parser->tokens.ptr[*i], parser);
		typedeffed;

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

		Bool processed = false;
		gotoIfError(clean, Parser_classifyBase(parser, i, alloc, &processed))

		if(!processed)
			gotoIfError(clean, Error_invalidState(1, "Parser_classifyClassBase() invalid expression in type definition"))
			
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

Error Parser_classifyFunctionOrVariable(Parser *parser, U64 *i, Allocator alloc, Bool *consumed) {
	(void)parser; (void)i; (void)alloc; (void)consumed;
	return Error_unimplemented(0, "Parser_classifyFunctionOrVariable() is not implemented yet");		//TODO:
}

Error Parser_classifyEnumBody(Parser *parser, U64 *i, Allocator alloc) {
	(void)parser; (void)i; (void)alloc;
	return Error_unimplemented(0, "Parser_classifyEnumBody() is not implemented yet");		//TODO:
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

	typeName;

	//typedef struct {} T;
	//                   ^

	gotoIfError(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon))

	//TODO: Insert symbol here

clean:
	return err;
}

Error Parser_classifyBase(Parser *parser, U64 *i, Allocator alloc, Bool *processed) {

	Token tok = parser->tokens.ptr[*i];
	CharString tokStr = Token_asString(tok, parser);
	Error err = Error_none();
	Bool consumed = false;

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

			default:
				break;
		}

		//If it's not a reserved keyword, then it's probably a variable or function
		//Or it's an annotation [likeThis]
		//Or it's an expression, which we won't handle here.

		if(!consumed)
			gotoIfError(clean, Parser_classifyFunctionOrVariable(parser, i, alloc, &consumed))
	}

	*processed = consumed;
clean:
	return err;
}

Error Parser_classify(Parser *parser, Allocator alloc) {

	if (!parser || !parser->lexer)
		return Error_nullPointer(0, "Parser_classify()::parser is required");

	//Classify tokens

	Error err = Error_none();
	gotoIfError(clean, ListSymbol_reserve(&parser->symbols, 64, alloc))

	for (U64 i = 0; i < parser->tokens.length; ) {

		Bool processed = false;
		gotoIfError(clean, Parser_classifyBase(parser, &i, alloc, &processed))

		if(!processed)
			gotoIfError(clean, Error_invalidState(0, "Parser_classify() couldn't classify one of the symbols"))
	}

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

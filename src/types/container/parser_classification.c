/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/container/list_impl.h"
#include "types/container/string.h"
#include "types/container/parser.h"
#include "types/container/lexer.h"
#include "types/base/c8.h"

//Helpers for classification

Bool Parser_assert(Parser *parser, U32 *i, ETokenType type, Error *e_rr) {

	Bool s_uccess = true;

	if (*i >= parser->tokens.length)
		retError(clean, Error_outOfBounds(1, *i, parser->tokens.length, "Parser_assert() out of bounds"))

	if (parser->tokens.ptr[*i].tokenType != type)
		retError(clean, Error_invalidParameter(1, 0, "Parser_assert() unexpected token"))

clean:
	return s_uccess;
}

Bool Parser_assert2(Parser *parser, U32 *i, ETokenType type1, ETokenType type2, Error *e_rr) {

	Bool s_uccess = true;

	if (*i >= parser->tokens.length)
		retError(clean, Error_outOfBounds(1, *i, parser->tokens.length, "Parser_assert2() out of bounds"))

	ETokenType type = parser->tokens.ptr[*i].tokenType;

	if (type != type1 && type != type2)
		retError(clean, Error_invalidParameter(1, 0, "Parser_assert2() unexpected token"))

clean:
	return s_uccess;
}

Bool Parser_assertSymbol(Parser *parser, U32 *i, Error *e_rr) {

	Bool s_uccess = true;

	if (*i >= parser->tokens.length)
		retError(clean, Error_outOfBounds(1, *i, parser->tokens.length, "Parser_assertAndSkip() out of bounds"))

	switch (parser->tokens.ptr[*i].tokenType) {

		case ETokenType_Identifier:
		case ETokenType_Integer:
		case ETokenType_SignedInteger:
		case ETokenType_Double:
		case ETokenType_Float:
		case ETokenType_String:
			retError(clean, Error_invalidParameter(1, 0, "Parser_assertSymbol() unexpected token"))

		default:
			break;
	}

clean:
	return s_uccess;
}

Bool Parser_assertAndSkip(Parser *parser, U32 *i, ETokenType type, Error *e_rr) {

	Bool s_uccess = true;
	gotoIfError3(clean, Parser_assert(parser, i, type, e_rr))

	++*i;

clean:
	return s_uccess;
}

Bool Parser_assertAndSkip2(Parser *parser, U32 *i, ETokenType type1, ETokenType type2, Error *e_rr) {

	Bool s_uccess = true;
	gotoIfError3(clean, Parser_assert2(parser, i, type1, type2, e_rr))

	++*i;

clean:
	return s_uccess;
}

Bool Parser_next(Parser *parser, U32 *i, ETokenType type) {
	return *i < parser->tokens.length && parser->tokens.ptr[*i].tokenType == type;
}

Bool Parser_next2(Parser *parser, U32 *i, ETokenType type1, ETokenType type2) {					//If next is type1 or type2
	return *i < parser->tokens.length && (
		parser->tokens.ptr[*i].tokenType == type1 || parser->tokens.ptr[*i].tokenType == type2
	);
}

Bool Parser_eof(Parser *parser, U32 *i) {
	return *i >= parser->tokens.length;
}

Bool Parser_skipIfNext(Parser *parser, U32 *i, ETokenType type) {

	if (Parser_next(parser, i, type)) {
		++*i;
		return true;
	}

	return false;
}

//Classifying anything (in global scope, structs or namespaces for example)
//If it's not detected as processed, it means it might be an expression

Bool Parser_classifyBase(Parser *parser, U32 *i, U32 parent, Allocator alloc, Error *e_rr);

Bool Parser_classifyClass(Parser *parser, U32 *i, U32 parent, Allocator alloc, Error *e_rr);
Bool Parser_classifyStruct(Parser *parser, U32 *i, U32 parent, Allocator alloc, Error *e_rr);
Bool Parser_classifyUnion(Parser *parser, U32 *i, U32 parent, Allocator alloc, Error *e_rr);
Bool Parser_classifyInterface(Parser *parser, U32 *i, U32 parent, Allocator alloc, Error *e_rr);
Bool Parser_classifyEnum(Parser *parser, U32 *i, U32 parent, Allocator alloc, Error *e_rr);

//Classifying type

U16 getC8x2(CharString str, U64 i) {
	return Buffer_readU16(Buffer_createRefConst(str.ptr + i, 2), 0, NULL);
}

U32 getC8x4(CharString str, U64 i) {
	return Buffer_readU32(Buffer_createRefConst(str.ptr + i, 4), 0, NULL);
}

U64 getC8x8(CharString str, U64 i) {
	return Buffer_readU64(Buffer_createRefConst(str.ptr + i, 8), 0, NULL);
}

Bool Parser_classifyType(Parser *parser, U32 *i, U32 parent, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;
	gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))

	Token tok = parser->tokens.ptr[*i];
	CharString tokStr = Token_asString(tok, parser);
	U32 len = tok.tokenSize;

	Bool consumed = false;
	U32 c8x4 = len < 4 ? 0 : getC8x4(tokStr, 0);

	ESymbolFlag modifiers = ESymbolFlag_None;
	ESymbolFlag typeModifiers = ESymbolFlag_None;

	//struct
	//union
	//enum
	//class
	//interface
	//snorm
	//unorm
	//in
	//out
	//inout
	//^

	//Search to next token
	//in/out F32
	//	  ^

	if (len == 2 && getC8x2(tokStr, 0) == C8x2('i', 'n')) {								//in
		modifiers |= ESymbolFlagVar_IsIn;
		++*i;
		gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))
	}

	else if(len == 3 && getC8x2(tokStr, 0) == C8x2('o', 'u') && tokStr.ptr[2] == 't') {	//out
		modifiers |= ESymbolFlagVar_IsOut;
		++*i;
		gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))
	}

	else switch (c8x4) {

		case C8x4('s', 't', 'r', 'u'):		//struct

			if (len == 6 && getC8x2(tokStr, 4) == C8x2('c', 't')) {
				consumed = true;
				gotoIfError3(clean, Parser_classifyStruct(parser, i, parent, alloc, e_rr))
				break;
			}

			break;

		case C8x4('c', 'o', 'n', 's'):		//const

			if (len == 5 && tokStr.ptr[4] == 't') {

				//Search to next token
				//const F32
				//	  ^

				++*i;
				gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))

				modifiers |= ESymbolFlagFuncVar_IsConst;
				break;
			}

			break;

		case C8x4('u', 'n', 'i', 'o'):		//union

			if (len == 5 && tokStr.ptr[4] == 'n') {
				consumed = true;
				gotoIfError3(clean, Parser_classifyUnion(parser, i, parent, alloc, e_rr))
				break;
			}

			break;

		case C8x4('e', 'n', 'u', 'm'):		//enum

			if (len == 4) {
				consumed = true;
				gotoIfError3(clean, Parser_classifyEnum(parser, i, parent, alloc, e_rr))
				break;
			}

			break;

		case C8x4('c', 'l', 'a', 's'):		//class

			if (len == 5 && tokStr.ptr[4] == 's') {
				consumed = true;
				gotoIfError3(clean, Parser_classifyClass(parser, i, parent, alloc, e_rr))
				break;
			}

			break;

		case C8x4('i', 'n', 't', 'e'):		//interface

			if (len == 9 && getC8x4(tokStr, 4) == C8x4('r', 'f', 'a', 'c') && tokStr.ptr[8] == 'e') {
				consumed = true;
				gotoIfError3(clean, Parser_classifyInterface(parser, i, parent, alloc, e_rr))
				break;
			}

			break;

		case C8x4('s', 'n', 'o', 'r'):		//snorm
		case C8x4('u', 'n', 'o', 'r'):		//unorm

			if (len == 5 && tokStr.ptr[4] == 'm') {

				//Search to next token
				//unorm F32
				//	  ^

				++*i;
				gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))

				typeModifiers |= (c8x4 == C8x4('s', 'n', 'o', 'r') ? ESymbolFlagType_IsSnorm : ESymbolFlagType_IsUnorm);
			}

			break;

		case C8x4('i', 'n', 'o', 'u'):		//inout

			if (len == 5 && tokStr.ptr[4] == 't') {

				//Search to next token
				//inout F32
				//	  ^

				++*i;
				gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))

				modifiers |= ESymbolFlagVar_IsIn | ESymbolFlagVar_IsOut;
			}

			break;
	}

	(void) modifiers;
	(void) typeModifiers;

	//T<F32, F32>
	//T
	//^

	if (!consumed) {

		CharString typedeffed = Token_asString(parser->tokens.ptr[*i], parser);
		(void) typedeffed;
		++*i;

		//T<F32, F32>
		// ^

		if (Parser_skipIfNext(parser, i, ETokenType_Lt)) {

			//T<F32, F32>
			//  ^	^

			do {
				gotoIfError3(clean, Parser_classifyType(parser, i, parent, alloc, e_rr))
				gotoIfError3(clean, Parser_assert2(parser, i, ETokenType_Comma, ETokenType_Gt, e_rr))
			}

			//T<F32, F32>
			//	 ^

			while(Parser_skipIfNext(parser, i, ETokenType_Comma));

			//T<F32, F32>
			//		  ^

			gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Gt, e_rr))
		}
	}

clean:
	return s_uccess;
}

//Classifying classes, structs, interfaces and unions

typedef enum EClassType {
	EClassType_Class,
	EClassType_Struct,
	EClassType_Interface,
	EClassType_Union
} EClassType;

Bool Parser_classifyClassBase(Parser *parser, U32 *i, U32 parent, Allocator alloc, EClassType classType, Error *e_rr) {

	ESymbolType symbolType;

	switch (classType) {
		default:					symbolType = ESymbolType_Class;			break;
		case EClassType_Struct:		symbolType = ESymbolType_Struct;		break;
		case EClassType_Interface:	symbolType = ESymbolType_Interface;		break;
		case EClassType_Union:		symbolType = ESymbolType_Union;			break;
	}

	Bool s_uccess = true;

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

	U32 classStart = *i;
	++*i;

	//class	 T
	//struct	T
	//interface T
	//		  ^

	Bool enteredOne = false;
	CharString className = CharString_createNull();

	if (Parser_next(parser, i, ETokenType_Identifier)) {
		className = Token_asString(parser->tokens.ptr[*i], parser);
		++*i;
		enteredOne = true;
	}

	//Add as symbol

	Symbol s = (Symbol) { 0 };
	gotoIfError3(clean, Symbol_create(symbolType, ESymbolFlag_None, classStart, e_rr, &s))

	U32 symbolId = U32_MAX;
	gotoIfError3(clean, Parser_registerSymbol(parser, s, parent, alloc, &symbolId, e_rr))

	if(CharString_length(className))
		gotoIfError3(clean, Parser_setSymbolName(parser, symbolId, &className, alloc, e_rr))

	//class T	 { }
	//class	   { }
	//struct T	{ }
	//struct	  { }
	//interface T { }
	//interface   { }
	//			^

	if (Parser_skipIfNext(parser, i, ETokenType_CurlyBraceStart)) {

		enteredOne = true;

		//class T	 { }
		//class	   { }
		//struct T	{ }
		//struct	  { }
		//interface T { }
		//interface   { }
		//			 ^

		while(!Parser_next(parser, i, ETokenType_CurlyBraceEnd))
			gotoIfError3(clean, Parser_classifyBase(parser, i, symbolId, alloc, e_rr))

		//class T	 { }
		//class	   { }
		//struct T	{ }
		//struct	  { }
		//interface T { }
		//interface   { }
		//			  ^

		gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_CurlyBraceEnd, e_rr))
	}

	U32 tokenCount = *i - classStart;

	if(tokenCount - 1 > U16_MAX)
		retError(clean, Error_outOfBounds(
			0, tokenCount, U16_MAX, "Parser_classifyClassBase() symbol token count is limited to 65536"
		))

	parser->symbols.ptrNonConst[parser->symbolMapping.ptr[symbolId]].tokenCount = (U16)(tokenCount - 1);

	if(!enteredOne)
		retError(clean, Error_invalidState(0, "Parser_classifyClassBase() can't define class without identifier or {}"))

clean:
	return s_uccess;
}

Bool Parser_classifyInitializer(Parser *parser, U32 *i, ETokenType endToken, ETokenType endToken2, Error *e_rr) {

	Bool s_uccess = true;

	//U32 a = 0xDEADBEEF,
	//U32 b = (0x12 << 8) | 0x34,
	//U32 ab{ .test = 123 };
	//		^

	if(Parser_next(parser, i, endToken) || Parser_eof(parser, i))
		retError(clean, Error_invalidParameter(0, 0, "Parser_classifyInitializer() default value expected"))

	typedef enum EBracketType {
		EBracketType_Curly,
		EBracketType_Square,
		EBracketType_Round,
		EBracketType_Count
	} EBracketType;

	U32 tokenCounter = 0;	//U2[32] of EBracketType
	U8 bracketCounter = 0;

	for (; !Parser_eof(parser, i); ++*i) {

		EBracketType type = EBracketType_Count;
		Bool isOpen = true;

		//U32 a = 0xDEADBEEF,
		//				  ^
		//U32 b = (0x12 << 8) | 0x34,
		//						  ^
		//U32 c = 123;
		//		   ^

		if(!bracketCounter && (Parser_next(parser, i, endToken) || Parser_next(parser, i, endToken2)))
			goto clean;

		//U32 b = (0x12 << 8) | 0x34,
		//		^		 ^

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
		//		^		 ^

		if(type != EBracketType_Count) {

			//Close bracket

			if (!isOpen) {

				if(!bracketCounter)
					retError(clean, Error_invalidParameter(
						0, 0, "Parser_classifyInitializer() found one of }]) but had no opening brackets before it"
					))

				EBracketType type2 = (tokenCounter >> ((bracketCounter - 1) << 1)) & 3;

				if(type2 != type)
					retError(clean, Error_invalidParameter(
						0, 0, "Parser_classifyInitializer() found one of mismatching }]) with one of {[("
					))

				--bracketCounter;
			}

			//Open bracket

			else {

				if(bracketCounter >= 32)
					retError(clean, Error_outOfBounds(
						0, 32, 32, "Parser_classifyInitializer() only allows {[( of 32 levels deep, but it was exceeded"
					))

				tokenCounter &= ~((U64)3 << (bracketCounter << 1));
				tokenCounter |= (U64)type << (bracketCounter << 1);

				++bracketCounter;
			}
		}
	}

	retError(clean, Error_invalidParameter(0, 0, "Parser_classifyInitializer() didn't find end of statement"))

clean:
	return s_uccess;
}

Bool Parser_classifyFunction(Parser *parser, U32 *i, U32 parent, Allocator alloc, Error *e_rr) {

	//TODO: Strangely enough, it's valid for a function to be inside of ().
	//		Might have to handle this too? (e.g. void ((((main))))())

	Bool s_uccess = true;

	while (!Parser_next(parser, i, ETokenType_RoundParenthesisEnd)) {

		U32 paramStart = *i;

		//static F32 test(F32 a, F32 b);
		//				^	  ^

		gotoIfError3(clean, Parser_classifyType(parser, i, parent, alloc, e_rr))

		//static F32 test(F32 a, F32 b);
		//					^	  ^

		CharString paramStr = Token_asString(parser->tokens.ptr[*i], parser);
		++*i;

		//Add as symbol
		//TODO: Find out/in

		Symbol s = (Symbol) { 0 };
		gotoIfError3(clean, Symbol_create(ESymbolType_Parameter, ESymbolFlag_None, paramStart, e_rr, &s))

		U32 symbolId = U32_MAX;
		gotoIfError3(clean, Parser_registerSymbol(parser, s, parent, alloc, &symbolId, e_rr))
		gotoIfError3(clean, Parser_setSymbolName(parser, symbolId, &paramStr, alloc, e_rr))

		//static F32 test(F32 a[3], F32 b[3][3]);
		//					 ^		 ^  ^

		while (Parser_skipIfNext(parser, i, ETokenType_SquareBracketStart)) {

			//static F32 test(F32 a[3], F32 b[3][3]);
			//					  ^		 ^  ^

			gotoIfError3(clean, Parser_assert2(parser, i, ETokenType_Integer, ETokenType_Identifier, e_rr))
			//U64 paramArrayCount = parser->tokens.ptr[*i].valueu;
			//paramArrayCount;

			//TODO: Parse array

			++*i;

			//TODO: Store symbol info

			//static F32 test(F32 a[3], F32 b[3][3]);
			//					   ^		 ^  ^

			gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_SquareBracketEnd, e_rr))
		}

		//F32 a : A,
		//	  ^

		if (Parser_skipIfNext(parser, i, ETokenType_Colon)) {

			gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))

			U32 semanticNameTokenId = *i;
			CharString semanticStr = Token_asString(parser->tokens.ptr[semanticNameTokenId], parser);
			++*i;

			//Insert symbol

			Symbol sem = (Symbol) { 0 };
			gotoIfError3(clean, Symbol_create(ESymbolType_Semantic, ESymbolFlag_None, semanticNameTokenId, e_rr, &sem))

			U32 semanticSymbolId = U32_MAX;
			gotoIfError3(clean, Parser_registerSymbol(parser, sem, symbolId, alloc, &semanticSymbolId, e_rr))
			gotoIfError3(clean, Parser_setSymbolName(parser, semanticSymbolId, &semanticStr, alloc, e_rr))
		}

		//U32 a = 0xDEADBEEF,
		//	  ^

		if(Parser_next(parser, i, ETokenType_Asg)) {

			gotoIfError3(
				clean, Parser_classifyInitializer(parser, i, ETokenType_Comma, ETokenType_RoundParenthesisEnd, e_rr)
			)

			//TODO: Store symbol info
		}

		//Resolve token end

		U32 tokenCount = *i - paramStart;

		if(tokenCount - 1 > U16_MAX)
			retError(clean, Error_outOfBounds(
				0, tokenCount, U16_MAX, "Parser_classifyFunction() symbol token count is limited to 65536"
			))

		parser->symbols.ptrNonConst[parser->symbolMapping.ptr[symbolId]].tokenCount = (U16)(tokenCount - 1);

		//void func(F32 a, F32 b)
		//					  ^

		if(Parser_next(parser, i, ETokenType_RoundParenthesisEnd))
			break;

		//F32 a : A,
		//		 ^

		gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Comma, e_rr))
	}

	//static F32 test(F32 a, F32 b);
	//							^

	gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_RoundParenthesisEnd, e_rr))

	//static F32 test(F32 a, F32 b);
	//							 ^

	if(Parser_skipIfNext(parser, i, ETokenType_Semicolon))
		goto clean;

	//F32x4 myMainFunction(): SV_TARGET {
	//					  ^

	if (Parser_skipIfNext(parser, i, ETokenType_Colon)) {

		gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))

		U32 semanticNameTokenId = *i;
		CharString semanticStr = Token_asString(parser->tokens.ptr[semanticNameTokenId], parser);
		++*i;

		//Insert symbol

		Symbol sem = (Symbol) { 0 };
		gotoIfError3(clean, Symbol_create(ESymbolType_Semantic, ESymbolFlag_None, semanticNameTokenId, e_rr, &sem))

		U32 semanticSymbolId = U32_MAX;
		gotoIfError3(clean, Parser_registerSymbol(parser, sem, parent, alloc, &semanticSymbolId, e_rr))
		gotoIfError3(clean, Parser_setSymbolName(parser, semanticSymbolId, &semanticStr, alloc, e_rr))
	}

	//static F32 test(F32 a, F32 b) {
	//							  ^

	gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_CurlyBraceStart, e_rr))

	//static F32 test(F32 a, F32 b) { }
	//							   ^  (Skip everything in body for now)

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
	//								^

	gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_CurlyBraceEnd, e_rr))

clean:
	return s_uccess;
}

Bool Parser_classifyFunctionOrVariable(Parser *parser, U32 *i, U32 parent, Allocator alloc, Error *e_rr) {

	U32 tokenStart = *i;

	//Detect all modifiers

	ESymbolFlag flag = ESymbolFlag_None;
	Bool s_uccess = true, expectsVar = false;

	//if(*scopeAccess == ESymbolAccess_Private)		flag |= ESymbolFlag_IsPrivate;		//TODO:
	//else if(*scopeAccess == ESymbolAccess_Public)	flag |= ESymbolFlag_IsPublic;

	//extern static const F32 x[5], y, z, w;
	//^	  ^	  ^

	//static F32 test(F32 a, F32 b);
	//^

	while (*i < parser->tokens.length) {

		Token tok = parser->tokens.ptr[*i];

		if(tok.tokenType != ETokenType_Identifier)
			break;

		CharString tokStr = Token_asString(tok, parser);
		U32 len = tok.tokenSize;
		U32 c8x4 = len < 4 ? 0 : getC8x4(tokStr, 0);
		Bool consumed = false;

		if (len == 2 && getC8x2(tokStr, 0) == C8x2('i', 'n')) {								//in
			consumed = true;
			flag |= ESymbolFlagVar_IsIn;
			expectsVar = true;
		}

		else if(len == 3 && getC8x2(tokStr, 0) == C8x2('o', 'u') && tokStr.ptr[2] == 't') {	//out
			consumed = true;
			flag |= ESymbolFlagVar_IsOut;
			expectsVar = true;
		}

		else switch (c8x4) {

			case C8x4('i', 'n', 'o', 'u'):		//inout

				if (len == 5 && tokStr.ptr[4] == 't') {
					consumed = true;
					flag |= ESymbolFlagVar_IsIn | ESymbolFlagVar_IsOut;
					expectsVar = true;
				}

				break;

			case C8x4('f', 'l', 'a', 't'):		//flat

				if (len == 4) {
					consumed = true;
					flag |= ESymbolFlagVar_NoInterpolation;
					expectsVar = true;
				}

				break;

			case C8x4('n', 'o', 'i', 'n'):		//nointerpolation

				if(
					len == 15 &&
					getC8x8(tokStr, 4) == C8x8('t', 'e', 'r', 'p', 'o', 'l', 'a', 't') &&
					getC8x2(tokStr, 12) == C8x2('i', 'o') &&
					tokStr.ptr[14] == 'n'
				) {
					consumed = true;
					flag |= ESymbolFlagVar_NoInterpolation;
					expectsVar = true;
				}

				break;

			case C8x4('l', 'i', 'n', 'e'):		//linear

				if (len == 6 && getC8x2(tokStr, 4) == C8x2('a', 'r')) {
					consumed = true;
					flag |= ESymbolFlagVar_Linear;
					expectsVar = true;
				}

				break;

			case C8x4('s', 'm', 'o', 'o'):		//smooth

				if (len == 6 && getC8x2(tokStr, 4) == C8x2('t', 'h')) {
					consumed = true;
					flag |= ESymbolFlagVar_Linear;
					expectsVar = true;
				}

				break;

			case C8x4('s', 'a', 'm', 'p'):		//sample

				if (len == 6 && getC8x2(tokStr, 4) == C8x2('l', 'e')) {
					consumed = true;
					flag |= ESymbolFlagVar_Sample;
					expectsVar = true;
				}

				break;

			case C8x4('c', 'e', 'n', 't'):		//centroid

				if (len == 8 && getC8x4(tokStr, 4) == C8x4('r', 'o', 'i', 'd')) {
					consumed = true;
					flag |= ESymbolFlagVar_Centroid;
					expectsVar = true;
				}

				break;

			case C8x4('n', 'o', 'p', 'e'):		//noperspective

				if(
					len == 13 &&
					getC8x8(tokStr, 4) == C8x8('r', 's', 'p', 'e', 'c', 't', 'i', 'v') &&
					tokStr.ptr[12] == 'e'
				) {
					consumed = true;
					flag |= ESymbolFlagVar_NoPerspective;
					expectsVar = true;
				}

				break;

			case C8x4('c', 'o', 'n', 's'):		//const, constexpr

				if (len == 5 && tokStr.ptr[4] == 't') {
					consumed = true;
					flag |= ESymbolFlagFuncVar_IsConst;
				}

				else if (len == 9 && getC8x4(tokStr, 4) == C8x4('t', 'e', 'x', 'p') && tokStr.ptr[8] == 'r') {
					consumed = true;
					flag |= ESymbolFlagFuncVar_IsConstexpr;
				}

				break;

			case C8x4('p', 'u', 'b', 'l'):		//public

				if (len == 6 && getC8x2(tokStr, 4) == C8x2('i', 'c')) {

					U32 temp = *i + 1;

					if(Parser_next(parser, &temp, ETokenType_Colon)) {
						++*i;
						//*scopeAccess = ESymbolAccess_Public;		//TODO:
					}

					consumed = true;
					flag &= ~ESymbolFlag_IsPrivate;
					flag |= ESymbolFlag_IsPublic;
				}

				break;

			case C8x4('p', 'r', 'i', 'v'):		//private

				if (len == 7 && getC8x2(tokStr, 4) == C8x2('a', 't') && tokStr.ptr[6] == 'e') {

					U32 temp = *i + 1;

					if(Parser_next(parser, &temp, ETokenType_Colon)) {
						++*i;
						//*scopeAccess = ESymbolAccess_Private;		//TODO:
					}

					consumed = true;
					flag &= ~ESymbolFlag_IsPublic;
					flag |= ESymbolFlag_IsPrivate;
				}

				break;

			case C8x4('p', 'r', 'o', 't'):		//protected

				if (len == 9 && getC8x4(tokStr, 4) == C8x4('e', 'c', 't', 'e') && tokStr.ptr[8] == 'd') {

					U32 temp = *i + 1;

					if(Parser_next(parser, &temp, ETokenType_Colon)) {
						++*i;
						//*scopeAccess = ESymbolAccess_Protected;	//TODO:
					}

					consumed = true;
					flag &= ~ESymbolFlag_Access;
				}

				break;

			case C8x4('e', 'x', 't', 'e'):		//extern

				if (len == 6 && getC8x2(tokStr, 4) == C8x2('r', 'n')) {
					consumed = true;
					flag |= ESymbolFlagFuncVar_IsExtern;
				}

				break;

			case C8x4('s', 't', 'a', 't'):		//static

				if (len == 6 && getC8x2(tokStr, 4) == C8x2('i', 'c')) {
					consumed = true;
					flag |= ESymbolFlagFuncVar_IsStatic;
				}

				break;

			case C8x4('i', 'm', 'p', 'l'):		//impl

				if (len == 4) {
					consumed = true;
					flag |= ESymbolFlagFuncVar_HasImpl;
				}

				break;
		}

		if(!consumed)
			break;

		++*i;
	}

	//extern static const F32 x[5], y, z, w;
	//					^

	//static F32 test(F32 a, F32 b);
	//	   ^

	gotoIfError3(clean, Parser_classifyType(parser, i, parent, alloc, e_rr))

	//extern static const F32 x[5], y, z, w;
	//						^

	//static F32 test(F32 a, F32 b);
	//		   ^

	gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))

	//Continue until we find the end

	while (*i < parser->tokens.length) {

		Token tok = parser->tokens.ptr[*i];

		if(tok.tokenType != ETokenType_Identifier)
			break;

		U32 nameTokenId = *i;
		CharString nameStr = Token_asString(tok, parser);
		++*i;

		//F32 operator+(
		//	^

		if(tok.tokenSize == 8 && getC8x8(nameStr, 0) == C8x8('o', 'p', 'e', 'r', 'a', 't', 'o', 'r')) {

			flag |= ESymbolFlagFunc_IsOperator;

			//F32 operator+(
			//			^

			gotoIfError3(clean, Parser_assertSymbol(parser, i, e_rr))
			ETokenType tokType = parser->tokens.ptr[*i].tokenType;
			nameStr = Token_asString(parser->tokens.ptr[*i], parser);
			++*i;

			switch (tokType) {

				default:
					break;

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
					retError(clean, Error_invalidState(
						0,
						"Parser_classifyFunctionOrVariable() operator is disallowed with one of the following: "
						".?]){}:; or [[, ]], ::"
					))
			}

			//F32 operator[](
			//			^

			//TODO: operator[] and operator() are called [ and ( due to quirks

			if (tokType == ETokenType_SquareBracketStart || tokType == ETokenType_RoundParenthesisStart) {

				ETokenType expected =
					tokType == ETokenType_SquareBracketStart ? ETokenType_SquareBracketEnd : ETokenType_RoundParenthesisEnd;

				gotoIfError3(clean, Parser_assertAndSkip(parser, i, expected, e_rr))
			}

			//F32 operator()(
			//			  ^

			gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_RoundParenthesisStart, e_rr))
		}

		//static F32 test(F32 a, F32 b);
		//			   ^

		if (Parser_skipIfNext(parser, i, ETokenType_RoundParenthesisStart)) {

			if(expectsVar)
				retError(clean, Error_invalidState(
					0, "Parser_classifyFunctionOrVariable() expected a variable due to modifiers, but got a function"
				))

			//Insert symbol

			Symbol s = (Symbol) { 0 };
			gotoIfError3(clean, Symbol_create(ESymbolType_Function, flag, tokenStart, e_rr, &s))

			U32 symbolId = U32_MAX;
			gotoIfError3(clean, Parser_registerSymbol(parser, s, parent, alloc, &symbolId, e_rr))

			gotoIfError3(clean, Parser_setSymbolName(parser, symbolId, &nameStr, alloc, e_rr))

			//Parse rest of function

			gotoIfError3(clean, Parser_classifyFunction(parser, i, symbolId, alloc, e_rr))

			//Resolve token end

			U32 tokenCount = *i - tokenStart;

			if(tokenCount - 1 > U16_MAX)
				retError(clean, Error_outOfBounds(
					0, tokenCount, U16_MAX, "Parser_classifyFunctionOrVariable() symbol token count is limited to 65536"
				))

			parser->symbols.ptrNonConst[parser->symbolMapping.ptr[symbolId]].tokenCount = (U16)(tokenCount - 1);
			goto clean;
		}

		//Insert symbol

		Symbol s = (Symbol) { 0 };
		gotoIfError3(clean, Symbol_create(ESymbolType_Variable, flag, nameTokenId, e_rr, &s))

		U32 symbolId = U32_MAX;
		gotoIfError3(clean, Parser_registerSymbol(parser, s, parent, alloc, &symbolId, e_rr))

		gotoIfError3(clean, Parser_setSymbolName(parser, symbolId, &nameStr, alloc, e_rr))

		//TODO: (test)[5] is legal and ((test))[5] is as well and ((test)[5]), etc.

		//extern static const F32 x[5], y[3][3], z, w;
		//						 ^	 ^  ^

		while (Parser_skipIfNext(parser, i, ETokenType_SquareBracketStart)) {

			//extern static const F32 x[5], y[3][3], z, w, a[];
			//											   ^

			if (Parser_next(parser, i, ETokenType_SquareBracketEnd)) {
				++*i;
				break;
			}

			//extern static const F32 x[5], y[3][3], z, w, a[];
			//						  ^	 ^  ^

			gotoIfError3(clean, Parser_assert2(parser, i, ETokenType_Integer, ETokenType_Identifier, e_rr))

			//U64 arrayCount = parser->tokens.ptr[*i].valueu;		//TODO: parse arraySize
			//arrayCount;
			++*i;

			//extern static const F32 x[5], y[3][3], z, w;
			//						   ^	 ^  ^

			gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_SquareBracketEnd, e_rr))
		}

		//F32 a : A;
		//	  ^

		if (Parser_skipIfNext(parser, i, ETokenType_Colon)) {

			gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))

			U32 semanticNameTokenId = *i;
			CharString semanticStr = Token_asString(parser->tokens.ptr[semanticNameTokenId], parser);
			++*i;

			//Insert symbol

			Symbol sem = (Symbol) { 0 };
			gotoIfError3(clean, Symbol_create(ESymbolType_Semantic, ESymbolFlag_None, semanticNameTokenId, e_rr, &sem))

			U32 semanticSymbolId = U32_MAX;
			gotoIfError3(clean, Parser_registerSymbol(parser, sem, symbolId, alloc, &semanticSymbolId, e_rr))
			gotoIfError3(clean, Parser_setSymbolName(parser, semanticSymbolId, &semanticStr, alloc, e_rr))

			//register(t0, space0)
			//		^

			if (Parser_skipIfNext(parser, i, ETokenType_RoundParenthesisStart)) {

				U64 counter = 1;

				//register(t0, space0)
				//		 ^

				for (; *i < parser->tokens.length; ++*i) {

					if(Parser_next(parser, i, ETokenType_RoundParenthesisStart))
						++counter;

					else if(Parser_next(parser, i, ETokenType_RoundParenthesisEnd))
						--counter;

					if(!counter)
						break;
				}

				//register(t0, space0)
				//				   ^

				gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_RoundParenthesisEnd, e_rr))
			}
		}

		//static constexpr U32 a = 0xDEADBEEF;
		//					   ^

		if(Parser_skipIfNext(parser, i, ETokenType_Asg))
			gotoIfError3(clean, Parser_classifyInitializer(parser, i, ETokenType_Semicolon, ETokenType_Count, e_rr))

		//T myParam{ .myTest = 123 };
		//		 ^

		else if (Parser_skipIfNext(parser, i, ETokenType_CurlyBraceStart)) {
			gotoIfError3(clean, Parser_classifyInitializer(parser, i, ETokenType_CurlyBraceEnd, ETokenType_Count, e_rr))
			gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_CurlyBraceEnd, e_rr))
		}

		//Resolve token end

		U32 tokenCount = *i - nameTokenId;

		if(tokenCount - 1 > U16_MAX)
			retError(clean, Error_outOfBounds(
				0, tokenCount, U16_MAX, "Parser_classifyFunctionOrVariable() symbol token count is limited to 65536"
			))

		parser->symbols.ptrNonConst[parser->symbolMapping.ptr[symbolId]].tokenCount = (U16)(tokenCount - 1);

		//extern static const F32 x[5], y[3][3], z, w;
		//							^		^  ^  ^

		//extern static const F32 x[5], y[3][3], z, w;
		//										   ^

		if(Parser_next(parser, i, ETokenType_Semicolon))
			break;

		gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Comma, e_rr))
	}

	gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon, e_rr))

clean:
	return s_uccess;
}

Bool Parser_classifyEnumBody(Parser *parser, U32 *i, U32 parent, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;

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

		U32 enumNameStart = *i;

		gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))
		CharString enumName = Token_asString(parser->tokens.ptr[*i], parser);
		++*i;

		//Insert symbol

		Symbol s = (Symbol) { 0 };
		gotoIfError3(clean, Symbol_create(ESymbolType_EnumValue, ESymbolFlag_None, enumNameStart, e_rr, &s))

		U32 symbolId = U32_MAX;
		gotoIfError3(clean, Parser_registerSymbol(parser, s, parent, alloc, &symbolId, e_rr))

		gotoIfError3(clean, Parser_setSymbolName(parser, symbolId, &enumName, alloc, e_rr))

		//Z,
		// ^

		if(Parser_skipIfNext(parser, i, ETokenType_Comma))
			continue;

		//Z }
		//  ^

		if (Parser_next2(parser, i, ETokenType_Comma, ETokenType_CurlyBraceEnd))
			break;

		//X = (1 << 3),
		//Y = 123,
		//  ^

		gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Asg, e_rr))

		//X = (1 << 3),
		//Y = 123,
		//	^

		U64 bracketCounter = 0;

		for (; *i < parser->tokens.length; ++*i) {

			//X = (1 << 3),
			//	^

			if(Parser_next(parser, i, ETokenType_RoundParenthesisStart))
				++bracketCounter;

			//X = (1 << 3),
			//		   ^

			else if(Parser_next(parser, i, ETokenType_RoundParenthesisEnd)) {

				if(!bracketCounter)
					retError(clean, Error_invalidState(
						0, "Parser_classifyEnumBody() ')' is found, but isn't matched to '('"
					))

				--bracketCounter;
			}

			//X = (1 << 3),
			//Y = 123	 ,
			//			^

			else if(!bracketCounter && Parser_next2(parser, i, ETokenType_Comma, ETokenType_CurlyBraceEnd))
				break;
		}

		//Resolve token end

		U32 tokenCount = *i - enumNameStart;

		if(tokenCount - 1 > U16_MAX)
			retError(clean, Error_outOfBounds(
				0, tokenCount, U16_MAX, "Parser_classifyEnumBody() symbol token count is limited to 65536"
			))

		parser->symbols.ptrNonConst[parser->symbolMapping.ptr[symbolId]].tokenCount = (U16)(tokenCount - 1);

		//Y = 123 };
		//		^

		if(Parser_next(parser, i, ETokenType_CurlyBraceEnd))
			break;

		//X = (1 << 3),
		//Y = 123	 ,
		//			^

		gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Comma, e_rr))
	}

clean:
	return s_uccess;
}

Bool Parser_classifyEnum(Parser *parser, U32 *i, U32 parent, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;

	//enum T
	//enum
	//^

	U32 enumStart = *i;
	++*i;

	//enum T
	//enum class
	//	 ^

	Bool enteredOne = false;
	CharString className = CharString_createNull();

	ESymbolFlag flags = ESymbolFlag_None;

	//CharString enumTypeStr = CharString_createNull();
	U64 enumType = U64_MAX;

	if (Parser_next(parser, i, ETokenType_Identifier)) {

		className = Token_asString(parser->tokens.ptr[*i], parser);
		++*i;

		//enum class
		//	 ^

		if (
			CharString_length(className) == 5 &&
			getC8x4(className, 0) == C8x4('c', 'l', 'a', 's') &&
			className.ptr[4] == 's'
		) {

			flags = ESymbolFlagEnum_IsClass;

			gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))
			className = Token_asString(parser->tokens.ptr[*i], parser);
			++*i;

			//enum class T : U8
			//			 ^

			if (Parser_skipIfNext(parser, i, ETokenType_Colon)) {

				gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))
				//enumTypeStr = Token_asString(parser->tokens.ptr[*i], parser);
				++*i;

				if (Parser_next(parser, i, ETokenType_Lt))
					retError(clean, Error_unimplemented(0, "Parser_classifyEnum() can't handle templates yet"))		//TODO:

				(void)enumType;

				//TODO: fetch enumType
			}
		}

		enteredOne = true;
	}

	//Insert symbol

	Symbol s = (Symbol) { 0 };
	gotoIfError3(clean, Symbol_create(ESymbolType_Enum, flags, enumStart, e_rr, &s))

	U32 symbolId = U32_MAX;
	gotoIfError3(clean, Parser_registerSymbol(parser, s, parent, alloc, &symbolId, e_rr))

	if(CharString_length(className))
		gotoIfError3(clean, Parser_setSymbolName(parser, symbolId, &className, alloc, e_rr))

	//enum T			{ }
	//enum			  { }
	//enum class T	  { }
	//enum class T : U8 { }
	//				  ^

	if (Parser_skipIfNext(parser, i, ETokenType_CurlyBraceStart)) {

		enteredOne = true;

		//enum T			{ }
		//enum			  { }
		//enum class T	  { }
		//enum class T : U8 { }
		//				   ^

		gotoIfError3(clean, Parser_classifyEnumBody(parser, i, symbolId, alloc, e_rr))

		//enum T			{ }
		//enum			  { }
		//enum class T	  { }
		//enum class T : U8 { }
		//					^

		gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_CurlyBraceEnd, e_rr))
	}

	if(!enteredOne)
		retError(clean, Error_invalidState(0, "Parser_classifyEnum() can't define enum without identifier or {}"))

	//Resolve token end

	U32 tokenCount = *i - enumStart;

	if(tokenCount - 1 > U16_MAX)
		retError(clean, Error_outOfBounds(
			0, tokenCount, U16_MAX, "Parser_classifyEnum() symbol token count is limited to 65536"
		))

	parser->symbols.ptrNonConst[parser->symbolMapping.ptr[symbolId]].tokenCount = (U16)(tokenCount - 1);

clean:
	return s_uccess;
}

Bool Parser_classifyClass(Parser *parser, U32 *i, U32 parent, Allocator alloc, Error *e_rr) {
	return Parser_classifyClassBase(parser, i, parent, alloc, EClassType_Class, e_rr);
}

Bool Parser_classifyStruct(Parser *parser, U32 *i, U32 parent, Allocator alloc, Error *e_rr) {
	return Parser_classifyClassBase(parser, i, parent, alloc, EClassType_Struct, e_rr);
}

Bool Parser_classifyUnion(Parser *parser, U32 *i, U32 parent, Allocator alloc, Error *e_rr) {
	return Parser_classifyClassBase(parser, i, parent, alloc, EClassType_Union, e_rr);
}

Bool Parser_classifyInterface(Parser *parser, U32 *i, U32 parent, Allocator alloc, Error *e_rr) {
	return Parser_classifyClassBase(parser, i, parent, alloc, EClassType_Interface, e_rr);
}

Bool Parser_classifyUsing(Parser *parser, U32 *i, U32 parent, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;

	//using T2 = T;
	//^

	U32 usingStart = *i;
	++*i;

	//using T2 = T;
	//	  ^

	gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))
	CharString typeName = Token_asString(parser->tokens.ptr[*i], parser);

	//using T2 = T;
	//		 ^

	gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Asg, e_rr))

	//using T2 = T;
	//		   ^

	gotoIfError3(clean, Parser_classifyType(parser, i, parent, alloc, e_rr))

	U32 usingEnd = *i - 1;

	//Resolve token end

	U32 tokenCount = usingEnd - usingStart;

	if(tokenCount - 1 > U16_MAX)
		retError(clean, Error_outOfBounds(
			0, tokenCount, U16_MAX, "Parser_classifyUsing() symbol token count is limited to 65536"
		))

	//Add as symbol

	Symbol s = (Symbol) { 0 };
	gotoIfError3(clean, Symbol_create(ESymbolType_Typedef, ESymbolFlagTypedef_IsUsing, usingStart, e_rr, &s))

	s.tokenCount = (U16)(tokenCount - 1);

	U32 symbolId = U32_MAX;
	gotoIfError3(clean, Parser_registerSymbol(parser, s, parent, alloc, &symbolId, e_rr))

	gotoIfError3(clean, Parser_setSymbolName(parser, symbolId, &typeName, alloc, e_rr))

clean:
	return s_uccess;
}

Bool Parser_classifyTypedef(Parser *parser, U32 *i, U32 parent, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;

	//typedef T<x, y> T2;
	//^

	U32 typedefStart = *i;
	++*i;

	//typedef T<x, y> T2;
	//		^

	gotoIfError3(clean, Parser_classifyType(parser, i, parent, alloc, e_rr))

	//typedef struct {} T;
	//typedef T2		T;
	//				  ^

	gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))
	CharString typeName = Token_asString(parser->tokens.ptr[*i], parser);
	++*i;

	//typedef struct {} T;
	//				   ^

	U32 typedefEnd = *i;
	gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon, e_rr))

	//Resolve token end

	U32 tokenCount = typedefEnd - typedefStart;

	if(tokenCount - 1 > U16_MAX)
		retError(clean, Error_outOfBounds(
			0, tokenCount, U16_MAX, "Parser_classifyTypedef() symbol token count is limited to 65536"
		))

	//Add as symbol

	Symbol s = (Symbol) { 0 };
	gotoIfError3(clean, Symbol_create(ESymbolType_Typedef, ESymbolFlag_None, typedefStart, e_rr, &s))

	s.tokenCount = (U16)(tokenCount - 1);

	U32 symbolId = U32_MAX;
	gotoIfError3(clean, Parser_registerSymbol(parser, s, parent, alloc, &symbolId, e_rr))

	gotoIfError3(clean, Parser_setSymbolName(parser, symbolId, &typeName, alloc, e_rr))

clean:
	return s_uccess;
}

Bool Parser_classifyTemplate(Parser *parser, U32 *i, U32 parent, Allocator alloc, Error *e_rr) {

	(void) alloc;

	Bool s_uccess = true;

	U32 templateStart = *i;

	//template<>
	//^

	++*i;

	//template<>
	//		^

	gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Lt, e_rr))

	//template< >
	//		 ^

	U32 counter = 1;

	while (counter && *i < parser->tokens.length) {

		if(Parser_next(parser, i, ETokenType_Lt))
			++counter;

		else if(Parser_next(parser, i, ETokenType_Gt))
			--counter;

		if(counter)
			++*i;
	}

	//template< >
	//		  ^

	gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Gt, e_rr))

	U32 templateEnd = *i;

	//Resolve token end

	U32 tokenCount = templateEnd - templateStart;

	if(tokenCount - 1 > U16_MAX)
		retError(clean, Error_outOfBounds(
			0, tokenCount, U16_MAX, "Parser_classifyTemplate() symbol token count is limited to 65536"
		))

	//Add as symbol

	Symbol s = (Symbol) { 0 };
	gotoIfError3(clean, Symbol_create(ESymbolType_Template, ESymbolFlag_None, templateStart, e_rr, &s))

	s.tokenCount = (U16)(tokenCount - 1);

	U32 symbolId = U32_MAX;
	gotoIfError3(clean, Parser_registerSymbol(parser, s, parent, alloc, &symbolId, e_rr))

clean:
	return s_uccess;
}

Bool Parser_classifyBase(Parser *parser, U32 *i, U32 parent, Allocator alloc, Error *e_rr) {

	Token tok = parser->tokens.ptr[*i];
	CharString tokStr = Token_asString(tok, parser);
	Bool consumed = false;
	Bool s_uccess = true;

	//Most expressions

	if (tok.tokenType == ETokenType_Identifier) {

		U32 len = tok.tokenSize;
		U32 c8x4 = len < 4 ? 0 : getC8x4(tokStr, 0);

		switch (c8x4) {

			case C8x4('t', 'e', 'm', 'p'):		//template

				if (len == 8 && getC8x4(tokStr, 4) == C8x4('l', 'a', 't', 'e')) {
					consumed = true;
					gotoIfError3(clean, Parser_classifyTemplate(parser, i, parent, alloc, e_rr))
					gotoIfError3(clean, Parser_classifyFunctionOrVariable(parser, i, parent, alloc, e_rr))
					break;
				}

				break;

			case C8x4('t', 'y', 'p', 'e'):		//typedef

				if (len == 7 && getC8x2(tokStr, 4) == C8x2('d', 'e') && tokStr.ptr[6] == 'f') {
					consumed = true;
					gotoIfError3(clean, Parser_classifyTypedef(parser, i, parent, alloc, e_rr))
					break;
				}

				break;

			case C8x4('u', 's', 'i' ,'n'):		//using

				if (len == 5 && tokStr.ptr[4] == 'g') {
					consumed = true;
					gotoIfError3(clean, Parser_classifyUsing(parser, i, parent, alloc, e_rr))
					gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon, e_rr))
					break;
				}

				break;

			case C8x4('c', 'l', 'a', 's'):		//class

				if (len == 5 && tokStr.ptr[4] == 's') {
					consumed = true;
					gotoIfError3(clean, Parser_classifyClass(parser, i, parent, alloc, e_rr))
					gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon, e_rr))
					break;
				}

				break;

			case C8x4('s', 't', 'r', 'u'):		//struct

				if (len == 6 && getC8x2(tokStr, 4) == C8x2('c', 't')) {
					consumed = true;
					gotoIfError3(clean, Parser_classifyStruct(parser, i, parent, alloc, e_rr))
					gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon, e_rr))
					break;
				}

				break;

			case C8x4('u', 'n', 'i', 'o'):		//union

				if (len == 5 && tokStr.ptr[4] == 'n') {
					consumed = true;
					gotoIfError3(clean, Parser_classifyUnion(parser, i, parent, alloc, e_rr))
					gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon, e_rr))
					break;
				}

				break;

			case C8x4('e', 'n', 'u', 'm'):		//enum

				if (len == 4) {
					consumed = true;
					gotoIfError3(clean, Parser_classifyEnum(parser, i, parent, alloc, e_rr))
					gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon, e_rr))
					break;
				}

				break;

			case C8x4('i', 'n', 't', 'e'):		//interface

				if (len == 9 && getC8x4(tokStr, 4) == C8x4('r', 'f', 'a', 'c') && tokStr.ptr[8] == 'e') {
					consumed = true;
					gotoIfError3(clean, Parser_classifyInterface(parser, i, parent, alloc, e_rr))
					gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon, e_rr))
					break;
				}

				break;

			case C8x4('n', 'a', 'm', 'e'):		//namespace

				if (len == 9 && getC8x4(tokStr, 4) == C8x4('s', 'p', 'a', 'c') && tokStr.ptr[8] == 'e') {

					consumed = true;

					//namespace T
					//^

					U32 symbolStart = *i;
					++*i;

					//namespace T
					//		  ^

					gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))
					CharString name = Token_asString(parser->tokens.ptr[*i], parser);

					//Create symbol for namespace

					Symbol s = (Symbol) { 0 };
					gotoIfError3(clean, Symbol_create(ESymbolType_Namespace, ESymbolFlag_None, symbolStart, e_rr, &s))

					U32 symbolId = U32_MAX;
					gotoIfError3(clean, Parser_registerSymbol(parser, s, U32_MAX, alloc, &symbolId, e_rr))

					gotoIfError3(clean, Parser_setSymbolName(parser, symbolId, &name, alloc, e_rr))

					//namespace T { }
					//			^

					gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_CurlyBraceStart, e_rr))

					//namespace T { }
					//			 ^

					gotoIfError3(clean, Parser_classifyBase(parser, i, symbolId, alloc, e_rr))

					//namespace T { }
					//			  ^

					U32 symbolEnd = *i;
					gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_CurlyBraceEnd, e_rr))

					//Find token end

					U32 tokenCount = symbolEnd - symbolStart;

					if(tokenCount - 1 > U16_MAX)
						retError(clean, Error_outOfBounds(
							0, tokenCount, U16_MAX, "Parser_classifyBase() namespace token count is limited to 65536"
						))

					//Update symbol end

					parser->symbols.ptrNonConst[parser->symbolMapping.ptr[symbolId]].tokenCount = (U16)(tokenCount - 1);
					break;
				}

				break;

			default:
				break;
		}

		//If it's not a reserved keyword, then it's probably a variable or function
		//Or it's an expression, which we won't handle here.

		if(!consumed)
			gotoIfError3(clean, Parser_classifyFunctionOrVariable(parser, i, parent, alloc, e_rr))
	}

	//Annotations

	else {

		//[shader("test")]
		//^

		if (Parser_skipIfNext(parser, i, ETokenType_SquareBracketStart)) {

			//[shader("test")]
			// ^

			U32 symbolStart = *i;

			U32 counter = 1;

			for (; *i < parser->tokens.length; ++*i) {

				switch (parser->tokens.ptr[*i].tokenType) {
					case ETokenType_SquareBracketStart:	++counter;	break;
					case ETokenType_SquareBracketEnd:	--counter;	break;
				}

				if(!counter)
					break;
			}

			//[shader("test")]
			//			   ^

			U32 symbolEnd = *i;

			gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_SquareBracketEnd, e_rr))

			//Find token end

			U32 tokenCount = symbolEnd - symbolStart;

			if(tokenCount - 1 > U16_MAX)
				retError(clean, Error_outOfBounds(
					0, tokenCount, U16_MAX, "Parser_classifyBase() annotation [] token count is limited to 65536"
				))

			//Add symbol

			Symbol s = (Symbol) { 0 };
			gotoIfError3(clean, Symbol_create(ESymbolType_Annotation, ESymbolFlag_None, symbolStart, e_rr, &s))

			s.tokenCount = (U16)(tokenCount - 1);

			U32 symbolId = U32_MAX;
			gotoIfError3(clean, Parser_registerSymbol(parser, s, parent, alloc, &symbolId, e_rr))
		}

		//[[vk::binding(1, 0)]]
		//^

		else if (Parser_skipIfNext(parser, i, ETokenType_SquareBracketStart2)) {

			U32 symbolStart = *i;

			//[[vk::binding(1, 0)]]
			//  ^

			for (; *i < parser->tokens.length; ++*i)
				if (Parser_next(parser, i, ETokenType_SquareBracketEnd2))
					break;

			//[[vk::binding(1, 0)]]
			//				   ^

			U32 symbolEnd = *i;

			gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_SquareBracketEnd2, e_rr))

			//Find token end

			U32 tokenCount = symbolEnd - symbolStart;

			if(tokenCount - 1 > U16_MAX)
				retError(clean, Error_outOfBounds(
					0, tokenCount, U16_MAX, "Parser_classifyBase() annotation [[]] token count is limited to 65536"
				))

			//Add symbol

			Symbol s = (Symbol) { 0 };
			gotoIfError3(clean, Symbol_create(
				ESymbolType_Annotation, ESymbolFlagAnnotation_IsDoubleBracket, symbolStart, e_rr, &s
			))

			s.tokenCount = (U16)(tokenCount - 1);

			U32 symbolId = U32_MAX;
			gotoIfError3(clean, Parser_registerSymbol(parser, s, parent, alloc, &symbolId, e_rr))
		}

		//; is always allowed as an empty expression

		else if(Parser_skipIfNext(parser, i, ETokenType_Semicolon))
			;

		//Unrecognized

		else retError(clean, Error_invalidParameter(0, 0, "Parser_classify() couldn't classify element"))
	}

clean:
	return s_uccess;
}

Bool Parser_classify(Parser *parser, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (!parser || !parser->lexer)
		retError(clean, Error_nullPointer(0, "Parser_classify()::parser is required"))

	if((parser->tokens.length + 1) >> 32)
		retError(clean, Error_outOfBounds(
			0, parser->tokens.length, U32_MAX, "Parser_classify()::parser->tokens out of bounds"
		))

	//Classify tokens

	gotoIfError2(clean, ListSymbol_reserve(&parser->symbols, 64, alloc))
	gotoIfError2(clean, ListU32_reserve(&parser->symbolMapping, 64, alloc))
	gotoIfError2(clean, ListCharString_reserve(&parser->symbolNames, 64, alloc))

	U32 i = 0;

	for (; i < (U32) parser->tokens.length; )
		gotoIfError3(clean, Parser_classifyBase(parser, &i, U32_MAX, alloc, e_rr))

clean:

	if(!s_uccess) {
		parser->symbols.length = 0;
		parser->symbolMapping.length = 0;
		parser->symbolNames.length = 0;
		parser->rootSymbols = 0;
	}

	return s_uccess;
}

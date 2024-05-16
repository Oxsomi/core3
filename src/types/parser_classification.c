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

//Helpers for classification

Bool Parser_assert(Parser *parser, U64 *i, ETokenType type, Error *e_rr) {

	Bool s_uccess = true;

	if (*i >= parser->tokens.length)
		retError(clean, Error_outOfBounds(1, *i, parser->tokens.length, "Parser_assert() out of bounds"))

	if (parser->tokens.ptr[*i].tokenType != type)
		retError(clean, Error_invalidParameter(1, 0, "Parser_assert() unexpected token"))

clean:
	return s_uccess;
}

Bool Parser_assert2(
	Parser *parser, U64 *i, ETokenType type1, ETokenType type2, Error *e_rr
) {

	Bool s_uccess = true;

	if (*i >= parser->tokens.length)
		retError(clean, Error_outOfBounds(1, *i, parser->tokens.length, "Parser_assert2() out of bounds"))

	ETokenType type = parser->tokens.ptr[*i].tokenType;

	if (type != type1 && type != type2)
		retError(clean, Error_invalidParameter(1, 0, "Parser_assert2() unexpected token"))

clean:
	return s_uccess;
}

Bool Parser_assertSymbol(Parser *parser, U64 *i, Error *e_rr) {

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

Bool Parser_assertAndSkip(Parser *parser, U64 *i, ETokenType type, Error *e_rr) {

	Bool s_uccess = true;
	gotoIfError3(clean, Parser_assert(parser, i, type, e_rr))

	++*i;

clean:
	return s_uccess;
}

Bool Parser_assertAndSkip2(Parser *parser, U64 *i, ETokenType type1, ETokenType type2, Error *e_rr) {

	Bool s_uccess = true;
	gotoIfError3(clean, Parser_assert2(parser, i, type1, type2, e_rr))

	++*i;

clean:
	return s_uccess;
}

Bool Parser_next(Parser *parser, U64 *i, ETokenType type) {
	return *i < parser->tokens.length && parser->tokens.ptr[*i].tokenType == type;
}

Bool Parser_next2(Parser *parser, U64 *i, ETokenType type1, ETokenType type2) {					//If next is type1 or type2
	return *i < parser->tokens.length && (
		parser->tokens.ptr[*i].tokenType == type1 || parser->tokens.ptr[*i].tokenType == type2
	);
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

Bool Parser_classifyBase(Parser *parser, U64 *i, Allocator alloc, Error *e_rr);

Bool Parser_classifyClass(Parser *parser, U64 *i, Allocator alloc, U64 *classId, Error *e_rr);
Bool Parser_classifyStruct(Parser *parser, U64 *i, Allocator alloc, U64 *structId, Error *e_rr);
Bool Parser_classifyUnion(Parser *parser, U64 *i, Allocator alloc, U64 *unionId, Error *e_rr);
Bool Parser_classifyInterface(Parser *parser, U64 *i, Allocator alloc, U64 *interfaceId, Error *e_rr);
Bool Parser_classifyEnum(Parser *parser, U64 *i, Allocator alloc, U64 *enumId, Error *e_rr);

//Classifying type

Bool Parser_classifyType(Parser *parser, U64 *i, Allocator alloc, U64 *typeId, Error *e_rr) {

	Bool s_uccess = true;
	gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))

		Token tok = parser->tokens.ptr[*i];
	CharString tokStr = Token_asString(tok, parser);
	U64 len = tok.tokenSize;

	Bool consumed = false;
	U32 c8x4 = len < 4 ? 0 : *(const U32*)tokStr.ptr;

	ESymbolFlag modifiers = ESymbolFlag_None;

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

	*typeId = U64_MAX;

	//Search to next token
	//in/out F32
	//      ^

	if (len == 2 && *(const U16*)tokStr.ptr == C8x2('i', 'n')) {								//in
		modifiers |= ESymbolFlag_IsIn;
		++*i;
		gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))
	}

	else if(len == 3 && *(const U16*)tokStr.ptr == C8x2('o', 'u') && tokStr.ptr[2] == 't') {	//out
		modifiers |= ESymbolFlag_IsOut;
		++*i;
		gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))
	}

	else switch (c8x4) {

	case C8x4('s', 't', 'r', 'u'):		//struct

		if (len == 6 && *(const U16*)&tokStr.ptr[4] == C8x2('c', 't')) {
			consumed = true;
			gotoIfError3(clean, Parser_classifyStruct(parser, i, alloc, typeId, e_rr))
				break;
		}

		break;

	case C8x4('u', 'n', 'i', 'o'):		//union

		if (len == 5 && tokStr.ptr[4] == 'n') {
			consumed = true;
			gotoIfError3(clean, Parser_classifyUnion(parser, i, alloc, typeId, e_rr))
				break;
		}

		break;

	case C8x4('e', 'n', 'u', 'm'):		//enum

		if (len == 4) {
			consumed = true;
			gotoIfError3(clean, Parser_classifyEnum(parser, i, alloc, typeId, e_rr))
				break;
		}

		break;

	case C8x4('c', 'l', 'a', 's'):		//class

		if (len == 5 && tokStr.ptr[4] == 's') {
			consumed = true;
			gotoIfError3(clean, Parser_classifyClass(parser, i, alloc, typeId, e_rr))
				break;
		}

		break;

	case C8x4('i', 'n', 't', 'e'):		//interface

		if (len == 9 && *(const U32*)&tokStr.ptr[4] == C8x4('r', 'f', 'a', 'c') && tokStr.ptr[8] == 'e') {
			consumed = true;
			gotoIfError3(clean, Parser_classifyInterface(parser, i, alloc, typeId, e_rr))
				break;
		}

		break;

	case C8x4('s', 'n', 'o', 'r'):		//snorm
	case C8x4('u', 'n', 'o', 'r'):		//unorm

		if (len == 5 && tokStr.ptr[4] == 'm') {

			//Search to next token
			//unorm F32
			//      ^

			++*i;
			gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))

				modifiers |= (c8x4 == C8x4('s', 'n', 'o', 'r') ? ESymbolFlag_IsSnorm : ESymbolFlag_IsUnorm);
		}

		break;

	case C8x4('i', 'n', 'o', 'u'):		//inout

		if (len == 5 && tokStr.ptr[4] == 't') {

			//Search to next token
			//inout F32
			//      ^

			++*i;
			gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))

				modifiers |= ESymbolFlag_IsIn | ESymbolFlag_IsOut;
		}

		break;
	}

	//T<F32, F32>
	//T
	//^

	if (!consumed) {

		CharString typedeffed = Token_asString(parser->tokens.ptr[*i], parser);
		typedeffed;
		++*i;

		//T<F32, F32>
		// ^

		if (Parser_skipIfNext(parser, i, ETokenType_Lt)) {

			//T<F32, F32>
			//  ^    ^

			do {

				U64 templateTypeId = 0;
				gotoIfError3(clean, Parser_classifyType(parser, i, alloc, &templateTypeId, e_rr))

					gotoIfError3(clean, Parser_assert2(parser, i, ETokenType_Comma, ETokenType_Gt, e_rr))
			}

			//T<F32, F32>
			//     ^

			while(Parser_skipIfNext(parser, i, ETokenType_Comma));

				//T<F32, F32>
				//          ^

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

Bool Parser_classifyClassBase(Parser *parser, U64 *i, Allocator alloc, U64 *classId, EClassType classType, Error *e_rr) {

	(void)classType; (void)classId;

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

	Log_debugLn(alloc, "Type name: %.*s", (int)CharString_length(className), className.ptr);		//Dbg

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
			gotoIfError3(clean, Parser_classifyBase(parser, i, alloc, e_rr))

			//class T     { }
			//class       { }
			//struct T    { }
			//struct      { }
			//interface T { }
			//interface   { }
			//              ^

			gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_CurlyBraceEnd, e_rr))
	}

	if(!enteredOne)
		retError(clean, Error_invalidState(0, "Parser_classifyClassBase() can't define class without identifier or {}"))

		clean:
	return s_uccess;
}

Bool Parser_classifyInitializer(Parser *parser, U64 *i, ETokenType endToken, ETokenType endToken2, Error *e_rr) {

	Bool s_uccess = true;

	//U32 a = 0xDEADBEEF,
	//U32 b = (0x12 << 8) | 0x34,
	//U32 ab{ .test = 123 };
	//        ^

	if(Parser_next(parser, i, endToken) || Parser_eof(parser, i))
		retError(clean, Error_invalidParameter(0, 0, "Parser_classifyInitializer() default value expected"), e_rr)

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
					retError(clean, Error_invalidParameter(
						0, 0, "Parser_classifyInitializer() found one of }]) but had no opening brackets before it"
					), e_rr)

					EBracketType type2 = (tokenCounter >> ((bracketCounter - 1) << 1)) & 3;

				if(type2 != type)
					retError(clean, Error_invalidParameter(
						0, 0, "Parser_classifyInitializer() found one of mismatching }]) with one of {[("
					), e_rr)

					--bracketCounter;
			}

			//Open bracket

			else {

				if(bracketCounter >= 32)
					retError(clean, Error_outOfBounds(
						0, 32, 32, "Parser_classifyInitializer() only allows {[( of 32 levels deep, but it was exceeded"
					), e_rr)

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

Bool Parser_classifyFunction(Parser *parser, U64 *i, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;

	while (!Parser_next(parser, i, ETokenType_RoundParenthesisEnd)) {

		//static F32 test(F32 a, F32 b);
		//                ^      ^

		U64 typeId;
		gotoIfError3(clean, Parser_classifyType(parser, i, alloc, &typeId, e_rr))

			//static F32 test(F32 a, F32 b);
			//                    ^      ^

			CharString paramStr = Token_asString(parser->tokens.ptr[*i], parser);
		++*i;

		Log_debugLn(alloc, "Param name: %.*s", (int)CharString_length(paramStr), paramStr.ptr);		//Dbg

		//static F32 test(F32 a[3], F32 b[3][3]);
		//                     ^         ^  ^

		while (Parser_skipIfNext(parser, i, ETokenType_SquareBracketStart)) {

			//static F32 test(F32 a[3], F32 b[3][3]);
			//                      ^         ^  ^

			gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Integer, e_rr))
				U64 paramArrayCount = parser->tokens.ptr[*i].valueu;
			paramArrayCount;
			++*i;

			//static F32 test(F32 a[3], F32 b[3][3]);
			//                       ^         ^  ^

			gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_SquareBracketEnd, e_rr))
		}

		//F32 a : A,
		//      ^

		if (Parser_skipIfNext(parser, i, ETokenType_Colon)) {

			gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))

				CharString variableSemanticStr = Token_asString(parser->tokens.ptr[*i], parser);
			++*i;

			variableSemanticStr;
		}

		//U32 a = 0xDEADBEEF,
		//      ^

		if(Parser_next(parser, i, ETokenType_Asg))
			gotoIfError3(clean, Parser_classifyInitializer(parser, i, ETokenType_Comma, ETokenType_RoundParenthesisEnd, e_rr))

			if(Parser_next(parser, i, ETokenType_RoundParenthesisEnd))
				break;

		//F32 a : A,
		//         ^

		gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Comma, e_rr))
	}

	//static F32 test(F32 a, F32 b);
	//                            ^

	gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_RoundParenthesisEnd, e_rr))

		//static F32 test(F32 a, F32 b);
		//                             ^

		if(Parser_skipIfNext(parser, i, ETokenType_Semicolon))
			goto clean;

	//F32x4 myMainFunction(): SV_TARGET {
	//                      ^

	if (Parser_skipIfNext(parser, i, ETokenType_Colon)) {

		gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))
			CharString semantic = Token_asString(parser->tokens.ptr[*i], parser);
		++*i;

		Log_debugLn(alloc, "Semantic name: %.*s", (int)CharString_length(semantic), semantic.ptr);		//Dbg
	}

	//static F32 test(F32 a, F32 b) {
	//                              ^

	gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_CurlyBraceStart, e_rr))

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

	gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_CurlyBraceEnd, e_rr))

		clean:
	return s_uccess;
}

Bool Parser_classifyFunctionOrVariable(Parser *parser, U64 *i, Allocator alloc, ESymbolAccess *scopeAccess, Error *e_rr) {

	//Detect all modifiers

	ESymbolFlag flag = ESymbolFlag_None;
	Bool s_uccess = true;

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
	gotoIfError3(clean, Parser_classifyType(parser, i, alloc, &typeId, e_rr))

		//extern static const F32 x[5], y, z, w;
		//                        ^

		//static F32 test(F32 a, F32 b);
		//           ^

		gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))

		//Continue until we find the end

		while (*i < parser->tokens.length) {

			Token tok = parser->tokens.ptr[*i];

			if(tok.tokenType != ETokenType_Identifier)
				break;

			CharString nameStr = Token_asString(tok, parser);
			++*i;

			//F32 operator+(
			//    ^

			if(tok.tokenSize == 8 && *(const U64*)nameStr.ptr == C8x8('o', 'p', 'e', 'r', 'a', 't', 'o', 'r')) {

				flag |= ESymbolFlag_IsOperator;

				//F32 operator+(
				//            ^

				gotoIfError3(clean, Parser_assertSymbol(parser, i, e_rr))
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
					retError(clean, Error_invalidState(
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

					gotoIfError3(clean, Parser_assertAndSkip(parser, i, expected, e_rr))
				}

				//F32 operator()(
				//              ^

				gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_RoundParenthesisStart, e_rr))
			}

			//static F32 test(F32 a, F32 b);
			//               ^

			if (Parser_skipIfNext(parser, i, ETokenType_RoundParenthesisStart)) {
				Log_debugLn(alloc, "Function name: %.*s", (int)CharString_length(nameStr), nameStr.ptr);		//Dbg
				gotoIfError3(clean, Parser_classifyFunction(parser, i, alloc, e_rr))
					goto clean;
			}

			Log_debugLn(alloc, "Variable name: %.*s", (int)CharString_length(nameStr), nameStr.ptr);		//Dbg

			//TODO: (test)[5] is legal and ((test))[5] is as well and ((test)[5]), etc.

			//extern static const F32 x[5], y[3][3], z, w;
			//                         ^     ^  ^

			while (Parser_skipIfNext(parser, i, ETokenType_SquareBracketStart)) {

				//extern static const F32 x[5], y[3][3], z, w, a[];
				//                                               ^

				if (Parser_next(parser, i, ETokenType_SquareBracketEnd)) {
					++*i;
					break;
				}

				//extern static const F32 x[5], y[3][3], z, w, a[];
				//                          ^     ^  ^

				gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Integer, e_rr))

					U64 arrayCount = parser->tokens.ptr[*i].valueu;
				arrayCount;
				++*i;

				Log_debugLn(alloc, "Array: %"PRIu64, arrayCount);		//Dbg

				//extern static const F32 x[5], y[3][3], z, w;
				//                           ^     ^  ^

				gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_SquareBracketEnd, e_rr))
			}

			//F32 a : A;
			//      ^

			if (Parser_skipIfNext(parser, i, ETokenType_Colon)) {

				gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))

					CharString semanticStr = Token_asString(parser->tokens.ptr[*i], parser);
				++*i;

				Log_debugLn(alloc, "Semantic name: %.*s", (int)CharString_length(semanticStr), semanticStr.ptr);		//Dbg

				//register(t0, space0)
				//        ^

				if (Parser_skipIfNext(parser, i, ETokenType_RoundParenthesisStart)) {

					U64 counter = 1;

					//register(t0, space0)
					//         ^

					for (; *i < parser->tokens.length; ++*i) {

						if(Parser_next(parser, i, ETokenType_RoundParenthesisStart))
							++counter;

						else if(Parser_next(parser, i, ETokenType_RoundParenthesisEnd))
							--counter;

						if(!counter)
							break;
					}

					//register(t0, space0)
					//                   ^

					gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_RoundParenthesisEnd, e_rr))
				}
			}

			//static constexpr U32 a = 0xDEADBEEF;
			//                       ^

			if(Parser_skipIfNext(parser, i, ETokenType_Asg))
				gotoIfError3(clean, Parser_classifyInitializer(parser, i, ETokenType_Semicolon, ETokenType_Count, e_rr))

				//T myParam{ .myTest = 123 };
				//         ^

			else if (Parser_skipIfNext(parser, i, ETokenType_CurlyBraceStart)) {
				gotoIfError3(clean, Parser_classifyInitializer(parser, i, ETokenType_CurlyBraceEnd, ETokenType_Count, e_rr))
					gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_CurlyBraceEnd, e_rr))
			}

			//extern static const F32 x[5], y[3][3], z, w;
			//                            ^        ^  ^  ^

			//extern static const F32 x[5], y[3][3], z, w;
			//                                           ^

			if(Parser_next(parser, i, ETokenType_Semicolon))
				break;

			gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Comma, e_rr))
		}

	gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon, e_rr))

		clean:
	return s_uccess;
}

Bool Parser_classifyEnumBody(Parser *parser, U64 *i, Allocator alloc, Error *e_rr) {

	(void) alloc;

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

		gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))
			CharString enumName = Token_asString(parser->tokens.ptr[*i], parser);
		++*i;

		Log_debugLn(alloc, "Enum name: %.*s", (int)CharString_length(enumName), enumName.ptr);		//Dbg

		//Z,
		// ^

		if(Parser_skipIfNext(parser, i, ETokenType_Comma))
			continue;

		//X = (1 << 3),
		//Y = 123,
		//  ^

		gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Asg, e_rr))

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
					retError(clean, Error_invalidState(
						0, "Parser_classifyEnumBody() ')' is found, but isn't matched to '('"
					))

					--bracketCounter;
			}

			//X = (1 << 3),
			//Y = 123     ,
			//            ^

			else if(!bracketCounter && Parser_next2(parser, i, ETokenType_Comma, ETokenType_CurlyBraceEnd))
				break;
		}

		//Y = 123 };
		//        ^

		if(Parser_next(parser, i, ETokenType_CurlyBraceEnd))
			break;

		//X = (1 << 3),
		//Y = 123     ,
		//            ^

		gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Comma, e_rr))
	}

clean:
	return s_uccess;
}

Bool Parser_classifyEnum(Parser *parser, U64 *i, Allocator alloc, U64 *enumId, Error *e_rr) {

	(void)enumId;

	Bool s_uccess = true;

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

			gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))
				className = Token_asString(parser->tokens.ptr[*i], parser);
			++*i;

			//enum class T : U8
			//             ^

			if (Parser_skipIfNext(parser, i, ETokenType_Colon)) {

				gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))
					enumTypeStr = Token_asString(parser->tokens.ptr[*i], parser);
				++*i;

				if (Parser_next(parser, i, ETokenType_Lt))
					retError(clean, Error_unimplemented(0, "Parser_classifyEnum() can't handle templates yet"))		//TODO:

					(void)enumType;

				//TODO: fetch enumType
			}
		}

		enteredOne = true;
	}

	Log_debugLn(alloc, "Enum: %.*s", (int)CharString_length(className), className.ptr);		//Dbg

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

		gotoIfError3(clean, Parser_classifyEnumBody(parser, i, alloc, e_rr))

			//enum T            { }
			//enum              { }
			//enum class T      { }
			//enum class T : U8 { }
			//                    ^

			gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_CurlyBraceEnd, e_rr))
	}

	if(!enteredOne)
		retError(clean, Error_invalidState(0, "Parser_classifyEnum() can't define enum without identifier or {}"))

		clean:
	return s_uccess;
}

Bool Parser_classifyClass(Parser *parser, U64 *i, Allocator alloc, U64 *classId, Error *e_rr) {
	return Parser_classifyClassBase(parser, i, alloc, classId, EClassType_Class, e_rr);
}

Bool Parser_classifyStruct(Parser *parser, U64 *i, Allocator alloc, U64 *structId, Error *e_rr) {
	return Parser_classifyClassBase(parser, i, alloc, structId, EClassType_Struct, e_rr);
}

Bool Parser_classifyUnion(Parser *parser, U64 *i, Allocator alloc, U64 *unionId, Error *e_rr) {
	return Parser_classifyClassBase(parser, i, alloc, unionId, EClassType_Union, e_rr);
}

Bool Parser_classifyInterface(Parser *parser, U64 *i, Allocator alloc, U64 *interfaceId, Error *e_rr) {
	return Parser_classifyClassBase(parser, i, alloc, interfaceId, EClassType_Interface, e_rr);
}

Bool Parser_classifyUsing(Parser *parser, U64 *i, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;

	//using T2 = T;
	//^

	++*i;

	//using T2 = T;
	//      ^

	gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))
		CharString typeName = Token_asString(parser->tokens.ptr[*i], parser);

	Log_debugLn(alloc, "Using: %.*s", (int)CharString_length(typeName), typeName.ptr);		//Dbg

	//using T2 = T;
	//         ^

	gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Asg, e_rr))

		//using T2 = T;
		//           ^

		U64 typeId;
	gotoIfError3(clean, Parser_classifyType(parser, i, alloc, &typeId, e_rr))

		//TODO: Insert symbol here

	clean:
	return s_uccess;
}

Bool Parser_classifyTypedef(Parser *parser, U64 *i, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;

	//typedef T<x, y> T2;
	//^

	++*i;

	//typedef T<x, y> T2;
	//        ^

	U64 typeId;
	gotoIfError3(clean, Parser_classifyType(parser, i, alloc, &typeId, e_rr))

		//typedef struct {} T;
		//typedef T2        T;
		//                  ^

		gotoIfError3(clean, Parser_assert(parser, i, ETokenType_Identifier, e_rr))
		CharString typeName = Token_asString(parser->tokens.ptr[*i], parser);
	++*i;

	Log_debugLn(alloc, "Typedef: %.*s", (int)CharString_length(typeName), typeName.ptr);	//Dbg

	//typedef struct {} T;
	//                   ^

	gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon, e_rr))

		//TODO: Insert symbol here

	clean:
	return s_uccess;
}

Bool Parser_classifyTemplate(Parser *parser, U64 *i, Allocator alloc, Error *e_rr) {

	(void) alloc;

	Bool s_uccess = true;

	//template<>
	//^

	++*i;

	//template<>
	//        ^

	gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Lt, e_rr))

		//template< >
		//         ^

		U64 counter = 1;

	while (counter && *i < parser->tokens.length) {

		if(Parser_next(parser, i, ETokenType_Lt))
			++counter;

		else if(Parser_next(parser, i, ETokenType_Gt))
			--counter;

		if(counter)
			++*i;
	}

	//template< >
	//          ^

	gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Gt, e_rr))

		clean:
	return s_uccess;
}

Bool Parser_classifyBase(Parser *parser, U64 *i, Allocator alloc, Error *e_rr) {

	Token tok = parser->tokens.ptr[*i];
	CharString tokStr = Token_asString(tok, parser);
	Bool consumed = false;
	Bool s_uccess = true;

	//Most expressions

	if (tok.tokenType == ETokenType_Identifier) {

		U64 len = tok.tokenSize;
		U32 c8x4 = len < 4 ? 0 : *(const U32*)tokStr.ptr;

		switch (c8x4) {

		case C8x4('t', 'e', 'm', 'p'):		//template

			if (len == 8 && *(const U32*)&tokStr.ptr[4] == C8x4('l', 'a', 't', 'e')) {

				ESymbolAccess symbolAccess = ESymbolAccess_Protected;		//TODO:

				consumed = true;
				gotoIfError3(clean, Parser_classifyTemplate(parser, i, alloc, e_rr))
					gotoIfError3(clean, Parser_classifyFunctionOrVariable(parser, i, alloc, &symbolAccess, e_rr))
					break;
			}

			break;

		case C8x4('t', 'y', 'p', 'e'):		//typedef

			if (len == 7 && *(const U16*)&tokStr.ptr[4] == C8x2('d', 'e') && tokStr.ptr[6] == 'f') {
				consumed = true;
				gotoIfError3(clean, Parser_classifyTypedef(parser, i, alloc, e_rr))
					break;
			}

			break;

		case C8x4('u', 's', 'i' ,'n'):		//using

			if (len == 5 && tokStr.ptr[4] == 'g') {
				consumed = true;
				gotoIfError3(clean, Parser_classifyUsing(parser, i, alloc, e_rr))
					gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon, e_rr))
					break;
			}

			break;

		case C8x4('c', 'l', 'a', 's'):		//class

			if (len == 5 && tokStr.ptr[4] == 's') {
				consumed = true;
				gotoIfError3(clean, Parser_classifyClass(parser, i, alloc, NULL, e_rr))
					gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon, e_rr))
					break;
			}

			break;

		case C8x4('s', 't', 'r', 'u'):		//struct

			if (len == 6 && *(const U16*)&tokStr.ptr[4] == C8x2('c', 't')) {
				consumed = true;
				gotoIfError3(clean, Parser_classifyStruct(parser, i, alloc, NULL, e_rr))
					gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon, e_rr))
					break;
			}

			break;

		case C8x4('u', 'n', 'i', 'o'):		//union

			if (len == 5 && tokStr.ptr[4] == 'n') {
				consumed = true;
				gotoIfError3(clean, Parser_classifyUnion(parser, i, alloc, NULL, e_rr))
					gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon, e_rr))
					break;
			}

			break;

		case C8x4('e', 'n', 'u', 'm'):		//enum

			if (len == 4) {
				consumed = true;
				gotoIfError3(clean, Parser_classifyEnum(parser, i, alloc, NULL, e_rr))
					gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon, e_rr))
					break;
			}

			break;

		case C8x4('i', 'n', 't', 'e'):		//interface

			if (len == 9 && *(const U32*)&tokStr.ptr[4] == C8x4('r', 'f', 'a', 'c') && tokStr.ptr[8] == 'e') {
				consumed = true;
				gotoIfError3(clean, Parser_classifyInterface(parser, i, alloc, NULL, e_rr))
					gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_Semicolon, e_rr))
					break;
			}

			break;

		case C8x4('n', 'a', 'm', 'e'):		//namespace

			if (len == 9 && *(const U32*)&tokStr.ptr[4] == C8x4('s', 'p', 'a', 'c') && tokStr.ptr[8] == 'e') {

				consumed = true;

				++*i;
				gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_CurlyBraceStart, e_rr))
					gotoIfError3(clean, Parser_classifyBase(parser, i, alloc, e_rr))
					gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_CurlyBraceEnd, e_rr))
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
			gotoIfError3(clean, Parser_classifyFunctionOrVariable(parser, i, alloc, &symbolAccess, e_rr))
		}
	}

	//Annotations

	else {

		//[shader("test")]
		//^

		if (Parser_skipIfNext(parser, i, ETokenType_SquareBracketStart)) {

			U64 startIdDbg = *i;

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

			gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_SquareBracketEnd, e_rr))

				const C8 *startDbg = Token_asString(parser->tokens.ptr[startIdDbg], parser).ptr;
			const C8 *endDbg = Token_asString(parser->tokens.ptr[*i - 1], parser).ptr;

			Log_debugLn(alloc, "Annotation: %.*s", (int) (endDbg - startDbg), startDbg);
		}

		//[[vk::binding(1, 0)]]
		//^

		else if (Parser_skipIfNext(parser, i, ETokenType_SquareBracketStart2)) {

			U64 startIdDbg = *i;

			//[[vk::binding(1, 0)]]
			//  ^

			for (; *i < parser->tokens.length; ++*i)
				if (Parser_next(parser, i, ETokenType_SquareBracketEnd2))
					break;

			//[[vk::binding(1, 0)]]
			//                   ^

			gotoIfError3(clean, Parser_assertAndSkip(parser, i, ETokenType_SquareBracketEnd2, e_rr))

				const C8 *startDbg = Token_asString(parser->tokens.ptr[startIdDbg], parser).ptr;
			const C8 *endDbg = Token_asString(parser->tokens.ptr[*i - 1], parser).ptr;

			Log_debugLn(alloc, "Annotation: %.*s", (int) (endDbg - startDbg), startDbg);
		}

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

		//Classify tokens

		gotoIfError2(clean, ListSymbol_reserve(&parser->symbols, 64, alloc))

		U64 i = 0;

	for (; i < parser->tokens.length; )
		gotoIfError3(clean, Parser_classifyBase(parser, &i, alloc, e_rr))

		clean:
	return s_uccess;
}

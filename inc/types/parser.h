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

#pragma once
#include "types/list.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct CharString CharString;
typedef struct Lexer Lexer;

//A token is a resolved LexerToken (because a LexerToken can contain multiple real tokens for example)

typedef enum ETokenType {

	ETokenType_Identifier,				// [A-Za-z_$]+[0-9A-Za-z_$]*
	ETokenType_Integer,					// [+]?[0-9]+
	ETokenType_SignedInteger,			// -[0-9]+
	ETokenType_Double,					// Approximately equal to: [-+]?[0-9]*[.[0-9]*]?[[eE][-+]?[0-9]+]?
	ETokenType_Float,					// Approximately equal to: [-+]?[0-9]*[.[0-9]*]?[[eE][-+]?[0-9]+]?[fF]
	ETokenType_String,					// "[0-9A-Za-z_\s$]*"

	ETokenType_Inc,						// ++
	ETokenType_Dec,						// --

	ETokenType_Not,						// ~ (bitwise)
	ETokenType_BooleanNot,				// !

	ETokenType_Mul,						// *
	ETokenType_Div,						// /
	ETokenType_Mod,						// %
	ETokenType_Add,						// +
	ETokenType_Sub,						// -

	ETokenType_MulAsg,					// *=
	ETokenType_DivAsg,					// /=
	ETokenType_ModAsg,					// %=
	ETokenType_AddAsg,					// +=
	ETokenType_SubAsg,					// -=

	ETokenType_BooleanAnd,				// &&
	ETokenType_BooleanOr,				// ||

	ETokenType_And,						// &
	ETokenType_Or,						// |
	ETokenType_Xor,						// ^

	ETokenType_AndAsg,					// &=
	ETokenType_OrAsg,					// |=
	ETokenType_XorAsg,					// ^=

	ETokenType_Lsh,						// <<
	ETokenType_Rsh,						// >>

	ETokenType_LshAsg,					// <<=
	ETokenType_RshAsg,					// >>=

	ETokenType_Lt,						// <
	ETokenType_Gt,						// >
	ETokenType_Leq,						// <=
	ETokenType_Geq,						// >=

	ETokenType_Asg,						// =
	ETokenType_Eq,						// ==
	ETokenType_Neq,						// !=

	ETokenType_RoundParenthesisStart,	// (
	ETokenType_RoundParenthesisEnd,		// )

	ETokenType_SquareBracketStart,		// [
	ETokenType_SquareBracketEnd,		// ]

	ETokenType_CurlyBraceStart,			// {
	ETokenType_CurlyBraceEnd,			// }

	ETokenType_Colon,					// :
	ETokenType_Colon2,					// ::
	ETokenType_Semicolon,				// ;

	ETokenType_Comma,					// ,
	ETokenType_Period,					// .

	ETokenType_Ternary,					// ?

	ETokenType_SquareBracketStart2,		// [[
	ETokenType_SquareBracketEnd2,		// ]]

	ETokenType_Arrow,					// ->
	ETokenType_Flagship,				// <=>

	ETokenType_Count

} ETokenType;

typedef struct Token {

	union {
		I64 valuei;		//If token type is signed int
		U64 valueu;		//If token type is int or string (if string; index of string literal)
		F64 valuef;		//If token type is double
	};

	U32 naiveTokenId;

	U16 tokenType;			//ETokenType
	U8 lexerTokenSubId;		//For example *=- is split into *= (subId=0) and - (subId=2)
	U8 tokenSize;

} Token;

typedef struct Parser Parser;

CharString Token_asString(Token t, const Parser *p);

//A symbol is a function, constant or typedef.

typedef enum ESymbolType {

	ESymbolType_Function,
	ESymbolType_Constant,
	ESymbolType_Variable,
	ESymbolType_Typedef,		//typedef T T2
	ESymbolType_Using,			//using T2 = T
	ESymbolType_Struct,			//typedef struct ... or struct X {}
	ESymbolType_Class,			//class X {}
	ESymbolType_Enum,			//typedef enum ... or enum X {}
	ESymbolType_EnumClass,		//enum class X : Y {} or enum class X {}
	ESymbolType_Interface,		//interface X {}
	ESymbolType_Annotation,		//[[vk::binding(0, 0)]] or [shader("vertex")] for example
	ESymbolType_Count			//Keep <=32 due to packing

} ESymbolType;

typedef enum ESymbolFlag {

	ESymbolFlag_None				= 0,

	//For constants

	ESymbolFlag_IsConst				= 1 << 0,		//const ...
	ESymbolFlag_IsConstexpr			= 1 << 1,		//constexpr X
	ESymbolFlag_IsExtern			= 1 << 2,		//extern ...

	//For functions

	ESymbolFlag_HasImpl				= 1 << 3,		//impl ...
	ESymbolFlag_HasUserImpl			= 1 << 4,		//user_impl ...

	//Access

	ESymbolFlag_IsStatic			= 1 << 5,		//static ...

	//Maps to ESymbolAccess, if !(Private | Public) then it's protected

	ESymbolFlag_IsPrivate			= 1 << 6,		//private: X or private X
	ESymbolFlag_IsPublic			= 1 << 7,		//public: X or public X

	ESymbolFlag_Access				= ESymbolFlag_IsPrivate | ESymbolFlag_IsPublic,

	//Function modifiers

	ESymbolFlag_IsOperator			= 1 << 8,

	//HLSL/GLSL modifiers

	ESymbolFlag_IsUnorm				= 1 << 9,
	ESymbolFlag_IsSnorm				= 1 << 10,

	ESymbolFlag_Sample				= 1 << 11,
	ESymbolFlag_NoInterpolation		= 1 << 12,		//flat in GLSL
	ESymbolFlag_NoPerspective		= 1 << 13,
	ESymbolFlag_Centroid			= 1 << 14,
	ESymbolFlag_Linear				= 1 << 15,		//smooth in GLSL

	ESymbolFlag_IsOut				= 1 << 16,		//out or inout
	ESymbolFlag_IsIn				= 1 << 17,		//in or inout (in is not automatically set, though it is implied if !out)

	ESymbolFlag_Count				= 18,			//Keep less than 19 due to packing

} ESymbolFlag;

typedef enum ESymbolAccess {
	ESymbolAccess_Protected,
	ESymbolAccess_Private,
	ESymbolAccess_Public,
	ESymbolAccess_Count
} ESymbolAccess;

typedef struct Symbol {

	U32 symbolFlagTypeAnnotations;

	U32 name;			//If first bit is set, is a resolved name. Otherwise tokenId

	U32 baseType;		//SymbolId to base type

	U32 parent;			//See SymbolId

	U32 localIdChildren;

} Symbol;

Bool Symbol_create(
	ESymbolType type, ESymbolFlag flags, U8 annotations,
	U32 name,
	U32 baseType,
	U32 parent,
	U32 localId,		//U20
	U16 children,		//U10
	Error *e_rr,
	Symbol *symbol
);

U32 Symbol_getLocalId(Symbol s);
U32 Symbol_getChildren(Symbol s);

ESymbolType Symbol_getType(Symbol s);
ESymbolFlag Symbol_getFlags(Symbol s);
U8 Symbol_getAnnotations(Symbol s);

//Parser takes the output from the lexer and splits it up in real tokens and handles preprocessor-specific things.
//After the parser, the file's symbols can be obtained.

TList(Token);
TList(Symbol);

typedef struct Parser {
	const Lexer *lexer;
	ListToken tokens;
	ListSymbol symbols;
	ListCharString parsedLiterals;		//Need copies, because \" and \\ have to be parsed
} Parser;

Bool Parser_create(const Lexer *lexer, Parser *parser, Allocator alloc, Error *e_rr);
void Parser_free(Parser *parser, Allocator alloc);
void Parser_printTokens(Parser parser, Allocator alloc);
void Parser_printSymbols(Parser parser, Allocator alloc);

//The classification step can be done manually if the syntax is different than normal C++-like syntax.
//C/C++, C#, HLSL and GLSL all share similar syntax, which means we can re-use this a lot.

Bool Parser_classify(Parser *parser, Allocator alloc, Error *e_rr);

//Classify a token at str.ptr[*subTokenOffset]
//Increment subTokenOffset by the length of the token

ETokenType Parser_getTokenType(CharString str, U64 *subTokenOffset);

#ifdef __cplusplus
	}
#endif

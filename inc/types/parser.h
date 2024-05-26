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
	ESymbolType_Namespace,
	ESymbolType_Template,
	ESymbolType_Function,
	ESymbolType_Variable,
	ESymbolType_Parameter,
	ESymbolType_Typedef,		//typedef T T2 or using T2 = T
	ESymbolType_Struct,			//typedef struct ... or struct X {}
	ESymbolType_Class,			//typedef class ... or class X {}
	ESymbolType_Interface,		//typedef interface ... or interface X {}
	ESymbolType_Union,			//typedef union ... or union X {}
	ESymbolType_Enum,			//typedef enum ... or enum X {} or enum class
	ESymbolType_EnumValue,
	ESymbolType_Annotation,		//[[vk::binding(0, 0)]] or [shader("vertex")] for example
	ESymbolType_Count			//Keep <=256 due to packing
} ESymbolType;

extern const C8 *ESymbolType_names[ESymbolType_Count];

typedef enum ESymbolFlag {

	ESymbolFlag_None							= 0,

	//For ESymbolType_Function, Object or Variable

	ESymbolFlag_HasTemplate						= 1 << 0,		//Consume previous template<> symbol

	//If not Typedef
	//Maps to ESymbolAccess, if !(Private | Public) then it's protected

	ESymbolFlag_IsPrivate						= 1 << 1,		//private: X or private X
	ESymbolFlag_IsPublic						= 1 << 2,		//public: X or public X

	ESymbolFlag_Access							= ESymbolFlag_IsPrivate | ESymbolFlag_IsPublic,

	//For ESymbolType_Function or Variable

	ESymbolFlagFuncVar_IsConst					= 1 << 3,		//const ...
	ESymbolFlagFuncVar_IsConstexpr				= 1 << 4,		//constexpr X

	ESymbolFlagFuncVar_HasImpl					= 1 << 5,		//impl ...
	ESymbolFlagFuncVar_HasUserImpl				= 1 << 6,		//user_impl ...
	ESymbolFlagFuncVar_IsStatic					= 1 << 7,		//static ...
	ESymbolFlagFuncVar_IsExtern					= 1 << 8,		//extern ...

	//Function modifiers

	ESymbolFlagFunc_IsOperator					= 1 << 9,

	//Enum modifiers

	ESymbolFlagEnum_IsClass						= 1 << 10,

	//Annotation modifiers

	ESymbolFlagAnnotation_IsDoubleBracket		= 1 << 11,

	//For both annotation and templates when it's been consumed into a function, variable, class, etc.

	ESymbolFlag_IsParented	= 1 << 12,			//When an variable, annotation or template has been "consumed"

	//Typedef modifiers

	ESymbolFlagTypedef_IsUsing					= 1 << 13,

	//HLSL/GLSL modifiers

	//If ESymbolType_Type

	ESymbolFlagType_IsUnorm						= 1 << 14,
	ESymbolFlagType_IsSnorm						= 1 << 15,

	//If ESymbolType_Variable

	ESymbolFlagVar_IsOut						= 1 << 16,		//out or inout
	ESymbolFlagVar_IsIn							= 1 << 17,		//in or inout (in not automatically set, though implied if !out)

	ESymbolFlagVar_Sample						= 1 << 18,
	ESymbolFlagVar_NoInterpolation				= 1 << 19,		//flat in GLSL
	ESymbolFlagVar_NoPerspective				= 1 << 20,
	ESymbolFlagVar_Centroid						= 1 << 21,
	ESymbolFlagVar_Linear						= 1 << 22,		//smooth in GLSL

	ESymbolFlag_Count							= 23			//Keep less than 32

} ESymbolFlag;

typedef enum ESymbolAccess {
	ESymbolAccess_Protected,
	ESymbolAccess_Private,
	ESymbolAccess_Public,
	ESymbolAccess_Count
} ESymbolAccess;

typedef struct Symbol {

	U32 parent, child;
	U32 name;

	ESymbolFlag flags;

	U16 tokenCount;			//Count to last token (tokenCount + 1 = length of tokens that represent the symbol)
	U8 symbolType;			//ESymbolType
	U8 annotations;			//How many annotations are after this symbol

	U32 childCount;

	U32 tokenId;			//Where the symbol is defined

	U32 symbolId;			//Mapping back to the real id in the symbolMappings

} Symbol;

Bool Symbol_create(
	ESymbolType type,
	ESymbolFlag flags,
	U32 tokenId,
	Error *e_rr,
	Symbol *symbol
);

//How long the symbol is including decorators.
//Templates and annotations are located after the Symbol, so this tells how many of those have to be skipped.
U32 Symbol_size(Symbol s);

//Parser takes the output from the lexer and splits it up in real tokens and handles preprocessor-specific things.
//After the parser, the file's symbols can be obtained.

TList(Token);
TList(Symbol);

typedef struct Parser {

	const Lexer *lexer;
	ListToken tokens;

	ListCharString parsedLiterals;		//Need copies, because \" and \\ have to be parsed

	ListCharString symbolNames;

	ListSymbol symbols;
	ListU32 symbolMapping;				//Maps global id to local id to allow moving around
	U32 rootSymbols;					//Root symbols that are located at the start of symbols.ptr

} Parser;

Bool Parser_create(const Lexer *lexer, Parser *parser, Allocator alloc, Error *e_rr);
void Parser_free(Parser *parser, Allocator alloc);
void Parser_printTokens(Parser parser, Allocator alloc);
void Parser_printSymbols(Parser parser, U32 parent, Bool recursive, Allocator alloc);

//Registering a symbol with parent U32_MAX indicates it's a root symbol.
//A root symbol doesn't belong to any other symbol (e.g. it's in the "global namespace").

Bool Parser_registerSymbol(Parser *parser, Symbol s, U32 parent, Allocator alloc, U32 *symbolId, Error *e_rr);

//Moves name, unless it's a ref. Make sure the ref is still valid throughout the lifetime of parser.
Bool Parser_setSymbolName(Parser *parser, U32 symbolId, CharString *name, Allocator alloc, Error *e_rr);

//The classification step can be done manually if the syntax is different than normal C++-like syntax.
//C/C++, C#, HLSL and GLSL all share similar syntax, which means we can re-use this a lot.

Bool Parser_classify(Parser *parser, Allocator alloc, Error *e_rr);

//Classify a token at str.ptr[*subTokenOffset]
//Increment subTokenOffset by the length of the token

ETokenType Parser_getTokenType(CharString str, U64 *subTokenOffset);

#ifdef __cplusplus
	}
#endif

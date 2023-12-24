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

#pragma once
#include "types/list.h"

typedef struct CharString CharString;
typedef struct Lexer Lexer;

//A token is a resolved LexerToken (because a LexerToken can contain multiple real tokens for example)

typedef enum ETokenType {

	ETokenType_Identifier,				// [A-Za-z_$]+[0-9A-Za-z_$]*
	ETokenType_Integer,					// [-+]?[0-9]+
	ETokenType_Double,					// Approximately equal to: [-+]?[0-9]*[.[0-9]*]?[[eE][-+]?[0-9]+]?
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
	ETokenType_Semicolon,				// ;

	ETokenType_Comma,					// ,

	ETokenType_Count

} ETokenType;

typedef struct Token {

	I64 value;				//If tokenType = int this represents the int value, if token type is double (*(F64*)&value)

	U32 naiveTokenId;

	U16 tokenType;			//ETokenType
	U8 lexerTokenSubId;		//For example *=- is split into *= (subId=0) and - (subId=2)
	U8 padding;

} Token;

//A symbol is a function, constant or typedef.

typedef enum ESymbolType {

	ESymbolType_Function,
	ESymbolType_Constant,
	ESymbolType_Typedef,	//typedef T T2
	ESymbolType_Struct,		//typedef struct ...
	ESymbolType_Enum,		//typedef enum ...
	ESymbolType_Count

} ESymbolType;

typedef enum ESymbolFlag {

	//For constants

	ESymbolFlag_IsConst			= 1 << 0,		//const
	ESymbolFlag_IsPtr			= 1 << 1,		//T*
	ESymbolFlag_IsPtrRef		= 1 << 2,		//T**
	ESymbolFlag_IsExtern		= 1 << 3,		//extern ...

	//For functions

	ESymbolFlag_HasImpl			= 1 << 4,		//impl ...
	ESymbolFlag_HasUserImpl		= 1 << 5		//user_impl ...

} ESymbolFlag;

typedef struct Symbol {

	U16 symbolFlag;			//ESymbolFlag
	U8 symbolType;			//ESymbolType
	U8 children;			//For example enum values, struct members or function parameters

	U32 nameTokenId;

	U32 typeTokenId;		//U32_MAX indicates void (only for enum and 

	U32 childStartTokenId;	//See Symbol::children

	U32 bodyStartTokenId;	//U32_MAX for enum & struct & typedef

} Symbol;

//A define can have four types; (value or macro)(empty, full). Macro is like a function that generates code.
//The list of defines at the end of parsing the file will be stored; also including the defines of the includes (if present).
//Standard includes get excluded from the define list, only libraries will have this property.

typedef enum EDefineType {

	EDefineType_ValueEmpty,		//The define will always evaluate to nothing, but it's present (#define X)
	EDefineType_MacroEmpty,		//The macro will always evaluate to nothing, but it's present (#define X(y))

	EDefineType_ValueFull,		//#define MY_VALUE 1
	EDefineType_MacroFull,		//#define MY_VALUE(y) 1

	EDefineType_Count,

	EDefineType_IsMacro		= 1 << 0,
	EDefineType_IsFull		= 1 << 1

} EDefineType;

typedef struct Define {

	U8 defineType;			//EDefineType
	U8 children;			//For macros defines 
	U16 valueTokens;		//How many tokens the define's value contains

	U32 nameTokenId;		//Lexer token id

	U32 childStartTokenId;	//Lexer token id of the child start (skip a token in between; that's ,)

	U32 valueStartId;		//Lexer token id

} Define;

//Parser takes the output from the lexer and splits it up in real tokens and handles preprocessor-specific things.
//After the parser, the file's symbols can be obtained.

typedef struct Parser {
	const Lexer *lexer;
	List tokens;			//<Token>
	List symbols;			//<Symbol>
	List defines;			//<Define>
} Parser;

typedef struct UserDefine {		//Value can be empty/null to indicate present (but no value)
	CharString name, value;
} UserDefine;

Error Parser_create(const Lexer *lexer, Parser *parser, List/*<UserDefine>*/ userDefine);
Bool Parser_free(Parser *parser);

//Classify a token at str.ptr[*subTokenOffset]
//Increment subTokenOffset by the length of the token

ETokenType Parser_getTokenType(CharString str, U64 *subTokenOffset);

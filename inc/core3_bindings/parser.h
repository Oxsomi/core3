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

//An expression is a bunch of NaiveTokens that are grouped together.
//If the type is generic then one token can actually be multiple due 
// and the tokenizer will decide what it represents.
//For example: abc*def will be seen as 3 tokens but abc*=+1 is also seen as 3 tokens.
//The tokenizer will then split *=+ up into *= and + from NaiveToken to Token.

typedef enum EExpressionType {

	EExpressionType_None,
	EExpressionType_Preprocessor,
	EExpressionType_Comment,
	EExpressionType_MultiLineComment,
	EExpressionType_Generic

} EExpressionType;

typedef struct Expression {

	U16 type, tokenCount;
	U32 tokenOffset;

} Expression;

//A NaiveToken is defined as any subsequent 

typedef enum ENaiveTokenType {
	ENaiveTokenType_Identifier,		//[A-Za-z_$]+[0-9A-Za-z_$]*
	ENaiveTokenType_Integer,		//[0-9]
	ENaiveTokenType_Float,			//Approximately equal to: [-+]?[0-9]*[.[0-9]*]?[[eE][-+]?[0-9]+]?
	ENaiveTokenType_Symbols,		//Any number of subsequent symbols
	ENaiveTokenType_Count
} ENaiveTokenType;

typedef struct NaiveToken {

	U16 lineId;
	U8 charId, length;

	U32 offsetType;			//upper 2 bits is type

} NaiveToken;

const C8 *NaiveToken_getTokenStart(NaiveToken tok, CharString c);
const C8 *NaiveToken_getTokenEnd(NaiveToken tok, CharString c);

typedef struct Parser {
	List tokens;			//<NaiveToken>
	List expressions;		//<Expression>
} Parser;

Error Parser_create(CharString source, Parser *parser);
Bool Parser_free(Parser *parser);

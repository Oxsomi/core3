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

//A lexer expression is a bunch of LexerTokens that are grouped together.
//If the type is generic then one token can actually be multiple and the parser will decide what it represents.
//For example: abc*def will be seen as 3 tokens but abc*=+1 is also seen as 3 tokens.
//The tokenizer will then split *=+ up into *= and + from LexerToken to ParsedToken.

typedef enum ELexerExpressionType {

	ELexerExpressionType_None,
	ELexerExpressionType_Preprocessor,
	ELexerExpressionType_Comment,
	ELexerExpressionType_MultiLineComment,
	ELexerExpressionType_Generic

} ELexerExpressionType;

typedef struct LexerExpression {

	U16 type, tokenCount;
	U32 tokenOffset;

} LexerExpression;

typedef struct Lexer Lexer;

CharString LexerExpression_asString(LexerExpression e, Lexer p);

//A LexerToken is defined as any subsequent characters that have a similar type.

typedef enum ELexerTokenType {
	ELexerTokenType_Identifier,		//[A-Za-z_$]+[0-9A-Za-z_$]*
	ELexerTokenType_Integer,		//[0-9]
	ELexerTokenType_Double,			//Approximately equal to: [0-9]*[.[0-9]*]?[[eE][-+]?[0-9]+]?
	ELexerTokenType_Symbols,		//Any number of subsequent symbols
	ELexerTokenType_Count
} ELexerTokenType;

typedef struct LexerToken {

	U16 lineId;
	U8 charId, length;

	U32 offsetType;			//upper 2 bits is type

} LexerToken;

const C8 *LexerToken_getTokenStart(LexerToken tok, Lexer p);
const C8 *LexerToken_getTokenEnd(LexerToken tok, Lexer p);
CharString LexerToken_asString(LexerToken tok, Lexer p);

typedef struct Lexer {
	CharString source;		//Ref to source (source needs to be kept active)
	List tokens;			//<LexerToken>
	List expressions;		//<LexerExpression>
} Lexer;

Error Lexer_create(CharString source, Lexer *lexer);
Bool Lexer_free(Lexer *lexer);

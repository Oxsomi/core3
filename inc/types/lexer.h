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
#include "types/string.h"

#ifdef __cplusplus
	extern "C" {
#endif

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
	ELexerTokenType_Symbols,		//Any number of subsequent symbols

	ELexerTokenType_Float,			//Approximately equal to: [0-9]*[.[0-9]*]?[[eE][-+]?[0-9]+]?f
	ELexerTokenType_Double,			//Approximately equal to: [0-9]*[.[0-9]*]?[[eE][-+]?[0-9]+]?

	ELexerTokenType_IntegerDec,		//[-+][1-9]+[0-9]*
	ELexerTokenType_IntegerHex,		//0[xX][0-9A-Fa-f]+
	ELexerTokenType_IntegerOctal,	//0[0-7]+ or 0[oO][0-7]+
	ELexerTokenType_IntegerBinary,	//0[bB][0-1]+
	ELexerTokenType_IntegerNyto,	//0[nN][0-9A-Za-z$_]+

	ELexerTokenType_String,			//"anything" \" to escape quote and \\ to escape backslash

	ELexerTokenType_Count,

	ELexerTokenType_NumberStart		= ELexerTokenType_Float,
	ELexerTokenType_NumberEnd		= ELexerTokenType_IntegerNyto,

	ELexerTokenType_IntBegin		= ELexerTokenType_IntegerDec,
	ELexerTokenType_IntEnd			= ELexerTokenType_IntegerNyto

} ELexerTokenType;

Bool ELexerTokenType_isNumber(ELexerTokenType tokenType);

typedef struct LexerToken {

	U16 lineId;						//Upper bits of realLineIdAndLineIdExt stores extended lineId just in case
	U8 typeId, length;

	U32 offset;

	U32 charId;	

	U16 fileId;						//Index into sourceLocations representing file location. U16_MAX for none
	U16 realLineIdAndLineIdExt;		//Line id in the source location, rather than the line id in the preprocessed file

} LexerToken;

U32 LexerToken_getLineId(LexerToken tok);					//Line id goes up to 128Ki, because it's on preprocessed input
U32 LexerToken_getOriginalLineId(LexerToken tok);			//Original line id only goes up to 8k

const C8 *LexerToken_getTokenStart(LexerToken tok, Lexer p);
const C8 *LexerToken_getTokenEnd(LexerToken tok, Lexer p);
CharString LexerToken_asString(LexerToken tok, Lexer p);

TList(LexerToken);
TList(LexerExpression);

typedef struct Lexer {
	CharString source;							//Ref to source (source needs to be kept active)
	ListLexerToken tokens;
	ListLexerExpression expressions;
	ListCharString sourceLocations;				//Used by tokens to reference their real location
} Lexer;

Bool Lexer_create(CharString source, Allocator alloc, Lexer *lexer, Error *e_rr);
void Lexer_free(Lexer *lexer, Allocator alloc);

void Lexer_print(Lexer lexer, Allocator alloc);

#ifdef __cplusplus
	}
#endif

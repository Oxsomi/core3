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
		ELexerTokenType lextType = lext.typeId;

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

				if(CharString_endsWithInsensitive(lextStr, 'u', 0))
					--lextStr.lenAndNullTerminated;

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

			default: {

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
	ListCharString_freeUnderlying(&parser->symbolNames, alloc);

	ListU32_free(&parser->symbolMapping, alloc);
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

const C8 *ESymbolType_names[ESymbolType_Count] = {
	"Namespace",
	"Template",
	"Function",
	"Variable",
	"Parameter",
	"Typedef",
	"Struct",
	"Class",
	"Interface",
	"Union",
	"Enum",
	"EnumValue",
	"Annotation",
	"Semantic"
};

Bool Parser_printSymbol(
	Parser parser,
	Symbol sym,
	U32 recursion,
	Allocator alloc,
	CharString *output,
	CharString outputSymbolFile,
	Error *e_rr
) {

	Bool s_uccess = true;
	const C8 *symbolType = ESymbolType_names[sym.symbolType];

	CharString name = sym.name == U32_MAX ? CharString_createRefCStrConst("(unnamed)") : parser.symbolNames.ptr[sym.name];

	CharString tmp = CharString_createNull();
	CharString tmp2 = CharString_createNull();
	CharString tmp3 = CharString_createNull();
	gotoIfError2(clean, CharString_create('\t', recursion, alloc, &tmp))

	Token tok = parser.tokens.ptr[sym.tokenId];
	LexerToken ltok = parser.lexer->tokens.ptr[tok.naiveTokenId];

	if(sym.symbolType == ESymbolType_Annotation || sym.symbolType == ESymbolType_Template) {

		Token endTok = parser.tokens.ptr[sym.tokenId + sym.tokenCount];
		LexerToken ltokEnd = parser.lexer->tokens.ptr[endTok.naiveTokenId];

		U64 start = ltok.offset + tok.lexerTokenSubId;
		U64 end = ltokEnd.offset + endTok.lexerTokenSubId + endTok.tokenSize;

		name = CharString_createRefSizedConst(
			parser.lexer->source.ptr + start,
			end - start,
			false
		);
	}

	CharString fileName =
		ltok.fileId == U16_MAX ? CharString_createRefCStrConst("source file") :
		parser.lexer->sourceLocations.ptr[ltok.fileId];

	//Don't care if error, we will cleanup after

	//If there's no output symbol file, always output

	Bool isFiltered = true;

	if(!CharString_length(outputSymbolFile)) {

		gotoIfError2(clean, CharString_format(
			alloc, &tmp2, "%s%s %.*s at line %.*s:%"PRIu64":%"PRIu64"\n",
			!tmp.ptr ? "" : tmp.ptr, symbolType, (int)CharString_length(name), name.ptr,
			(int) CharString_length(fileName), fileName.ptr,
			(U64)LexerToken_getOriginalLineId(ltok), (U64)(ltok.charId + tok.lexerTokenSubId)
		))

		isFiltered = false;
	}

	//Otherwise, we need to filter if the file name is the same

	else if(CharString_equalsStringSensitive(outputSymbolFile, fileName)) {

		isFiltered = false;

		gotoIfError2(clean, CharString_format(
			alloc, &tmp2, "%s%s %.*s at L#%"PRIu64":%"PRIu64"\n",
			!tmp.ptr ? "" : tmp.ptr, symbolType, (int)CharString_length(name), name.ptr,
			(U64)LexerToken_getOriginalLineId(ltok), (U64)(ltok.charId + tok.lexerTokenSubId)
		))
	}

	//Handle flags

	U32 flagsToQuery =
		ESymbolFlagEnum_IsClass |
		ESymbolFlagFuncVar_IsConst |
		ESymbolFlagFuncVar_IsConstexpr |
		ESymbolFlagFuncVar_HasImpl |
		ESymbolFlagFuncVar_HasUserImpl |
		ESymbolFlagFuncVar_IsStatic |
		ESymbolFlagFuncVar_IsExtern |
		ESymbolFlag_IsPrivate |
		ESymbolFlag_IsPublic |
		ESymbolFlag_HasTemplate |
		ESymbolFlagTypedef_IsUsing |
		ESymbolFlagVar_IsOut |
		ESymbolFlagVar_IsIn |
		ESymbolFlagVar_Sample |
		ESymbolFlagVar_NoInterpolation |
		ESymbolFlagVar_NoPerspective |
		ESymbolFlagVar_Centroid |
		ESymbolFlagVar_Linear;

	if (!isFiltered && (sym.flags & flagsToQuery)) {

		const C8 *stringsToCombine[18];
		U64 counter = 0;

		if(sym.flags & ESymbolFlagTypedef_IsUsing)
			stringsToCombine[counter++] = "using";

		if(sym.flags & ESymbolFlagEnum_IsClass)
			stringsToCombine[counter++] = "enum class";

		U32 inout = ESymbolFlagVar_IsIn | ESymbolFlagVar_IsOut;

		if((sym.flags & inout) == inout)
			stringsToCombine[counter++] = "inout";

		else if(sym.flags & ESymbolFlagVar_IsOut)
			stringsToCombine[counter++] = "out";

		else if(sym.flags & ESymbolFlagVar_IsIn)
			stringsToCombine[counter++] = "in";

		if(sym.flags & ESymbolFlagVar_Sample)
			stringsToCombine[counter++] = "sample";

		if(sym.flags & ESymbolFlagVar_NoInterpolation)
			stringsToCombine[counter++] = "nointerpolation";

		if(sym.flags & ESymbolFlagVar_NoPerspective)
			stringsToCombine[counter++] = "noperspective";

		if(sym.flags & ESymbolFlagVar_Centroid)
			stringsToCombine[counter++] = "centroid";

		if(sym.flags & ESymbolFlagVar_Linear)
			stringsToCombine[counter++] = "linear";

		if(sym.flags & ESymbolFlagFuncVar_IsConst)
			stringsToCombine[counter++] = "const";

		if(sym.flags & ESymbolFlagFuncVar_IsConstexpr)
			stringsToCombine[counter++] = "constexpr";

		if(sym.flags & ESymbolFlagFuncVar_HasImpl)
			stringsToCombine[counter++] = "impl";

		if(sym.flags & ESymbolFlagFuncVar_HasUserImpl)
			stringsToCombine[counter++] = "user_impl";

		if(sym.flags & ESymbolFlagFuncVar_IsStatic)
			stringsToCombine[counter++] = "static";

		if(sym.flags & ESymbolFlagFuncVar_IsExtern)
			stringsToCombine[counter++] = "extern";

		if(sym.flags & ESymbolFlag_HasTemplate)
			stringsToCombine[counter++] = "template";

		if(sym.flags & ESymbolFlag_IsPrivate)
			stringsToCombine[counter++] = "private";

		else if(sym.flags & ESymbolFlag_IsPublic)
			stringsToCombine[counter++] = "private";

		gotoIfError2(clean, CharString_append(&tmp2, '\t', alloc))
		gotoIfError2(clean, CharString_appendString(&tmp2, tmp, alloc))

		gotoIfError2(clean, CharString_appendString(&tmp2, CharString_createRefCStrConst("Flags: "), alloc))

		for (U64 i = 0; i < counter; ++i) {

			if(i)
				gotoIfError2(clean, CharString_appendString(&tmp2, CharString_createRefCStrConst(", "), alloc))

			gotoIfError2(clean, CharString_appendString(&tmp2, CharString_createRefCStrConst(stringsToCombine[i]), alloc))
		}

		gotoIfError2(clean, CharString_append(&tmp2, '\n', alloc))
	}

	//If there's no output, output to log, otherwise output

	if(!output)
		Log_log(alloc, ELogLevel_Error, ELogOptions_None, tmp2);

	else gotoIfError2(clean, CharString_appendString(output, tmp2, alloc))

clean:
	CharString_free(&tmp3, alloc);
	CharString_free(&tmp2, alloc);
	CharString_free(&tmp, alloc);
	return s_uccess;
}

Bool Parser_printSymbolsRecursive(
	Parser parser,
	U32 recursion,
	U32 parent,
	Bool recursive,
	Allocator alloc,
	CharString *output,
	CharString outputSymbolFile,
	Error *e_rr
);

Bool Parser_printAll(
	Parser parser,
	U32 recursion,
	U32 i,
	Bool recursive,
	Allocator alloc,
	CharString *output,
	CharString outputSymbolFile,
	U32 *skipOut,
	Error *e_rr
) {

	Bool s_uccess = true;
	Symbol sym = parser.symbols.ptr[i];
	gotoIfError3(clean, Parser_printSymbol(parser, sym, recursion, alloc, output, outputSymbolFile, e_rr))

	U32 skip = Symbol_size(sym) - 1;

	for (U16 k = 0; k < skip; ++k)
		gotoIfError3(clean, Parser_printSymbol(
			parser, parser.symbols.ptr[i + 1 + k], recursion + 1, alloc, output, outputSymbolFile, e_rr
		))

	if(recursive && sym.child != U32_MAX) {

		U32 child = parser.symbolMapping.ptr[sym.child];	//Child start

		for (U32 j = child; j < child + sym.childCount; ++j)
			gotoIfError3(clean, Parser_printSymbolsRecursive(
				parser, recursion + 1, j, true, alloc, output, outputSymbolFile, e_rr
			))
	}

	*skipOut += skip + 1;

clean:
	return s_uccess;
}

Bool Parser_printSymbolsRecursive(
	Parser parser,
	U32 recursion,
	U32 parent,
	Bool recursive,
	Allocator alloc,
	CharString *output,
	CharString outputSymbolFile,
	Error *e_rr
) {

	Bool s_uccess = true;

	//Traverse symbol tree

	if(parent == U32_MAX)
		for (U32 i = 0; i < parser.rootSymbols; )
			gotoIfError3(clean, Parser_printAll(parser, recursion, i, recursive, alloc, output, outputSymbolFile, &i, e_rr))

	else {
		U32 i = 0;
		gotoIfError3(clean, Parser_printAll(parser, recursion, parent, recursive, alloc, output, outputSymbolFile, &i, e_rr))
	}

clean:
	return s_uccess;
}

Bool Parser_printSymbols(
	Parser parser,
	U32 parent,
	Bool recursive,
	Allocator alloc,
	CharString *output,
	CharString outputSymbolFile,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(parent != U32_MAX) {		//Resolve

		if(parent >= parser.symbolMapping.length)
			retError(clean, Error_outOfBounds(
				1, parent, parser.symbolMapping.length, "Parser_printSymbols()::parent out of bounds"
			))

		parent = parser.symbolMapping.ptr[parent];
	}

	gotoIfError3(clean, Parser_printSymbolsRecursive(parser, 0, parent, recursive, alloc, output, outputSymbolFile, e_rr))

clean:
	return s_uccess;
}

Bool Symbol_create(
	ESymbolType type,
	ESymbolFlag flags,
	U32 tokenId,
	Error *e_rr,
	Symbol *symbol
) {

	Bool s_uccess = true;

	if(!symbol)
		retError(clean, Error_nullPointer(9, "Symbol_create()::symbol is required"))

	if(type >> 8)
		retError(clean, Error_outOfBounds(2, type, U8_MAX, "Symbol_create()::type out of bounds"))

	*symbol = (Symbol) {

		.parent = U32_MAX,
		.child = U32_MAX,
		.name = U32_MAX,

		.tokenId = tokenId,
		.tokenCount = 0,

		.flags = flags,

		.symbolType = (U8) type
	};

clean:
	return s_uccess;
}

U32 Symbol_size(Symbol s) {
	return (U32)s.annotations + (Bool)(s.flags & ESymbolFlag_HasTemplate) + 1;
}

Bool Parser_registerSymbol(Parser *parser, Symbol s, U32 parent, Allocator alloc, U32 *symbolId, Error *e_rr) {

	Bool s_uccess = true;
	U32 pushedId = U32_MAX;

	if(!parser)
		retError(clean, Error_nullPointer(0, "Parser_registerSymbol()::parser is required"))

	if(!symbolId)
		retError(clean, Error_nullPointer(4, "Parser_registerSymbol()::symbolId is required"))

	//Find spot from parent

	U32 resolvedId;

	if (parent == U32_MAX)
		resolvedId = parser->rootSymbols;

	//Register in parent

	else {

		if(parent >= parser->symbolMapping.length)
			retError(clean, Error_outOfBounds(
				1, parent, parser->symbolMapping.length, "Parser_registerSymbol()::parent is out of bounds"
			))

		Symbol parentSymbol = parser->symbols.ptr[parser->symbolMapping.ptr[parent]];

		if (parentSymbol.child == U32_MAX)
			resolvedId = (U32) parser->symbolMapping.length;

		else resolvedId = parser->symbolMapping.ptr[parentSymbol.child] + parentSymbol.childCount;
	}

	//Count all annotations of same parent that aren't parented to a symbol yet and parent them

	U8 annotations = 0;

	if(s.symbolType != ESymbolType_Annotation) {

		for (U32 i = resolvedId - 1; i != U32_MAX; --i) {

			Symbol symbol = parser->symbols.ptr[i];

			if(
				(symbol.symbolType != ESymbolType_Annotation && symbol.symbolType != ESymbolType_Template) ||
				(symbol.flags & ESymbolFlag_IsParented) ||
				(symbol.parent != parent)
			)
				break;

			if (symbol.symbolType == ESymbolType_Template) {
				s.flags |= ESymbolFlag_HasTemplate;
				break;
			}

			if(annotations == U8_MAX)
				retError(clean, Error_outOfBounds(
					1, U8_MAX, U8_MAX, "Parser_registerSymbol() only supporting up to 256 annotations!"
				))

			++annotations;
		}

		//Set resolvedId to before the annotations.
		//This is to be able to easily loop over variables and functions only.
		//If annotations and templates are in-between, then we'd make that more annoying.
		//Instead, it can use Symbol_symbolLength(s) to skip over anything that's not important.

		U32 total = (U32)annotations + (Bool)(s.flags & ESymbolFlag_HasTemplate);

		resolvedId -= total;

		//Flag all annotations as consumed, before we append the symbol at resolvedId (in front of the annotations)

		for(U32 i = 0; i < total; ++i)
			parser->symbols.ptrNonConst[resolvedId + i].flags |= ESymbolFlag_IsParented;
	}

	s.annotations = annotations;

	//Push into symbol array

	if(parser->symbols.length >= U32_MAX - 1)
		retError(clean, Error_outOfBounds(
			1, U32_MAX, U32_MAX, "Parser_registerSymbol()::parser->symbols is out of bounds"
		))

	s.symbolId = (U32) parser->symbolMapping.length;
	gotoIfError2(clean, ListSymbol_insert(&parser->symbols, resolvedId, s, alloc))
	pushedId = resolvedId;

	//Append symbol id and return that id

	gotoIfError2(clean, ListU32_pushBack(&parser->symbolMapping, resolvedId, alloc))
	*symbolId = s.symbolId;

	//Register in parent

	if(parent == U32_MAX)
		++parser->rootSymbols;

	else {

		Symbol *parentSym = &parser->symbols.ptrNonConst[parser->symbolMapping.ptr[parent]];

		if(parentSym->child == U32_MAX)		//Redirect parent to point to first child
			parentSym->child = *symbolId;

		++parentSym->childCount;
	}

	//Fix symbol references that are at >=resolvedId.
	//Only if there are any symbols after

	if(resolvedId != parser->symbols.length - 1) {

		U32 dontTouch = *symbolId;

		for(U32 i = 0; i < (U32) parser->symbolMapping.length; ++i) {

			if(i == dontTouch)
				continue;

			U32 *curr = &parser->symbolMapping.ptrNonConst[i];

			if(*curr >= resolvedId)
				++*curr;
		}
	}

clean:

	if(!s_uccess && pushedId != U32_MAX)
		ListSymbol_erase(&parser->symbols, pushedId);

	return s_uccess;
}

Bool Parser_setSymbolName(Parser *parser, U32 symbolId, CharString *name, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true, push = false;

	if(!parser)
		retError(clean, Error_nullPointer(0, "Parser_setSymbolName()::parser is required"))

	if(!name)
		retError(clean, Error_nullPointer(2, "Parser_setSymbolName()::name is required"))

	if(symbolId >= parser->symbolMapping.length)
		retError(clean, Error_outOfBounds(
			1, symbolId, parser->symbolMapping.length, "Parser_setSymbolName()::symbolId is out of bounds"
		))

	if(parser->symbolNames.length == U32_MAX - 1)
		retError(clean, Error_outOfBounds(
			0, parser->symbolNames.length, U32_MAX - 1, "Parser_setSymbolName()::parser->symbolNames is out of bounds"
		))

	//Move to symbolNames

	gotoIfError2(clean, ListCharString_pushBack(&parser->symbolNames, *name, alloc))
	*name = CharString_createNull();
	push = true;								//We pushed new, another error would have to pop

	//Erase old

	U32 resolvedSymbolId = parser->symbolMapping.ptr[symbolId];
	U32 *nameId = &parser->symbols.ptrNonConst[resolvedSymbolId].name, prevName = *nameId;

	if(prevName != U32_MAX) {
		CharString_free(&parser->symbolNames.ptrNonConst[prevName], alloc);
		gotoIfError2(clean, ListCharString_erase(&parser->symbolNames, prevName))
	}

	//Update symbol

	*nameId = (U32)(parser->symbolNames.length - 1);

clean:

	if(!s_uccess && push)
		ListCharString_popBack(&parser->symbolNames, NULL);

	return s_uccess;
}

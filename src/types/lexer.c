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
#include "types/lexer.h"

TListImpl(LexerToken);
TListImpl(LexerExpression);

U32 LexerToken_getLineId(LexerToken tok) {
	return tok.lineId | ((U32)(tok.realLineIdAndLineIdExt >> 15) << 16);
}

U32 LexerToken_getOriginalLineId(LexerToken tok) {
	return tok.realLineIdAndLineIdExt << 1 >> 1;
}

U32 LexerToken_getOffset(LexerToken tok) {
	return tok.offsetType << 4 >> 4;
}

ELexerTokenType LexerToken_getType(LexerToken tok) {
	return tok.offsetType >> (32 - 4);
}

const C8 *LexerToken_getTokenEnd(LexerToken tok, Lexer l) {

	if(LexerToken_getOffset(tok) + tok.length > CharString_length(l.source))
		return NULL;

	return l.source.ptr + LexerToken_getOffset(tok) + tok.length;
}

const C8 *LexerToken_getTokenStart(LexerToken tok, Lexer l) {

	if(LexerToken_getOffset(tok) > CharString_length(l.source))
		return NULL;

	return l.source.ptr + LexerToken_getOffset(tok);
}

CharString LexerToken_asString(LexerToken tok, Lexer l) {
	const C8 *ptr = LexerToken_getTokenStart(tok, l);
	return CharString_createRefSizedConst(ptr, tok.length, false);
}

CharString LexerExpression_asString(LexerExpression e, Lexer l) {

	LexerToken tStart = l.tokens.ptr[e.tokenOffset];
	LexerToken tEnd = l.tokens.ptr[e.tokenOffset + e.tokenCount - 1];

	const C8 *cStart = LexerToken_getTokenStart(tStart, l);
	const C8 *cEnd = LexerToken_getTokenEnd(tEnd, l);

	return CharString_createRefSizedConst(cStart, cEnd - cStart, false);
}

Bool ELexerTokenType_isNumber(ELexerTokenType tokenType) {
	return tokenType >= ELexerTokenType_NumberStart && tokenType <= ELexerTokenType_NumberEnd;
}

Error Lexer_endExpression(
	U64 *lastTokenPtr,
	ListLexerExpression *expressions,
	U64 tokenCount,
	ELexerExpressionType *expressionType,
	Allocator alloc
) {

	Error err = Error_none();

	U64 lastToken = *lastTokenPtr;

	if(lastToken == tokenCount)
		return err;

	if(lastToken >> 32)
		gotoIfError(clean, Error_outOfBounds(
			0, lastToken, U32_MAX, "Lexer_create() LexerExpression offset is limited to U32_MAX tokens"
		))

	if((tokenCount - lastToken) >> 16)
		gotoIfError(clean, Error_outOfBounds(
			0, tokenCount - lastToken, U16_MAX, "Lexer_create() LexerExpression is limited to U16_MAX tokens"
		))

	LexerExpression exp = (LexerExpression) {
		.type = *expressionType,
		.tokenOffset = (U32) lastToken,
		.tokenCount = (U16) (tokenCount - lastToken)
	};

	gotoIfError(clean, ListLexerExpression_pushBack(expressions, exp, alloc))
	*lastTokenPtr = tokenCount;
	*expressionType = ELexerExpressionType_None;

clean:
	return err;
}

Bool Lexer_isSymbol(C8 c) {

	Bool symbolRange0 = c > ' ' && c < '0' && c != '$';		//~"#%&'()*+,-./
	Bool symbolRange1 = c > '9' && c < 'A';					//:;<=>?@
	Bool symbolRange2 = c > 'Z' && c < 'a' && c != '_';		//[\]^`
	Bool symbolRange3 = c > 'z' && c < 0x7F;				//{|}~

	return symbolRange0 || symbolRange1 || symbolRange2 || symbolRange3;
}

Error Lexer_create(CharString str, Allocator alloc, Lexer *lexer) {

	if(!lexer)
		return Error_nullPointer(!str.ptr ? 0 : 1, "Lexer_create()::str and lexer are required");

	if(lexer->tokens.ptr)
		return Error_invalidParameter(1, 0, "Lexer_create()::lexer wasn't empty, might indicate memleak");

	if(CharString_length(str) >> 28)
		return Error_outOfBounds(0, CharString_length(str), 1 << 28, "Lexer_create()::str is limited to 256MiB");

	Error err = Error_none();
	ListLexerToken tokens = { 0 };
	ListLexerExpression expressions = { 0 };
	ListCharString sourceLocations = { 0 };
	CharString currentSource = CharString_createNull();
	CharString tempSource = CharString_createNull();

	gotoIfError(clean, ListCharString_reserve(&sourceLocations, 64, alloc))
	gotoIfError(clean, ListLexerExpression_reserve(&expressions, 64 + CharString_length(str) / 32, alloc))
	gotoIfError(clean, ListLexerToken_reserve(&tokens, 64 + CharString_length(str) / 3, alloc))

	const C8 *prevIt = NULL;
	C8 prev = 0;
	ELexerExpressionType expressionType = ELexerExpressionType_None;
	ELexerTokenType tokenType = ELexerTokenType_Count;

	U64 lineCounter = 1, lastLineStart = 0;
	U64 lastToken = 0;
	U64 lastLineId = U64_MAX;

	U64 fileLineOffset = 0, fileLineStart = 0;
	U64 fileId = U16_MAX;

	for (U32 i = 0, k = (U32) CharString_length(str); i < k; ++i) {

		C8 c = CharString_getAt(str, i);

		C8 next = CharString_getAt(str, i + 1);
		C8 prevTemp = prev;
		prev = c;

		Bool isString = tokenType == ELexerTokenType_String;

		if(isString) {

			if(c == '\\' && next == '\\')		//Skip to next \ to allow escaping
				continue;

			if(c == '\\' && next == '"') {		//Skip after \" to avoid it mistakingly ending the string
				++i;
				continue;
			}

			else if (C8_isNewLine(c)) {			//Newline in strings, for multi line strings

				if(c == '\r' && next == '\n')	//Consume \r\n as one char
					++i;

				++lineCounter;
				continue;
			}

			else if(c != '\"')					//Look for end of string
				continue;
		}

		Bool multiChar = false;
		Bool endExpression = false;
		Bool endToken = false;

		if(c == '"') {

			if(isString && lastLineId != lineCounter)
				gotoIfError(clean, Error_invalidState(0, "Lexer_create() String \"\" needs to be created on the same line"))

			//If we encounter a " in the middle of an expression
			//Example: ("") then we have to break up the previous token and start anew

			if (tokenType != ELexerTokenType_String && tokenType != ELexerTokenType_Count) {
				endToken = endExpression = true;
				--i;
			}

			if(prevIt)
				endToken = true;		//Ensure we add tokens before

			else {
				prevIt = str.ptr + i + 1;
				tokenType = ELexerTokenType_String;
			}

			lastLineId = lineCounter;
		}

		//Newline; end last expression in some cases

		if(c == '\r' && next == '\n')		//Consume \r\n as one char
			multiChar = true;

		if (prevTemp != '\\' && C8_isNewLine(c))
			endExpression = true;

		if(C8_isNewLine(c)) {

			//Handle #line, we simply consume the tokens and will never show them to the parser.
			//Because, we need to know the real location of tokens too, we will just pass that info along.
			//If we don't, the lexer can't give accurate error messages.
			//# line 123 or # line 123 "test"

			U64 tokenCount = tokens.length - lastToken;

			if (
				tokenCount && expressionType == ELexerExpressionType_Preprocessor &&
				(tokenCount == 3 || tokenCount == 4)
			) {

				LexerToken preprocessorDirective = tokens.ptr[lastToken + 1];
				Lexer fakeLexer = (Lexer) { .source = str };

				if (CharString_equalsStringSensitive(
					LexerToken_asString(preprocessorDirective, fakeLexer),
					CharString_createRefCStrConst("line")
				)) {

					LexerToken lineId = tokens.ptr[lastToken + 2];

					if(!(
						LexerToken_getType(lineId) >= ELexerTokenType_IntBegin &&
						LexerToken_getType(lineId) <= ELexerTokenType_IntEnd
					))
						gotoIfError(clean, Error_invalidState(1, "Lexer_create() #line expected uint (U64)"))

					U64 lineIdi = 0;

					if(!CharString_parseU64(LexerToken_asString(lineId, fakeLexer), &lineIdi))
						gotoIfError(clean, Error_invalidState(2, "Lexer_create() #line couldn't parse U64"))

					fileLineStart = lineCounter + 1;
					fileLineOffset = lineIdi;

					//# line 123 "testFile"
					//           ^

					if(tokenCount == 4) {

						LexerToken file = tokens.ptr[lastToken + 3];

						if(LexerToken_getType(file) != ELexerTokenType_String)
							gotoIfError(clean, Error_invalidState(3, "Lexer_create() #line expected string next"))

						//Parse string forreal

						CharString_clear(&tempSource);

						Bool isEscaped = false;
						Bool prevSlash = false;
						Bool leadingSlashSlash = false;

						for (U64 l = LexerToken_getOffset(file), j = l; j < l + file.length; ++j) {

							C8 cj = str.ptr[j];

							if (cj == '\\') {

								if(isEscaped) {
									gotoIfError(clean, CharString_append(&tempSource, cj, alloc))
									isEscaped = false;
									continue;
								}

								isEscaped = true;
								continue;
							}

							if(prevSlash && cj == '/') {						//Allow //
								CharString_clear(&tempSource);
								leadingSlashSlash = true;
							}

							prevSlash = cj == '/';

							gotoIfError(clean, CharString_append(&tempSource, cj, alloc))
							isEscaped = false;
						}

						if(isEscaped)
							gotoIfError(clean, Error_invalidParameter(
								0, 0, "Lexer_create() source was invalid. String can't end with an escaped quote!"
							))

						//Fix path

						if(leadingSlashSlash)
							gotoIfError(clean, CharString_insert(&tempSource, '/', 0, alloc))

						//Find string

						U64 sourceLoc = 0;

						for(; sourceLoc < sourceLocations.length; ++sourceLoc)
							if (CharString_equalsStringInsensitive(sourceLocations.ptr[sourceLoc], tempSource))
								break;

						//Insert new

						if(sourceLoc == sourceLocations.length) {

							if(sourceLoc >> 16)
								gotoIfError(clean, Error_invalidState(4, "Lexer_create() source count limited to U16_MAX"))

							gotoIfError(clean, CharString_createCopy(tempSource, alloc, &currentSource))
							gotoIfError(clean, ListCharString_pushBack(&sourceLocations, currentSource, alloc))
							currentSource = CharString_createNull();		//Moved
						}

						//Ensure token after links to this string

						fileId = sourceLoc;
					}

					gotoIfError(clean, ListLexerToken_resize(&tokens, lastToken, alloc))
					endExpression = false;		//Pretend we didn't see that
					prevIt = NULL;
					tokenType = ELexerTokenType_Count;
				}
			}

			//Increment line as normal, make sure we don't run out of line counter tho (17 bits)

			++lineCounter;
			lastLineStart = i + multiChar + 1;

			if (lineCounter >> 17)
				gotoIfError(clean, Error_outOfBounds(
					0, lineCounter, (1 << 17) - 1, "Lexer_create() line count is limited to 128Ki"
				))
		}

		//Preprocessor

		if(c == '#' && !expressionType) {
			expressionType = ELexerExpressionType_Preprocessor;
			prevIt = str.ptr + i;
		}

		//Handle starting comment

		if(c == '/' && next == '/' && expressionType != ELexerExpressionType_MultiLineComment) {
			gotoIfError(clean, Lexer_endExpression(&lastToken, &expressions, tokens.length, &expressionType, alloc))
			expressionType = ELexerExpressionType_Comment;
			multiChar = true;
			endToken = true;
		}

		//Handle starting multi line comment

		if(c == '/' && next == '*' && !expressionType) {
			gotoIfError(clean, Lexer_endExpression(&lastToken, &expressions, tokens.length, &expressionType, alloc))
			expressionType = ELexerExpressionType_MultiLineComment;
			multiChar = true;
			endToken = true;
		}

		//Deal with closing multi line comment

		else if (expressionType == ELexerExpressionType_MultiLineComment) {

			if (c == '*' && next == '/') {
				multiChar = true;
				endExpression = true;
			}
		}

		//Parse ; as end of expression

		if(c == ';' && expressionType == ELexerExpressionType_Generic)
			endExpression = true;

		//First non whitespace char (excluding ")

		if(c != '"') {

			if(!C8_isWhitespace(c)) {

				if(!expressionType)
					expressionType = ELexerExpressionType_Generic;

				if(!prevIt)
					prevIt = str.ptr + i;

				//Numbers: 0x 0n 0o 0b .0f 1.3 etc.

				if (
					tokenType != ELexerTokenType_Identifier &&
					(C8_isDec(c) || c == '.' || c == '+' || c == '-') &&
					(Lexer_isSymbol(prevTemp) || C8_isWhitespace(prevTemp) || !prevTemp)
				) {

					//Oops, we're still busy with a symbol.
					//If next token

					if (tokenType == ELexerTokenType_Symbols) {

						if (!C8_isDec(next))
							goto classification;

						endToken = true;
						--i;
						goto classification;
					}

					Bool forceType = false;		//For example 0123.32 is a float (from oct) or 123.32 is a float too (from dec)
					Bool requiredNext = true;	//If next character is expected
					Bool isEnded = false;
					Bool containsDot = false;
					Bool isLastE = false;

					//Can be basically everything; float, Hex, Nyto, Binary, Octal, Or just 0
					//Classify it

					if (c == '0') {

						forceType = true;

						switch (next) {

							case 'x':	case 'X':	tokenType = ELexerTokenType_IntegerHex;		++i;	break;
							case 'b':	case 'B':	tokenType = ELexerTokenType_IntegerBinary;	++i;	break;
							case 'n':	case 'N':	tokenType = ELexerTokenType_IntegerNyto;	++i;	break;

							case '0':	case '1':	case '2':	case '3':	case '4':
							case '5':	case '6':	case '7':	case '8':	case '9':
								requiredNext = false;
								tokenType = ELexerTokenType_IntegerOctal;
								++i;
								break;

							case 'o':	case 'O':
								tokenType = ELexerTokenType_IntegerOctal;
								++i;
								break;

							case '.':	case 'e':	case 'E':

								if (next == '.') {
									C8 c2 = CharString_getAt(str, i + 2);
									if(c2 == 'f' || c2 == 'F') {
										tokenType = ELexerTokenType_Float;
										requiredNext = false;				//0f is the final float
										isEnded = true;
										i += 3;
										break;
									}
								}

								tokenType = ELexerTokenType_Double;
								i += 2;
								containsDot = next == '.';
								isLastE = !containsDot;
								break;

							case 'f':	case 'F':
								tokenType = ELexerTokenType_Float;
								requiredNext = false;				//0f is the final float
								isEnded = true;
								i += 2;
								break;
						}

						if(!isEnded) {

							if ((Lexer_isSymbol(next) || next == C8_MAX || C8_isWhitespace(next)) && !containsDot) {	//0
								isEnded = true;
								tokenType = ELexerTokenType_IntegerDec;
								++i;
							}

							else {
								requiredNext &= !C8_isWhitespace(next) && next != C8_MAX && forceType;
								++i;
							}
						}
					}

					//Float/double, int, uint

					else {

						//Starts with . is always a double (if it's parsable as one)

						if(c == '.') {
							tokenType = ELexerTokenType_Double;
							containsDot = true;
						}

						//Otherwise, we assume Integer dec, until we hit something that makes us doubti t

						else {

							requiredNext = c == '-' || c == '+';

							if(!C8_isDec(next) && c != '.' && requiredNext)
								goto classification;

							tokenType = ELexerTokenType_IntegerDec;
						}

						++i;
					}

					//Parse number

					if (!isEnded) {

						switch (tokenType) {

							//Find end of number

							case ELexerTokenType_IntegerBinary:
							case ELexerTokenType_IntegerHex:
							case ELexerTokenType_IntegerNyto:
							case ELexerTokenType_IntegerOctal:
							case ELexerTokenType_IntegerDec:

								if(i == k && requiredNext)
									gotoIfError(clean, Error_invalidState(0, "Lexer_create() expected next integer"))

								for (; i < k; ++i) {

									C8 c2 = CharString_getAt(str, i);
									Bool success = false;

									switch(tokenType) {

										case ELexerTokenType_IntegerHex:		success = C8_isHex(c2);		break;
										case ELexerTokenType_IntegerBinary:		success = C8_isBin(c2);		break;
										case ELexerTokenType_IntegerNyto:		success = C8_isNyto(c2);	break;

										//Special cases; octal can be casted away if it's not prefixed by 0o/0O.
										//In this case, we will require to see the dot after.

										case ELexerTokenType_IntegerOctal: {

											success = C8_isOct(c2);
											Bool isF = c2 == 'f' || c2 == 'F';

											if(!success && (C8_isDec(c2) || c2 == 'e' || c2 == 'E' || isF || c2 == '.')) {

												if (c2 == '.') {

													C8 c3 = CharString_getAt(str, i + 1);

													//This is possible, for example 0123.xxxx

													if(!(c3 == 'e' || c3 == 'E' || c3 == 'f' || c3 == 'F' || C8_isDec(c3)))
														break;
												}

												++i;
												tokenType = isF ? ELexerTokenType_Float : ELexerTokenType_Double;
												containsDot = c2 == '.';
												isLastE = c2 == 'e' || c2 == 'E';
												break;
											}

											break;
										}

										case ELexerTokenType_IntegerDec: {

											success = C8_isDec(c2);
											Bool isF = c2 == 'f' || c2 == 'F';

											if(!success && (c2 == 'e' || c2 == 'E' || isF || c2 == '.')) {

												if (c2 == '.') {

													C8 c3 = CharString_getAt(str, i + 1);

													//This is possible, for example 1.xxxx

													if(!(
														c3 == 'e' || c3 == 'E' || c3 == 'f' || c3 == 'F' || C8_isDec(c3) ||
														Lexer_isSymbol(c3)
													))
														break;
												}

												++i;
												tokenType = isF ? ELexerTokenType_Float : ELexerTokenType_Double;
												containsDot = c2 == '.';
												isLastE = c2 == 'e' || c2 == 'E';
												break;
											}

											break;
										}
									}

									if(success)
										continue;

									if(Lexer_isSymbol(c2) || C8_isWhitespace(c2))
										break;

									if(!(tokenType >= ELexerTokenType_IntBegin && tokenType <= ELexerTokenType_IntEnd))
										break;

									gotoIfError(clean, Error_invalidState(0, "Lexer_create() invalid character in number"))
								}

								break;
						}

						//Validate float a little bit

						if (!isEnded && (tokenType == ELexerTokenType_Float || tokenType == ELexerTokenType_Double)) {

							Bool containsE = isLastE;
							U64 lastE = (!containsE ? U64_MAX : i) - 1;

							for (; i < k; ++i) {

								C8 c2 = CharString_getAt(str, i);

								if(C8_isDec(c2))
									continue;

								if (c2 == '.') {

									if(containsDot)
										break;

									if(lastE + 1 == i)
										gotoIfError(clean, Error_invalidState(0, "Lexer_create() invalid float \"e.\"?"))

									if(containsE)		//1e0.xxxx can terminate a float for example
										break;

									containsDot = true;
									continue;
								}

								if (c2 == '-' || c2 == '+') {

									if(!containsE)
										gotoIfError(clean, Error_invalidState(0, "Lexer_create() expected e- or e+ in float"))

									if(lastE + 1 != i)		//Otherwise, - or + can terminate a float
										break;

									continue;
								}

								if(c2 == 'f' || c2 == 'F') {
									tokenType = ELexerTokenType_Float;
									++i;
									break;
								}

								if (c2 == 'e' || c2 == 'E') {

									if(containsE)
										gotoIfError(clean, Error_invalidState(0, "Lexer_create() contains two e's in float"))

									containsE = true;
									lastE = i;
									continue;
								}

								if(Lexer_isSymbol(c2) || C8_isWhitespace(c2))
									break;

								if(c == '.') {				//Misclassification using a .xxx for example
									tokenType = ELexerTokenType_Count;
									--i;
									goto classification;
								}

								//Misclassification with for example 1.xxx

								if(containsDot)
									--i;

								--i;
								goto classification;
							}
						}
					}
				}

			classification:

				if(!(tokenType >= ELexerTokenType_NumberStart && tokenType <= ELexerTokenType_NumberEnd))
					tokenType = Lexer_isSymbol(c) ? ELexerTokenType_Symbols : ELexerTokenType_Identifier;

				else --i;

				ELexerTokenType nextType =
						C8_isWhitespace(next) ? ELexerTokenType_Count : (
							 Lexer_isSymbol(next) ? ELexerTokenType_Symbols : ELexerTokenType_Identifier
						);

				if(nextType != tokenType)
					endToken = true;
			}

			else if (prevIt)
				endToken = true;
		}

		if(endToken || (prevIt && endExpression)) {

			U64 endChar = 1 - isString;
			U64 len = (str.ptr + i + endChar + multiChar) - prevIt;
			U64 off = i + endChar + multiChar - len;

			if(len >> 8)
				gotoIfError(clean, Error_outOfBounds(0, len, 256, "Lexer_create() tokens are limited to 256 C8s"))

			if(off >> (32 - 4))
				gotoIfError(clean, Error_outOfBounds(0, off, 1 << (32 - 4), "Lexer_create() offset out of bounds"))

			U64 charId = i + 1 - len - lastLineStart;

			if((charId + 1) >> 8)
				gotoIfError(clean, Error_outOfBounds(0, charId + 1, 256, "Lexer_create() lines are limited to 256 C8s"))

			U64 resolvedLineId = lineCounter - fileLineStart + fileLineOffset;

			if(resolvedLineId >> 15)
				gotoIfError(clean, Error_outOfBounds(
					0, resolvedLineId, 1 << 15, "Lexer_create() source line id is out of bounds (32Ki)"
				))

			LexerToken tok = (LexerToken) {

				.lineId = (U16) lineCounter,
				.charId = (U8) (charId + 1),
				.length = (U8) len,

				.offsetType = (U32)(off | (tokenType << (32 - 4))),

				.fileId = (U16)fileId,
				.realLineIdAndLineIdExt = (U16)((lineCounter >> 16 << 15) | resolvedLineId)
			};

			gotoIfError(clean, ListLexerToken_pushBack(&tokens, tok, alloc))
			prevIt = NULL;
			tokenType = ELexerTokenType_Count;
		}

		//Push it as an expression

		if (endExpression)
			gotoIfError(clean, Lexer_endExpression(&lastToken, &expressions, tokens.length, &expressionType, alloc))

		if(multiChar)
			++i;
	}

	if (lastToken != tokens.length)
		gotoIfError(clean, Lexer_endExpression(&lastToken, &expressions, tokens.length, &expressionType, alloc))

clean:

	if(err.genericError) {
		ListLexerToken_free(&tokens, alloc);
		ListLexerExpression_free(&expressions, alloc);
		ListCharString_freeUnderlying(&sourceLocations, alloc);
	}
	else *lexer = (Lexer) {
		.source = CharString_createRefSizedConst(str.ptr, CharString_length(str), false),
		.expressions = expressions,
		.tokens = tokens,
		.sourceLocations = sourceLocations
	};

	CharString_free(&currentSource, alloc);
	CharString_free(&tempSource, alloc);
	return err;
}

Bool Lexer_free(Lexer *lexer, Allocator alloc) {

	if(!lexer)
		return true;

	ListLexerExpression_free(&lexer->expressions, alloc);
	ListLexerToken_free(&lexer->tokens, alloc);
	ListCharString_freeUnderlying(&lexer->sourceLocations, alloc);
	return true;
}

void Lexer_print(Lexer lexer, Allocator alloc) {

	if(!lexer.tokens.length)
		Log_debugLn(alloc, "Lexer: Empty");

	else Log_debugLn(alloc, "Lexer:");

	CharString nul = CharString_createRefCStrConst("null");

	for (U64 i = 0; i < lexer.tokens.length; ++i) {

		LexerToken lt = lexer.tokens.ptr[i];
		CharString tokenRaw = LexerToken_asString(lt, lexer);

		const C8 *tokenType[] = {
			"Identifier",
			"Symbols",
			"F32",
			"F64",
			"Int:dec",
			"Int:hex",
			"Int:oct",
			"Int:bin",
			"Int:nyto",
			"C8[]"
		};

		CharString mine = nul;

		if(lt.fileId != U16_MAX)
			mine = lexer.sourceLocations.ptr[lt.fileId];

		U64 lastSlash = CharString_findLastSensitive(mine, '/', 0);

		if(lastSlash == 1 && mine.ptr[0] == '/')		//Exception for //x
			lastSlash = U64_MAX;

		const C8 *format1 = "T%05"PRIu64"(L#%04"PRIu64": %03"PRIu64")\t% 16s: %.*s\t\t\t(%s L#%05"PRIu64")";
		const C8 *format2 = "T%05"PRIu64"(L#%04"PRIu64": %03"PRIu64")\t% 16s: %.*s\t\t\t(.../%s L#%05"PRIu64")";

		Log_debugLn(

			alloc,

			lastSlash == U64_MAX ? format1 : format2,

			i,

			(U64)LexerToken_getLineId(lt),
			(U64)lt.charId,

			tokenType[LexerToken_getType(lt)],
			CharString_length(tokenRaw),
			tokenRaw.ptr,

			lastSlash == U64_MAX ? mine.ptr : mine.ptr + lastSlash,
			(U64)LexerToken_getOriginalLineId(lt)
		);
	}
}

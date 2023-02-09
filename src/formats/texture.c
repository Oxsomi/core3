/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
*/

#include "formats/texture.h"

ETexturePrimitive ETextureFormat_getPrimitive(ETextureFormat f) { 
	return (ETexturePrimitive)((f >> 24) & 7); 
}

Bool ETextureFormat_getIsCompressed(ETextureFormat f) { 
	return ETextureFormat_getPrimitive(f) == ETexturePrimitive_Compressed; 
}

U64 ETextureFormat_getBits(ETextureFormat f) { 
	U64 length = (f >> 27) + 1;
	return length << (ETextureFormat_getIsCompressed(f) ? 6 : 2);
}

U64 ETextureFormat_getAlphaBits(ETextureFormat f) { 
	return ETextureFormat_getIsCompressed(f) ? f & 7 : (f & 077) << 1; 
}

U64 ETextureFormat_getBlueBits(ETextureFormat f) { 
	return ETextureFormat_getIsCompressed(f) ? (f >> 6) & 7 : ((f >> 3) & 077) << 1; 
}

U64 ETextureFormat_getGreenBits(ETextureFormat f) { 
	return ETextureFormat_getIsCompressed(f) ? (f >> 12) & 7 : ((f >> 6) & 077) << 1; 
}

U64 ETextureFormat_getRedBits(ETextureFormat f) { 
	return ETextureFormat_getIsCompressed(f) ? (f >> 18) & 7 : ((f >> 9) & 077) << 1; 
}

Bool ETextureFormat_hasRed(ETextureFormat f) { return ETextureFormat_getRedBits(f); }
Bool ETextureFormat_hasGreen(ETextureFormat f) { return ETextureFormat_getGreenBits(f); }
Bool ETextureFormat_hasBlue(ETextureFormat f) { return ETextureFormat_getBlueBits(f); }
Bool ETextureFormat_hasAlpha(ETextureFormat f) { return ETextureFormat_getAlphaBits(f); }

U8 ETextureFormat_getChannels(ETextureFormat f) {
	return 
		(U8)ETextureFormat_hasRed(f)  + (U8)ETextureFormat_hasGreen(f) + 
		(U8)ETextureFormat_hasBlue(f) + (U8)ETextureFormat_hasAlpha(f);
}

ETextureCompressionType ETextureFormat_getCompressionType(ETextureFormat f) {

	if(!ETextureFormat_getIsCompressed(f))
		return ETextureCompressionType_Invalid;

	return (ETextureCompressionType)((f >> 9) & 7);
}

Bool ETextureFormat_getAlignment(ETextureFormat f, U8 *x, U8 *y) {

	if(!ETextureFormat_getIsCompressed(f))
		return false;

	*x = (f >> 15) & 7;
	*y = (f >> 21) & 7;

	return true;
}

U64 ETextureFormat_getSize(ETextureFormat f, U32 w, U32 h) {

	U8 alignW = 1, alignH = 1;

	//If compressed; the size of the texture format is specified per blocks

	if (ETextureFormat_getAlignment(f, &alignW, &alignH)) {

		if(w % alignW || h % alignH)
			return U64_MAX;

		w /= alignW;
		h /= alignH;
	}

	return (((U64)w * h * ETextureFormat_getBits(f)) + 7) >> 3;
}
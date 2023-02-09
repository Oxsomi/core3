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

#pragma once
#include "string.h"

typedef ShortString TimerFormat;

typedef enum EFormatStatus {
	EFormatStatus_Success,
	EFormatStatus_Overflow,
	EFormatStatus_InvalidValue,
	EFormatStatus_InvalidFormat,
	EFormatStatus_InvalidInput,
	EFormatStatus_InvalidTime
} EFormatStatus;

Ns Time_now();
DNs Time_elapsed(Ns prev);

//e.g. year: 1970, month: 1, day: 1, hour: 0, minute: 0, seconds: 0, ns: 0 = (Ns)0

Ns Time_date(U16 year, U8 month, U8 day, U8 hour, U8 minute, U8 second, U32 ns);		//U64_MAX on invalid
Bool Time_getDate(Ns timestamp, U16 *year, U8 *month, U8 *day, U8 *hour, U8 *minute, U8 *second, U32 *ns);

DNs Time_dns(Ns timeStamp0, Ns timeStamp1);
F32 Time_dt(Ns timeStamp0, Ns timeStamp1);

U64 Time_clocks();
I64 Time_clocksElapsed(U64 prevClocks);

void Time_format(Ns time, TimerFormat timer);
EFormatStatus Time_parseFormat(Ns *time, TimerFormat format);

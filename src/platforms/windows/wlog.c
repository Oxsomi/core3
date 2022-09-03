#include "platforms/log.h"
#include "platforms/thread.h"
#include "types/timer.h"

//Unfortunately before Windows 10 it doesn't support printing colors into console using printf
//We also use Windows dependent stack tracing

#include <Windows.h>
#include <signal.h>
#include <DbgHelp.h>
#include <stdio.h>

#pragma comment(lib, "DbgHelp.lib")

//Carried over from core2

void Log_captureStackTrace(void **stack, usz stackSize, usz skip) {
	RtlCaptureStackBackTrace(
		(DWORD)(1 + skip), 
		(DWORD) stackSize, 
		stack, NULL
	);
}

struct CapturedStackTrace {

	//Module and symbol

	const c8 *mod, *sym;

	//File and line don't have to be specified, for external calls

	const c8 *fil;
	u32 lin;
};

static const WORD colors[] = {
	2,	/* green */
	3,	/* cyan */
	14,	/* yellow */
	4,	/* red */
	12	/* bright red */
};

void Log_printCapturedStackTrace(const StackTrace stackTrace, enum LogLevel lvl) {

	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(handle, colors[lvl]);

	struct CapturedStackTrace captured[STACKTRACE_MAX_SIZE] = { 0 };

	usz stackCount = 0;

	//Obtain process

	HANDLE process = GetCurrentProcess();
	HMODULE processModule = GetModuleHandleA(NULL);

	bool hasSymbols = SymInitialize(process, NULL, TRUE);
	bool anySymbol = false;

	if(hasSymbols)
		for (usz i = 0; i < STACKTRACE_MAX_SIZE && stackTrace[i]; ++i, ++stackCount) {

			usz addr = (usz) stackTrace[i];

			//Get module name

			usz moduleBase = SymGetModuleBase(process, addr);

			c8 modulePath[MAX_PATH + 1] = { 0 };
			if (!moduleBase || !GetModuleFileNameA((HINSTANCE)moduleBase, modulePath, MAX_PATH))
				continue;

			anySymbol = true;

			c8 symbolData[sizeof(IMAGEHLP_SYMBOL) + MAX_PATH + 1] = { 0 };

			PIMAGEHLP_SYMBOL symbol = (PIMAGEHLP_SYMBOL)symbolData;
			symbol->SizeOfStruct = sizeof(symbolData);
			symbol->MaxNameLength = MAX_PATH;

			const c8 *symbolName = symbol->Name;

			if (!SymGetSymFromAddr(process, addr, NULL, symbol))
				continue;

			DWORD offset = 0;
			IMAGEHLP_LINE line = { 0 };
			line.SizeOfStruct = sizeof(line);

			SymGetLineFromAddr(process, addr, &offset, &line);	//Can fail, meaning that line is null

			if (line.FileName && strlen(line.FileName) > MAX_PATH)
				continue;

			struct CapturedStackTrace *capture = captured + i;
			capture->mod = modulePath;
			capture->sym = symbolName;

			if (moduleBase == (usz)processModule)
				capture->mod = String_lastPath(capture->mod);

			if(line.FileName)
				capture->fil = line.FileName;

			capture->lin = line.LineNumber;
		}

	if(hasSymbols && anySymbol)
		printf("Stacktrace:\n");

	else printf("Stacktrace: (No symbols)\n");

	for (usz i = 0; i < stackCount; ++i) {

		struct CapturedStackTrace capture = captured[i];

		if(!capture.sym)
			printf("%p\n", stackTrace[i]);

		else if(capture.lin)
			printf(
				"%p: %s!%s (%s, Line %u)\n", 
				stackTrace[i], 
				capture.mod, capture.sym, 
				capture.fil, capture.lin
			);

		else printf("%p: %s!%s\n", stackTrace[i], capture.mod, capture.sym);
	}

	if(hasSymbols)
		SymCleanup(process);
}

void Log_log(enum LogLevel lvl, enum LogOptions options, struct LogArgs args) {

	ns t = Timer_now();

	u32 thread = Thread_getId();

	const c8 *hrErr = "";

	if (lvl > LogLevel_Debug) {

		HRESULT hr = GetLastError();
		
		if (!SUCCEEDED(hr))
			FormatMessageA(

				FORMAT_MESSAGE_FROM_SYSTEM | 
				FORMAT_MESSAGE_ALLOCATE_BUFFER |
				FORMAT_MESSAGE_IGNORE_INSERTS,  

				NULL,
				hr,
				MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL),

				(LPTSTR) &hrErr,  

				0, NULL
			);
	}

	//Prepare for message

	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(handle, colors[lvl]);

	//[<thread> <time>]: <hr\n><ourStuff> <\n if enabled>

	bool hasTimestamp = options & LogOptions_Timestamp;
	bool hasThread = options & LogOptions_Thread;
	bool hasNewLine = options & LogOptions_NewLine;
	bool hasPrepend = hasTimestamp || hasThread;

	if (hasPrepend)
		printf("[");

	if (hasThread) {

		LongString str;
		Log_num10(str, thread);

		printf("%s", str);
	}

	if (hasTimestamp) {

		TimerFormat tf;
		Timer_format(t, tf);

		printf("%s%s", hasThread ? " " : "", tf);
	}

	if (hasPrepend)
		printf("]: ");

	if (hrErr)
		printf("%s\n", hrErr);

	//Print to console and debug window

	const c8 *newLine = hasNewLine ? "\n" : "";

	for (usz i = 0; i < args.argc; ++i)
		printf(
			"%s%s", 
			args.args[i], 
			newLine
		);

	//Debug utils such as output to VS

	if (!IsDebuggerPresent())
		return;

	for (usz i = 0; i < args.argc; ++i) {

		OutputDebugStringA(args.args[i]);

		if (hasNewLine)
			OutputDebugStringA("\n");
	}

	if (lvl >= LogLevel_Error)
		DebugBreak();
}
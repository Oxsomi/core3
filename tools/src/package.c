#include "platforms/file.h"
#include "platforms/ext/listx.h"
#include "platforms/ext/errorx.h"
#include "platforms/log.h"

Error onFile(FileInfo info, List *output) {
	return List_insertx(output, output->length, Buffer_createRef(&info, sizeof(info)));
}

int Program_run() {

	//For now we don't need to use any parameters from cli
	//In the future we might want to specify how we should pack this, 
	//but for now we just yeet the binaries in a zip file

	//First we search for the binaries in recursive fashion;
	//So it's bottom up

	List output = List_createEmpty(sizeof(FileInfo));

	Error e = File_recurse(".", (FileCallback) onFile, &output);

	if(e.genericError) {
		List_freex(&output);
		Error_printx(e, ELogLevel_Error, ELogOptions_Default);
		return 1;
	}

	//Now we need to create and write a zip file

	return 0;
}

void Program_exit() { }
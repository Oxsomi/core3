#include "platforms/file.h"

//TODO:!

Error File_removeVirtual(String loc) { loc; return Error_unimplemented(0); }

Error File_writeVirtual(Buffer buf, String loc) { buf; loc; return Error_unimplemented(0); }
Error File_readVirtual(String loc, Buffer *output) { loc; output; return Error_unimplemented(0); }
Error File_getInfoVirtual(String loc, FileInfo *info) { loc; info; return Error_unimplemented(0); }

Error File_foreachVirtual(String loc, FileCallback callback, void *userData, bool isRecursive) { 
	loc; callback; userData; isRecursive;
	return Error_unimplemented(0); 
}

Error File_queryFileObjectCountVirtual(String loc, FileType type, bool isRecursive, U64 *res) { 
	loc; type; isRecursive; res;
	return Error_unimplemented(0); 
}

Error File_queryFileObjectCountAllVirtual(String loc, bool isRecursive, U64 *res) { 
	loc; isRecursive; res;
	return Error_unimplemented(0); 
}

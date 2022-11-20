#pragma once
#include "formats/oiCA.h"
#include "formats/oiDL.h"
#include "platforms/platform.h"

//oiCA

inline Error CAFile_createx(CASettings settings, CAFile *caFile) {
	return CAFile_create(settings, Platform_instance.alloc, caFile);
}

inline Error CAFile_freex(CAFile *caFile) {
	return CAFile_free(caFile, Platform_instance.alloc);
}

inline Error CAFile_addEntryx(CAFile *caFile, CAEntry entry) {
	return CAFile_addEntry(caFile, entry, Platform_instance.alloc);
}

inline Error CAFile_writex(CAFile caFile, Buffer *result) {
	return CAFile_write(caFile, Platform_instance.alloc, result);
}

//inline Error CAFile_readx(Buffer file, CAFile *caFile) {								TODO:
//	return CAFile_read(file, Platform_instance.alloc, caFile);
//}

//oiDL

inline Error DLFile_createx(DLSettings settings, DLFile *dlFile) {
	return DLFile_create(settings, Platform_instance.alloc, dlFile);
}

inline Error DLFile_freex(DLFile *dlFile) {
	return DLFile_free(dlFile, Platform_instance.alloc);
}

inline Error DLFile_addEntryx(DLFile *dlFile, Buffer entry) {
	return DLFile_addEntry(dlFile, entry, Platform_instance.alloc);
}

inline Error DLFile_addEntryAsciix(DLFile *dlFile, String entry) {
	return DLFile_addEntryAscii(dlFile, entry, Platform_instance.alloc);
}

//Error DLFile_addEntryUTF8x(DLFile *dlFile, String entry, Allocator alloc);		TODO:

inline Error DLFile_writex(DLFile dlFile, Buffer *result) {
	return DLFile_write(dlFile, Platform_instance.alloc, result);
}

//inline Error DLFile_readx(Buffer file, DLFile *dlFile) {							TODO:
//	return DLFile_read(file, Platform_instance.alloc, dlFile);
//}

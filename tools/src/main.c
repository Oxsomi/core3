#include "types/time.h"
#include "types/buffer.h"
#include "types/file.h"
#include "platforms/ext/listx.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/formatx.h"
#include "platforms/log.h"
#include "formats/oiDL.h"
#include "cli.h"

int Program_run() {

	Operations_init();

	if(!CLI_execute(Platform_instance.args))
		return -1;

	return 0;
}

void Program_exit() { }

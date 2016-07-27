#include "log.h"
#include <execinfo.h>
#include <stdlib.h>
//#include "../mm/dsmtx_malloc.h"

void log_backtrace(void *addr)
{
	int j, nptrs;
	void *buffer[100];
	char **strings;
	LOG("backtrace() addr: %p\n", addr);
	// sysmm_sysMalloc_begin();
	nptrs = backtrace(buffer, 100);
	LOG("backtrace() returned %d addresses\n", nptrs);


	strings = backtrace_symbols(buffer, nptrs);
	if (strings == NULL) {
		perror("backtrace_symbols");
		exit(1);
	}

	for (j = 0; j < nptrs && j<7; j++)
		LOG("%s for %p sigsegv\n", strings[j], addr);

	free(strings);
	// sysmm_sysMalloc_end();
}



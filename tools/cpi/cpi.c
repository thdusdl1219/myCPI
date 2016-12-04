#include "cpi.h"
#include <stdio.h>
#include <asm/prctl.h>
#include <sys/prctl.h>

void* __cpi_table = 0;
void __cpi_init() {
  
  __cpi_table = mmap((void*) 0,
                CPI_TABLE_NUM_ENTRIES * sizeof(cpi_entry),
                PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);

  if(__cpi_table == (void *) -1) {
    perror("mmap error in cpi.cc");
  }

  int res = arch_prctl(ARCH_SET_GS, __cpi_table);
  if(res != 0) {
    perror("arch_prctl error in cpi.cc");
  }

}

void __cpi_set(void **ptr, void *val) {
  size_t offset = cpi_offset(ptr);
  __CPI_SET(offset, val);
}

void* __cpi_get(void **ptr) {
  size_t offset = cpi_offset(ptr);
  return __CPI_GET(offset);
}

void __cpi_fini() {
  
}

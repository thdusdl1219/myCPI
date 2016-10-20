#include "cpi.h"

void* __cpi_table = 0;
void __cpi_init() {
  
  __cpi_table = mmap((void*) 0,
                CPI_TABLE_NUM_ENTRIES * sizeof(cpi_entry),
                PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);

}

#include <cstdint>
#include <sys/mman.h>

#ifdef CPI_DEBUG
# include<cstdio>
# define DEBUG(...)               \
  do {                            \
    fprintf(stderr, __VA_ARGS__); \
  } while (0)
#else
# define DEBIG(...) do {} while(0)
#endif

#define CPI_TABLE_NUM_ENTRIES (1ull << (40 - 3))

typedef struct {
  void* data;
  uintptr_t lower;
  uintptr_t upper;
  void* id;
} cpi_entry;


void __cpi_init();
void __cpi_fini();



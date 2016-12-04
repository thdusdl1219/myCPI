#include <stdint.h>
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
#define CPI_ADDR_MASK (0xfffffffff8ull)
#define entry_size_n (sizeof(cpi_entry) / sizeof(void *))

typedef struct {
  void* data;
  void* id;
} cpi_entry;


void __cpi_init();
void __cpi_fini();

void __cpi_set(void **ptr, void *val);

// assembly

#define __CPI_SET(off, val) \
  __asm__ __volatile__ ("movq %0, %%gs: (%1)" : \
                          : "ir" (val), \
                          "r" (off));
#define __CPI_GET(off) \
  ({ size_t val; \
    __asm__ __volatile__ ("movq %%gs:(%1), %0" \
                          : "=r" (val) \
                          : "r" (off)); \
   val; })

// 
#define cpi_offset(address) \
  ((((size_t)(address)) & CPI_ADDR_MASK) * entry_size_n)

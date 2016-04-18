#ifndef LLVM_CORELAB_OBJTRACE_RUNTIME_H
#define LLVM_CORELAB_OBJTRACE_RUNTIME_H

#include <vector>
#include <map>
#include <algorithm>

#define DEBUG

#ifdef DEBUG
  #define DEBUG(fmt, ...) fprintf(stderr, "DEBUG: %s(): " fmt, \
      __func__, ##__VA_ARGS__)
#else
  #define DEBUG(fmt, ...)
#endif

typedef uint16_t InstrID;
typedef uint64_t FullID;

struct AllocTableElem {
  uint64_t size;
  FullID fullId;
};

struct MemoryAccessHistoryTableElem {
  FullID loadFullId;
  FullID storeFullId;
  FullID allocFullId;
  void *addr;
};

auto pred = [](const void *e1, const void *e2) -> bool { return e1 < e2; };
std::map<void *, struct AllocTableElem, decltype(pred)> AllocTable(pred); // This table is always sorted with ascending order of key (addr) 
std::vector<struct MemoryAccessHistoryTableElem> MemoryAccessHistoryTable;

FILE *profOut;

#endif

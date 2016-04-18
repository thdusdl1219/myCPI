#include <stdio.h>
#include <iostream>
#include <assert.h>

#include "objtraceruntime.h"

/****
 * objtraceruntime.cpp
 *
 * objTrace[Malloc,Calloc,Realloc,Free] functions deal with "AllocTable" which
 * maintains the information where was allocated, how big and who allocated.
 * objTrace[Load,Store]Instr functions check whether given "addr" indicates
 * heap space or not.
 *
 * Written by Bongjun.
 ****/

typedef uint16_t InstrID;
typedef uint64_t FullID;

extern "C"
void objTraceInitialize () {
  DEBUG("@@@ OBJTRACE RUNTIME PROFILER INITIALIZE @@@\n");
  profOut = fopen("prof.objtrace.log", "w");
  fprintf(profOut, "##### OBJTRACE RUNTIME PROFILER INITIALIZE #####\n");
}

extern "C"
void objTraceFinalize () {
  DEBUG("\n@@@ OBJTRACE RUNTIME PROFILER FINALIZE @@@\n");
  for (auto i : MemoryAccessHistoryTable) {
    DEBUG("ld: %lu, st: %lu, alloc: %lu, addr: %p\n", i.loadFullId,
        i.storeFullId,
        i.allocFullId,
        i.addr);
    fprintf(profOut, "%lu:%lu:%lu:%p\n", i.loadFullId,
        i.storeFullId,
        i.allocFullId,
        i.addr);
  }
  fprintf(profOut, "##### OBJTRACE RUNTIME PROFILER FINALIZE #####\n");
}

extern "C"
void objTraceLoadInstr (void* addr, FullID fullId) {
  DEBUG("RUNTIME: Load addr %p, fullId %lu\n", addr, fullId);

  auto it = AllocTable.lower_bound(addr);
  auto it2 = AllocTable.find(it->first);

  DEBUG("LOAD: it lower_bound address is %p\n", it->first);

  if (AllocTable.find(addr) != AllocTable.end()) {
    DEBUG("LOAD: start address  %p\n\n", it2->first);
    MemoryAccessHistoryTable.push_back({fullId, 0 ,AllocTable[addr].fullId, addr});
  } else if (it->first != NULL) {
    size_t distance = std::distance(AllocTable.begin(), it);
    //DEBUG("LOAD: distance %zu\n", distance);
    //assert(distance > 0 && "Distance should be larger than 0?");
    size_t i = 0;
    for (auto it3 : AllocTable) { // XXX: is it a best method?
      if (distance == 0) {
        DEBUG("STORE: It might be a access to global variable\n\n");
        break;
      } else if (i == distance-1) {
        char *cp = reinterpret_cast<char*>(it3.first);
        void *maxAddr = (void*)(cp + it3.second.size);
        if (addr < maxAddr) {
          // TODO: need another param as align and check it.
          DEBUG("LOAD: addr %p ~ %p\n\n", it3.first, maxAddr);
          MemoryAccessHistoryTable.push_back({fullId, 0, AllocTable[it3.first].fullId, addr});
          break;
        } else {
          DEBUG("LOAD: It might be a access to stack variable instead of heap access\n\n");
          break;
        }
      }
      i++;
    } // for
  }
}

extern "C"
void objTraceStoreInstr (void* addr, FullID fullId) {
  DEBUG("RUNTIME: Store addr %p, FullId %lu\n", addr, fullId);

  auto it = AllocTable.lower_bound(addr);
  auto it2 = AllocTable.find(it->first);

  DEBUG("STORE: it lower_bound address is %p\n", it->first);

  if (AllocTable.find(addr) != AllocTable.end()) {
    DEBUG("STORE: start address %p\n\n", it2->first);
    MemoryAccessHistoryTable.push_back({0, fullId, AllocTable[addr].fullId, addr});
  } else if (it->first != NULL) {
    size_t distance = std::distance(AllocTable.begin(), it);
    //DEBUG("STORE: distance %zu\n", distance);
    //assert(distance > 0 && "Distance should be lager than 0?");
    size_t i = 0;
    for (auto it3 : AllocTable) { // XXX: is it best method?
      if (distance == 0) {
        DEBUG("STORE: It might be a access to global variable\n\n");
        break;
      } else if (i == distance-1) {
        char *cp = reinterpret_cast<char*>(it3.first);
        void *maxAddr = (void*)(cp + it3.second.size);
        if (addr < maxAddr) {
          // TODO: need another param as align and check it.
          DEBUG("STORE: addr %p ~ %p\n\n", it3.first, maxAddr);
          MemoryAccessHistoryTable.push_back({0, fullId, AllocTable[it3.first].fullId, addr});
          break;
        } else {
          DEBUG("STORE: It might be a access to stack variable instead of heap access\n\n");
          break;
        }
      }
      i++;
    } // for
  }
}

extern "C" void*
objTraceMalloc (size_t size, FullID fullId){
  void* addr = malloc (size);
  DEBUG("RUNTIME: malloc addr %p, fullId %lu\n\n", addr, fullId);
  AllocTableElem elem = {size, fullId};
  AllocTable[addr] = elem;
  /*std::cout << "AllocTable:\n";
  for(auto i = AllocTable.begin(); i != AllocTable.end(); ++i) {
    std::cout << i->first << " " << i->second.size << " " << i->second.fullId << " / ";
  }
  std::cout << std::endl;*/
  return addr;
}

extern "C" void*
objTraceCalloc (size_t num, size_t size, FullID fullId){
  void* addr = calloc (num, size);
  DEBUG("RUNTIME: calloc addr %p, num %zu, size %zu, fullId %lu\n\n", addr, num, size, fullId);
  AllocTableElem elem = {num*size, fullId};
  AllocTable[addr] = elem;
  /*std::cout << "AllocTable:\n";
  for(auto i = AllocTable.begin(); i != AllocTable.end(); ++i) {
    std::cout << i->first << " " << i->second.size << " " << i->second.fullId << " / ";
  }
  std::cout << std::endl;*/
  return addr;
}

extern "C" void*
objTraceRealloc (void* addr, size_t size, FullID fullId){
  void* naddr = NULL;
  naddr = realloc (addr, size);
  DEBUG("RUNTIME: realloc addr %p, naddr %p, size %zu, fullID %lu\n\n", addr, naddr, size, fullId);
  //auto it = std::find_if(AllocTable.begin(),
  //                       AllocTable.end(),
  //                       [&addr](AllocTableElem& elem){ return (elem.addr == addr); });
  assert((AllocTable.find(addr) != AllocTable.end()) \
         && "Something wrong! Realloc have to be called after Malloc or Calloc is called.");
  AllocTable.erase(addr);
  AllocTable[naddr] = {size, fullId};
  /*std::cout << "AllocTable:\n";
  for(auto i = AllocTable.begin(); i != AllocTable.end(); ++i) {
    std::cout << i->first << " " << i->second.size << " " << i->second.fullId << " / ";
  }
  std::cout << std::endl;*/
  return naddr;
}

extern "C" void
objTraceFree (void* addr, FullID fullId){
  free (addr);
  DEBUG("RUNTIME: free addr %p, fullId %lu\n\n", addr, fullId);
  //AllocTable.erase(std::remove_if(AllocTable.begin(),
  //           AllocTable.end(),
  //           [addr](AllocTableElem& elem){ return (elem.addr == addr); }), AllocTable.end());
  assert((AllocTable.find(addr) != AllocTable.end()) && "Something Wrong! Free should have a address which was surely allocated before.");
  AllocTable.erase(addr);
  /*std::cout << "AllocTable:\n";
  for(auto i = AllocTable.begin(); i != AllocTable.end(); ++i) {
    std::cout << i->first << " " << i->second.size << " " << i->second.fullId << " / ";
  }
  std::cout << std::endl;*/
}

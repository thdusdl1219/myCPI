enum {
  THREAD_EXIT = -1,
  HEAP_ALLOC_REQ = 0,
  HEAP_ALLOC_REQ_ACK = 1,
  LOAD_REQ = 2,
  LOAD_REQ_ACK = 3,
  STORE_REQ = 4,
  STORE_REQ_ACK = 5,
  MMAP_REQ = 6,
  MMAP_REQ_ACK = 7,
  MEMSET_REQ = 8,
  MEMSET_REQ_ACK = 9,
  MEMCPY_REQ = 10,
  MEMCPY_REQ_ACK = 11,
  MEMMOVE_REQ = 12,
  MEMMOVE_REQ_ACK = 13,
  INVALID_REQ = 14,
  INVALID_REQ_ACK = 15,
  RELEASE_REQ = 16,
  RELEASE_REQ_ACK = 17,
  HEAP_SEGFAULT_REQ = 28, 
  HEAP_SEGFAULT_REQ_ACK = 29, 
  GLOBAL_SEGFAULT_REQ = 30, 
  GLOBAL_SEGFAULT_REQ_ACK = 31, 
  GLOBAL_INIT_COMPLETE_SIG = 32,
  GLOBAL_INIT_COMPLETE_SIG_ACK = 33
};

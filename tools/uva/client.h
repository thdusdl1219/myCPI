namespace corelab {
  namespace UVA {
    extern "C" void UVAClientCallbackSetter(CommManager *comm);
    extern "C" void UVAClientInitializer(CommManager *comm, uint32_t isGVInitializer, uint32_t destid);
    extern "C" void UVAClientFinalizer();

    extern "C" void UVAClientLoadInstr(void *addr);
    extern "C" void UVAClientStoreInstr(void *addr);
    
    extern "C" void uva_load(size_t len, void *addr);
    extern "C" void uva_store(size_t len, void *data, void *addr); 
    extern "C" void *uva_memset(void *addr, int value, size_t num);
    extern "C" void *uva_memcpy(void *dest, void *src, size_t num);
    
    extern "C" void uva_acquire();
    extern "C" void uva_release();

    extern "C" void uva_sync();

    extern "C" void sendInitCompleteSignal();

    extern "C" void waiter(void **stackAddr, uint32_t numGV);
  }
}

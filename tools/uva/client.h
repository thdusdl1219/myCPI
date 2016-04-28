namespace corelab {
  namespace UVA {
    extern "C" void UVAClientInitialize(uint32_t isGVInitializer);
    extern "C" void UVAClientFinalize();

    extern "C" void UVAClientLoadInstr(void *addr);
    extern "C" void UVAClientStoreInstr(void *addr);
    
    extern "C" void uva_load(size_t len, void *addr);
    extern "C" void uva_store(size_t len, void *data, void *addr); 
    extern "C" void *uva_memset(void *addr, int value, size_t num);
    extern "C" void *uva_memcpy(void *dest, void *src, size_t num);
  }
}

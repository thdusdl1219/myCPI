namespace corelab {
  namespace UVA {
    extern "C" void UVAClientInitialize(uint32_t isGVInitializer);
    extern "C" void UVAClientFinalize();

    extern "C" void UVAClientLoadInstr(void *addr);
    extern "C" void UVAClientStoreInstr(void *addr);
    
    extern "C" void uva_load(void *addr, size_t len);
    extern "C" void uva_store(void *addr, size_t len, void *data); 
  }
}

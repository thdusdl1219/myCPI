
namespace corelab {
  namespace UVA {
    extern "C" void UVAServerInitialize();
    extern "C" void UVAServerFinalize();
    void* ServerOpenRoutine(void*);
    void* ClientRoutine(void*);

    /* These two function do nothing. Everybody are client */
    extern "C" void uva_server_load(void *addr, uint64_t len);
    extern "C" void uva_server_store(void *addr, uint64_t len, void *data); 
  }
}


namespace corelab {
  namespace UVA {
    struct RuntimeClientConnElem {
      int *clientId;
      QSocket *socket;
    };
    extern "C" void UVAServerInitialize();
    extern "C" void UVAServerFinalize();
    void* ServerOpenRoutine(void*);
    void* ClientRoutine(void*);

    /* These two function do nothing. Everybody are client */
    extern "C" void uva_server_load(void *addr, size_t len);
    extern "C" void uva_server_store(void *addr, size_t len, void *data); 
    
    std::map<int *, QSocket *> RuntimeClientConnTb;
  }
}

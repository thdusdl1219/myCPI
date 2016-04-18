


namespace corelab {
  namespace UVA {
    extern "C" void UVAClientInitialize();
    extern "C" void UVAClientFinalize();

    extern "C" void UVAClientLoadInstr(void *addr);
    extern "C" void UVAClientStoreInstr(void *addr);
  }
}

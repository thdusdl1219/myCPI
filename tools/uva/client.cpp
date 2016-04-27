#include <cstring>
#include <cstdio>
#include <cassert>
#include <unistd.h>
#include <csignal>

#include "client.h"
#include "qsocket.h"
#include "mm.h" // FIXME
#include "uva_manager.h"
#include "xmem_info.h"

#include "log.h"
#include "hexdump.h"

#define GET_PAGE_ADDR(x) ((x) & 0xFFFFF000)

#define LOCALTEST 0
#define CORELAB_SERVER_TEST 1

#define FORKTEST 0

namespace corelab {
  namespace UVA {
    static QSocket *Msocket;

		struct sigaction segvAction;
		static void segfaultHandler (int sig, siginfo_t* si, void* unused);

    extern "C" void UVAClientInitialize() {
      char ip[20];
      char port[10];

      FILE *fdesc = NULL;
      fdesc = fopen("server_desc", "r");
      
      if (fdesc != NULL) {
        fscanf(fdesc, "%20s %10s", ip, port);
        fclose(fdesc);
      } else {
        printf("Server IP : ");
        scanf("%20s", ip);
        printf("Server Port : ");
        scanf("%10s", port);
      }
      
      printf("[CLIENT] ip (%s), port (%s)\n", ip, port);

#if FORKTEST
      pid_t pid;
      pid = fork();
      if (pid == -1)
        assert(0 && "fork failed");
      else if (pid == 0) {
        printf("[CLIENT] child\n");
        Msocket = new QSocket();
        Msocket->connect(ip, port);
  
        //XMemory::XMemoryManager::initialize(Msocket);
        UVAManager::initialize (Msocket);
      } else {
        printf("[CLIENT] parent\n");
        Msocket = new QSocket();
        Msocket->connect(ip, port);
      
        //XMemory::XMemoryManager::initialize(Msocket);
        UVAManager::initialize (Msocket);

      }
#else
      Msocket = new QSocket();
      Msocket->connect(ip, port);

      //XMemory::XMemoryManager::initialize(Msocket);
      UVAManager::initialize (Msocket);

      // segfault handler
			segvAction.sa_flags = SA_SIGINFO | SA_NODEFER;
			sigemptyset (&segvAction.sa_mask);
			segvAction.sa_sigaction = segfaultHandler;
			int hr = sigaction (SIGSEGV, &segvAction, NULL);
			assert (hr != -1);
#endif

      /* For synchronized clients start */
      Msocket->receiveQue();
      int mayIstart = Msocket->takeWordF();
      if (mayIstart == 1) {
        return;
      } else {
        assert(false && "[CLIENT] server doesn't allow me start.\n");
      }
    }
    extern "C" void UVAClientFinalize() {
      // Msocket->sendQue();
      void *ptNoConstBegin;
      void *ptNoConstEnd;
      //void *ptConstBegin;
      //void *ptConstEnd;
      UVAManager::getFixedGlobalAddrRange(&ptNoConstBegin, &ptNoConstEnd/*, &ptConstBegin, &ptConstEnd*/);
      xmemDumpRange(ptNoConstBegin, 32);
      //xmemDumpRange(ptConstBegin, 32);
      Msocket->disconnect();
    }
		/*** Internals ***/
		static void segfaultHandler (int sig, siginfo_t* si, void* unused) {
			void *fault_addr = si->si_addr;
      LOG("[client] segfaultHandler | fault_addr : %p\n", fault_addr);
      mmap((void*) GET_PAGE_ADDR((uintptr_t)si->si_addr), 
          PAGE_SIZE, 
          PROT_WRITE | PROT_READ,
          MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, (off_t) 0);
      LOG("[client] segfaultHandler | mmap page_addr : %p, mmap size: %d | handling  complete\n", (void*) GET_PAGE_ADDR((uintptr_t)si->si_addr), PAGE_SIZE);
      if (fault_addr < (void*)0x15000000) assert(0);
      void *ptNoConstBegin;
      void *ptNoConstEnd;
      //void *ptConstBegin;
      //void *ptConstEnd;
      UVAManager::getFixedGlobalAddrRange(&ptNoConstBegin, &ptNoConstEnd/*, &ptConstBegin, &ptConstEnd*/);
      if (ptNoConstBegin <= fault_addr && fault_addr < (void*)0x16000000) {
        //if (UVAManager::isFixedGlobalAddr(fault_addr)) {
        LOG("[client] segfaultHandler | fault_addr is in FixedGlobalAddr space %p\n",ptNoConstBegin);
        Msocket->pushWordF(8); // send GLOBAL_SEGFAULT_REQ
        Msocket->pushRangeF(&ptNoConstBegin, sizeof(void*));
        Msocket->pushRangeF(&ptNoConstEnd, sizeof(void*));
        //Msocket->pushRangeF(&ptConstBegin, sizeof(void*));
        //Msocket->pushRangeF(&ptConstEnd, sizeof(void*));
        Msocket->sendQue();

        Msocket->receiveQue();
        Msocket->takeRangeF(ptNoConstBegin, (uintptr_t)ptNoConstEnd - (uintptr_t)ptNoConstBegin);
        //Msocket->takeRangeF(ptConstBegin, (uintptr_t)ptConstEnd - (uintptr_t)ptConstBegin);
        LOG("[client] segfaultHandler | get global variables done\n");
        LOG("[client] segfaultHandler (TEST print)\n");
        xmemDumpRange(ptNoConstBegin, 32);
        //xmemDumpRange(ptConstBegin, 32);
      }
      return;
      /*
			if (hasPage (addr)) {
				UVAManager::resolveModified (addr);
			}
			else {
				socket->pushWord (CLIENT_REQUEST);
				socket->pushWord (QSOCKET_WORD (addr));
				socket->sendQue ();

				UVAManager::fetchIn (socket, addr);
			}*/
		}
    extern "C" void uva_load(void *addr, size_t len) {
      UVAManager::loadHandler(Msocket, addr, len);
    }

    extern "C" void uva_store(void *addr, size_t len, void *data) {
      UVAManager::storeHandler(Msocket, addr, len, data);
    }
  }
}

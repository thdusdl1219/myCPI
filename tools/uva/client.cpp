#include <cstring>
#include <cstdio>
#include <cassert>
#include <unistd.h>
#include <csignal>

#include "client.h"
#include "qsocket.h"
#include "mm.h"

#include "log.h"
#include "hexdump.h"

#define GET_PAGE_ADDR(x) ((x) & 0xFFFFF000)

#define TEST true 
#define LOCALTEST false
#define FORKTEST 0

namespace corelab {
  namespace UVA {
    static QSocket *Msocket;

		struct sigaction segvAction;
		static void segfaultHandler (int sig, siginfo_t* si, void* unused);

    extern "C" void UVAClientInitialize() {
      char ip[20];
      char port[10];

#if LOCALTEST
        strcpy(ip,"127.0.0.1");
#else
        LOG("UVA client : uva client init\n");
        strcpy(ip,"192.168.11.2");
#endif

#if TEST
        strcpy(port, "5959");
#else
        // printf("Server IP : ");
        // scanf("%20s", ip);
        printf("Server Port: ");
        scanf("%10s", port);
#endif
      
#if FORKTEST
      pid_t pid;
      pid = fork();
      if (pid == -1)
        assert(0 && "fork failed");
      else if (pid == 0) {
        printf("[CLIENT] child\n");
        Msocket = new QSocket();
        Msocket->connect(ip, port);
      
        XMemory::XMemoryManager::initialize(Msocket);
      } else {
        printf("[CLIENT] parent\n");
        Msocket = new QSocket();
        Msocket->connect(ip, port);
      
        XMemory::XMemoryManager::initialize(Msocket);
      }
#else
      Msocket = new QSocket();
      Msocket->connect(ip, port);

      XMemory::XMemoryManager::initialize(Msocket);
			
      // segfault handler
			segvAction.sa_flags = SA_SIGINFO | SA_NODEFER;
			sigemptyset (&segvAction.sa_mask);
			segvAction.sa_sigaction = segfaultHandler;
			int hr = sigaction (SIGSEGV, &segvAction, NULL);
			assert (hr != -1);
#endif
      
    }
    extern "C" void UVAClientFinalize() {
      // Msocket->sendQue();
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
  }
}

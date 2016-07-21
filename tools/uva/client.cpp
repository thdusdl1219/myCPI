#include <cstring>
#include <cstdio>
#include <cassert>
#include <unistd.h>
#include <csignal>
#include <stdint.h>

#include "../esperanto/comm/comm_manager.h"
#include "client.h"
#include "qsocket.h"
#include "mm.h" // FIXME I don't wanna access directly to mm module (better: through xmem)
#include "uva_manager.h"
#include "xmem_info.h"
#include "uva_macro.h"

#include "TimeUtil.h"

#include "log.h"
#include "hexdump.h"

#include "uva_comm_enum.h"

#include "uva_debug_eval.h"

#define GET_PAGE_ADDR(x) ((x) & 0xFFFFF000)

#define HLRC


#define LOCALTEST 0
#define CORELAB_SERVER_TEST 1

extern "C" void __decl_const_global_range__();
extern "C" void __fixed_global_initializer__();
extern "C" void uva_sync();
extern "C" void sendInitCompleteSignal();

namespace corelab {
  namespace UVA {
    //static QSocket *Msocket;
    static CommManager *comm;
    static int *destid = NULL;

		struct sigaction segvAction;
		static void segfaultHandler (int sig, siginfo_t* si, void* unused);
		static void segfaultHandlerForHLRC (int sig, siginfo_t* si, void* unused);

		static inline void* truncToPageAddr (void *addr) {
			return (void *)((XmemUintPtr)addr & XMEM_PAGE_MASK);
		}

    extern "C" void UVACallbackSetter(CommManager *comm) { 
      // XXX Currently, no need callback in client.
    }

    extern "C" void UVAClientInitializer(CommManager *comm_, uint32_t isGVInitializer, int *destid_) {

      // FIXME: for sync
      sleep(5);

      comm = comm_;
      destid = destid_;
/*
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
      
#ifdef DEBUG_UVA
      printf("[CLIENT] ip (%s), port (%s)\n", ip, port);
#endif

      //Msocket = new QSocket();
      //Msocket->connect(ip, port);

      //XMemory::XMemoryManager::initialize(Msocket);
      //UVAManager::initialize (Msocket);
*/
      UVAManager::initialize (comm, destid);

      // segfault handler
			segvAction.sa_flags = SA_SIGINFO | SA_NODEFER;
			sigemptyset (&segvAction.sa_mask);
#ifdef HLRC
			segvAction.sa_sigaction = segfaultHandlerForHLRC;
#else
			segvAction.sa_sigaction = segfaultHandler;
#endif
			int hr = sigaction (SIGSEGV, &segvAction, NULL);
			assert (hr != -1);

      /* For declaration Constant Gloabal Variables Range */
      __decl_const_global_range__();

      /* For synchronized clients start */
      if(!isGVInitializer) {
        //Msocket->receiveQue();
        comm->receiveQue(destid);
        //int mayIstart = Msocket->takeWordF();
        uint32_t mayIstart = comm->takeWord(destid);
        if (mayIstart == 1) {
#ifdef DEBUG_UVA
          printf("[CLIENT] I got start permission !!\n");
#endif
          return;
        } else {
          assert(false && "[CLIENT] server doesn't allow me start.\n");
        }
      } else { /* GV Initializer */
        __fixed_global_initializer__();
        uva_sync();
        sendInitCompleteSignal();
      }
    }
    extern "C" void UVAClientFinalizer() {
      // Msocket->sendQue();
      void *ptNoConstBegin;
      void *ptNoConstEnd;
      //void *ptConstBegin;
      //void *ptConstEnd;
      UVAManager::getFixedGlobalAddrRange(&ptNoConstBegin, &ptNoConstEnd/*, &ptConstBegin, &ptConstEnd*/);
      xmemDumpRange(ptNoConstBegin, 32);
      //xmemDumpRange(ptConstBegin, 32);
      //Msocket->disconnect();
    }
		/*** Internals ***/
		static void segfaultHandler (int sig, siginfo_t* si, void* unused) {
			void *fault_addr = si->si_addr;
#ifdef DEBUG_UVA
      LOG("[client] segfaultHandler | fault_addr : %p\n", fault_addr);
#endif
      
      if (fault_addr < (void*)0x15000000) {
        LOG_BACKTRACE(fault_addr);
        assert(0 && "fault_addr : under 0x15000000");
      }
      if (fault_addr > (void*)0x38000000) {
        LOG_BACKTRACE(fault_addr);
        assert(0 && "fault_addr : above 0x38000000");
      }
      mmap((void*) GET_PAGE_ADDR((uintptr_t)si->si_addr), 
          PAGE_SIZE, 
          PROT_WRITE | PROT_READ,
          MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, (off_t) 0);
      
#ifdef DEBUG_UVA
      LOG("[client] segfaultHandler | mmap page_addr : %p, mmap size: %d | handling  complete\n", 
          (void*) GET_PAGE_ADDR((uintptr_t)si->si_addr), PAGE_SIZE);
#endif
      
      
      void *ptNoConstBegin;
      void *ptNoConstEnd;
      
      UVAManager::getFixedGlobalAddrRange(&ptNoConstBegin, &ptNoConstEnd/*, &ptConstBegin, &ptConstEnd*/);
      if (ptNoConstBegin <= fault_addr && fault_addr < (void*)0x16000000) {
#ifdef DEBUG_UVA
        LOG("[client] segfaultHandler | fault_addr is in FixedGlobalAddr space %p\n",ptNoConstBegin);
#endif
        comm->pushWord(GLOBAL_SEGFAULT_HANDLER, GLOBAL_SEGFAULT_REQ, destid); // send GLOBAL_SEGFAULT_REQ
        //uint32_t intAddrBegin = reinterpret_cast<uint32_t>(ptNoConstBegin);
        //uint32_t intAddrEnd = reinterpret_cast<uint32_t>(ptNoConstEnd);
        uint32_t intAddrBegin;
        uint32_t intAddrEnd;
        
        memcpy(&intAddrBegin, &ptNoConstBegin, 4);
        memcpy(&intAddrEnd, &ptNoConstEnd, 4);
        
        comm->pushWord(GLOBAL_SEGFAULT_HANDLER, intAddrBegin, destid);
        comm->pushWord(GLOBAL_SEGFAULT_HANDLER, intAddrEnd, destid);
        comm->sendQue(GLOBAL_SEGFAULT_HANDLER, destid);

        comm->receiveQue(destid);
        uint32_t ack = comm->takeWord(destid);
        assert(ack == GLOBAL_SEGFAULT_REQ_ACK && "wrong!!!");
        comm->takeRange(ptNoConstBegin, (uintptr_t)ptNoConstEnd - (uintptr_t)ptNoConstBegin, destid);
#ifdef DEBUG_UVA
        LOG("[client] segfaultHandler | get global variables done\n");
        LOG("[client] segfaultHandler (TEST print)\n");
        hexdump("segfault", ptNoConstBegin, 24);
#endif
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
		} // segfaultHandler

		static void segfaultHandlerForHLRC (int sig, siginfo_t* si, void* unused) {
#ifdef UVA_EVAL
      StopWatch watch;
      watch.start();
#endif
			void *fault_addr = si->si_addr;
#ifdef DEBUG_UVA
      LOG("[client] segfaultHandler | fault_addr : %p\n", fault_addr);
#endif
      
      if (fault_addr < (void*)0x15000000) {
        LOG_BACKTRACE(fault_addr);
        assert(0 && "fault_addr : under 0x15000000");
      }
      if (fault_addr > (void*)0x38000000) {
        LOG_BACKTRACE(fault_addr);
        assert(0 && "fault_addr : above 0x38000000");
      }
      mmap((void*) GET_PAGE_ADDR((uintptr_t)si->si_addr), 
          PAGE_SIZE, 
          PROT_WRITE | PROT_READ,
          MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, (off_t) 0);
      
#ifdef DEBUG_UVA
      LOG("[client] segfaultHandler | mmap page_addr : %p, mmap size: %d | handling  complete\n", 
          (void*) GET_PAGE_ADDR((uintptr_t)si->si_addr), PAGE_SIZE);
#endif
      
      
      void *ptNoConstBegin;
      void *ptNoConstEnd;
      
      UVAManager::getFixedGlobalAddrRange(&ptNoConstBegin, &ptNoConstEnd/*, &ptConstBegin, &ptConstEnd*/);
      if (ptNoConstBegin <= fault_addr && fault_addr < (void*)0x16000000) {
#ifdef DEBUG_UVA
        LOG("[client] segfaultHandler | fault_addr is in FixedGlobalAddr space %p\n",ptNoConstBegin);
#endif
        comm->pushWord(GLOBAL_SEGFAULT_HANDLER, GLOBAL_SEGFAULT_REQ, destid); // send GLOBAL_SEGFAULT_REQ
        //uint32_t intAddrBegin = reinterpret_cast<uint32_t>(ptNoConstBegin);
        //uint32_t intAddrEnd = reinterpret_cast<uint32_t>(ptNoConstEnd);
        uint32_t intAddrBegin;
        uint32_t intAddrEnd;
        
        memcpy(&intAddrBegin, &ptNoConstBegin, 4);
        memcpy(&intAddrEnd, &ptNoConstEnd, 4);
        
        comm->pushWord(GLOBAL_SEGFAULT_HANDLER, intAddrBegin, destid);
        comm->pushWord(GLOBAL_SEGFAULT_HANDLER, intAddrEnd, destid);
        comm->sendQue(GLOBAL_SEGFAULT_HANDLER, destid);

/*#ifdef UVA_EVAL
        StopWatch watchRecv;
        watchRecv.start();
#endif*/
        comm->receiveQue(destid);
/*#ifdef UVA_EVAL
        watchRecv.end();
        FILE *fp = fopen("uva-eval.txt", "a");
        fprintf(fp, "SEGFAULT | RECV %lf\n", watchRecv.diff());
#endif*/
        uint32_t ack = comm->takeWord(destid);
        assert(ack == GLOBAL_SEGFAULT_REQ_ACK && "wrong!!!");
        comm->takeRange(ptNoConstBegin, (uintptr_t)ptNoConstEnd - (uintptr_t)ptNoConstBegin, destid);
#ifdef DEBUG_UVA
        LOG("[client] segfaultHandler | get global variables done\n");
        LOG("[client] segfaultHandler (TEST print)\n");
        hexdump("segfault", fault_addr, 24);
#endif
#ifdef UVA_EVAL
        watch.end();
        FILE *fp = fopen("uva-eval.txt", "a");
        fprintf(fp, "SEGFAULT %lf %d\n", watch.diff(), 16 + ((uintptr_t)ptNoConstEnd - (uintptr_t)ptNoConstBegin));
        fclose(fp);
#endif
      } else if ((void*)0x18000000 <= fault_addr && fault_addr < (void*)0x38000000) {
#ifdef DEBUG_UVA
        LOG("[client] segfaultHandler | fault_addr is in UVA HeapAddr space %p\n",fault_addr);
#endif
        uint32_t intFaultAddr;
        memcpy(&intFaultAddr, &fault_addr, 4);
        comm->pushWord(HEAP_SEGFAULT_HANDLER, HEAP_SEGFAULT_REQ, destid);
        comm->pushWord(HEAP_SEGFAULT_HANDLER, intFaultAddr, destid);
        comm->sendQue(HEAP_SEGFAULT_HANDLER, destid);

/*#ifdef UVA_EVAL
        StopWatch watchRecv;
        watchRecv.start();
#endif*/
        comm->receiveQue(destid);
/*#ifdef UVA_EVAL
        watchRecv.end();
        FILE *fp = fopen("uva-eval.txt", "a");
        fprintf(fp, "SEGFAULT | RECV %lf\n", watchRecv.diff());
#endif*/
        comm->takeRange(truncToPageAddr(fault_addr), PAGE_SIZE, destid);
#ifdef DEBUG_UVA
        LOG("[client] segfaultHandler | getting a page in heap is done\n");
        LOG("[client] segfaultHandler (TEST print)\n");
        hexdump("segfault", fault_addr, 24);
#endif
#ifdef UVA_EVAL
        watch.end();
        FILE *fp = fopen("uva-eval.txt", "a");
        fprintf(fp, "SEGFAULT %lf %d\n", watch.diff(), 8 + PAGE_SIZE);
        fclose(fp);
#endif
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
		} // segfaultHandler
    extern "C" void uva_load(size_t len, void *addr) {
#ifndef HLRC
      UVAManager::loadHandler(comm, destid, len, addr);
#endif
      return;
    }

    extern "C" void uva_store(size_t len, void *data, void *addr) {
#ifdef HLRC
      UVAManager::storeHandlerForHLRC(len, data, addr);
#else
      UVAManager::storeHandler(comm, destid, len, data, addr);
#endif
      return;
    }

    extern "C" void *uva_memset(void *addr, int value, size_t num) {
#ifdef HLRC
      return UVAManager::memsetHandlerForHLRC(addr, value, num);
#else
      return UVAManager::memsetHandler(comm, destid, addr, value, num);
#endif
    }

    extern "C" void *uva_memcpy(void *dest, void *src, size_t num) {
#ifdef HLRC
      return UVAManager::memcpyHandlerForHLRC(comm, destid, dest, src, num);
#else
      return UVAManager::memcpyHandler(comm, destid, dest, src, num);
#endif
    }
    
    extern "C" void uva_acquire() {
#ifdef HLRC
      //UVAManager::acquireHandler(Msocket);
#endif
    }
    
    extern "C" void uva_release() {
#ifdef HLRC
      //UVAManager::releaseHandler(Msocket);
#endif
    }
    
    extern "C" void uva_sync() {
#ifdef HLRC
      UVAManager::syncHandler(comm, destid);
#endif
    }
    
    extern "C" void sendInitCompleteSignal() {
      //Msocket->pushWordF(GLOBAL_INIT_COMPLETE_SIG); // Init complete signal
      //Msocket->sendQue();

      //Msocket->receiveQue();
      //int ack = Msocket->takeWordF(); 
      comm->pushWord(GLOBAL_INIT_COMPLETE_HANDLER, GLOBAL_INIT_COMPLETE_SIG, destid); // Init complete signal
      comm->sendQue(GLOBAL_INIT_COMPLETE_HANDLER, destid);

      comm->receiveQue(destid);
      uint32_t ack = comm->takeWord(destid); 
      assert(ack == GLOBAL_INIT_COMPLETE_SIG_ACK && "Server says \"Hey, I didn't get global initialization complete signal correctly. \"");
      return;
    }
  }
}

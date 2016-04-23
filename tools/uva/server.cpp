#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
#include <cassert>

#include "mm.h"
#include "qsocket.h"
#include "server.h"
#include "log.h"
#include "hexdump.h"

#define TEST 1

using namespace corelab::XMemory;
namespace corelab {
  namespace UVA {

    static QSocket* socket;
    pthread_t openThread; 

    extern "C" void UVAServerInitialize() {
        LOG("UVA manager(server) : initialize\n");
        pthread_create(&openThread, NULL, ServerOpenRoutine, NULL); 
    }

    extern "C" void UVAServerFinalize() {
        pthread_join(openThread, NULL);
    }
    void* ServerOpenRoutine(void *) {
      LOG("UVA manager(server) : after pthread_create serveropenroutine\n");
      char port[10];

#if TEST
        strcpy(port, "5959");
#else
        fprintf(stderr, "system : Enter port to listen : ");
        scanf("%s", port);
#endif

      socket = new QSocket ();
      socket->setClientRoutine(ClientRoutine);
      socket->open(port);
      fprintf (stderr, "system : client connected.\n");
      return NULL;
    }

    void* ClientRoutine(void * data) {
      int *clientId = (int *)data;
      pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
      int mode;
      int datalen;
      int rval = 0;
      void* HeapTop;
      void * allocAddr = NULL;
      char *valOfRequestedAddr = NULL;

      while(true) {
        pthread_mutex_lock(&mutex);
        socket->receiveQue(clientId);
        mode = socket->takeWordF(clientId);
        datalen = socket->takeWordF(clientId);
        fprintf(stderr, "[system] (mode, datalen) : (%d , %d)\n", mode, datalen);
        int lenbuf;
        uint64_t lenType;
        void *requestedAddr;
        void *valueToStore;
        switch(mode) {
          case -1 :
            pthread_exit(&rval);
            fprintf(stderr, "[system] thread exit!");
            break;
          case 0 : /*** allocate request ***/
            socket->takeRange(&lenbuf, datalen, clientId);
            fprintf(stderr, "[system] (takeRange) : (%d)\n", lenbuf);
            // memory operation
            
            HeapTop = XMemoryManager::getHeapTop(); 
            fprintf(stderr, "[system] heapTop : %p\n", HeapTop);
            // allocAddr = XMemoryManager::allocate(lenbuf, true);
            allocAddr = XMemoryManager::allocateServer(HeapTop, lenbuf);
            HeapTop = XMemoryManager::getHeapTop(); 
            fprintf(stderr, "[system] allocAddr : (%p)\n", allocAddr);
            fprintf(stderr, "[system] heapTop : %p\n", HeapTop);
            
            // memory operation end
            socket->pushWordF(1, clientId);
            socket->pushWordF(sizeof(allocAddr), clientId);
            socket->pushRangeF(&allocAddr, sizeof(allocAddr), clientId);
            socket->sendQue(clientId);
            break;
          case 1 : /*** wrong request ***/
            assert(false && "something is strange!");
            break;
          case 2 : /*** load request ***/
            LOG("[server] get Load request from client\n");

            // receive type length (how much load in byte)
            lenType = socket->takeWordF(clientId);
            LOG("[server] type length (how much): %d\n", lenType);

            // receive requested addr (where)
            socket->takeRangeF(&requestedAddr, datalen, clientId);
            LOG("[server] requestedAddr (where): (%p)\n", requestedAddr);
            
            // send ack with value (what to load)
            socket->pushWordF(3, clientId);
            socket->pushWordF(lenType, clientId);
            socket->pushRangeF(requestedAddr, lenType, clientId);
            LOG("[server] TEST loaded value (what): %d\n", *((int*)requestedAddr));
            socket->sendQue(clientId);
            break;
          case 3 : /*** wrong request ***/
            assert(false && "something is strange!");
            break;
          case 4 : /*** store request ***/
            LOG("[server] get store request from client\n");

            // get type length (how much store in byte)
            lenType = socket->takeWordF(clientId);
            LOG("[server] type length (how much store in byte): %d\n", lenType);

            // get requested addr (where)
            socket->takeRangeF(&requestedAddr, datalen, clientId);
            LOG("[server] requestedAddr (where): (%p)\n", requestedAddr);

            // get value which client want to store (what to store)
            valueToStore = malloc(lenType);
            socket->takeRangeF(valueToStore, lenType, clientId);
            LOG("[server] TEST stored value (what): %d\n", *((int*)valueToStore));
            
            // store value in UVA address.
            memcpy(requestedAddr, valueToStore, lenType);

            // send ack
            socket->pushWordF(5, clientId);
            socket->pushWordF(0, clientId); // ACK ( 0: normal, -1: abnormal )
            socket->sendQue(clientId);

            // test
            hexdump(requestedAddr, lenType);
            XMemory::XMemoryManager::dumpRange(requestedAddr, lenType);
            break;
        }
        pthread_mutex_unlock(&mutex);
      }
      return NULL;
    }

    extern "C" void uva_server_load(void *addr, uint64_t len) {
      LOG("[server] Load instr, addr %p, len %d\n", addr, len); 
    }

    extern "C" void uva_server_store(void *addr, uint64_t len, void *data) {
      LOG("[server] Store instr, addr %p, len %d\n", addr, len); 
    }
  }
}

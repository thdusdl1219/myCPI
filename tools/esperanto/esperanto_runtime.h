#ifndef ESPERANTO_INIT_H
#define ESPERANTO_INIT_H
/* Esperanto :: Runtime Functions 
 * Written by Seonyeong */

// #include "../Eevent/eventManager.h"
#include "esperantoUtils.h"
#include "../networkUtils/tcpComm.h"
#include "dataQ/dataQManager.h"
#include "deviceRuntimeManager.h"
#include "comm/comm_manager.h"

namespace corelab{

// Declaration for function pointer
typedef void (*ApiCallback) (int, int);

enum{
  REMOTE_CALL = 1,
  ASYNC_REMOTE_CALL = 2,
  RETURN_VALUE = 3,
  REGISTER_DEVICE = 4,
  BLOCKING = 1000
};

DeviceRuntimeManager* drm;// = new DeviceRuntimeManager();
DataQManager* dqm;
CommManager* comm_manager;

struct RemoteCallHeader {
  char type;
  int size;
};

struct ConnectionInfo{
  char ip[16];
  int port;
};

// FIXME: Structure to Class?
struct FunctionEntry {
  int fid;
  int sock_d;
};

struct MyFID {
  int* fids;
  int num;
};

extern "C"
void debugAddress(void*);

extern "C"
void EspInit (ApiCallback, int, int);

void esperanto_initializer(CommManager*);
void uva_initializer(CommManager*);

};
#endif


#ifndef ESPERANTO_INIT_H
#define ESPERANTO_INIT_H
/* Esperanto :: Runtime Functions 
 * Written by Seonyeong */

// #include "../Eevent/eventManager.h"
#include "esperantoUtils.h"
#include "dataQ/dataQManager.h"
#include "deviceRuntimeManager.h"
#include "gatewayRuntimeManager.h"


// Declaration for function pointer
typedef void (*ApiCallback) (int, int);

GatewayRuntimeManager* grm;// = new GatewayRuntimeManager();
DeviceRuntimeManager* drm;// = new DeviceRuntimeManager();
DataQManager* dqm;

struct RemoteCallHeader {
  char type;
  int size;
};

struct BroadcastHeader{
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
void gatewayInit(ApiCallback, int);

extern "C"
void deviceInit (ApiCallback, int);

#endif


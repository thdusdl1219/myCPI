/* Esperanto :: Runtime Functions 
 * Written by Seonyeong */ 

#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include <fcntl.h>
#include <map>
#include <time.h>
#include "esperanto_runtime.h"
#include "TimeUtil.h"
#include "log.h"

namespace corelab{

#define READ_TIMEOUT 5
#define NUM_DEVICE 2

#define TABLE_SIZE 100
#define BROADCAST_PORT 56700
#define MAX_THREAD 8


//#define DEBUG_ESP

using namespace std;


extern "C" void UVAClientInitializer(CommManager*, int, uint32_t);
extern "C" void UVAClientCallbackSetter(CommManager*);
extern "C" void uva_sync();

// common variables
pthread_mutex_t handleArgsLock= PTHREAD_MUTEX_INITIALIZER;
ApiCallback callback; 
FunctionEntry ft[TABLE_SIZE];
int numFID;
MyFID myFID;

// device
char deviceName[256];
int dNameSize;
int deviceID = -1;
uint32_t connectionID = -1;


extern "C" void debugAddress(void* d){
  LOG("DEBUG :: address = %p\n",d);
}

void hexdump(char *desc, void *addr, int len) {
  int i;
  unsigned char buff[17];
  unsigned char *pc = (unsigned char*)addr;

  // Output description if given.
  if (desc != NULL)
    printf ("%s:\n", desc);

  // Process every byte in the data.
  for (i = 0; i < len; i++) {
    // Multiple of 16 means new line (with line offset).

    if ((i % 16) == 0) {
      // Just don't print ASCII for the zeroth line.
      if (i != 0)
        printf ("  %s\n", buff);

      // Output the offset.
      printf ("  %04x ", i);
    }

    // Now the hex code for the specific character.
    printf (" %02x", pc[i]);

    // And store a printable ASCII character for later.
    if ((pc[i] < 0x20) || (pc[i] > 0x7e))
      buff[i % 16] = '.';
    else
      buff[i % 16] = pc[i];
    buff[(i % 16) + 1] = '\0';
  }

  // Pad out last line if not exactly 16 characters.
  while ((i % 16) != 0) {
    printf ("   ");
    i++;
  }

  // And print the final ASCII bit.
  printf ("  %s\n", buff);
}

int fd_set_blocking(int fd, int blocking) {
  /* Save the current flags */
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    return 0;

  if (blocking)
    flags &= ~O_NONBLOCK;
  else
    flags |= O_NONBLOCK;
  return fcntl(fd, F_SETFL, flags) != -1;
}

void 
sendChar(int sock_d, char c){
  write(sock_d, &c, sizeof(char));
}

bool
waitForChar(int sock_d, char c) {
  char received = 0;
  struct timeval tv;

  tv.tv_sec = READ_TIMEOUT;
  tv.tv_usec = 0;

  setsockopt(sock_d, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval));
  read(sock_d, &received, 1);

  // FIXME: revert timeout setting?
  return received == c;
}

/* -------------------------- Functions For IR  ---------------------*/
extern "C"
int generateJobId(int functionID){
  int jobID;
  jobID = drm->getJobID();
  drm->insertRunningJob(jobID, functionID);
  return jobID;
}

extern "C" 
void pushArgument(int rc_id, void* buf, int size){
  drm->insertArgsInfo(rc_id,buf, size); 
}

extern "C" 
void produceAsyncFunctionArgs(int functionID, int rc_id){
  int size = drm->getArgsTotalSize(rc_id); 
  void* buf = drm->getArgsOfRC(rc_id);
  int jobID = -2;

  TAG tag = ASYNC_REMOTE_CALL;
  comm_manager->pushRange(tag, (void*)&jobID, 4, connectionID);
  comm_manager->pushWord(tag, (uint32_t)functionID, connectionID);
  comm_manager->pushRange(tag, buf, size, connectionID);
  comm_manager->sendQue(tag, connectionID);

#ifdef DEBUG_ESP
  LOG("-------------------------------------------------------------------------------------\n");
  LOG("Send async function call (DEVICE) -> sourceJobID = %d, functionID = %d\n", jobID, functionID);
  hexdump("Args",buf,size);
  LOG("-------------------------------------------------------------------------------------\n");
#endif

  free(buf);
}

extern "C"
void produceReturn(int jobID, void* buf, int size){
  if(jobID == -2)
    return;

  if(jobID != -1){
    int sourceJobID = drm->getSourceJobID(jobID);
    TAG tag = RETURN_VALUE;

    comm_manager->pushWord(tag, (uint32_t)jobID, connectionID);
    comm_manager->pushRange(tag, buf, size, connectionID);
    comm_manager->sendQue(tag, connectionID);
    
#ifdef DEBUG_ESP
    LOG("-------------------------------------------------------------------------------------\n");
    LOG("Send return value (DEVICE) -> localJobID = %d, sourceJobID = %d\n", jobID, sourceJobID);
    hexdump("Return",ret,size);
    LOG("-------------------------------------------------------------------------------------\n");
#endif

    drm->deleteJobIDMapping(jobID);
  } // send return value 

}

extern "C"
void produceFunctionArgs(int jobID, int rc_id){

  int size = drm->getArgsTotalSize(rc_id); 
  void* buf = drm->getArgsOfRC(rc_id);
  int functionID = drm->getRunningJobFID(jobID);

  drm->insertConsumeWait(jobID);

  TAG tag = REMOTE_CALL;
  comm_manager->pushRange(tag, (void*)&jobID, 4, connectionID);
  comm_manager->pushWord(tag, (uint32_t)functionID, connectionID);
  comm_manager->pushRange(tag, buf, size, connectionID);
  comm_manager->sendQue(tag, connectionID);

#ifdef DEBUG_ESP
  LOG("-------------------------------------------------------------------------------------\n");
  LOG("Send remote function call (DEVICE) -> sourceJobID = %d, functionID = %d\n", jobID, functionID);
  hexdump("Args",buf,size);
  LOG("-------------------------------------------------------------------------------------\n");
#endif

  free(buf);
}

extern "C"
void registerDevice(void* addr){
  
  LOG("Address of device is %p\n",addr);

  uint32_t registerID;
  memcpy(&registerID,&addr,4);

  TAG tag = REGISTER_DEVICE;
  comm_manager->pushWord(tag,registerID, connectionID);
  comm_manager->sendQue(tag, connectionID);
#ifdef DEBUG_ESP
    LOG("-------------------------------------------------------------------------------------\n");
    hexdump("RegisterDevice",&registerID,4);
    LOG("-------------------------------------------------------------------------------------\n");
#endif

}

extern "C"
void* consumeFunctionArgs(int jobID){	

  pthread_mutex_lock(&handleArgsLock);
  void* ret = drm->getArgs(jobID);
  pthread_mutex_unlock(&handleArgsLock);
  return ret;
}

extern "C"
void* consumeReturn(int jobID){
  void* ret;
  while(1){
    if(drm->checkConsumeWait(jobID)){
      ret = drm->getReturnValue(jobID);
      break;
    }
  }
#ifdef DEBUG_ESP
  LOG("return value is successfully arrived\n");
#endif
  return ret;
}



/*--------------------------------------------------------------------*/
/* ---------------------------DEVICE-------------------------- */
/*                                                             */
/* ---------------------------DEVICE-------------------------- */

void remotecall_callback(void* data, uint32_t size, uint32_t sourceID){

  if(size < 8){
#ifdef DEBUG_ESP
    LOG("DEBUG::Remotecall needs least 8 bytes argument\n");
#endif
    assert(0);
  }

  char* data_ = (char*)data;

  int sourceJobID = *(int*)data_;
  int functionID = *(int*)(data_+4);
  int localJobID = drm->getJobID();

  uva_sync();

#ifdef DEBUG_ESP
  LOG("-------------------------------------------------------------------------------------\n");
  LOG("Recv function call (DEVICE) -> localJobID = %d, sourceJobID = %d, functionID = %d\n",localJobID, sourceJobID, functionID);
  hexdump("Args",data_,size);
  LOG("-------------------------------------------------------------------------------------\n");
#endif
  drm->insertJobIDMapping(localJobID,sourceJobID);

  pthread_mutex_lock(&handleArgsLock);
  drm->insertArgs(localJobID, (data_+8));
  pthread_mutex_unlock(&handleArgsLock);

  //FIXME : please change mutex lock to spin lock (for speed up)

  callback(functionID,localJobID);

  //FIXME : please change callback call to call using thread pool
}

void async_remotecall_callback(void* data, uint32_t size, uint32_t sourceID){

  if(size < 8){
#ifdef DEBUG_ESP
    LOG("DEBUG::Async Remotecall needs least 8 bytes argument\n");
#endif
    assert(0);
  }

  char* data_ = (char*)data;

  int sourceJobID = *(int*)data_;
  int functionID = *(int*)(data_+4);
  int localJobID = -2;

  uva_sync();

#ifdef DEBUG_ESP
  LOG("-------------------------------------------------------------------------------------\n");
  LOG("Recv function call (DEVICE) -> localJobID = %d, sourceJobID = %d, functionID = %d\n",localJobID, sourceJobID, functionID);
  hexdump("Args",data_,size);
  LOG("-------------------------------------------------------------------------------------\n");
#endif

  drm->insertJobIDMapping(localJobID,sourceJobID);

  pthread_mutex_lock(&handleArgsLock);
  drm->insertArgs(localJobID, (data_+8));
  pthread_mutex_unlock(&handleArgsLock);

  callback(functionID,localJobID);
}

void return_value_callback(void* data, uint32_t size, uint32_t sourceID){

  if(size < 4){
#ifdef DEBUG_ESP
    LOG("DEBUG::ReturnValue needs least 4 bytes argument\n");
#endif
    assert(0);
  }

  char* data_ = (char*)data;

  int localJobID  = *(int*)data_;

#ifdef DEBUG_ESP
  LOG("-------------------------------------------------------------------------------------\n");
  LOG("Recv Return value (DEVICE) -> localJobID = %d\n",localJobID);
  hexdump("Return",data_,size);
  LOG("-------------------------------------------------------------------------------------\n");
#endif

  drm->insertReturnValue(localJobID,(data_+4));
  drm->deleteRunningJob(localJobID);
  drm->onValueReturn(localJobID);
}

void esperanto_callback_setter(CommManager* comm_manager){
  TAG tag;

  tag = REMOTE_CALL;
  comm_manager->setCallback(tag,remotecall_callback);
  
  tag = ASYNC_REMOTE_CALL;
  comm_manager->setCallback(tag,async_remotecall_callback);

  tag = RETURN_VALUE;
  comm_manager->setCallback(tag,return_value_callback);
  // Set callback functions for esperanto device runtime
}

void uva_callback_setter(CommManager* comm_manager){
  UVAClientCallbackSetter(comm_manager);
}

void network_initializer(CommManager* comm_manager){
  FILE* server_desc = fopen("server_desc","r");
  char serverIP[20];
  int port;

  fscanf(server_desc,"%s %d",serverIP,&port);
  connectionID = comm_manager->tryConnect(serverIP,port);
}

void esperanto_initializer(CommManager* comm_manager){
  comm_manager->start();
}

void uva_initializer(CommManager* comm_manager, int isGvarInitializer, uint32_t homeID){
  UVAClientInitializer(comm_manager, isGvarInitializer, homeID);
}

extern "C"
void EspInit(ApiCallback fcn, int id, int isGvarInitializer){

  char filename[20];
  comm_manager = new CommManager();
  LOG("DEBUG :: EspInit start\n");

  callback = fcn;
  sprintf(filename,"functionTable-%d",id);

  esperanto_callback_setter(comm_manager);

  LOG("DEBUG :: EspInit start\n");
  uva_callback_setter(comm_manager);

  LOG("DEBUG :: EspInit start\n");
  network_initializer(comm_manager);
  LOG("DEBUG :: EspInit start\n");
  esperanto_initializer(comm_manager);
  LOG("DEBUG :: EspInit start\n");
  uva_initializer(comm_manager, isGvarInitializer, connectionID);

  LOG("DEBUG :: EspInit start\n");
}

extern "C"
void main_fini(){
  while(1);
}



};

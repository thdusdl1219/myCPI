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


extern "C" void uva_sync();

// common variables
int runningCallback = 0;;
int callbackIter = 0;
pthread_barrier_t barrier;
pthread_barrier_t commBarrier;
pthread_t callbackHandler[MAX_THREAD];
pthread_t sendQHandlerThread;
pthread_t localQHandlerThread;
pthread_mutex_t handleArgsLock= PTHREAD_MUTEX_INITIALIZER;
//pthread_mutex_t settingLock= PTHREAD_MUTEX_INITIALIZER;
//pthread_mutexattr_t lockInit;
pthread_mutex_t sendQLock= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t localQLock= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sendQHandleLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t localQHandleLock = PTHREAD_MUTEX_INITIALIZER;
ApiCallback callback; 
FunctionEntry ft[TABLE_SIZE];
int numFID;
MyFID myFID;
bool isGateway = false;
bool settingComplete = false;
bool sendQsetting =false;
bool localQsetting = false;
bool sendQHandling = false;
bool localQHandling = false;


// gateway
int settingIter = 0;
int socketBuffer[2*NUM_DEVICE];
//pthread_mutex_t recvQLocks[NUM_DEVICE];
bool connectReady = false;
int numConnect = 0;
int connectPort = 20000;
pthread_t listeners[2*NUM_DEVICE];
void *connectionInit (void* arg);

// device
char deviceName[256];
int dNameSize;
int deviceID = -1;
pthread_t listenThread;
pthread_mutex_t recvQLock= PTHREAD_MUTEX_INITIALIZER;

// commmon functions
void recvQHandler(int sock_d);
void *sendQHandler (void* arg);
void *localQHandler(void* arg);
void updateFunctionTable (int sock_d, int size);
bool updateMyFIDTable (const char* fname);
void sendMyFID (int sock_d);

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
int recvComplete(int socket, char* buffer, int size){
  int recvSize = 0;
  int tempSize;
  while(recvSize != size){
    tempSize = read(socket,buffer,size);
    if(tempSize != -1){
      recvSize += tempSize;
      buffer += tempSize;
    }
  }
  return recvSize;
}

int sendComplete(int socket, char* buffer, int size){
  int sendSize = 0;
  int tempSize;
  while(sendSize != size){
    tempSize = write(socket,buffer,size);
    if(tempSize != -1){
      sendSize += tempSize;
      buffer += tempSize;
    }
  }
  return sendSize;
}

void* worker_func(void* data){

  return NULL;
}

//working here


void* callbackWrapper(void* arg){
  //printf("callback wrapper is called\n");
  //LOG("tid : %u / pid : %u\n",(unsigned int)pthread_self(),(unsigned int)getpid());

  int* args = (int*)arg;
  callback(args[0],args[1]);
  //pthread_exit(NULL);
  return NULL;
}

void initIOSocket(int sendSocket, int recvSocket){
  char initHeader = 'I';
  char ack;
  int intBuffer[2];
  write(sendSocket,&initHeader,1);
  read(sendSocket,(char*)&deviceID,4);	
  intBuffer[0] = deviceID;
  intBuffer[1] = myFID.num;
  initHeader = 'O';
  write(recvSocket,&initHeader,1);
  read(recvSocket,&ack,1);
  write(recvSocket,(char*)intBuffer,8);
  read(recvSocket,&ack,1);
  write(recvSocket,(char*)myFID.fids,myFID.num*4);
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
  int sendSocket = drm->getSendSocket();
  char header[9];
  char* payload;
  int payloadSize = 0;
  int jobID = -2;
  char ack = 1;
  size += 4;
  memcpy(header+1,&jobID,4);
  memcpy(header+5,&size,4);
  header[0] = 'A';
  payload = (char*)malloc(size);
  if(size>0){
    memcpy(payload,&functionID,4);
    memcpy(payload+4,buf,size-4);
#ifdef DEBUG_ESP	
    hexdump("args",buf,size-4);
#endif
  }
  else{
    memcpy(payload,&functionID,4);
  }
  sendComplete(sendSocket,header,9);
  read(sendSocket,&ack,1);
  if(size >0)
    payloadSize = sendComplete(sendSocket,payload,(size));
  else
    payloadSize = sendComplete(sendSocket,payload,4);
#ifdef DEBUG_ESP
  LOG("-------------------------------------------------------------------------------------\n");
  LOG("Send async function call (DEVICE) -> sourceJobID = %d, functionID = %d\n", jobID, functionID);
  hexdump("Args",buf,size-4);
  LOG("-------------------------------------------------------------------------------------\n");
#endif
  free(payload);
  free(buf);
}

extern "C"
void produceReturn(int jobID, void* buf, int size){
  if(jobID == -2)
    return;
  //DataQElem* elem = new DataQElem();
  void* ret = (void*)malloc(size);
  memcpy(ret,buf,size);
  char ack =1;
  int sendSocket = drm->getSendSocket();
  char header[9];

  if(jobID != -1){
    int sourceJobID = drm->getSourceJobID(jobID);
    header[0] = 'R';
    memcpy(header+1,&sourceJobID,4);
    memcpy(header+5,&size,4);
    sendComplete(sendSocket,header,9);
    if(size >0)
      sendComplete(sendSocket,(char*)ret,size);
#ifdef DEBUG_ESP
    LOG("-------------------------------------------------------------------------------------\n");
    LOG("Send return value (DEVICE) -> localJobID = %d, sourceJobID = %d\n", jobID, sourceJobID);
    hexdump("Return",ret,size);
    LOG("-------------------------------------------------------------------------------------\n");
#endif

    drm->deleteJobIDMapping(jobID);
  } // send return value 

  free(ret);

}

extern "C"
void produceFunctionArgs(int jobID, int rc_id){
  int size = drm->getArgsTotalSize(rc_id); 
  void* buf = drm->getArgsOfRC(rc_id);
  int sendSocket = drm->getSendSocket();
  char ack = 1;
#ifdef DEBUG_ESP	
  hexdump("produced args",buf,size);
#endif

  char header[9];
  char* payload;
  int payloadSize = 0;
  drm->insertConsumeWait(jobID);
  size += 4;
  memcpy(header+1,&jobID,4);
  memcpy(header+5,&size,4);
  header[0] = 'F';
  payload = (char*)malloc(size);
  int functionID = drm->getRunningJobFID(jobID);
  if(size>0){
    memcpy(payload,&functionID,4);
    memcpy(payload+4,buf,size-4);
#ifdef DEBUG_ESP	
    hexdump("args",buf,size-4);
#endif
  }
  else{
    memcpy(payload,&functionID,4);
  }
  sendComplete(sendSocket,header,9);
  if(size >0)
    payloadSize = sendComplete(sendSocket,payload,(size));
  else
    payloadSize = sendComplete(sendSocket,payload,4);
#ifdef DEBUG_ESP
  LOG("-------------------------------------------------------------------------------------\n");
  LOG("Send function call (DEVICE) -> sourceJobID = %d, functionID = %d\n", jobID, functionID);
  hexdump("Args",buf,size-4);
  LOG("-------------------------------------------------------------------------------------\n");
#endif
  free(payload);
  free(buf);
}

extern "C"
void registerDevice(void* addr){
  LOG("Address of device is %p\n",addr);
  uint32_t temp;
  memcpy(&temp,&addr,4);
#ifdef DEBUG_ESP	
  hexdump("register",&addr,sizeof(addr));
#endif
  char* info = (char*)malloc(8);
  //sprintf(info,"%zu%d",temp,deviceID);
  memcpy(info,&temp,4);
  memcpy(info+4,&deviceID,4);
  hexdump("register",(void*)info,8);
  int sendSocket = drm->getSendSocket();
  char header[9];
  int sourceJobID = -1;
  char ack;
  int size = 8;
  header[0] = 'D';
  memcpy(header+1,&sourceJobID,4);
  memcpy(header+5,&size,4);
  sendComplete(sendSocket,header,9);
  sendComplete(sendSocket,info,size);

  printf("end of register device\n");

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

/* listener: listen returnValue & remote functioncall from gateway */

void* listenerFunction(void* arg){
  //LOG("DEBUG :: listen thread in device is started\n");
  int recvSocket = drm->getRecvSocket();
  char* header = (char*)malloc(9);
  char type;
  char ack = 1;
  int sourceJobID;
  int payloadSize;
  int cv = 0;
  int r = read(recvSocket,&ack,1);

  int start;
  int end;
  float diff;
  StopWatch watch;
  pthread_barrier_wait(&barrier);
  while(1){
    if(cv == 0){
      watch.start();
      cv=1;
    }
    int readSize = read(recvSocket,header,9);
    if(readSize == 9){

      type = header[0];
      int* temp = (int*)(header+1);
      sourceJobID = temp[0];
      payloadSize = temp[1];
      char* buffer = (char*)malloc(payloadSize);
      DataQElem* elem = new DataQElem();
      //write(recvSocket,&ack,1);
      if(type == 'F' || type == 'A'){
        recvComplete(recvSocket,buffer,payloadSize);
        int FID = *(int*)buffer;
        void* args;
        if(payloadSize > 4)
          args = (void*)(buffer+4);	
        else 
          args = NULL;

        int localJobID; // = -2;
        if(type != 'A')
          localJobID = drm->getJobID();
        else
          localJobID = -2;
        uva_sync();
#ifdef DEBUG_ESP
        LOG("-------------------------------------------------------------------------------------\n");
        LOG("Recv function call (DEVICE) -> localJobID = %d, sourceJobID = %d, functionID = %d\n",localJobID, sourceJobID, FID);
        hexdump("Args",args,payloadSize-4);
        LOG("-------------------------------------------------------------------------------------\n");
#endif
        drm->insertJobIDMapping(localJobID,sourceJobID);

         pthread_mutex_lock(&handleArgsLock);
        drm->insertArgs(localJobID, args);
        pthread_mutex_unlock(&handleArgsLock);
        int callbackArgs[2];
        callbackArgs[0] = FID;
        callbackArgs[1] = localJobID;
        pthread_create(&callbackHandler[callbackIter%8],NULL,&callbackWrapper,(void*)callbackArgs);
        callbackIter++;
        watch.end();
        if(callbackIter == 100){
          printf("reg.dat is modified\n");
          FILE* fp = fopen("reg.dat","w");
          fprintf(fp,"%f / %d\n",((float)callbackIter)/watch.diff(),callbackIter);
          fclose(fp);
        }
      }
      else{
        //LOG("return is received\n");
        if(payloadSize !=0)
          recvComplete(recvSocket,buffer,payloadSize);

        void* retVal = (void*)malloc(payloadSize);
        memcpy(retVal,buffer,payloadSize);

#ifdef DEBUG_ESP
        LOG("-------------------------------------------------------------------------------------\n");
        LOG("Recv Return value (DEVICE) -> localJobID = %d\n",sourceJobID);
        hexdump("Return",retVal,payloadSize);
        LOG("-------------------------------------------------------------------------------------\n");

        LOG("\n\nlocal Q handling is true  : %d\n\n",dqm->getLocalQSize());

        LOG("-------------------------------------------------------------------------------------\n");
        LOG("Handle return value (DEVICE) -> localJobID = %d\n", sourceJobID);
        hexdump("Return",retVal,payloadSize);
        LOG("-------------------------------------------------------------------------------------\n");
#endif

        drm->insertReturnValue(sourceJobID,retVal);
        drm->deleteRunningJob(sourceJobID);
        drm->onValueReturn(sourceJobID);


      }
    }
  }
  //LOG("DEBUG :: listen thread in device is ended\n");
  return NULL;
}

/* localQHandlerFunction */

void* localQHandlerFunction(void* arg){
  pthread_barrier_wait(&barrier);
  while(1){
    usleep(1);
    int localQSize = 0;
    localQSize = dqm->getLocalQSize();
    if(localQSize != 0){
#ifdef DEBUG_ESP
      LOG("local Q is handling\n\n");
#endif
    }

    if(dqm->getLocalQSize()>0){
#ifdef DEBUG_ESP
      LOG("local Q is handling / tid : %u / pid : %u\n",(unsigned int)pthread_self(),(unsigned int)getpid());
#endif
      DataQElem* localElem = dqm->getLocalQElement();
      if(localElem->getIsFunctionCall()){ // local function call
        int functionID = localElem->getFunctionID();
        int jobID = localElem->getJobID();
        void* args = localElem->getArgs();
        pthread_mutex_lock(&handleArgsLock);
        drm->insertArgs(jobID, args);
        pthread_mutex_unlock(&handleArgsLock);
        int callbackArgs[2];
        callbackArgs[0] = functionID;
        callbackArgs[1] = jobID;
        pthread_create(&callbackHandler[callbackIter%16],NULL,&callbackWrapper,(void*)callbackArgs);
        pthread_join(callbackHandler[callbackIter%16],NULL);
        callbackIter++;

      }
      else{ // return from gateway
        int jobID = localElem->getJobID();
#ifdef DEBUG_ESP
        LOG("-------------------------------------------------------------------------------------\n");
        LOG("Handle return value (DEVICE) -> localJobID = %d\n", jobID);
        hexdump("Return",localElem->getRetVal(),localElem->getRetSize());
        LOG("-------------------------------------------------------------------------------------\n");
#endif
        drm->insertReturnValue(jobID,localElem->getRetVal());
        drm->deleteRunningJob(jobID);
        drm->onValueReturn(jobID);
      }
    }
  }
  return NULL;	
}	

/* tryConnect: try to connect to gateway */

void*
tryConnect(void* arg){
  TCPCommHandler tcpHandlerIn, tcpHandlerOut;
  ConnectionInfo target = *(ConnectionInfo*)arg;
  tcpHandlerIn.createClientSocket(target.ip, target.port);
  tcpHandlerOut.createClientSocket(target.ip, target.port);

  int sendSocket = tcpHandlerIn.getSockDesc();
  int recvSocket = tcpHandlerOut.getSockDesc();
  initIOSocket(sendSocket, recvSocket);
  drm->setSockets(sendSocket, recvSocket);
  pthread_create(&listenThread,NULL,&listenerFunction,NULL);
  pthread_barrier_wait(&commBarrier);
  pthread_exit(NULL);
  return NULL;
}

void remotecall_callback(void* data, uint32_t size){

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
/*
  int callbackArgs[2];
  callbackArgs[0] = FID;
  callbackArgs[1] = localJobID;
  pthread_create(&callbackHandler[callbackIter%8],NULL,&callbackWrapper,(void*)callbackArgs);
  callbackIter++;
  watch.end();
  if(callbackIter == 100){
    printf("reg.dat is modified\n");
    FILE* fp = fopen("reg.dat","w");
    fprintf(fp,"%f / %d\n",((float)callbackIter)/watch.diff(),callbackIter);
    fclose(fp);
  }*/
}


void esperanto_initializer(CommManager* comm_manager){
  TAG tag;

  tag = 1;
  comm_manager->setCallback(tag,remotecall_callback);
  // Set callback functions for esperanto device runtime
}

void uva_initializer(CommManager* comm_manager){

}

extern "C"
void EspInit(ApiCallback fcn, int id){

  char filename[20];
  CommManager* comm_manager = new CommManager();

  callback = fcn;
  sprintf(filename,"functionTable-%d",id);
  
  if(updateMyFIDTable(filename)){
    esperanto_initializer(comm_manager);
    uva_initializer(comm_manager);
  }

  /*callback = fcn;
  char filename[20];
  sprintf(filename,"functionTable-%d",(int)id);
  pthread_barrier_init(&barrier,NULL,2);
  pthread_barrier_init(&commBarrier,NULL,4);
  pthread_t broadcastThread, connectThread;
  ConnectionInfo* server = (ConnectionInfo*)malloc(sizeof(ConnectionInfo));
  FILE* server_desc = fopen("server_desc","r");
  fscanf(server_desc,"%s %d",server->ip,&(server->port));
  fclose(server_desc);
  server->port += 1000;


  if(updateMyFIDTable(filename)){
    drm = new DeviceRuntimeManager();  
    dqm = new DataQManager();
    dqm->initQ();
    pthread_create(&connectThread, NULL, &tryConnect, (void*)server);
  }
  pthread_barrier_wait(&barrier);
  pthread_barrier_destroy(&barrier);*/
}

extern "C"
void main_fini(){
  while(1);
}


/* updateFunctionTable: update function table with received FIDs */
void
updateFunctionTable(int sock_d, int size){
  char* message = (char*) malloc(size);
  int readacc = 0;

  do {
    readacc += read(sock_d, message+readacc, size);
  } while (readacc < size);
  sendChar(sock_d, 'S');

  int* fids = (int*) message;
  for(int i = 0; i < size/4 ; i++){
    ft[numFID].fid = fids[i];
    ft[numFID++].sock_d = sock_d;
  }
}

/* updateMyFIDTable: open file and fill the FID array */
// FIXME: Optimization Pass should write the number of fids on the top of the output file
bool
updateMyFIDTable(const char *fname){
  FILE *fp = fopen(fname, "r");

  if(fp != NULL) {
    fscanf(fp, "%d\n", &myFID.num);
    myFID.fids = (int*) calloc(sizeof(int), myFID.num);
    for(int i = 0; i < myFID.num; i++)
      fscanf(fp,"%d ", myFID.fids+i);
    fclose(fp);  
    return true;
  }
#ifdef DEBUG_ESP
  LOG("Num : %d\n",myFID.num);
#endif
  //for(int i=0;i<myFID.num;i++)
  //	LOG("functionID : %d\n",myFID.fids[i]);

  return false;
}

/* sendMyFID: send its FID list through the target socket */
void
sendMyFID(int sock_d){
  int size = myFID.num * 4 + 1;
  char* message = (char*) malloc(size);
  message[0] = 'F';
  memcpy(message+1, myFID.fids, size-1);
  write(sock_d, message, size);
  return;
}
};

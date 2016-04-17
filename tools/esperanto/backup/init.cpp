/* Esperanto :: Runtime Functions 
 * Written by Seonyeong */ 

#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include <fcntl.h>
#include "init.h"
#include "log.h"

#define READ_TIMEOUT 5
#define BROADCAST_PERIOD 10
#define NUM_DEVICE 2

#define TABLE_SIZE 100
#define BROADCAST_PORT 56700
#define MAX_THREAD 16

// common variables
int runningCallback = 0;;
int callbackIter = 0;
pthread_t callbackHandler[MAX_THREAD];
pthread_t sendQHandlerThread;
pthread_t localQHandlerThread;
pthread_mutex_t handleArgsLock;
pthread_mutex_t settingLock;
pthread_mutex_t sendQLock;
pthread_mutex_t localQLock;
ApiCallback callback; 
FunctionEntry ft[TABLE_SIZE];
int numFID;
MyFID myFID;
bool isGateway = false;
bool settingComplete = false;
bool sendQsetting =false;
bool localQsetting = false;

// gateway
int settingIter = 0;
int socketBuffer[2*NUM_DEVICE];
pthread_mutex_t recvQLocks[NUM_DEVICE];
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
pthread_mutex_t recvQLock;

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

void* callbackWrapper(void* arg){
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
	if(isGateway){
		jobID = grm->getJID();
		grm->insertRunningJob(jobID, functionID);
		//grm->insertFuncJobMatching(jobID, functionID);		
	}
	else{
		jobID = drm->getJobID();
		drm->insertRunningJob(jobID, functionID);
	}

	LOG("DEBUG :: generate job ID = %d\n",jobID);
	return jobID;
}

extern "C"
void produceReturn(int jobID, void* buf, int size){
	DataQElem* elem = new DataQElem();
	void* ret = (void*)malloc(size);
	memcpy(ret,buf,size);
	if(isGateway){
		LOG("produceReturn gateway :%p\n",buf);
		LOG("return size : %d\n",size);
		hexdump("gateway return",buf,size);
		elem->setIsFunctionCall(false);
		elem->setArgs(NULL,0);
		elem->setFunctionID(grm->getRunningJobFID(jobID));
		elem->setJobID(jobID);
		elem->setRetVal(ret,size);
		pthread_mutex_lock(&sendQLock);
		dqm->insertElementToSendQ(elem);
		pthread_mutex_unlock(&sendQLock);
		//do nothing
	}
	else{
		LOG("produceReturn device\n");
		elem->setIsFunctionCall(false);
		elem->setArgs(NULL,0);
		elem->setFunctionID(drm->getRunningJobFID(jobID));
		elem->setJobID(drm->getSourceJobID(jobID));
		elem->setRetVal(ret,size);
		pthread_mutex_lock(&sendQLock);
		dqm->insertElementToSendQ(elem);
		pthread_mutex_unlock(&sendQLock);
	}
}

extern "C"
void produceFunctionArgs(int jobID, void* buf, int size){
	LOG("produce function Args / jobID = %d\n",jobID);
	DataQElem* elem = new DataQElem();
	if(isGateway){
		LOG("produceFArgs gateway\n");
		hexdump("produced args",buf,size);
		elem->setIsFunctionCall(true);
		elem->setArgs(buf,size);
		elem->setFunctionID(grm->getRunningJobFID(jobID));
		elem->setJobID(jobID);
		elem->setRetVal(NULL,0);
		grm->insertConsumeWait(jobID);
		pthread_mutex_lock(&sendQLock);
		dqm->insertElementToSendQ(elem);	
		pthread_mutex_unlock(&sendQLock);
		LOG("produce function args to send q\n");
		// nothing
	}
	else{
		LOG("produceFArgs device\n");
		//LOG("args address : %p\n",buf);
		hexdump("produced args",buf,size);
		elem->setIsFunctionCall(true);
		elem->setArgs(buf,size);
		elem->setFunctionID(drm->getRunningJobFID(jobID));
		//LOG("function %d is called\n",drm->getRunningJobFID(jobID));
		elem->setJobID(jobID);
		elem->setRetVal(NULL,0);
		pthread_mutex_lock(&sendQLock);
		dqm->insertElementToSendQ(elem);	
		pthread_mutex_unlock(&sendQLock);
		LOG("produce function args to send q\n");

	}	
}

extern "C"
void* consumeFunctionArgs(int jobID){	
	
		LOG("consume function args\n");
	if(isGateway){
		//donothing
		pthread_mutex_lock(&handleArgsLock);
		//LOG("consume function args\n");
		void* ret = grm->getArgs(jobID);
		//LOG("args : %p\n",ret);
		pthread_mutex_unlock(&handleArgsLock);
		return ret;
	}
	else{
		pthread_mutex_lock(&handleArgsLock);
		void* ret = drm->getArgs(jobID);
		pthread_mutex_unlock(&handleArgsLock);
		hexdump("ConsumeFunctionArgs return value",ret,8);
		return ret;

	}
}

extern "C"
void* consumeReturn(int jobID){
	void* ret;
	LOG("Consume Return is started\n");
	if(isGateway){
		//nothing
		while(1){
			//pthread_mutex_lock(&consumeLock);
			if(grm->checkConsumeWait(jobID)){
				LOG("return is received, jobID = %d\n",jobID);
				ret = grm->getReturnValue(jobID);
				//LOG("return is received\n");
				break;
			}
			//pthread_mutex_lock(&consumeLock);
		}
	}
	else{
		while(1){
			if(drm->checkConsumeWait(jobID)){
				ret = drm->getReturnValue(jobID);
				LOG("return is received, jobID = %d\n",jobID);
				//drm->deleteConsumeWait(jobID);
				//hexdump("return",ret,4);
				LOG("return is received\n");
				break;
			}
		}
	}
	return ret;
}

/*--------------------------------------------------------------------*/
/* ---------------------------DEVICE-------------------------- */
/*                                                             */
/* ---------------------------DEVICE-------------------------- */

/* listener: listen returnValue & remote functioncall from gateway */

void* listenerFunction(void* arg){
	LOG("DEBUG :: listen thread in device is started\n");
	int recvSocket = drm->getRecvSocket();
	char* header = (char*)malloc(9);
	char type;
	char ack = 1;
	int sourceJobID;
	int payloadSize;
	int r = read(recvSocket,&ack,1);
	if(r == 1){
		
		LOG("Success\n");
		pthread_mutex_lock(&settingLock);
		settingComplete = true;
		if(settingComplete)
			LOG("setting complete\n");
		//LOG("\n\nsetting address : %p\n\n",&settingComplete);
		pthread_mutex_unlock(&settingLock);

	}
	while(1){

		int readSize = read(recvSocket,header,9);
		if(readSize == 9){
			type = header[0];
			int* temp = (int*)(header+1);
			sourceJobID = temp[0];
			payloadSize = temp[1];
			char* buffer = (char*)malloc(payloadSize);
			DataQElem* elem = new DataQElem();
			write(recvSocket,&ack,1);
			if(type == 'F'){
				recvComplete(recvSocket,buffer,payloadSize);
				int FID = *(int*)buffer;
				void* args;
				if(payloadSize > 4)
					args = (void*)(buffer+4);	
				else 
					args = NULL;
				
				int localJobID = drm->getJobID();
				LOG("-------------------------------------------------------------------------------------\n");
				LOG("Recv function call (DEVICE) -> localJobID = %d, sourceJobID = %d, functionID = %d\n",localJobID, sourceJobID, FID);
				hexdump("Args",args,payloadSize-4);
				LOG("-------------------------------------------------------------------------------------\n");
				elem->setIsFunctionCall(true);
				elem->setArgs(args,payloadSize-4);
				elem->setFunctionID(FID);
				drm->insertJobIDMapping(localJobID,sourceJobID);
				elem->setJobID(localJobID);
				elem->setRetVal(NULL, 0);
				pthread_mutex_lock(&localQLock);
				dqm->insertElementToLocalQ(elem);
				pthread_mutex_unlock(&localQLock);
			}
			else if(type == 'R'){
				//LOG("return is received\n");
				if(payloadSize !=0)
					recvComplete(recvSocket,buffer,payloadSize);
				void* retVal = (void*)buffer;
				LOG("-------------------------------------------------------------------------------------\n");
				LOG("Recv Return value (DEVICE) -> localJobID = %d\n",sourceJobID);
				hexdump("Return",retVal,payloadSize);
				LOG("-------------------------------------------------------------------------------------\n");

				elem->setIsFunctionCall(false);
				elem->setArgs(NULL,0);
				elem->setFunctionID(0);
				elem->setJobID(sourceJobID);
				elem->setRetVal(retVal,payloadSize);
				pthread_mutex_lock(&localQLock);
				dqm->insertElementToLocalQ(elem);
				pthread_mutex_unlock(&localQLock);
			}
		}
	}
	LOG("DEBUG :: listen thread in device is ended\n");
	return NULL;
}

/* sendQHandlerFunction: send returnValue & remote functioncall to gateway*/

void* sendQHandlerFunction(void* arg){
	LOG("DEBUG :: sendQHandler Function in device is started\n");
		pthread_mutex_lock(&settingLock);
		sendQsetting = true;
		LOG("sendQ setting is complete\n");
		pthread_mutex_unlock(&settingLock);
	int sendSocket = drm->getSendSocket();
	//LOG("DEBUG :: get send socket\n");
	char ack = 1;
	while(1){
		pthread_mutex_lock(&sendQLock);
		int sendQSize = dqm->getSendQSize();
		pthread_mutex_unlock(&sendQLock);
		if(sendQSize>0){
			LOG("send Q is handling\n");
			pthread_mutex_lock(&sendQLock);
			DataQElem* sendElem = dqm->getSendQElement();
			pthread_mutex_unlock(&sendQLock);
			char header[9];
			char* payload;
			int payloadSize = 0;
			int jobID = sendElem->getJobID();
			int argSize = sendElem->getArgsSize();
			if(sendElem->getIsFunctionCall()){
				//LOG("DEBUG :: send Q handle function call\n");
				drm->insertConsumeWait(sendElem->getJobID());
				argSize += 4;
				memcpy(header+1,&jobID,4);
				memcpy(header+5,&argSize,4);
				header[0] = 'F';
				//sprintf(header,"%c%d%d",'F',sendElem->getJobID(),sendElem->getArgsSize()+4);
				//LOG("send job ID = %d, send arg size = %d\n",sendElem->getJobID(),sendElem->getArgsSize());
				//hexdump("send header",header,9);
				payload = (char*)malloc(argSize);
				int functionID;
				if(sendElem->getArgsSize() >0){
					functionID = sendElem->getFunctionID();
					memcpy(payload,&functionID,4);
					memcpy(payload+4,sendElem->getArgs(),argSize-4);
					//hexdump("args",sendElem->getArgs(),argSize-4);
					//LOG("argument : %d\n",*(int*)sendElem->getArgs());
					//sprintf(payload,"%d%s",sendElem->getFunctionID(),(char*)sendElem->getArgs());
				}
				else{
					functionID = sendElem->getFunctionID();
					memcpy(payload,&functionID,4);
				}
				//	sprintf(payload,"%d",sendElem->getFunctionID());
				
				//LOG("function %d is called\n",sendElem->getFunctionID());
				sendComplete(sendSocket,header,9);
				read(sendSocket,&ack,1);
				//LOG("DEBUG :: send Arg size is %d\n",sendElem->getArgsSize());
				if(sendElem->getArgsSize() >0)
					payloadSize = sendComplete(sendSocket,payload,(argSize));
				else
					payloadSize = sendComplete(sendSocket,payload,4);
				LOG("-------------------------------------------------------------------------------------\n");
				LOG("Send function call (DEVICE) -> sourceJobID = %d, functionID = %d\n", jobID, functionID);
				hexdump("Args",sendElem->getArgs(),argSize-4);
				LOG("-------------------------------------------------------------------------------------\n");
				//hexdump("send argument",payload+4,4);
				//LOG("DEBUG :: send Q function call is ended with %d\n",payloadSize);
			}
			else{
				//LOG("DEBUG :: send Q handle return value\n");
				int localJobID = sendElem->getJobID();
				int sourceJobID = drm->getSourceJobID(localJobID);
				int retSize = sendElem->getRetSize();
				header[0] = 'R';
				memcpy(header+1,&sourceJobID,4);
				memcpy(header+5,&retSize,4);
				//sprintf(header,"%c%d%d",'R',sourceJobID,);
				payload = (char*)malloc(sendElem->getRetSize());
				memcpy(payload,(char*)sendElem->getRetVal(),sendElem->getRetSize());
				//sprintf(payload,"%s",(char*)sendElem->getRetVal());
				sendComplete(sendSocket,header,9);
				read(sendSocket,&ack,1);
				if(sendElem->getRetSize() >0)
					sendComplete(sendSocket,payload,sendElem->getRetSize());
				LOG("-------------------------------------------------------------------------------------\n");
				LOG("Send return value (DEVICE) -> localJobID = %d, sourceJobID = %d\n", localJobID, sourceJobID);
				hexdump("Return",sendElem->getRetVal(),retSize);
				LOG("-------------------------------------------------------------------------------------\n");

				drm->deleteJobIDMapping(localJobID);
			}
		}
	}
	return NULL;
}

/* localQHandlerFunction */

void* localQHandlerFunction(void* arg){
	LOG("DEBUG :: localQHandler function in device is started\n");
	//ApiCallback fcn = *(ApiCallback*)arg;
		pthread_mutex_lock(&settingLock);
		LOG("localQ setting is complete\n");
		localQsetting = true;
		
		pthread_mutex_unlock(&settingLock);
	while(1){
		pthread_mutex_lock(&localQLock);
		int localQSize = dqm->getLocalQSize();
			pthread_mutex_unlock(&localQLock);
		if(localQSize>0){
			LOG("local Q is handling\n");
				pthread_mutex_lock(&localQLock);
			DataQElem* localElem = dqm->getLocalQElement();
			pthread_mutex_unlock(&localQLock);
			if(localElem->getIsFunctionCall()){ // local function call
				int functionID = localElem->getFunctionID();
				int jobID = localElem->getJobID();
				void* args = localElem->getArgs();
				pthread_mutex_lock(&handleArgsLock);
				drm->insertArgs(jobID, args);
				pthread_mutex_unlock(&handleArgsLock);
				LOG("-------------------------------------------------------------------------------------\n");
				LOG("Handle function call (DEVICE) -> localJobID = %d, functionID = %d\n", jobID, functionID);
				hexdump("Return",args,localElem->getArgsSize());
				LOG("-------------------------------------------------------------------------------------\n");
				int callbackArgs[2];
				callbackArgs[0] = functionID;
				callbackArgs[1] = jobID;
				pthread_create(&callbackHandler[callbackIter%16],NULL,&callbackWrapper,(void*)callbackArgs);
				//pthread_cancel(callbackHandler[callbackIter%16]);
				callbackIter++;
				//(callback)(functionID,jobID);		
				//printf("function is called\n");	
			}
			else{ // return from gateway
				int jobID = localElem->getJobID();
				LOG("-------------------------------------------------------------------------------------\n");
				LOG("Handle return value (DEVICE) -> localJobID = %d\n", jobID);
				hexdump("Return",localElem->getRetVal(),localElem->getRetSize());
				LOG("-------------------------------------------------------------------------------------\n");
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
	BroadcastHeader target = *(BroadcastHeader*)arg;
	tcpHandlerIn.createClientSocket(target.ip, target.port);
	tcpHandlerOut.createClientSocket(target.ip, target.port);

	int sendSocket = tcpHandlerIn.getSockDesc();
	int recvSocket = tcpHandlerOut.getSockDesc();
	initIOSocket(sendSocket, recvSocket);
	LOG("initIOSocket\n");
	drm->setSockets(sendSocket, recvSocket);
	LOG("setSocket\n");
	pthread_create(&listenThread,NULL,&listenerFunction,NULL);
	pthread_create(&sendQHandlerThread,NULL,&sendQHandlerFunction,NULL);
	pthread_create(&localQHandlerThread,NULL,&localQHandlerFunction, NULL);
	
	pthread_exit(NULL);
}

/* broadcastListener: Listen broadcasting from gateway */
void* 
broadcastListener(void* arg){
	pthread_t ct = *(pthread_t*)arg;
	UDPCommHandler udpHandler;
	BroadcastHeader *message = (BroadcastHeader*)malloc(sizeof(BroadcastHeader));

	udpHandler.createServerSocket(BROADCAST_PORT); 
	udpHandler.recv((char*)message, sizeof(BroadcastHeader));
	//LOG("DEBUG :: received message ip : %s, port : %d\n",message->ip,message->port);
	//LOG("DEBUG :: before create tryConnect function\n");
	pthread_create(&ct, NULL, &tryConnect, (void*)message);  
	//pthread_join(ct, NULL);
	pthread_exit(NULL);
	//LOG("DEBUG :: after create tryConnect function\n");
}

extern "C"
void deviceInit(ApiCallback fcn, int id){
	callback = fcn;
	char filename[20];
	sprintf(filename,"functionTable-%d",(int)id);
	LOG("filename : %s\n",filename);
	pthread_t broadcastThread, connectThread;
	drm = new DeviceRuntimeManager();  
	dqm = new DataQManager();
	dqm->initQ();
	//int k = dqm->getSendQSize();
	//LOG("sendQ : %d\n",k);

	if(updateMyFIDTable(filename)){
		LOG("updateMyFIDTable\n");
		//pthread_create(&handlerThread, NULL, &eventHandler, NULL);
		//pthread_create(&sendQHandlerThread,NULL,
		pthread_create(&broadcastThread, NULL, &broadcastListener, (void*)&connectThread);

		//pthread_join(handlerThread, NULL);
		LOG("1\n");
		//pthread_join(broadcastThread, NULL);
		LOG("1\n");
				LOG("1\n");
		//pthread_join(listenThread,NULL);
		LOG("1\n");
		//pthread_join(sendQHandlerThread,NULL);
		LOG("1\n");
		//pthread_join(localQHandlerThread,NULL);
		LOG("1\n");
	}
	LOG("\n\nsetting address main : %p\n\n",&settingComplete);

	while(1){
		pthread_mutex_lock(&settingLock);
		if(settingComplete && sendQsetting && localQsetting){
			LOG("setting is complete\n");
			pthread_mutex_unlock(&settingLock);
			break;
		
		}
		pthread_mutex_unlock(&settingLock);
		
	}
	LOG("DEBUG :: deviceInit end\n");
}


/* ---------------------------GATEWAY-------------------------- */
/*                                                              */
/* ---------------------------GATEWAY-------------------------- */

/* gatewayBroadcast: broadcasts gateway ip address and port */
void* 
gatewayBroadcast(void* arg){
	LOG("Gateway:: Broadcasting started\n");

	BroadcastHeader message;
	char* ip = (char*)malloc(16);
	LOG("before getIP\n");
	getIP(ip);
	LOG("DEBUG :: ip = %s\n",ip);
	strcpy(message.ip, ip);
	LOG("DEBUG :: ip in message \ %s\n",message.ip);
	message.port = 20000;

	while (numConnect < 2*NUM_DEVICE) {
		if(connectReady){
			broadcastSend((char*)&message, sizeof(BroadcastHeader), BROADCAST_PORT);
			sleep(BROADCAST_PERIOD);
		}
	}
	
	LOG("Gateway:: Broadcasting finished\n");
	return NULL;
}

/* connectHandler: Listen to connect socket */
void*
connectHandler(void* arg){
	LOG("1\n");
	int sock_d, addrlen;

	sockaddr addr;
	TCPCommHandler tcpHandler;
	LOG("here?\n");
	// Create a server socket for connection 
	tcpHandler.createServerSocket(connectPort);
	LOG("Gateway:: Started to wait for devices\n");
	LOG("DEBUG :: tcpHandler socket :%d\n",tcpHandler.getSockDesc());
	connectReady = true;
	pthread_create(&sendQHandlerThread,NULL,&sendQHandler,NULL);
	pthread_create(&localQHandlerThread,NULL,&localQHandler,NULL);
	while (numConnect < 2*NUM_DEVICE) {

		sock_d = accept(tcpHandler.getSockDesc(), (sockaddr*)&addr, (socklen_t *)&addrlen);
		socketBuffer[numConnect] = sock_d;	
		if(sock_d>0)
			pthread_create(&listeners[numConnect], NULL, &connectionInit, (void*)(&socketBuffer[numConnect++]));
	}
	LOG("DEBUG :: connectHandler is ended\n");
		return NULL;
}

void alertReady(){
	grm->alertReady();	
}

/* gatewayInit: gateway main function */
extern "C"
void gatewayInit(ApiCallback fcn, int id){
	LOG("DEBUG :: gateway init\n");
	//LOG("fname size : %d\n",(int)sizeof(fname));
	//std::string s = std::string(fname);
	//LOG("s size : %d\n",(int)(s.size()));
	//LOG("fname : %s\n",fname.c_str());
	LOG("id : %d\n",id);
	grm = new GatewayRuntimeManager();
	LOG("1\n");
	dqm = new DataQManager();
	isGateway = true;
	LOG("1\n");
	dqm->initQ();
	LOG("1\n");
	pthread_t handlerThread, connectThread, broadcastThread;
	callback = fcn;

	// Thread Creation
	//pthread_create(&handlerThread, NULL, &sendQHandler, NULL);
	pthread_create(&connectThread, NULL, &connectHandler, NULL);
	LOG("1\n");
	pthread_create(&broadcastThread, NULL, &gatewayBroadcast, NULL);
	LOG("1\n");
	LOG("\n\nsetting address main : %p\n\n",&settingComplete);
	while(1){
		pthread_mutex_lock(&settingLock);
		if((settingIter == NUM_DEVICE) && settingComplete && sendQsetting && localQsetting){
			LOG("setting is complete2\n");
			pthread_mutex_unlock(&settingLock);
			break;
		}
		pthread_mutex_unlock(&settingLock);
	}
	alertReady();
	LOG("DEBUG :: gateway Init is ended\n");
	// FIXME: Join thread before create?
	//pthread_join(handlerThread, NULL);
	//pthread_join(connectThread, NULL);
	//pthread_join(broadcastThread, NULL);
	//for(int i = 0; i < 2*NUM_DEVICE; i++)
//		pthread_join(listeners[i], NULL); 
}

void*
localQHandler(void* arg){
	LOG("DEBUG :: localQHandler function in gateway is started\n");
	pthread_mutex_lock(&settingLock);
	localQsetting = true;
	pthread_mutex_unlock(&settingLock);
	while(1){
		pthread_mutex_lock(&localQLock);
		int localQSize = dqm->getLocalQSize();
		if(localQSize != 0)
			LOG("local Q size = %d\n", localQSize);
		pthread_mutex_unlock(&localQLock);
		if(localQSize>0){
			LOG("DEBUG :: local Q is handling\n");
			pthread_mutex_lock(&localQLock);
			DataQElem* localElem = dqm->getLocalQElement();
			//LOG("address of local elem in local Q: %p\n",localElem);
			pthread_mutex_unlock(&localQLock);
			if(localElem->getIsFunctionCall()){ // local function call
				//LOG("DEBUG :: it is remote function call\n");
				int functionID = localElem->getFunctionID();
				int jobID = localElem->getJobID();
				//LOG("jobID = %d\n",jobID);
				void* args = localElem->getArgs();
				pthread_mutex_lock(&handleArgsLock);
				grm->insertArgs(jobID, args);
				pthread_mutex_unlock(&handleArgsLock);
				//printf("before function call : %d\n",functionID);
				//printf("args :%d\n",*(int*)args);
				//consumeFunctionArgs(jobID);
				//produceReturn(jobID,NULL,0);
				LOG("-------------------------------------------------------------------------------------\n");
				LOG("Handle function call (GATEWAY) -> localJobID = %d, functionID = %d\n", jobID, functionID);
				hexdump("Return",args,localElem->getArgsSize());
				LOG("-------------------------------------------------------------------------------------\n");
				int callbackArgs[2];
				callbackArgs[0] = functionID;
				callbackArgs[1] = jobID;
				pthread_create(&callbackHandler[callbackIter%16],NULL,&callbackWrapper,(void*)callbackArgs);
				//pthread_cancel(callbackHandler[callbackIter%16]);
				callbackIter++;

				//(callback)(functionID,jobID);		
				printf("function is called : %p\n",args);	
			}
			else{ // return from gateway
				int jobID = localElem->getJobID();
				LOG("-------------------------------------------------------------------------------------\n");
				LOG("Handle return value (GATEWAY) -> localJobID = %d\n", jobID);
				hexdump("Return",localElem->getRetVal(),localElem->getRetSize());
				LOG("-------------------------------------------------------------------------------------\n");

				grm->insertReturnValue(jobID,localElem->getRetVal());
				grm->deleteRunningJob(jobID);
				grm->onValueReturn(jobID);
			}
		}
	}
	return NULL;
}

/* remoteCallHelper: add a remote call to the event queue */
void
recvQHandler(int deviceID){
	LOG("DEBUG :: recvQHandler in gateway is started\n");
	int sock_d = grm->getInSocket(deviceID);
	char header[9];
	char ack = 1;
	while(1){
		int readSize = recvComplete(sock_d,header,9);	
		if(readSize ==9){
			char* payload;
			if(header[0] == 'F'){
				int sourceJobID = *(int*)(header+1);
				int argSize = *(int*)(header+5);
				int outSocket = grm->getOutSocketByDeviceID(deviceID);
				payload = (char*)malloc(argSize);
				write(sock_d,&ack,1);
				recvComplete(sock_d,payload,argSize);
				hexdump("RECV payload",payload,argSize);
				int functionID = *(int*)payload;
				void* args;
				if(argSize > 4)
					args = (void*)(payload+4);
				else 
					args = NULL;
				if(!grm->isLocalFunction(functionID)){
					DataQElem* sendElem = new DataQElem();
					sendElem->setIsFunctionCall(true);
					sendElem->setArgs(args,argSize-4);
					sendElem->setFunctionID(functionID);
					int localJobID = grm->getJID();
					sendElem->setJobID(localJobID);
					sendElem->setRetVal(NULL,0);
					grm->insertJobMapping(localJobID,outSocket,sourceJobID);
					//LOG("ljid : %d, os : %d, sjid :%d\n",localJobID,outSocket,sourceJobID);
					pthread_mutex_lock(&sendQLock);
					dqm->insertElementToSendQ(sendElem);
					pthread_mutex_unlock(&sendQLock);
					LOG("-------------------------------------------------------------------------------------\n");
					LOG("Recv function call (GATEWAY) -> localJobID = %d, sourceJobID = %d, functionID = %d\n",localJobID, sourceJobID, functionID);
					hexdump("Args",sendElem->getArgs(),sendElem->getArgsSize());
					LOG("-------------------------------------------------------------------------------------\n");

					//LOG("DEBUG :: send it to send Q\n");
				}
				else{
					DataQElem* localElem = new DataQElem();
					localElem->setIsFunctionCall(true);
					localElem->setArgs(args,argSize-4);
					localElem->setFunctionID(functionID);
					int localJobID = grm->getJID();
					localElem->setJobID(localJobID);
					localElem->setRetVal(NULL,0);
					grm->insertJobMapping(localJobID,outSocket,sourceJobID);
					//LOG("ljid : %d, os : %d, sjid :%d\n",localJobID,outSocket,sourceJobID);	
					pthread_mutex_lock(&localQLock);
					dqm->insertElementToLocalQ(localElem);
					//LOG("function %d is called\n",functionID);
					//hexdump("received argument",args,argSize-4);
					//LOG("address of local elem in recv q : %p\n",localElem);
					pthread_mutex_unlock(&localQLock);
					LOG("-------------------------------------------------------------------------------------\n");
					LOG("Recv function call (GATEWAY) -> localJobID = %d, sourceJobID = %d, functionID = %d\n",localJobID, sourceJobID, functionID);
					hexdump("Args",args,argSize-4);
					LOG("-------------------------------------------------------------------------------------\n");

					//LOG("DEBUG :: send it to local Q\n");
				}
			}
			else{
				int jobID = *(int*)(header+1);
				int retSize = *(int*)(header+5);
				payload = (char*)malloc(retSize);
				LOG("\n\njobID : %d, retSize : %d, payload : %s\n\n",jobID,retSize,payload);
				write(sock_d,&ack,1);
				if(retSize !=0)
					recvComplete(sock_d,payload,retSize);
				else
					payload = NULL;
				
				//check ret value is local -> assume that value is not local
				if(!grm->isLocalReturn(jobID)){
					LOG("insert returnvalue in sendQ\n");
					DataQElem* sendElem = new DataQElem();
					sendElem->setIsFunctionCall(false);
					sendElem->setArgs(NULL,0);
					sendElem->setJobID(jobID);
					sendElem->setFunctionID(0);
					sendElem->setRetVal(payload,retSize);
					pthread_mutex_lock(&sendQLock);
					dqm->insertElementToSendQ(sendElem);
					pthread_mutex_unlock(&sendQLock);
					LOG("-------------------------------------------------------------------------------------\n");
					LOG("Recv Return value (GATEWAY) -> localJobID = %d\n",jobID);
					hexdump("Return",payload,retSize);
					LOG("-------------------------------------------------------------------------------------\n");
				}
				else{
					DataQElem* localElem = new DataQElem();
					localElem->setIsFunctionCall(false);
					localElem->setArgs(NULL,0);
					localElem->setJobID(jobID);
					localElem->setFunctionID(0);
					localElem->setRetVal(payload,retSize);
					pthread_mutex_lock(&localQLock);
					dqm->insertElementToLocalQ(localElem);
					pthread_mutex_unlock(&localQLock);
					LOG("-------------------------------------------------------------------------------------\n");
					LOG("Recv Return value (GATEWAY) -> localJobID = %d\n",jobID);
					hexdump("Return",payload,retSize);
					LOG("-------------------------------------------------------------------------------------\n");



				}	
			}
		}
	}
	//int fid;
	//read(sock_d, &fid, sizeof(int));
	// FIXME: How to use addSlot?
	// EventManager::addSlot(callback, &fid);
}

/* eventHandler: Check the queue and process the queued events */
void*
sendQHandler(void* arg){
	LOG("DEBUG :: sendQHandler Function in gateway is started\n");
	//int sendSocket; // = grm->getSendSocket();
	char ack = 1;
	pthread_mutex_lock(&settingLock);
	sendQsetting = true;
	pthread_mutex_unlock(&settingLock);
	while(1){
		pthread_mutex_lock(&sendQLock);
		int sendQSize = dqm->getSendQSize();
		pthread_mutex_unlock(&sendQLock);
		if(sendQSize>0){
			pthread_mutex_lock(&sendQLock);
			DataQElem* sendElem = dqm->getSendQElement();
			pthread_mutex_unlock(&sendQLock);
			LOG("Send Q is handling\n");
			char header[9];
			char* payload;
			int payloadSize = 0;
			
			if(sendElem->getIsFunctionCall()){
				int jobID = sendElem->getJobID();
				int argSize = sendElem->getArgsSize()+4;
				int outSocket = grm->getFuncDest(sendElem->getFunctionID()); 
				memcpy(header+1,&jobID,4);
				memcpy(header+5,&argSize,4);
				header[0] = 'F';

				//sprintf(header,"%c%d%d",'F',sendElem->getJobID(),sendElem->getArgsSize()+4);
				int functionID = sendElem->getFunctionID();
				payload = (char*)malloc(sendElem->getArgsSize()+4);
				memcpy(payload,&functionID,4);
				if(sendElem->getArgsSize() >0)
					memcpy(payload+4,sendElem->getArgs(),argSize-4);
				//sprintf(payload,"%d%s",sendElem->getFunctionID(),(char*)sendElem->getArgs());
				sendComplete(outSocket,header,9);
				read(outSocket,&ack,1);
				sendComplete(outSocket,payload,(sendElem->getArgsSize()+4));
				LOG("-------------------------------------------------------------------------------------\n");
				LOG("Send function call (GATEWAY) -> sourceJobID = %d, functionID = %d\n", jobID, functionID);
				hexdump("Args",sendElem->getArgs(),argSize-4);
				LOG("-------------------------------------------------------------------------------------\n");
			}
			else{
				int localJobID = sendElem->getJobID();
				int sourceJobID = grm->getSourceJobID(localJobID);
				int outSocket = grm->getOutSocket(localJobID);
				int retSize = sendElem->getRetSize();
				LOG("local JobID :%d, sourceJobID : %d, outSocket : %d, retSize : %d\n",localJobID,sourceJobID,outSocket,retSize);
				header[0] = 'R';
				memcpy(header+1,&sourceJobID,4);
				memcpy(header+5,&retSize,4);
				//sprintf(header,"%c%d%d",'R',sourceJobID,sendElem->getRetSize());
				payload = (char*)malloc(sendElem->getRetSize());
				if(sendElem->getRetSize() > 0)
					memcpy(payload,sendElem->getRetVal(),sendElem->getRetSize());
					//sprintf(payload,"%s",(char*)sendElem->getRetVal());
				LOG("send return setting is complete, outSocket : %d\n",outSocket);
				//hexdump("header",header,9);
				sendComplete(outSocket,header,9);
				read(outSocket,&ack,1);
				if(sendElem->getRetSize() > 0)
					sendComplete(outSocket,payload,sendElem->getRetSize());
				LOG("-------------------------------------------------------------------------------------\n");
				LOG("Send return value (GATEWAY) -> localJobID = %d, sourceJobID = %d, return address = %p\n", localJobID, sourceJobID, sendElem->getRetVal());
				hexdump("Return",sendElem->getRetVal(),retSize);
				LOG("-------------------------------------------------------------------------------------\n");


				grm->deleteJobMapping(localJobID);
			}
		}
	}
	return NULL;
	//  while(true){
	// FIXME: look up the event queues and processAll?
	//    EventManager::processAll();
	//  }
}


/* remoteCallListener: Listen to remote call from other devices
   - One thread running this function per one device */
void*
connectionInit(void *sock_p){
	int sock_d = *(int*)sock_p;
	LOG("socket : %d\n",sock_d);
	char initHeader;
	char ackT = 'a';
	//write(sock_d,&ackT,1);
	LOG("recv initHeader\n");
	read(sock_d, &initHeader,1);
	LOG("initHeader : %c\n",initHeader);
	if(initHeader == 'I'){
		LOG("InSocket\n");
		int deviceID = grm->getDID();		
		write(sock_d,&deviceID,sizeof(int));
		grm->insertInSocket(deviceID,sock_d);
		dqm->addRecvQ(sock_d);
		recvQHandler(deviceID);	
		//DataQManager::addRecvQ(sock_d);
	}//init In socket
	else if(initHeader == 'O'){
		LOG("OutSocket\n");
		char ack = 'a';
		int buffer[2];
		int funcNum;
		int* functionID;
		write(sock_d,&ack,1);
		read(sock_d,buffer,8);
		LOG("DeviceID : %d\n",buffer[0]);
		functionID = (int*)malloc(sizeof(int)*buffer[1]);
		write(sock_d,&ack,1);
		read(sock_d,functionID,sizeof(int)*buffer[1]);
		for(int i=0;i<buffer[1];i++){
			grm->insertRouting(functionID[i],sock_d);
		}
		grm->insertOutSocket(buffer[0],sock_d);
		pthread_create(&sendQHandlerThread,NULL,&sendQHandler,NULL);
		grm->printRT();
		grm->printST(buffer[0]);
		LOG("\n\nsetting address : %p\n\n",&settingComplete);
		pthread_mutex_lock(&settingLock);
		settingIter++;
		settingComplete = true;
		if(settingComplete)
			LOG("setting is complete\n");
		pthread_mutex_unlock(&settingLock);

	}
	return NULL;	
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
	LOG("Num : %d\n",myFID.num);
	for(int i=0;i<myFID.num;i++)
		LOG("functionID : %d\n",myFID.fids[i]);

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

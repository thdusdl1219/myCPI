/***
 * comm_manager.cpp : communication manager using queue
 *
 * High-level Communication Layer using queue
 * written by: gyeongmin
 *
 * **/

#include <cstdlib>
#include <cstring>
#include <cassert>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "comm_manager.h"

using namespace std;

namespace corelab {

  void readComplete(int sock, char* data, uint32_t size){
    uint32_t recvSize = 0;
    while(size > 0){
      int rSize = read(sock,data,size);
      if(rSize > 0){
        data += rSize;
        size -= (uint32_t)rSize;
      }
    }
  }

  void writeComplete(int sock, char* data, uint32_t size){
    uint32_t writeSize = 0;
    while(size > 0){
      int wSize = write(sock,data,size);
      if(wSize > 0){
        data += wSize;
        size -= (uint32_t)wSize;
      }
    }
  }

  void* requestReceiver(void* cm){
    //LOG("request receiver is started\n");
    CommManager* commManager = (CommManager*)cm;
    int sock;
    int recvSize = 0;
    uint32_t recvHeader[3];
    char recvData[Q_MAX];

    while(1){

      size_t size = commManager->getSocketNumber();

      for(size_t i = 0; i < size; i++){
        sock = commManager->getSocketByIndex((uint32_t)i);
        recvSize = read(sock,recvHeader,12);
        if(recvSize > 0 ){
          printf("socket number is %d / recvSize : %d\n",(int)size,recvSize);

        }
        if(recvSize  == 12){
          //LOG("CommManager get job from client\n");
          readComplete(sock,recvData,recvHeader[0]);
          //LOG("CommManager get job from client complete\n");
          Job* newJob = new Job();
          newJob->tag = (TAG)recvHeader[1];
          newJob->data = (char*)malloc(recvHeader[0]);
          newJob->size = recvHeader[0];
          memcpy(newJob->data,recvData,recvHeader[0]);
          newJob->sourceID = recvHeader[2];
          //LOG("CommManager gives job to handler thread before\n");
          if(newJob->tag == 1000)
            commManager->insertDataToRecvQue(newJob->data,newJob->size,newJob->sourceID);         
          else if(newJob->tag == 3)
            (*(*(commManager->getCallbackList()))[newJob->tag])(newJob->data,newJob->size,newJob->sourceID);
          else
            commManager->insertJob(newJob);
          

          //LOG("CommManager gives job to handler thread\n");
                    //(*(*callbackList)[recvHeader[1]])((void*)recvData);
        }
      }  
    }
  }

  void* requestHandler(void* cm){
    CommManager* commManager = (CommManager*)cm;
    std::map<TAG, CallbackType>* callbackList = commManager->getCallbackList();

    while(1){
      int jobNum = commManager->getJobQueSize();
      if(jobNum > 0){
        Job* job = commManager->getJob();
        (*(*callbackList)[job->tag])(job->data,job->size,job->sourceID);
      }
    }
  }


   

  CommManager::CommManager(){
    callbackList = new std::map<TAG, CallbackType>();
    localID = -1;
    clntID = 1;
  }

  void CommManager::start(){
    pthread_create(&receivingThread,NULL,requestReceiver,(void*)this);
    pthread_create(&handlingThread,NULL,requestHandler,(void*)this);
    //pthread_detach(receivingThread);
    //pthread_detach(handlingThread);
  }

  int CommManager::open(int port, int num){

    LOG("DEBUG :: comm_manager open\n");
    struct sockaddr_in eptHost;

    int idHost = socket (PF_INET, SOCK_STREAM, 0);
    if (idHost < 0) {
      //perror ("socket");
      return -1;
    }

    memset (&eptHost, 0, sizeof (eptHost));
		eptHost.sin_family = AF_INET;
		eptHost.sin_addr.s_addr = htonl (INADDR_ANY);
		eptHost.sin_port = htons (port);

		int res = 0;
		if (bind (idHost, (sockaddr *)&eptHost, sizeof (eptHost)) == -1) {
			//perror ("bind");
			return false;
		}

		if (listen (idHost, 5) == -1) {
			//perror ("listen");
			return false;
		}

		struct sockaddr_in eptClient;
		char strClientIP[20];
		socklen_t sizeEptClient = sizeof (eptClient);

    while(num != 0) { 
      //pthread_mutex_lock(&mutex);
      int *clientID = (int *) malloc(sizeof(int));
      *clientID = accept (idHost, (sockaddr *)&eptClient, &sizeEptClient);

      if (*clientID == -1) {
        //perror ("accept");
        return false;
      }
      inet_ntop (AF_INET, &eptClient.sin_addr, strClientIP, 20);

      initializeSocketOpt (*clientID);

      std::map<TAG,Queue*>* sques = new std::map<TAG,Queue*>();
      Queue* rque = (Queue*)malloc(sizeof(Queue));
      assert(sques != NULL);
      assert(rque != NULL);
      initializeQueue(*rque);


      uint32_t initial_data[2];
      initial_data[0] = localID;
      initial_data[1] = clntID;
      writeComplete(*clientID,(char*)initial_data,8);

      socketMap[clntID] = *clientID;


      sendQues.insert(std::pair<int, std::map<TAG,Queue*>*>(clntID, sques));
      recvQues.insert(std::pair<int, Queue*>(clntID, rque)); 
      clntID++;

      if(num>0)
        num--;
    }  
		return 0;
  }

  int CommManager::tryConnect(const char* ip, int port){
    
    LOG("DEBUG :: tryConnect\n");
		struct sockaddr_in eptClient;

    int* idClient = (int*)malloc(sizeof(int));
		*idClient = socket (PF_INET, SOCK_STREAM, 0);
		if (idClient < 0) {
			//perror ("socket");
			return -1;
		}

		memset (&eptClient, 0, sizeof (sockaddr_in));
		eptClient.sin_family = AF_INET;
		eptClient.sin_addr.s_addr = inet_addr (ip);
		eptClient.sin_port = htons (port);
		if (connect(*idClient, (sockaddr *)&eptClient, sizeof (eptClient)) == -1) {
			//perror ("connect");
			return false;
		}

		initializeSocketOpt (*idClient);

    uint32_t initial_data[2];
    readComplete(*idClient,(char*)initial_data,8);

    socketMap[initial_data[0]] = *idClient;
    localID = initial_data[1];


    std::map<TAG,Queue*>* sques = new std::map<TAG,Queue*>();
    Queue* rque = (Queue*)malloc(sizeof(Queue));
    assert(sques != NULL);
    assert(rque != NULL);
    initializeQueue(*rque);

    sendQues.insert(std::pair<int, std::map<TAG,Queue*>*>(initial_data[0], sques));
    recvQues.insert(std::pair<int, Queue*>(initial_data[0], rque)); 

    return initial_data[0];
  }

  void CommManager::closeConnection(int* cid){
    close(socketMap[*cid]);
  }

  void CommManager::setNewConnectionCallback(void callback(void*)){
    connectionCallback = callback;
  }

  void CommManager::setLocalID(uint32_t id){
    localID = id;
  }

  uint32_t CommManager::getLocalID(){
    return localID;
  }

  bool CommManager::pushWord(TAG tag, QWord word, uint32_t destID){
    Queue* targetQue;
    if(tag == 0){
      //error
      return false;
    }
    
    if(sendQues[destID]->find(tag) == sendQues[destID]->end()){
      Queue* newQue = (Queue*)malloc(sizeof(Queue));
      initializeQueue(*newQue);
      sendQues[destID]->insert(std::pair<TAG,Queue*>(tag,newQue));
      targetQue = newQue;
    }
    else
      targetQue = (*(sendQues[destID]))[tag];
      
    //LOG("before check sendques\n");
    if((targetQue->size) > 262140)
      return false;
    *((QWord*)(targetQue->head)) = word;
    //LOG("before check sendques\n");
    targetQue->head += 4;
    //LOG("before check sendques\n");
    targetQue->size += 4;
    //LOG("before check sendques\n");
    return true;
  }

  bool CommManager::pushRange(TAG tag, const void* data, size_t size, uint32_t destID){
    Queue* targetQue;
    if(tag == 0){
      //error
      return false;
    }

    if(sendQues[destID]->find(tag) == sendQues[destID]->end()){
      Queue* newQue = (Queue*)malloc(sizeof(Queue));
      sendQues[destID]->insert(std::pair<TAG,Queue*>(tag,newQue));
      targetQue = newQue;
    }
    else
      targetQue = (*(sendQues[destID]))[tag];

    QWord lastSize = targetQue->size + size;
    if(lastSize > 262144)
      return false;
    memcpy(targetQue->head, data, size);
    targetQue->head += (int)size;
    targetQue->size += (QWord)size;
    return true;
  }

  void CommManager::sendQue(TAG tag, uint32_t destID){

    uint32_t header[3];


    int sock = socketMap[destID];

    if((sendQues[destID]->find(tag) == sendQues[destID]->end()) && (tag != 0))
      return; // error : There is no queue for cid & tag

    if(tag != 0){
      Queue* targetQue = (*(sendQues[destID]))[tag];

      header[0] = targetQue->size;
      header[1] = tag;
      header[2] = localID;
      writeComplete(sock,(char*)header,12);
      writeComplete(sock,targetQue->data,targetQue->size);
      
      memset(&header,0,12);
      initializeQueue(*targetQue);

    }
    else{
      std::map<TAG,Queue*>* queList = sendQues[destID];
      
      for(std::map<TAG,Queue*>::iterator it = queList->begin(); it!= queList->end(); ++it){
        Queue* targetQue = it->second;
        header[0] = targetQue->size;
        header[1] = it->first;
        header[2] = localID;
        writeComplete(sock,(char*)&header,12);
        writeComplete(sock,targetQue->data,header[0]);
        memset(&header,0,12);
        initializeQueue(*targetQue);
      }
    }

  }

  QWord CommManager::takeWord(uint32_t sourceID){
    Queue* targetQue = NULL;

    //assert((sourceID) && "cannot take word from queue whose cid is NULL");

    targetQue = recvQues[sourceID];
    assert((targetQue != NULL) && "cannot take word from NULL queue");
    
    QWord word;
    word = *(QWord*)(targetQue->head);
    
    targetQue->head += 4;
    targetQue->size -= 4;
    
    if(targetQue->size == 0)
      initializeQueue(*targetQue);

    return word;
  }

  bool CommManager::takeRange(void* buf, size_t size, uint32_t sourceID){
    Queue* targetQue = NULL;

    //assert((cid != NULL) && "cannot range word from queue whose cid is NULL");

    targetQue = recvQues[sourceID];
    assert((targetQue != NULL) && "cannot range word from NULL queue");

    memcpy(buf,targetQue->head,size);

    targetQue->head += size;
    targetQue->size -= size;

    if(targetQue->size == 0)
      initializeQueue(*targetQue);

    return true;
  }

  void CommManager::receiveQue(uint32_t sourceID){
    
    pthread_mutex_lock(&recvFlagLock);
    recvFlags[sourceID] = false;
    pthread_mutex_unlock(&recvFlagLock);
    //LOG("RECEIVE que is set\n");
    while(1){
      pthread_mutex_lock(&recvFlagLock);
      if(recvFlags[sourceID]){
        //LOG("RECEIVE message!!!\n");
        pthread_mutex_unlock(&recvFlagLock);
        break;
      }
      pthread_mutex_unlock(&recvFlagLock);
    }
  }

  //void CommManager::sendWord(QWord word, int* cid){
    
  //}

  void CommManager::initializeSocketOpt (int sock) {
    int res;

    assert (sock > 0 && "socket id must be assined before initializing socket");

    int bufsize = Q_MAX * 10;
    res = setsockopt (sock, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof (int));
    res = setsockopt (sock, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof (int));
    assert (!res && "initialization failed: cannot set socket buffer size");

    int reuseaddr = 1;
    res = setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof (int));
    assert (!res && "initialization failed: cannot set to reuse address");

    int flags;
    flags = fcntl (sock, F_GETFL, 0);
    assert (flags >= 0 && "initialization failed: cannot get socket flags");
    res = fcntl (sock, F_SETFL, flags | O_NONBLOCK);
    assert (res >= 0 && "initialization failed: cannot add nonblock flag to socket");

    int opt=1;
    res = setsockopt(sock, SOL_TCP, TCP_NODELAY, &opt, sizeof(opt));
    opt = 0;
    res = setsockopt(sock, SOL_TCP, TCP_CORK, &opt, sizeof(opt));
    assert (!res && "initialization failed: cannot set optimization flags");
  }

  void CommManager::initializeQueue (Queue& que) {
    memset (que.data, 0, Q_MAX);
    que.size = 0;
    que.head = que.data;
  }

  void CommManager::setCallback(TAG tag, void callback(void*, uint32_t, uint32_t)){
    pthread_mutex_lock(&callbackLock);
    callbackList->insert(std::pair<TAG, CallbackType>(tag, (CallbackType)callback));
    pthread_mutex_unlock(&callbackLock);
  }

  CallbackType CommManager::getCallback(TAG tag){
    return (*callbackList)[tag];
  }

  std::map<TAG, CallbackType>* CommManager::getCallbackList(){
    return callbackList;
  }

  int CommManager::getSocketByIndex(uint32_t idx){
    uint32_t i = 0;
    for(std::map<uint32_t,int>::iterator it = socketMap.begin(); it != socketMap.end(); ++it){
      if(i == idx)
        return it->second;
      i++;
    }
    return -1;
  }

  int CommManager::getSocketNumber(){
    return (int)socketMap.size();
  }

  void CommManager::insertJob(Job* newJob){
    //LOG("DEBUG :: comm manager : insert job\n");
    pthread_mutex_lock(&jobQueLock);

    //LOG("DEBUG :: comm manager : insert job before\n");
    jobQue.push(newJob);
    //LOG("DEBUG :: comm manager : insert job after \n");
    pthread_mutex_unlock(&jobQueLock);
    //LOG("DEBUG :: comm manager : insert job end \n");
  }

  void CommManager::insertDataToRecvQue(void* data, uint32_t size, uint32_t cid){
    pthread_mutex_lock(&recvFlagLock);
    Queue* targetQue = recvQues[cid];
    targetQue->size = size;
    targetQue->head = targetQue->data;
    memcpy(targetQue->data,data,size);
    recvFlags[cid] = true;
    //LOG("insert data to recv que\n");
    pthread_mutex_unlock(&recvFlagLock);

  }

  Job* CommManager::getJob(){
    Job* job;
    pthread_mutex_lock(&jobQueLock);
    job = jobQue.front();
    jobQue.pop();
    pthread_mutex_unlock(&jobQueLock);
    return job;
  }

  int CommManager::getJobQueSize(){
    int jobNum;
    pthread_mutex_lock(&jobQueLock);
    jobNum = (int)jobQue.size();
    pthread_mutex_unlock(&jobQueLock);
    return jobNum;
  }

}

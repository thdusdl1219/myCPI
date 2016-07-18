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
    CommManager* commManager = (CommManager*)cm;
    int sock;
    int recvSize = 0;
    uint32_t recvHeader[2];
    char recvData[Q_MAX];

    while(1){

      size_t size = commManager->getSocketNumber();

      for(size_t i = 0; i < size; i++){
        sock = commManager->getSocketByIndex((uint32_t)i);
        recvSize = read(sock,recvHeader,8);
        if(recvSize  == 8){
          readComplete(sock,recvData,recvHeader[0]);
          Job* newJob = new Job();
          newJob->tag = (TAG)recvHeader[1];
          newJob->data = (char*)malloc(recvHeader[0]);
          memcpy(newJob->data,recvData,recvHeader[0]);
          commManager->insertJob(newJob);
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
        (*(*callbackList)[job->tag])(job->data);
      }
    }
  }


  

  CommManager::CommManager(){
    callbackList = new std::map<TAG, CallbackType>();
  }

  void CommManager::start(){
    pthread_create(&receivingThread,NULL,requestReceiver,(void*)this);
    pthread_create(&handlingThread,NULL,requestHandler,(void*)this);
    pthread_detach(receivingThread);
    pthread_detach(handlingThread);
  }

  int CommManager::open(int port, int num){

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

      initializeSocketOpt (clientID);

      std::map<TAG,Queue*>* sques = new std::map<TAG,Queue*>();
      std::map<TAG,Queue*>* rques = new std::map<TAG,Queue*>();
      assert(sques != NULL);
      assert(rques != NULL);
      //initializeQueue(*sque);
      //initializeQueue(*rque);

      sendQues.insert(std::pair<int, std::map<TAG,Queue*>*>(*clientID, sques));
      recvQues.insert(std::pair<int, std::map<TAG,Queue*>*>(*clientID, rques)); 

      if(num>0)
        num--;
    }  

		return 0;
  }

  int CommManager::tryConnect(const char* ip, int port){
  
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

		initializeSocketOpt (idClient);
		
		return *idClient;
  }

  void CommManager::closeConnection(int* cid){
    close(socketMap[*cid]);
  }

  void CommManager::setNewConnectionCallback(void callback(void*)){
    connectionCallback = callback;
  }

  bool CommManager::pushWord(TAG tag, QWord word, int* cid){
    Queue* targetQue;
    if(tag == 0){
      //error
      return false;
    }
    
    if(sendQues[*cid]->find(tag) == sendQues[*cid]->end()){
      Queue* newQue = (Queue*)malloc(sizeof(Queue));
      sendQues[*cid]->insert(std::pair<TAG,Queue*>(tag,newQue));
      targetQue = newQue;
    }
    else
      targetQue = (*(sendQues[*cid]))[tag];
      
    if((targetQue->size) > 262140)
      return false;
    *((QWord*)(targetQue->head)) = word;
    targetQue->head += 4;
    targetQue->size += 4;
    return true;
  }

  bool CommManager::pushRange(TAG tag, const void* data, size_t size, int* cid){
    Queue* targetQue;
    if(tag == 0){
      //error
      return false;
    }

    if(sendQues[*cid]->find(tag) == sendQues[*cid]->end()){
      Queue* newQue = (Queue*)malloc(sizeof(Queue));
      sendQues[*cid]->insert(std::pair<TAG,Queue*>(tag,newQue));
      targetQue = newQue;
    }
    else
      targetQue = (*(sendQues[*cid]))[tag];

    QWord lastSize = targetQue->size + size;
    if(lastSize > 262144)
      return false;
    memcpy(targetQue->head, data, size);
    targetQue->head += (int)size;
    targetQue->size += (QWord)size;
    return true;
  }

  void CommManager::sendQue(TAG tag, int* cid){

    if(cid == NULL)
      return;

    int sock = socketMap[*cid];

    if(sendQues[*cid]->find(tag) == sendQues[*cid]->end())
      return; // error : There is no queue for cid & tag

    if(tag != 0){
      Queue* targetQue = (*(sendQues[*cid]))[tag];
      int sock = socketMap[*cid];
      writeComplete(sock,targetQue->data,targetQue->size);

      initializeQueue(*targetQue);

    }
    else{
      std::map<TAG,Queue*>* queList = sendQues[*cid];
      uint32_t header[2];
      
      for(std::map<TAG,Queue*>::iterator it = queList->begin(); it!= queList->end(); ++it){
        Queue* targetQue = it->second;
        header[0] = targetQue->size;
        header[1] = it->first;
        writeComplete(sock,(char*)&header,8);
        writeComplete(sock,targetQue->data,header[0]);
        memset(&header,0,8);
        initializeQueue(*targetQue);
      }
    }

  }

  //void CommManager::sendWord(QWord word, int* cid){
    
  //}

  void CommManager::initializeSocketOpt (int *clientID) {
    int res;
    int sock = socketMap[*clientID];

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

  void CommManager::setCallback(TAG tag, void callback(void*)){
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
    for(std::map<int,int>::iterator it = socketMap.begin(); it != socketMap.end(); ++it){
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
    pthread_spin_lock(&jobQueLock);
    jobQue.push(newJob);
    pthread_spin_unlock(&jobQueLock);
  }

  Job* CommManager::getJob(){
    Job* job;
    pthread_spin_lock(&jobQueLock);
    job = jobQue.front();
    jobQue.pop();
    pthread_spin_unlock(&jobQueLock);
    return job;
  }

  int CommManager::getJobQueSize(){
    int jobNum;
    pthread_spin_lock(&jobQueLock);
    jobNum = (int)jobQue.size();
    pthread_spin_unlock(&jobQueLock);
    return jobNum;
  }

  //static )Debug DEBUG("qsocket");

  //static inline int _linux_close (int fd);
  /*static inline int _linux_connect (int socket, const sockaddr *address,
    socklen_t address_len);*/

  //#ifdef OVERHEAD_TEST
	//OVERHEAD_TEST_DECL
	//#endif

//#ifdef SERVER_SIDE_OVERHEAD_TEST
//    FILE *fp = NULL;
//#endif
/*
	QSocket::QSocket () {
		initializeFields ();
    clientRoutine = NULL;
#ifdef DEBUG_UVA
		DEBUG.PRINT ("socket allocated (addr: %p)", this);
#endif

		#ifdef OVERHEAD_TEST
		OHDTEST_SETUP ();
		#endif

#ifdef SERVER_SIDE_OVERHEAD_TEST
    fp = fopen("SERVER_SIDE_OVERHEAD_TEST_RESULT.txt", "w");
#endif
	}

	// Send queue interface
	bool QSocket::pushWord (QSocketWord word, int *clientID) {
		  return pushToSendQue (&word, sizeof(QSocketWord), clientID);
	}

	void QSocket::pushWordF (QSocketWord word, int *clientID) {
    //printf("pushWordF: sizeof(QSocketWord) = %d\n", sizeof(QSocketWord));
		while (!pushToSendQue (&word, sizeof(QSocketWord), clientID)) {
			sendQue (clientID);			// flush queue
		}
    //hexdump(&word, sizeof(QSocketWord));
	}

	bool QSocket::pushRange (const void *data, size_t size, int *clientID) {
		  return pushToSendQue (data, size, clientID);
	}

	void QSocket::pushRangeF (const void *data, size_t size, int *clientID) {
		while (!pushToSendQue (data, size, clientID)) {
			sendQue (clientID);			// flush queue
		}
	}

	void QSocket::sendQue (int *clientID) {
    Queue* que;
    if(clientID) {
      que = sendQues[*clientID];
    }
    else {
      que = &queSend;
    }

		send (&que->size, sizeof(QSocketWord), clientID);
DEBUG_STMT (fprintf (stderr, "sendsize:%u\n", que->size));
		send (que->data, que->size, clientID);
DEBUG_STMT (fprintf (stderr, "data sended\n"));
		initializeQueue (*que);
	}
	
	// Direct send interface
	void QSocket::sendWord (QSocketWord word, int *clientID) {
		send (&word, sizeof(QSocketWord), clientID);
	}

	void QSocket::sendRange (const void *data, size_t size, int *clientID) {
		send (data, size, clientID);
DEBUG_STMT (fprintf (stderr, "direct_sendsize:%u\n", size));
	}

	// Receive queue interface
	QSocketWord QSocket::takeWord (bool *hr, int* clientID) {
		QSocketWord word;
    //printf("takeWord: sizeof(QSocketWord) = %d\n", sizeof(QSocketWord));
		bool _hr = takeFromRecvQue (&word, sizeof(QSocketWord), clientID);

    //hexdump(&word, sizeof(QSocketWord));
		if (hr != NULL) *hr = _hr;
		return word;
	}
	
	QSocketWord QSocket::takeWordF (int* clientID) {
		QSocketWord word;

    //printf("takeWordF: sizeof(QSocketWord) = %d\n", sizeof(QSocketWord));
		while (!takeFromRecvQue (&word, sizeof(QSocketWord), clientID)) {
		  receiveQue (clientID);			// refill queue
		}

		return word;
	}

	bool QSocket::takeRange (void *buf, size_t size, int* clientID) {
		return takeFromRecvQue (buf, size, clientID);
	}

	void QSocket::takeRangeF (void *buf, size_t size, int* clientID) {
		while (!takeFromRecvQue (buf, size, clientID)) {
			receiveQue (clientID);			// refill queue
		}
	}

	void QSocket::receiveQue (int *clientID) {
    Queue* que;

    if(clientID) {
#ifdef DEBUG_UVA
      //printf("recvQues size : %d\n", recvQues.size());
#endif
      que = recvQues[*clientID];
    }
    else {
      que = &queRecv;
    }
    assert(que != NULL);
		initializeQueue (*que);
		receive (&que->size, sizeof(QSocketWord), clientID);
DEBUG_STMT (fprintf (stderr, "recvsize:%u\n", que->size));
		receive (que->data, que->size, clientID);
DEBUG_STMT (fprintf (stderr, "data received\n"));
	} 

	// Direct receive interface
	QSocketWord QSocket::receiveWord (int *clientID) {
		QSocketWord res;
		receive (&res, sizeof(QSocketWord), clientID);
		return res;
	}

	void QSocket::receiveRange (void *buf, size_t size, int* clientID) {
		receive (buf, size, clientID);
DEBUG_STMT (fprintf (stderr, "direct_recvsize:%u\n", size));
	}


	// Socket host interface
	bool QSocket::open (const char *port) {
		struct sockaddr_in eptHost;

		initializeFields ();

		DEBUG.BEGIN_TASK ("CONFIG", "configuring socket..");
		idHost = socket (PF_INET, SOCK_STREAM, 0);
		if (idHost < 0) {
			DEBUG.EXIT_TASK ("CONFIG", "failed: cannot assign socket id");
			perror ("socket");
			return false;
		}
#ifdef DEBUG_UVA
		DEBUG.PRINT ("socket id assined (id:%d)", idHost);
#endif

		memset (&eptHost, 0, sizeof (eptHost));
		eptHost.sin_family = AF_INET;
		eptHost.sin_addr.s_addr = htonl (INADDR_ANY);
		eptHost.sin_port = htons (atoi (port));

		int res = 0;
		if (bind (idHost, (sockaddr *)&eptHost, sizeof (eptHost)) == -1) {
			DEBUG.EXIT_TASK ("CONFIG", "failed: cannot bind socket");
			perror ("bind");
			return false;
		}
#ifdef DEBUG_UVA
		DEBUG.PRINT ("socket bound (port:%hu)", ntohs (eptHost.sin_port));
#endif
		DEBUG.END_TASK ("CONFIG");

		DEBUG.BEGIN_TASK ("LISTEN", "listening..");
		if (listen (idHost, 5) == -1) {
			DEBUG.EXIT_TASK ("LISTEN", "failed: cannot listen to client");
			perror ("listen");
			return false;
		}

		struct sockaddr_in eptClient;
		char strClientIP[20];
		socklen_t sizeEptClient = sizeof (eptClient);
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    // for multi-client server model implementation. @soyeon
    while(true) { 
		  pthread_mutex_lock(&mutex);
        int *clientID = (int *) malloc(sizeof(int));
        *clientID = accept (idHost, (sockaddr *)&eptClient, &sizeEptClient);
		   
      if (idClient == -1) {
			  DEBUG.EXIT_TASK ("LISTEN", "failed: cannot accept client");
			  perror ("accept");
			  return false;
      }
#ifdef DEBUG_UVA
      printf("client ID : %d\n", *clientID);
#endif
		  inet_ntop (AF_INET, &eptClient.sin_addr, strClientIP, 20);
#ifdef DEBUG_UVA
		  DEBUG.PRINT ("connected. (ip:%s)", strClientIP);
#endif

		  initializeSocketOpt (clientID);

      Queue* sque = (Queue*) malloc(sizeof(Queue));
      Queue* rque = (Queue*) malloc(sizeof(Queue));
      assert(sque != NULL);
      assert(rque != NULL);
      initializeQueue(*sque);
      initializeQueue(*rque);


      sendQues.insert(std::pair<int, Queue*>(*clientID, sque));
      recvQues.insert(std::pair<int, Queue*>(*clientID, rque));

      pthread_t clientThread;
      pthread_attr_t threadAttr;
      
      if(pthread_attr_init(&threadAttr) != 0) {
			  DEBUG.PRINT ("failed: cannot pthread attr init");
			  perror ("accept");
        return false;
      }
      
      if(pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED) != 0) {
			  DEBUG.PRINT ("failed: cannot pthread attr setdetachstate");
			  perror ("accept");
        return false;
      }
      assert(clientRoutine != NULL && "client routine cannot null");
      pthread_create(&clientThread, &threadAttr , clientRoutine, clientID);
		  pthread_mutex_unlock(&mutex);
    }


		tySocket = QSOCK_HOST;
		return true;
	}

	bool QSocket::close () {
		assert (tySocket != QSOCK_CLIENT && "client socket cannot call 'close'");
		if (tySocket == QSOCK_NONE) {
			DEBUG.PRINT ("close() failed: cannot close un-opened socket");
			return false;
		}

		_linux_close (idHost);
		_linux_close (idClient);
		DEBUG.PRINT ("closed.");

		#ifdef OVERHEAD_TEST
		FILE *fres = fopen ("overhead.server.profile", "w");
		OHDTEST_PRINT_RESULT (fres);
		fclose (fres);
		#endif

#ifdef SERVER_SIDE_OVERHEAD_TEST_RESULT
    fclose(fp);
#endif
		return true;
	}

  void QSocket::setClientRoutine(void* func(void *)) {
    clientRoutine = func;
  }


	// Socket client interface
	bool QSocket::connect (const char *ip, const char *port) {
		struct sockaddr_in eptClient;

		initializeFields ();

		DEBUG.BEGIN_TASK ("CONFIG", "configuring socket..");
		idClient = socket (PF_INET, SOCK_STREAM, 0);
		if (idClient < 0) {
			DEBUG.EXIT_TASK ("CONFIG", "failed: cannot assign socket id");
			perror ("socket");
			return false;
		}

		memset (&eptClient, 0, sizeof (sockaddr_in));
		eptClient.sin_family = AF_INET;
		eptClient.sin_addr.s_addr = inet_addr (ip);
		eptClient.sin_port = htons (atoi (port));
		DEBUG.END_TASK ("CONFIG");

		DEBUG.BEGIN_TASK ("CONNECT", "connecting to server.. (ip:%s, port:%hu)", ip, ntohs (eptClient.sin_port));
		if (_linux_connect (idClient, (sockaddr *)&eptClient, sizeof (eptClient)) == -1) {
			DEBUG.EXIT_TASK ("CONNECT", "failed: cannot connect to server");
			perror ("connect");
			return false;
		}
#ifdef DEBUG_UVA
		DEBUG.PRINT ("connected.");
#endif
		DEBUG.END_TASK ("CONNECT");

		initializeSocketOpt ();
		
		tySocket = QSOCK_CLIENT;

		#ifdef OVERHEAD_TEST
		OHDTEST_PUSH_SECTION ("Client Non-targets Exe.");
		#endif

		return true;
	}

	bool QSocket::disconnect () {
		assert (tySocket != QSOCK_HOST && "host socket cannot call 'disconnect'");
		if (tySocket == QSOCK_NONE) {
			DEBUG.PRINT ("disconnect() failed: cannot disconnect un-connected socket");
			return false;
		}

    pushWordF(0xffffffff);
    pushWordF(0x00000000);
    sendQue();

		_linux_close (idClient);
		DEBUG.PRINT ("closed.");

		#ifdef OVERHEAD_TEST
		FILE *fres = fopen ("overhead.client.profile", "w");
		OHDTEST_PRINT_RESULT (fres);
		fclose (fres);
		#endif

		return true;
	}
		
	// State interface
	QSocketType QSocket::getType () {
		return tySocket;
	}


	/// (Private) Initializer
	void QSocket::initializeFields () {
		idHost = 0;
		idClient = 0;
		tySocket = QSOCK_NONE;

		initializeQueue (queSend);
		initializeQueue (queRecv);
	}

	void QSocket::initializeSocketOpt (int *clientID) {
		int res;
    int id = idClient;
    if(clientID)
      id = *clientID;

		assert (id > 0 && "socket id must be assined before initializing socket");

		int bufsize = QUE_MAX * 10;
		res = setsockopt (id, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof (int));
		res = setsockopt (id, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof (int));
		assert (!res && "initialization failed: cannot set socket buffer size");
		
		int reuseaddr = 1;
		res = setsockopt (id, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof (int));
		assert (!res && "initialization failed: cannot set to reuse address");

		int flags;
		flags = fcntl (id, F_GETFL, 0);
		assert (flags >= 0 && "initialization failed: cannot get socket flags");
		res = fcntl (id, F_SETFL, flags | O_NONBLOCK);
		assert (res >= 0 && "initialization failed: cannot add nonblock flag to socket");
	}

	inline void QSocket::initializeQueue (Queue& que) {
		memset (que.data, 0, QUE_MAX);
		que.size = 0;
		que.head = que.data;
	}

	/// (Private) Internals
	inline bool QSocket::pushToSendQue (const void *data, size_t size, int *clientID) {
    Queue* que;

    if(clientID) {
      que = sendQues[*clientID];
    }
    else {
      que = &queSend; 
    }

		if (que->size + size > QUE_MAX)
			return false;

		memcpy (que->head, data, size);
		que->size += size;
		que->head += size;

		return true;
	}

	inline bool QSocket::takeFromRecvQue (void *data, size_t size, int *clientID) {
    Queue* que;
    
    if(clientID) {
      que = recvQues[*clientID];
    }
    else {
      que = &queRecv;
    }

		if ((uintptr_t)que->head + size > (uintptr_t)que->data + que->size)
			return false;

		memcpy (data, que->head, size);
		que->head += size;

		return true;
	}

	inline void QSocket::send (const void *data, size_t size, int *clientID) {
		const char *_data = (const char *)data;
    int id = idClient;
    if(clientID){
      id = *clientID;
#ifdef SERVER_SIDE_OVERHEAD_TEST
      fp = fopen("SERVER_SIDE_OVERHEAD_TEST_RESULT.txt", "a");
      fprintf(fp, "SEND %lu\n", size);
      printf(stderr, "SEND %lu\n", size);
      fclose(fp);
#endif
    }
//int i = 0;
//DEBUG_STMT (fprintf (stderr, "start sending.. (size:%u)\n", size));
		while (size > 0) {
			size_t sendSize = write (id, _data, size);

			if (sendSize != -1) {
				size -= sendSize;
				_data += sendSize;
			}
//if (i < 3) DEBUG_STMT (fprintf (stderr, "\tremain_size:%d\n", size));
//i++;
		}
	}

	inline void QSocket::receive (void *buf, size_t size, int *clientID)	{
		char *_buf = (char *)buf;
    int id = idClient;
    if(clientID){
      id = *clientID;
#ifdef SERVER_SIDE_OVERHEAD_TEST
      fp = fopen("SERVER_SIDE_OVERHEAD_TEST_RESULT.txt", "a");
      fprintf(fp, "RECV %lu\n", size);
      printf(stderr, "RECV %lu\n", size);
      fclose(fp);
#endif
    }
//int i = 0;
//DEBUG_STMT (fprintf (stderr, "start receiving.. (size:%u)\n", size));
		while (size > 0) {
			size_t recvSize = read (id, _buf, size);

			if (recvSize != -1) {
				size -= recvSize;
				_buf += recvSize;
			}
		}
//if (i < 3) DEBUG_STMT (fprintf (stderr, "\tremain_size:%d\n", size));
//i++;
	}

	
	/// (Static) Global function wrapper
	static inline int _linux_close (int fd) {
		return close (fd);
	}

	static inline int _linux_connect (int socket, const sockaddr *address,
		socklen_t address_len) {
		return connect (socket, address, address_len);
	}
}*/
}

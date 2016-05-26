/***
 * qsocket.cpp: Queue Socket
 *
 * High-level queue socket
 * XXX ASSUME HOST & CLIENT HAVE THE SAME ENDIANNESS XXX
 * written by: gwangmu
 *
 * **/

#include <stdio.h>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "corelab/Utilities/Debug.h"
#include "qsocket.h"
#define NDEBUG // BONGJUN
#include "debug.h"

#include "hexdump.h" // BONGJUN

//#define SERVER_SIDE_OVERHEAD_TEST
//#define OVERHEAD_TEST

#ifdef OVERHEAD_TEST
#include "overhead.h"
#endif

#define DEBUG_UVA

//using namespace std;

namespace corelab {
	static Debug DEBUG("qsocket");

	static inline int _linux_close (int fd);
	static inline int _linux_connect (int socket, const sockaddr *address,
		socklen_t address_len);

	#ifdef OVERHEAD_TEST
	OVERHEAD_TEST_DECL
	#endif

#ifdef SERVER_SIDE_OVERHEAD_TEST
    FILE *fp = NULL;
#endif

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
		   
      if (*clientID == -1) {
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
      fprintf(stderr, "SEND %lu\n", size);
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
      fprintf(stderr, "RECV %lu\n", size);
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
}

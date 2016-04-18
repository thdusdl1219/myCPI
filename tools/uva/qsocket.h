/***
 * qsocket.h: Queue Socket
 *
 * High-level queue socket
 * XXX ASSUME HOST & CLIENT HAVE THE SAME ENDIANNESS XXX
 * written by: gwangmu
 *
 * **/

#ifndef CORELAB_QSOCKET_H
#define CORELAB_QSOCKET_H

#include <inttypes.h>
#include <sys/socket.h>
#include <cstddef>
#include <map>
#include <pthread.h>

#ifdef __x86_64__
#	define QSOCKET_WORD(x) ((QSocketWord)(uint64_t)x)  // takes lower 32 bits
#else
#	define QSOCKET_WORD(x) ((QSocketWord)x)
#endif

#define QUE_MAX 	524288 			// max queue elements (512kB)

namespace corelab {
	enum QSocketType { QSOCK_NONE, QSOCK_HOST, QSOCK_CLIENT };
	typedef uint32_t QSocketWord;

	class QSocket {
	public:
		QSocket ();

		// Send queue interface
		bool pushWord (QSocketWord word, int *clientID = NULL);
		void pushWordF (QSocketWord word, int *clientID = NULL);
		bool pushRange (const void *data, size_t size, int *clientID = NULL);
		void pushRangeF (const void *data, size_t size, int *clientID = NULL);
		void sendQue (int *clientID = NULL);
		
		// Direct send interface
		void sendWord (QSocketWord word, int *clientID = NULL);
		void sendRange (const void *data, size_t size, int *clientID = NULL);

		// Receive queue interface
		QSocketWord takeWord (bool *hr = NULL, int *clientID = NULL);
		QSocketWord takeWordF (int *clientID = NULL);
		bool takeRange (void *buf, size_t size, int *clientID = NULL);
		void takeRangeF (void *buf, size_t size, int *clientID = NULL);
		void receiveQue (int *clientID = NULL);

		// Direct receive interface
		QSocketWord receiveWord (int *clientID = NULL);
		void receiveRange (void *buf, size_t size, int *clientID = NULL);

		// Socket host interface
		bool open (const char *port);
		bool close ();
    
    // set client thread callback routine
    void setClientRoutine(void* func(void *));

		// Socket client interface
		bool connect (const char *ip, const char *port);
		bool disconnect ();

		// State interface
		QSocketType getType ();

	private:
		struct Queue {
			// Data field
			QSocketWord size;
			char data[QUE_MAX];
      
			// State field
			char *head;
      int *ID;
		};

		Queue queSend;
		Queue queRecv;

    std::map<int, Queue*> sendQues;
    std::map<int, Queue*> recvQues;

		int idHost;
		int idClient;
		QSocketType tySocket;
    void* (*clientRoutine)(void *);

		void initializeFields ();
		void initializeSocketOpt (int *clientID = NULL);
		inline void initializeQueue (Queue& que);

		inline bool pushToSendQue (const void *data, size_t size, int *clientID = NULL);
		inline bool takeFromRecvQue (void *data, size_t size, int *clientID = NULL);
		inline void send (const void *data, size_t size, int *clientID = NULL);
		inline void receive (void *buf, size_t size, int *clientID = NULL);
	};
}

#endif

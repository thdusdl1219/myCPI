/***
 * comm_manager.h: communication manager using queue
 *
 * High-level Communication Layer using queue
 * written by: gyeongmin
 *
 * **/

#ifndef CORELAB_COMM_MANAGER_H
#define CORELAB_COMM_MANAGER_H

#include <inttypes.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <cstddef>
#include <map>
#include <pthread.h>
#include <functional>
#include <vector>
#include <memory>
#include <string.h>
#include <queue>

#define Q_MAX 262144 // max queue elements (256kB)

namespace corelab {
	typedef uint32_t QWord;
  typedef uint32_t TAG; 
  typedef void (*CallbackType)(void*);

  typedef struct Job {
    TAG tag;
    void* data;
  }Job;

  class CommManager {
    public:
      CommManager ();

      // CommManager Initialization 
      void start();
      int open(int port, int num); // wait for connection
    int tryConnect(const char* ip, int port); // create connection
    void closeConnection(int* cid); // close socket
    void setNewConnectionCallback(void callback(void*));
    //template<typename TFunction, typename... TArgs>
      //void setCallback(TAG tag, int cid, TFunction&& a_func, TArgs&&... a_args); // set callback
    void setCallback(TAG tag, void callback(void*)); // set callback
    CallbackType getCallback(TAG tag);


    // send interface using queue
    bool pushWord(TAG tag, QWord word, int* cid = NULL);
    bool pushRange(TAG tag, const void* data, size_t size, int* cid = NULL);
    void sendQue(TAG tag, int *cid = NULL);

    // direct send interface
    //void sendWord(QWord word, int* cid = NULL);
    //void sendRange(const void* data, size_t size, int* cid = NULL);

    // receive interface using queue
    QWord takeWord(int* cid = NULL);
    bool takeRange(void* buf, size_t size, int* cid = NULL);
    void receiveQue(int* cid = NULL);

    // direct receive interface
    //QWord receiveWord(int cid);
    //void receiveRange(void* buf, size_t size, int* cid = NULL);

    std::map<TAG,CallbackType>* getCallbackList(); 
    int getSocketNumber();
    int getSocketByIndex(uint32_t i);

    Job* getJob();
    void insertJob(Job* job);
    int getJobQueSize();

  private:

    

    struct Queue {
      // Data field
      QWord size;
      char data[Q_MAX];

      // State field
      char *head;
      int *ID;
    };

    void (*connectionCallback)(void*);

    std::queue<Job*> jobQue;

    std::map<int, std::map<TAG,Queue*>* > sendQues; // client_id : send_queue
    std::map<int, std::map<TAG,Queue*>* > recvQues; // client_id : recv_queue

    std::map<int,int> socketMap; // client_id : socket_desc

    std::map<TAG,CallbackType>* callbackList; // callback function list with TAG (key) 

    pthread_mutex_t callbackLock;
    pthread_spinlock_t jobQueLock;
    pthread_t handlingThread;
    pthread_t receivingThread;

    void initializeSocketOpt(int* clientID);
    void initializeQueue(Queue& que);


  };
}

#endif

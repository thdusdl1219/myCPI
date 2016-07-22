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

#include "log.h"
//#include "tag.h"

#define Q_MAX 262144 // max queue elements (256kB)

namespace corelab {
	typedef uint32_t QWord;
  typedef uint32_t TAG; 
  typedef void (*CallbackType)(void*, uint32_t, uint32_t);

  typedef struct Job {
    TAG tag;
    void* data;
    uint32_t size;
    uint32_t sourceID;
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
    void setCallback(TAG tag, void callback(void*,uint32_t,uint32_t)); // set callback
    void setLocalID(uint32_t id);
    uint32_t getLocalID();
    CallbackType getCallback(TAG tag);


    // send interface using queue
    bool pushWord(TAG tag, QWord word, uint32_t destID);
    bool pushRange(TAG tag, const void* data, size_t size, uint32_t destID);
    void sendQue(TAG tag, uint32_t destID);

    // direct send interface
    //void sendWord(QWord word, int* cid = NULL);
    //void sendRange(const void* data, size_t size, int* cid = NULL);

    // receive interface using queue
    QWord takeWord(uint32_t destID);
    bool takeRange(void* buf, size_t size, uint32_t destID);
    void receiveQue(uint32_t destID);

    // direct receive interface
    //QWord receiveWord(int cid);
    //void receiveRange(void* buf, size_t size, int* cid = NULL);

    std::map<TAG,CallbackType>* getCallbackList(); 
    int getSocketNumber();
    int getSocketByIndex(uint32_t i);

    Job* getJob();
    void insertJob(Job* job);
    void insertDataToRecvQue(void* data, uint32_t size, uint32_t cid);
    int getJobQueSize();

    pthread_mutex_t callbackLock;
    pthread_mutex_t jobQueLock;
    pthread_mutex_t recvFlagLock;
    pthread_t handlingThread;
    pthread_t receivingThread;


    private:

    struct Queue {
      // Data field
      QWord size;
      char data[Q_MAX];

      // State field
      char *head;
      int *ID;
    };

    uint32_t localID;
    uint32_t clntID;

    void (*connectionCallback)(void*);

    std::queue<Job*> jobQue;

    std::map<uint32_t, std::map<TAG,Queue*>* > sendQues; // client_id : send_queue
    std::map<uint32_t, Queue*> recvQues; // client_id : recv_queue

    std::map<uint32_t,int> socketMap; // client_id : socket_desc
    std::map<uint32_t,bool> recvFlags;

    std::map<TAG,CallbackType>* callbackList; // callback function list with TAG (key) 

    void initializeSocketOpt(int clientID);
    void initializeQueue(Queue& que);


  };
}

#endif

#ifndef THREAD_MANAGER_H
#define THREAD_MANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <map>
#include <string.h>
#include <functional>
#include <vector>
#include <memory>
#include <thread>
#include <functional>
#include <vector>
#include <memory>
#include <pthread.h>
#include "log.h"

#define MAX_THREAD 100

using namespace std;

class ThreadManager{
	public:
		ThreadManager();
		template<typename TFunction, typename... TArgs>
			int createThread(TFunction&& a_func, TArgs&&... a_args); // create thread with function
		void joinThread(int functionID);
	private:
		int threadID;
		pthread_t threads[MAX_THREAD];
		//std::map<void* functionPtr,int functionID> functionTable;

};


#endif

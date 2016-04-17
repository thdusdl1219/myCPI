#include "threadManager.h"

void* thread_func_wrapper(void* thread_func){
	function<void(void)> func = *(function<void(void)>*)(thread_func);
	func();
	return nullptr;
}

ThreadManager::ThreadManager(){
	threadID = 0;
}

template<typename TFunction, typename... TArgs>
int ThreadManager::createThread(TFunction&& a_func, TArgs&&... a_args){
  // std::function<void*(void*)> func = std::bind(std::forward<TFunction>(a_func), std::forward<TArgs>(a_args)...);

	function<void(void)>* func = new function<void(void)>();
	*func = std::bind(std::forward<TFunction>(a_func), std::forward<TArgs>(a_args)...);
	
	pthread_create(&threads[threadID],NULL,thread_func_wrapper,(void*)func);
	return threadID++;
}

void ThreadManager::joinThread(int threadID){
	pthread_join(threads[threadID],NULL);	
}

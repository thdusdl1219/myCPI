#ifndef ESPERANTO_EVENT_PROCESSOR_H
#define ESPERANTO_EVENT_PROCESSOR_H

#include "event.h"

namespace esperantoEvent {
	class EventProcessor {
		public:
			EventProcessor() {
				lock = PTHREAD_MUTEX_INITIALIZER;
			}
			template <typename TFunction, typename... TArgs>
				void add(TFunction&& a_func, TArgs&&... a_args) {
					Slot slot;
					slot.add(a_func, a_args...);
					pthread_mutex_lock(&lock);
					eventQueue.push_back(slot);
					pthread_mutex_unlock(&lock);
				}

			void processAll();
		
		private:
			vector<Slot> eventQueue;
			pthread_mutex_t lock; 
	};
}

#endif

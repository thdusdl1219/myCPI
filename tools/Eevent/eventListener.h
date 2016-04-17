#ifndef ESPERANTO_EVENT_LISTENER_H
#define ESPERANTO_EVENT_LISTENER_H

#include <vector>

#include "event.h"
#include "timerInterrupt.h"

using namespace std;

namespace esperantoEvent {
	class EventListener {	
		public:
			EventListener();
			template <typename TFunction, typename... TArgs>
				void add(TFunction&& a_func, TArgs&&... a_args) {
					Signal signal;
					signal.add(a_func, a_args...);
					listenerList.push_back(signal);
				}
			void install();

			vector<Signal> listenerList;
			TimerInterrupt timer;
	};

}

#endif

#ifndef ESPERANTO_EVENT_H
#define ESPERANTO_EVENT_H

#include <stdio.h>
#include <functional>
#include <vector>
#include <memory>

using namespace std;

namespace esperantoEvent {
	typedef function<void(void)> signal_func_t;
	typedef function<void(void)> slot_func_t;

	template<typename T>
	class EventFunc {
		private:
			T func;
			bool isEventLoaded;

		public:
			EventFunc() { this->isEventLoaded = false; }
			bool isEventInstalled() { return isEventInstalled; }
			template <typename TFunction, typename... TArgs>
				void add(TFunction&& a_func, TArgs&&... a_args) {
					T func = 
						std::bind(std::forward<TFunction>(a_func), 
								std::forward<TArgs>(a_args)...);
					this->func = func;
					isEventLoaded = true;
				}
			T get() { return this->func; }
			
			void operator()(void) {
				(this->get())();
			}
	};

	typedef EventFunc<signal_func_t> Signal;
	typedef EventFunc<slot_func_t> Slot;
}

#endif


#ifndef ESPERANTO_EVENT_MANAGER_H
#define ESPERANTO_EVENT_MANAGER_H

#include <pthread.h>
#include "eventListener.h"
#include "eventProcessor.h"

// Event has two functions signal and slot. 
//
// Signal has bool return type and arbitrary arguments type. 
// Signal function triggers the event, which is called by interrupt_timer
// periodically by master processor. If event condition is satisfied, signal
// function calls addSlot() in the function body, to emit the event processing
// function. Unless, not to call addSlot().
//
// Slot has void retury type and arbitrary arguments type.
// Slot function is the event handling function when event driven from 'signal'
// is triggered. At first, slot function is enrolled in event queue and executed
// in event processing time, with callback method.
//
// Following is example code.
//
// void printMyName(char* a) // @slot function declaration
//
// void listenTemperatureChanged(float temp) { //@signal function 
//    if (math.abs(oldTemp - temp) > 1.0) {
//      EventManager::addSlot(&printMyName, "Hyunjoon"); // call with function pointer and parameters
//    } else {
//      // do nothing. That means not to emit the event.
//    }
//    oldTemp = temp;
// }
//
// void printMyName(char* a) {
//    fprintf(stderr, "I'm %s\n", a);
// } // @slot function description, ALL THE MAIN LOGIC WILL BE INSTALLED HERE.
// 
// float oldTemp // global variable;
//
// int main () {
//   float temp = ARDUINO.getTemp() // some sensing function
//
//   // calling convention is same with addSlot. 
//   // After you enroll the signal into manager, it will automatically be monitored
//   // with interrupt_timer
//   EventManager::addSignal(&listenTemperatureChanged, temp);
//   while(1) { EventManager::processAll() }
//   sleep(INF); // main do nothing. All the process will be handled in ;slot' function
// } 
//

using namespace esperantoEvent;

// extern EventListener eventListener; UNUSED
extern EventProcessor eventProcessor;

namespace esperantoEvent {
	class EventManager {
		public:
			template <typename TFunction, typename... TArgs>
				static void addSlot(TFunction&& a_func, TArgs&&... a_args) {
					eventProcessor.add(a_func, a_args...);
				}
			static void processAll() {
				eventProcessor.processAll();
			}
		private:
			EventManager(); // for skeleton
/*
			template <typename TFunction, typename... TArgs>
				static void addSignal(TFunction&& a_func, TArgs&&... a_args) {
					eventListener.add(a_func, a_args...);
			} // UNUSED 		
*/
	};
}

#endif

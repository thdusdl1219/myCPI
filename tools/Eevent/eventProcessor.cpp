#include "eventProcessor.h"

using namespace esperantoEvent;

EventProcessor eventProcessor;

void EventProcessor::processAll() {
	typedef std::vector<Slot>::iterator S;
	pthread_mutex_lock(&lock);
	for(S it = eventQueue.begin(), 
		ie = eventQueue.end(); it != ie; ++it) {
		Slot slot = *it;
		(slot.get())();
	} 
	eventQueue.clear();
	pthread_mutex_unlock(&lock);
}

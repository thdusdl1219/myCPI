#ifndef ESPERANTO_DATA_Q_MANAGER_H
#define ESPERANTO_DATA_Q_MANAGER_H

#include <queue>
#include "dataQElem.h"

using namespace std;

class DataQElem;

class DataQ {
public:
	DataQ();
	int getSize();
	void produce(DataQElem data);
	DataQElem* consume();
	
private:
	queue<DataQElem> dataQ;
	pthread_mutex_t lock;	
};

#endif

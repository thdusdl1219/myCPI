#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include <fcntl.h>
//#include "log.h"
#include "dataQ/dataQManager.h"

// typedef 
typedef void (*ApiCallback) (int, int);

// global variables ( protocols, threads, boolean)
DataQManager* dqm;


// runtime function that starts system
extern "C"
void start(ApiCallback fcn, int id);

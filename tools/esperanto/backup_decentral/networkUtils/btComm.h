#ifndef BT_COMM_H
#define BT_COMM_H

#include "baseComm.h"

class BTCommHandler : public CommHandler {
public:
	void createServerSocket(int port) override;
	void createClientSocket(char* baddr, int channel) override;
private:
	int sock_d;
};

#endif

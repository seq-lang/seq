#ifndef __CONNECT_H_
#define __CONNECT_H_

#include "socket.h"

bool Connect(CSocket& socket, const char* address, int port);
bool ConnectLocal(CSocket& socket, int port);
bool OpenChannel(CSocket& socket, int port);
void CloseChannel(CSocket& socket);

#endif

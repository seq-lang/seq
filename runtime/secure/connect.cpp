#include "connect.h"

const int RETRY_CONNECT = 100;
const int TIMEOUT_MS = 10000;

bool Connect(CSocket& socket, const char* address, int port) {
  bool success = false;
  for (int i = 0; i < RETRY_CONNECT; i++) {
    if (!socket.Socket()) {
      break;
    }
    if (socket.Connect(address, (uint16_t) port, TIMEOUT_MS)) {
      success = true;
      break;
    }
    cout << "Connection failed, retrying.." << endl;
    usleep(200 << 10);
  }

  if (success) {
  }

  return success;
}

bool ConnectLocal(CSocket& socket, int port) {
  return Connect(socket, "127.0.0.1", port);
}

bool Listen(CSocket& socket, int port) {

  if (!socket.Socket()) {
    return false;
  }

  if (!socket.Bind((uint16_t) port)) {
    return false;
  }

  if (!socket.Listen()) {
    return false;
  }

  CSocket sock;
  if (!socket.Accept(sock)) {
    return false;
  }

  socket.AttachFrom(sock);
  sock.Detach();

  return true;
}

bool OpenChannel(CSocket& socket, int port) {
  cout << "Starting to listen on port " << port << endl;
  return Listen(socket, port);
}

void CloseChannel(CSocket& socket) {
  socket.Close();
}

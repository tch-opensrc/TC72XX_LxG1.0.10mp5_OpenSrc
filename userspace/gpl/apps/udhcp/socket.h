/* socket.h */
#ifndef _SOCKET_H
#define _SOCKET_H

int listen_socket(unsigned int ip, int port, char *inf);
int raw_socket(int ifindex);

#endif

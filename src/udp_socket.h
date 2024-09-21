#ifndef UDP_SOCKET_H_
#define UDP_SOCKET_H_

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define MAXLINE 1024

int udp_socket_initialise(struct sockaddr_in **addr, int port);

int udp_socket_bind(int sock_fd, struct sockaddr_in *addr);

int udp_socket_send(int sock_fd, struct sockaddr_in *addr, uint8_t *buffer, size_t buf_len);

int udp_socket_receive_from(int sock_fd, struct sockaddr_in **addr, uint8_t *buffer, size_t buf_len, bool blocking);

#endif // UDP_SOCKET_H_

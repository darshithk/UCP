#ifndef TCP_SOCKET_H
#define TCP_SOCKET_H

#include "defines.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

typedef struct __tcp_sgmnt_t {
    uint8_t data[1024];
    ssize_t data_len;
} tcp_sgmnt_t;

typedef struct ip_dest_t {
    struct sockaddr_in addr;
    int sd;
    struct ip_dest_t *next;
} tcp_endpoint_t;

#define IP_ADDR_FORMAT "%s:%d"

#define IP_ADDR(addr) inet_ntoa(addr.sin_addr), ntohs(addr.sin_port)

typedef struct __tcp_client_t tcp_client_t;

typedef void (*tcp_receive_handler_t)(struct __tcp_client_t*, tcp_sgmnt_t*);

typedef void (*tcp_disconnect_handler_t)(struct __tcp_client_t*);

struct __tcp_client_t {
    int sd;
    uint16_t port;
    tcp_endpoint_t* server;
    tcp_receive_handler_t on_receive;
    tcp_disconnect_handler_t on_disconnect;
    void* user_data;
};

tcp_client_t* tcp_client_connect(tcp_endpoint_t* dest, tcp_receive_handler_t on_receive, tcp_disconnect_handler_t on_disconnect);

void tcp_client_disconnect(tcp_client_t* client);

uint8_t tcp_client_send(tcp_client_t* client, tcp_sgmnt_t* sgmnt);

void tcp_client_receive(tcp_client_t* client);

typedef struct __tcp_server_t tcp_server_t;

typedef void (*tcp_message_tx_cb_t) (tcp_server_t* tcp, tcp_endpoint_t* dest, tcp_sgmnt_t* res_sgmnt);
typedef void (*tcp_message_rx_cb_t) (tcp_server_t* tcp, tcp_endpoint_t* dest, tcp_sgmnt_t* res_sgmnt);

struct __tcp_server_t {
    int sd;
    uint16_t port;
    int max_sd;
    fd_set server_fd_set;
    tcp_endpoint_t *endpoints;    
    tcp_message_rx_cb_t on_rx;
    tcp_message_tx_cb_t on_tx;
};

tcp_server_t* tcp_server_start(uint16_t port);
void tcp_server_stop(tcp_server_t* server);
void tcp_server_tick(tcp_server_t* server);
void tcp_server_send(tcp_server_t* server, tcp_endpoint_t* dest, tcp_sgmnt_t* datagram);

void tcp_server_receive(tcp_server_t* server, int child_sd);
int  tcp_server_accept(tcp_server_t* server);

#endif // TCP_SOCKET_H

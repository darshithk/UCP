#include "tcp_socket.h"
#include "stdlib.h"
#include <errno.h>
#include <stdio.h>
#include <sys/select.h>
#include <string.h>
#include <unistd.h>

tcp_server_t* tcp_server_start(uint16_t port) {
    tcp_server_t* server = (tcp_server_t*) calloc(1, sizeof(tcp_server_t));

    if (server == NULL) {
        fprintf(stderr, "Failed to allocate memory for tcp_server_t\n");
    } else {
        // Create a socket
        server->sd = socket(AF_INET, SOCK_STREAM, 0);
        if (server->sd < 0) {
            fprintf(stderr, "Failed to create socket. Error: %s.\n", strerror(errno));
            free(server);
        } else {
            struct sockaddr_in server_addr = {0};
            SERVER_ADDR_PORT(server_addr, "0.0.0.0", port);

            // Use the SO_REUSEADDR option to allow the server to restart immediately after it is killed
            setsockopt(server->sd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

            // Bind the socket to the port
            if (bind(server->sd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
                fprintf(stderr, "Failed to bind socket. Error: %s.\n", strerror(errno));
                free(server);
            } else {
                if (listen(server->sd, 5) < 0) {
                    fprintf(stderr, "Failed to listen on socket. Error: %s.\n", strerror(errno));
                } else {
                    server->max_sd = server->sd;
                    FD_SET(server->sd, &server->server_fd_set);
                    fprintf(stdout, "TCP Server started on port %d\n", port);
                }
            }
        }
    }
    return server;
}

// Close a TCP Child Socket
static void close_child_socket(tcp_server_t* server, int child_sd) {
    if (server != NULL) {
        close(child_sd);
        FD_CLR(child_sd, &server->server_fd_set);
        if (child_sd == server->max_sd) {
            server->max_sd--;
        }
    }
}

// Close the TCP Server
void tcp_server_stop(tcp_server_t* server) {
    if (server != NULL) {
        while(server->max_sd > server->sd) {
            close_child_socket(server, server->max_sd);
        }
        close(server->sd);
        free(server);
    }
}

// Create a new TCP Child Socket
static void create_child_socket(tcp_server_t* server, int child_sd) {
    if (server != NULL) {
        tcp_endpoint_t* endpoint = calloc(1, sizeof(tcp_endpoint_t));
        endpoint->sd = child_sd;
        endpoint->next = server->endpoints;
        server->endpoints = endpoint;
        endpoint->addr = (struct sockaddr_in) {0};
        socklen_t addr_len = sizeof(endpoint->addr);
        // Get the address of the client
        if (getpeername(child_sd, (struct sockaddr*) &endpoint->addr, &addr_len) < 0) {
            fprintf(stderr, "Failed to get peer name. Error: %s.\n", strerror(errno));
        } else {
            // fprintf(stdout, "Peer name: " IP_ADDR_FORMAT, IP_ADDR(endpoint));
        }

        FD_SET(child_sd, &server->server_fd_set);
        if (child_sd > server->max_sd) {
            server->max_sd = child_sd;
        }
    }
}

// Accept a new connection. Open a Child Socket
int tcp_server_accept(tcp_server_t* server) {
    if (server != NULL) {
        int new_sd = -1;
        if ((new_sd = accept(server->sd, NULL, NULL)) < 0) {
            fprintf(stderr, "Failed to accept connection. Error: %s.\n", strerror(errno));
            return 0;
        } else {
            // Create a new child socket
            fprintf(stdout, "Accepted connection on socket %d\n", new_sd);
            create_child_socket(server, new_sd);
            return 1;
        }
    }
    return 0;
}

// Get TCP Endpoint from Socket Descriptor
static tcp_endpoint_t* get_endpoint(tcp_server_t* server, int sd) {
    tcp_endpoint_t* endpoint = NULL;
    if (server != NULL) {
        endpoint = server->endpoints;
        while(endpoint != NULL) {
            if (endpoint->sd == sd) {
                break;
            }
            endpoint = endpoint->next;
        }
    }
    return endpoint;
}

// Receive data from a Child Socket
void tcp_server_receive(tcp_server_t* server, int child_sd) {
    uint8_t buffer[1024] = {0};
    size_t bytes_read = read(child_sd, buffer, sizeof(buffer));
    if (bytes_read < 0) {
        fprintf(stderr, "Failed to read from socket. Error: %s.\n", strerror(errno));
    } else if (bytes_read == 0) {
        fprintf(stdout, "Client disconnected.\n");
        close_child_socket(server, child_sd);
    } else {
        tcp_endpoint_t* endpoint = get_endpoint(server, child_sd);
        // fprintf(stdout, "Received %ld bytes from "IP_ADDR_FORMAT" : %s", bytes_read, IP_ADDR(endpoint), buffer);
        tcp_sgmnt_t sgmnt = {0};
        memcpy(sgmnt.data, buffer, bytes_read);
        sgmnt.data_len = bytes_read;
        server->on_rx(server, endpoint, &sgmnt);
    }
}

// Server Loop. Accept new connections and receive data
void tcp_server_tick(tcp_server_t* server) {
    if (server != NULL) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        memcpy(&read_fds, &server->server_fd_set, sizeof(server->server_fd_set));

        struct timeval timeout = { .tv_sec = 1, .tv_usec = 100*1000 };

        int err = select(server->max_sd + 1, &read_fds, NULL, NULL, &timeout);
        if (err > 0) {
            for (int sd = server->sd; sd <= server->max_sd; sd++) {
                if (FD_ISSET(sd, &read_fds)) {
                    if (sd == server->sd) {
                        // New connection
                        tcp_server_accept(server);
                    } else {
                        // Receive data
                        tcp_server_receive(server, sd);
                    }
                }
            }
        }
    }
}

// Send data to a Child Socket
void tcp_server_send(tcp_server_t* server, tcp_endpoint_t* dst, tcp_sgmnt_t* segment) {
    if (server != NULL && dst != NULL && segment != NULL) {
        // fprintf(stdout, "Sending TCP Segment to "IP_ADDR_FORMAT" %.*s", IP_ADDR(dst), (int) segment->data_len, segment->data);
        // Send data to the client
        size_t bytes_sent = sendto(dst->sd, segment->data, segment->data_len, 0, (struct sockaddr*) &dst->addr, sizeof(struct sockaddr));
        if (bytes_sent < 0) {
            fprintf(stderr, "Failed to send TCP Segment. Error: %s.\n", strerror(errno));
        } else {
            fprintf(stdout, "Sent %ld bytes\n", bytes_sent);
        }
    }
}

tcp_client_t* tcp_client_connect(tcp_endpoint_t* dest, tcp_receive_handler_t on_receive, tcp_disconnect_handler_t on_disconnect) {
    tcp_client_t* client = malloc(sizeof(tcp_client_t));
    if (!client) {
        fprintf(stderr, "tcp_client_connect: Error allocating memory for client\n");
        return NULL;
    }

    client->server = dest;

    // Create a new socket
    client->sd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->sd < 0) {
        fprintf(stderr, "tcp_client_connect: Error creating socket\n");
        free(client);
        return NULL;
    }

    // Connect to TCP server
    if (connect(client->sd, (struct sockaddr*)&dest->addr, sizeof(struct sockaddr)) < 0) {
        fprintf(stderr, "tcp_client_connect: Error connecting to server\n");
        free(client);
        return NULL;
    }

    struct sockaddr_in addr = {0};
    socklen_t addr_len = sizeof(addr);

    // Get the dynamically allocated local address of the socket
    if (getsockname(client->sd, (struct sockaddr*)&addr, &addr_len) < 0) {
        fprintf(stderr, "tcp_client_connect: Failed to get socket name\n");
    } else {
        // Save the port number
        client->port = ntohs(addr.sin_port);
        fprintf(stdout, "Locally bound to port %d\n", client->port);
    }

    client->on_receive = on_receive;
    client->on_disconnect = on_disconnect;

    return client;
}

void tcp_client_disconnect(tcp_client_t* client) {
    if (client) {
        // Close the socket
        close(client->sd);
        // Free the client
        free(client);
    }
}

uint8_t tcp_client_send(tcp_client_t* client, tcp_sgmnt_t* sgmnt) {
    if (client) {
        // Send data to server
        if (send(client->sd, sgmnt->data, sgmnt->data_len, 0) == sgmnt->data_len) {
            // fprintf(stdout, "Sending TCP Segment (%ld bytes) to server", sgmnt->data_len);
            return 0;
        }
    }
    return 1;
}

void tcp_client_receive(tcp_client_t* client) {
    if (client) {
        tcp_sgmnt_t sgmnt = {0};
        // Read data from socket into buffer
        size_t bytes_read = recv(client->sd, sgmnt.data, sizeof(sgmnt.data), 0);
        if (bytes_read > 0) {
            sgmnt.data_len = bytes_read;
            fprintf(stdout, "Received %ld bytes from server\n", bytes_read);
            if (client->on_receive) {
                client->on_receive(client, &sgmnt);
            }
        } else {
            if (client->on_disconnect) {
                client->on_disconnect(client);
            }
        }
    }
}

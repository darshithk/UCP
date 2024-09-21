#include "udp_socket.h"

#include <sys/types.h>
#include <sys/time.h>

int udp_socket_initialise(struct sockaddr_in **addr, int port) {
    int sock_fd = -1;

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        printf("ERR: Failed to create a socket\n");
        return -1;
    }

    memset(*addr, 0, sizeof(struct sockaddr_in));

    (*addr)->sin_family = AF_INET;
    (*addr)->sin_port = htons(port);
    (*addr)->sin_addr.s_addr = INADDR_ANY;

    return sock_fd;
}

int udp_socket_bind(int sock_fd, struct sockaddr_in *addr) {
    if (bind(sock_fd, (const struct sockaddr *)addr, sizeof(struct sockaddr_in)) < 0 ) {
        fprintf(stderr, "Binding Failed\n");
        return -1;
    }
    return 0;
}

int udp_socket_send(int sock_fd, struct sockaddr_in *addr, uint8_t *buffer, size_t buf_len) {
    return sendto(sock_fd, (const void *)buffer, buf_len, 0, (const struct sockaddr *)addr, sizeof(struct sockaddr_in));
}

int udp_socket_receive_from(int sock_fd, struct sockaddr_in **addr, uint8_t *buffer, size_t buf_len, bool blocking) {
    socklen_t len = sizeof(struct sockaddr_in);
    return recvfrom(sock_fd, (void *)buffer, buf_len, blocking ? MSG_WAITALL : MSG_DONTWAIT, (struct sockaddr *)(*addr), &len);
}

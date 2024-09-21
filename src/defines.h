#ifndef DEFINES_H
#define DEFINES_H

#define CLIENT_BASE_PORT     6341
#define SERVER_BASE_PORT     6342

#define UDP_PACKET_DATA_SIZE        ((9 * 1024) - (50))
#define UDP_PACKET_OVERHEAD_MARGIN  (50)
#define UDP_PACKET_SIZE             (UDP_PACKET_DATA_SIZE + UDP_PACKET_OVERHEAD_MARGIN)

#define NUM_THREADS          10

#define SERVER_ADDR_PORT(addr, ip, port) do { \
    addr.sin_family = AF_INET; \
    addr.sin_addr.s_addr = inet_addr(ip); \
    addr.sin_port = htons(port); \
} while (0)

#endif // DEFINES_H

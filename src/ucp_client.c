#include <unistd.h>
#include <pthread.h>
#if defined(__linux__)
#include <sys/sysinfo.h>
#endif // __linux__
#include <stdio.h>
#include <stdlib.h>

#include "defines.h"
#include "file_io.h"
#include "ucp_packet.h"
#include "udp_socket.h"
#include "tcp_socket.h"
#include "linked_list.h"
#include <sys/time.h>

typedef struct _ucp_client_thread_context {
    pthread_t thread;
    struct timeval start_time;
    struct timeval end_time;
    file_io_partition_handle_t* handles;
    char* dst_ip;
    char* dst_filename;
} ucp_client_thread_context_t;

static LinkedList in_flight_packet_list;
static LinkedList pending_packet_list;


// Print the total time taken with the appropriate units
static void print_time(double time_microseconds) {    
    if (time_microseconds < 1000) {
        printf("Total time taken\t: %f us\n", (double)time_microseconds);
    } else if (time_microseconds < 1000 * 1000) {
        printf("Total time taken\t: %f ms\n", (double)time_microseconds / 1000);
    } else if (time_microseconds < 1000 * 1000 * 1000) {
        printf("Total time taken\t: %f s\n", (double)time_microseconds / 1000000);
    }
}

// Print the transfer rate with the appropriate units
void print_transfer_rate(double transfer_rate) {
    if (transfer_rate >= 1e9) {
        transfer_rate /= 1e9;
        printf("Transfer rate\t\t: %.2f Gbps\n", transfer_rate);
    } else if (transfer_rate >= 1e6) {
        transfer_rate /= 1e6;
        printf("Transfer rate\t\t: %.2f Mbps\n", transfer_rate);
    } else if (transfer_rate >= 1e3) {
        transfer_rate /= 1e3;
        printf("Transfer rate\t\t: %.2f Kbps\n", transfer_rate);
    } else {
        printf("Transfer rate\t\t: %.2f bps\n", transfer_rate);
    }
}

// Calculate transfer rate in bits per second (bps)
static double get_transfer_rate(double bits, double time_microseconds) {
    return (double)(bits / time_microseconds) * 1e6;
}

// Get the total time taken for the file transfer in microseconds
static double get_total_time_taken(struct timeval start_time, struct timeval end_time) {
    // Get time in microseconds
    long int start_time_micro = start_time.tv_sec * 1000000 + start_time.tv_usec;
    long int end_time_micro = end_time.tv_sec * 1000000 + end_time.tv_usec;
    long int total_time_micro = end_time_micro - start_time_micro;

    return total_time_micro;
}

// Report statistics for the file transfer
static void report_statistics(ucp_client_thread_context_t *thread_ctx, uint8_t num_threads) {
    //Get smallest start time compared to all threads
    struct timeval start_time = thread_ctx[0].start_time;
    for (uint8_t i = 1; i < num_threads; i++) {
        if (thread_ctx[i].start_time.tv_sec < start_time.tv_sec) {
            start_time = thread_ctx[i].start_time;
        } else if (thread_ctx[i].start_time.tv_sec == start_time.tv_sec) {
            if (thread_ctx[i].start_time.tv_usec < start_time.tv_usec) {
                start_time = thread_ctx[i].start_time;
            }
        }
    }

    //Get largest end time compared to all threads
    struct timeval end_time = thread_ctx[0].end_time;
    for (uint8_t i = 1; i < num_threads; i++) {
        if (thread_ctx[i].end_time.tv_sec > end_time.tv_sec) {
            end_time = thread_ctx[i].end_time;
        } else if (thread_ctx[i].end_time.tv_sec == end_time.tv_sec) {
            if (thread_ctx[i].end_time.tv_usec > end_time.tv_usec) {
                end_time = thread_ctx[i].end_time;
            }
        }
    }

    // Get the total number of bytes transferred
    uint64_t total_bytes = 0;
    for (uint8_t i = 0; i < num_threads; i++) {
        total_bytes += thread_ctx[i].handles->part_size;
    }
    printf("--------------------------------------------------------\n");
    printf("Total bytes transferred : %llu\n", total_bytes);

    // Get the total time taken for the file transfer in microseconds
    double total_time = get_total_time_taken(start_time, end_time);
    print_time(total_time);

    // Get the transfer rate for the file transfer in bps
    double transfer_rate = get_transfer_rate(total_bytes * 8, total_time);
    print_transfer_rate(transfer_rate);
    printf("--------------------------------------------------------\n");
}

// Search for a packet in the in-flight window with the given sequence number
static LinkedListElem* find_packet(LinkedList* packet_list, uint32_t seq_no) {
    LinkedListElem* elem = packet_list->anchor.next;
    while (elem != &(packet_list->anchor)) {
        ucp_packet_t* packet = (ucp_packet_t*)elem->obj;
        if (packet->data_packet.seq_no == seq_no) {
            return elem;
        }
        elem = elem->next;
    }
    return NULL;
}

int create_socket(int port, struct sockaddr_in *server_addr) {
    int sock_fd = -1;
    
    printf("%s: %d\n", __func__, port);
    sock_fd = udp_socket_initialise(&server_addr, port);
    if (sock_fd < 0) {
        free(server_addr);
        return -1;
    }

    if (udp_socket_bind(sock_fd, server_addr) < 0) {
        free(server_addr);
        return -1;
    }

    return sock_fd;
}

static int packet_count = 0;

static ucp_packet_t *get_next_packet(LinkedList* pending_packet_list, LinkedList* inflight_packet_list, file_io_partition_handle_t *handle) {
    // If there is a packet in the pending window, return it
    if (!LinkedListEmpty(pending_packet_list)) {
        LinkedListElem* elem = LinkedListFirst(pending_packet_list);
        ucp_packet_t* packet = (ucp_packet_t*)elem->obj;
        LinkedListUnlink(pending_packet_list, elem);
        // printf("Sending pending packet: %p\n", elem->obj);
        return packet;
    }
    // Else, read the next packet from the file and return it
    ucp_packet_t* file_packet = file_io_get_next_packet(handle);
    if (file_packet) {
        packet_count++;
        // printf("Sending file packet: %p\n", file_packet);
        return file_packet;
    }

    if (!LinkedListEmpty(inflight_packet_list)) {
        LinkedListElem* elem = LinkedListFirst(inflight_packet_list);
        ucp_packet_t* packet = (ucp_packet_t*)elem->obj;
        LinkedListUnlink(inflight_packet_list, elem);
        // printf("Sending in-flight packet: %p\n", packet);
        return packet;
    }

    return NULL;
}

static int done = 0;

static void on_ack_received(tcp_server_t* tcp, tcp_endpoint_t* dest, tcp_sgmnt_t* res_sgmnt) {

    (void)(tcp);
    (void)(dest);

    ucp_packet_t rsp_pkt;
    ucp_packet_decode(res_sgmnt->data, res_sgmnt->data_len, &rsp_pkt);
    if (rsp_pkt.type == UCP_PACKET_TYPE_CTRL) {
        if (rsp_pkt.ctrl_packet.flag == UCP_FLAG_ACK) {
            // printf("ACK received for %d\n", rsp_pkt.ctrl_packet.seq_no);
            // If the response is an ACK, drop the packet with the same sequence number from the in-flight window
            LinkedListElem* elem = find_packet(&in_flight_packet_list, rsp_pkt.ctrl_packet.seq_no);
            if (elem) {
                // ucp_packet_free((ucp_packet_t*)elem->obj);
                LinkedListUnlink(&in_flight_packet_list, elem);
            }
        } else if (rsp_pkt.ctrl_packet.flag == UCP_FLAG_NACK) {
            // If the response is a NACK, move the packet to the pending window
            // fprintf(stderr, "NACK received. Moving packet to pending window\n");
            LinkedListElem* elem = find_packet(&in_flight_packet_list, rsp_pkt.ctrl_packet.seq_no);
            if (elem) {
                LinkedListAppend(&pending_packet_list, elem->obj);
                LinkedListUnlink(&in_flight_packet_list, elem);
            }
        } else if (rsp_pkt.ctrl_packet.flag == UCP_FLAG_FIN) {
            printf("FIN received. Closing socket\n");
            // If the response is a FIN, close the socket and exit the thread
            done = 1;
        }
    } else {
        fprintf(stderr,"ACK failed\n");
    }
}


static void* block_thread(void* arg) {
    int sock_fd = -1;

    ucp_client_thread_context_t* curr_thread = (ucp_client_thread_context_t*)arg;
    file_io_partition_handle_t* handle = curr_thread->handles;

    struct sockaddr_in *local_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
    struct sockaddr_in *remote_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));

    // Create a UDP Socket with base port + idx
    sock_fd = create_socket(CLIENT_BASE_PORT + handle->idx, local_addr);
    if (sock_fd < 0) {
        fprintf(stderr, "Failed to create socket\n");
        return NULL;
    }

    // Create TCP Socket Server with base port + idx
    tcp_server_t* tcp_server = tcp_server_start(CLIENT_BASE_PORT + handle->idx);
    tcp_server->on_rx = on_ack_received;

    remote_addr->sin_addr.s_addr = inet_addr(curr_thread->dst_ip);
    remote_addr->sin_port = htons(SERVER_BASE_PORT + handle->idx);
    remote_addr->sin_family = AF_INET;
    

    uint8_t buf[UDP_PACKET_DATA_SIZE + UDP_PACKET_OVERHEAD_MARGIN];
    ucp_packet_t *packet = NULL;

    // TODO: Send the metadata for the thread
    ucp_packet_t *metadata_packet = ucp_packet_init_metadata(curr_thread->dst_filename, strlen(curr_thread->dst_filename), handle->idx, handle->part_size);
    size_t len = ucp_packet_encode(metadata_packet, buf, sizeof(buf));
    udp_socket_send(sock_fd, remote_addr, (void *)buf, len);

    // Accept a TCP connection from the server

    fprintf(stderr, "Waiting for connection\n");
    while (tcp_server_accept(tcp_server) == 0) {
        usleep(1000);
    }

    printf("Connected to receive ACKs\n");
    // Store start time
    gettimeofday(&curr_thread->start_time, NULL);

    // Send the data for the thread
    // Read the next packet from the API
    while ((packet = get_next_packet(&pending_packet_list, &in_flight_packet_list, handle)) != NULL) {
        // Send the packet to the server
        // printf("Sending packet: %p\n", packet);
        len = ucp_packet_encode(packet, buf, sizeof(buf));
        udp_socket_send(sock_fd, remote_addr, (void *)buf, len);
        fprintf(stdout, "Sending seq_no %d\n", packet->data_packet.seq_no);

        // Add the packet to the in-flight window
        LinkedListAppend(&in_flight_packet_list, packet);

        tcp_server_tick(tcp_server);

        if (done) {
            break;
        }
    }

    fprintf(stderr, "Total packets created %d\n", packet_count);

    // Print the number of packets in the in-flight window
    fprintf(stderr, "In-flight window size: %d\n", in_flight_packet_list.num_members);

    // Print the number of packets in the pending window
    fprintf(stderr, "Pending window size: %d\n", pending_packet_list.num_members);

    close(sock_fd);

    gettimeofday(&curr_thread->end_time, NULL);
    return NULL;
}

static void print_usage(void) {
    printf("Usage: ucp_client src remote_ip:dst\n");
}

int main(int argc, char** argv) {

    ucp_client_thread_context_t thread_ctx[NUM_THREADS];
    // Parse the command line arguments
    if (argc != 3) {
        print_usage();
        return -1;
    }

    char* src = argv[1];
    // TODO: Parse the destination 
    char* dst = argv[2];
    
    thread_ctx->dst_ip = strdup(strtok(dst, ":"));
    thread_ctx->dst_filename = strdup(strtok(NULL, ":"));
    if (!thread_ctx->dst_ip || !thread_ctx->dst_filename) {
        print_usage();
        return -1;
    }

    // Create a window for the in-flight packets
    memset(&in_flight_packet_list, 0, sizeof(LinkedList));
    (void)LinkedListInit(&in_flight_packet_list);

    // Create a window for the pending packets
    memset(&pending_packet_list, 0, sizeof(LinkedList));
    (void)LinkedListInit(&pending_packet_list);

    // Split the file into NUM_THREADS blocks
    file_io_partition_handle_t* handles = file_io_partition_file(src, NUM_THREADS);

    // Create a thread for each file block
    for (uint8_t i = 0; i < NUM_THREADS; i++) {
        thread_ctx[i].handles = &handles[i];
        pthread_create(&thread_ctx[i].thread, NULL, block_thread, &thread_ctx[i]);
    }

    // Wait for each thread to close
    for (uint8_t i = 0; i < NUM_THREADS; i++) {
        pthread_join(thread_ctx[i].thread, NULL);
    }

    // Report statistics for the file transfer
    report_statistics(thread_ctx, NUM_THREADS);

    // Close the file handles
    file_io_partition_release(handles, NUM_THREADS);

    return 0;
}


#include <unistd.h>
#include <stdio.h>
#if defined(__linux__)
#include <sys/sysinfo.h>
#endif // __linux__
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "defines.h"
#include "linked_list.h"
#include "udp_socket.h"
#include "tcp_socket.h"
#include "ucp_packet.h"
#include "file_io.h"
#include "sequencer.h"

typedef struct __ucp_server_thread_context {
    pthread_t rcv_thread;
    pthread_t seq_thread;
    tcp_client_t* client;
    uint16_t client_port;
    int udp_fd;
    LinkedList seq_queue;
    file_io_partition_handle_t handle;
} ucp_server_thread_context_t;

static void send_ctrl_packet(uint32_t seq_no, ucp_flag_t flag, void* arg) {
    uint8_t buf[10];
    tcp_sgmnt_t sgmnt;
    tcp_client_t* client = (tcp_client_t*)arg;

    ucp_packet_t *ctrl_packet = ucp_packet_init_ctrl(seq_no, flag);
    sgmnt.data_len = ucp_packet_encode(ctrl_packet, buf, sizeof(buf));
    memcpy(sgmnt.data, buf, sgmnt.data_len);
    
    tcp_client_send(client, &sgmnt);
    ucp_packet_free(ctrl_packet);
}


static void send_ack(uint32_t seq_no, void* arg) {
    send_ctrl_packet(seq_no, UCP_FLAG_ACK, arg);
}

static void send_nack(uint32_t seq_no, void* arg) {
    send_ctrl_packet(seq_no, UCP_FLAG_NACK, arg);
}

static void send_fin(uint32_t seq_no, void* arg) {
    send_ctrl_packet(seq_no, UCP_FLAG_FIN, arg);
}


typedef struct __socket_ctx {
    int sock_fd;
    struct sockaddr_in* client_addr;
} socket_ctx_t;

static void send_ctrl_packet(uint16_t seq_no, ucp_flag_t flag, void* arg) {
    socket_ctx_t* ctx = (socket_ctx_t*)arg;
    ucp_packet_t *ctrl_packet = ucp_packet_init_ctrl(seq_no, flag);
    uint8_t buf[5];
    size_t len = ucp_packet_encode(ctrl_packet, buf, sizeof(buf));
    // Print client address
    // printf("Client address: %s\n", inet_ntoa(ctx->client_addr->sin_addr));
    udp_socket_send(ctx->sock_fd, ctx->client_addr, (void *)buf, len);
    ucp_packet_free(ctrl_packet);
}

static void send_ack(uint16_t seq_no, void* arg) {
    // printf("Received %d\n", seq_no);
    send_ctrl_packet(seq_no, UCP_FLAG_ACK, arg);
}

static void send_nack(uint16_t seq_no, void* arg) {
    send_ctrl_packet(seq_no, UCP_FLAG_NACK, arg);
}

static void send_fin(uint16_t seq_no, void* arg) {
    // fprintf(stderr, "Completed. Sending FIN. \n");
    send_ctrl_packet(seq_no, UCP_FLAG_FIN, arg);
}


int create_udp_socket_and_bind(int port, struct sockaddr_in *server_addr) {
    int sock_fd = -1;
    
    // printf("%s: %d\n", __func__, port);
    sock_fd = udp_socket_initialise(&server_addr, port);
    if (sock_fd < 0) {
        free(server_addr);
        return -1;
    }

    int ret = udp_socket_bind(sock_fd, server_addr);
    if (ret < 0) {
        perror("ERR");
        free(server_addr);
        return -1;
    }
    return sock_fd;
}

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct __sequencing_queue_item_t {
    uint32_t seq_no;
    ucp_flag_t flag;
    bool is_last;
} sequencing_queue_item_t;

static void sequencing_queue_push(LinkedList* queue, uint32_t seq_no, ucp_flag_t flag, bool is_last) {
    pthread_mutex_lock(&mutex);
    sequencing_queue_item_t* item = (sequencing_queue_item_t*)malloc(sizeof(sequencing_queue_item_t));
    item->seq_no = seq_no;
    item->flag = flag;
    item->is_last = is_last;
    LinkedListAppend(queue, (void*)item);
    pthread_mutex_unlock(&mutex);
}

static int sequencing_queue_pop(LinkedList* queue, uint32_t* seq_no, ucp_flag_t* flag, bool* is_last) {
    int ret = 0;
    pthread_mutex_lock(&mutex);
    LinkedListElem* elem = LinkedListFirst(queue);
    if (elem) {
        sequencing_queue_item_t* item = (sequencing_queue_item_t*)elem->obj;
        *seq_no = item->seq_no;
        *flag = item->flag;
        *is_last = item->is_last;
        LinkedListUnlink(queue, elem);
        free(item);
        ret = 1;
    }
    pthread_mutex_unlock(&mutex);
    return ret;
}

static void* receiving_thread(void* arg) {    
    ucp_server_thread_context_t* curr_thread = (ucp_server_thread_context_t*)arg;
    uint8_t recv_buffer[UDP_PACKET_SIZE] = {0};
    ucp_packet_t rcv_pkt = {0};
    struct sockaddr_in* client_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));

    while (udp_socket_receive_from(curr_thread->udp_fd, &client_addr, recv_buffer, sizeof(recv_buffer), true) > 0) {
        ucp_packet_decode(recv_buffer, sizeof(recv_buffer), &rcv_pkt);
        if (rcv_pkt.type == UCP_PACKET_TYPE_DATA) {
            fprintf(stdout, "seq_no: %u\n", rcv_pkt.data_packet.seq_no);
            if (!file_io_save_packet(&(curr_thread->handle), &rcv_pkt)) {
                sequencing_queue_push(&(curr_thread->seq_queue), rcv_pkt.data_packet.seq_no, UCP_FLAG_NACK, false);
                fprintf(stderr, "Error saving packet\n");
                return NULL;
            } else {
                sequencing_queue_push(&(curr_thread->seq_queue), rcv_pkt.data_packet.seq_no, UCP_FLAG_ACK, rcv_pkt.data_packet.flag == UCP_FLAG_DATA_END);
            }
        } else {
            fprintf(stderr, "Unknown packet type\n");
        }
    }
    return NULL;
}

static void* sequencing_thread(void* arg) {

    ucp_server_thread_context_t* curr_thread = (ucp_server_thread_context_t*)arg;

    uint32_t seq_no = 0;
    ucp_flag_t flag = 0;
    bool is_last = false;

    sequencer_t* sequencer = sequencer_init();

    while(/*true || */!sequencer_complete(sequencer)) {
        if (sequencing_queue_pop(&(curr_thread->seq_queue), &seq_no, &flag, &is_last)) {
            send_ctrl_packet(seq_no, flag, curr_thread->client);
            sequencer_add(sequencer, seq_no, flag == UCP_FLAG_DATA_END);
            sequencer_iterate_missing_segments(sequencer, send_nack, curr_thread->client);
        }
    }

    send_fin(sequencer->expectedLastSeqNo, curr_thread->client);

    pthread_cancel(curr_thread->rcv_thread);
    sequencer_destroy(sequencer);

    return NULL;
}

int main(void) {
    ucp_server_thread_context_t thread_ctx;

    LinkedListInit(&(thread_ctx.seq_queue));

    struct sockaddr_in* server_addr = (struct sockaddr_in*) malloc(sizeof(struct sockaddr_in));
    struct sockaddr_in* client_addr = (struct sockaddr_in*) malloc(sizeof(struct sockaddr_in));

    thread_ctx.udp_fd = udp_socket_initialise(&server_addr, SERVER_BASE_PORT);
    if (thread_ctx.udp_fd < 0) {
        fprintf(stderr, "Error creating socket\n");
        return -1;
    }

    if (udp_socket_bind(thread_ctx.udp_fd, server_addr)) {
        fprintf(stderr, "Error binding socket\n");
        return -1;
    }

    uint8_t recv_buffer[UDP_PACKET_SIZE];

    if (udp_socket_receive_from(thread_ctx.udp_fd, &client_addr, recv_buffer, sizeof(recv_buffer), true) < 0) {
        fprintf(stderr, "Error receiving data\n");
        return -1;
    }

    ucp_packet_t rcv_pkt;
    ucp_packet_decode(recv_buffer, sizeof(recv_buffer), &rcv_pkt);
    if (rcv_pkt.type == UCP_PACKET_TYPE_METADATA) {
        fprintf(stderr, "Received metadata: %.*s\n", 20, rcv_pkt.metadata_packet.desination_name);
        file_io_open_file_of_size(&(thread_ctx.handle), rcv_pkt.metadata_packet.desination_name, rcv_pkt.metadata_packet.part_size);
    }

    printf("Received connection from client " IP_ADDR_FORMAT "\n", IP_ADDR((*client_addr)));


    thread_ctx.client_port = ntohs(client_addr->sin_port);

    tcp_endpoint_t* tcp_endpoint = (tcp_endpoint_t*)malloc(sizeof(tcp_endpoint_t));
    
    tcp_endpoint->addr.sin_family = client_addr->sin_family;
    tcp_endpoint->addr.sin_addr.s_addr = client_addr->sin_addr.s_addr;
    tcp_endpoint->addr.sin_port = htons(thread_ctx.client_port);

    tcp_endpoint->sd = -1;
    tcp_endpoint->next = NULL;

    thread_ctx.client = tcp_client_connect(tcp_endpoint, NULL, NULL);
    if (!thread_ctx.client) {
        fprintf(stderr, "Error forming reverse connection to client\n");
        return -1;
    }

    if (pthread_create(&(thread_ctx.rcv_thread), NULL, receiving_thread, &thread_ctx)) {
        fprintf(stderr, "Error creating receiving thread\n");
    }

  if (pthread_create(&(thread_ctx.seq_thread), NULL, sequencing_thread, &thread_ctx)) {
        fprintf(stderr, "Error creating sequencing thread\n");
    }

    pthread_join(thread_ctx.seq_thread, NULL);
    pthread_join(thread_ctx.rcv_thread, NULL);

    tcp_client_disconnect(thread_ctx.client);

    printf("File received successfully\n");

    return 0;
}

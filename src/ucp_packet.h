#ifndef UCP_PACKET_H
#define UCP_PACKET_H

#include <stdint.h>
#include <stdlib.h>

#include "defines.h"

typedef enum {
    UCP_PACKET_TYPE_CTRL = 0x01,
    UCP_PACKET_TYPE_DATA = 0x02,
    UCP_PACKET_TYPE_METADATA = 0x03,
} ucp_packet_type_t;

typedef enum {
    UCP_FLAG_ACK = 0x01,
    UCP_FLAG_NACK = 0x02,
    UCP_FLAG_FIN = 0x03,
} ucp_flag_t;

typedef enum {
    UCP_FLAG_DATA_SEGMENT = 0x00,
    UCP_FLAG_DATA_START = 0x01,
    UCP_FLAG_DATA_END
} ucp_flag_data_t;

typedef struct __ucp_data_packet_t {
    ucp_flag_data_t flag;
    uint32_t        seq_no;
    uint32_t        offset;
    size_t        seg_len;
    uint8_t         segment_data[UDP_PACKET_DATA_SIZE];
} ucp_data_packet_t;

typedef struct __ucp_ctrl_packet_t {
    uint32_t    seq_no;
    ucp_flag_t  flag;
} ucp_ctrl_packet_t;

typedef struct __ucp_metadata_packet_t {
    char desination_name[20];
    uint8_t part_index;
    uint32_t part_size;
} ucp_metadata_packet_t;

typedef struct __ucp_packet_t {
    ucp_packet_type_t type;
    union {
        ucp_ctrl_packet_t ctrl_packet;
        ucp_data_packet_t data_packet;
        ucp_metadata_packet_t metadata_packet;
    };
} ucp_packet_t;

ucp_packet_t* ucp_packet_init_metadata(char* dst_name, size_t dst_len, uint8_t part_index, uint32_t part_size);

ucp_packet_t* ucp_packet_init_data(uint32_t seq_no, size_t offset, uint8_t* buf, size_t buf_len);

ucp_packet_t* ucp_packet_init_ctrl(uint32_t seq_no, ucp_flag_t flag);

void ucp_packet_free(ucp_packet_t* packet);

size_t ucp_packet_encode(ucp_packet_t* packet, uint8_t *buf, size_t buf_len);

void ucp_packet_decode(uint8_t *buf, size_t buf_len, ucp_packet_t* packet);

#endif // UCP_PACKET_H

#include "ucp_packet.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static ucp_packet_t* ucp_packet_init(ucp_packet_type_t type) {
    ucp_packet_t* pkt = (ucp_packet_t*) malloc(sizeof(ucp_packet_t));
    if (!pkt) {
        perror("malloc");
        return NULL;
    }
    memset(pkt, 0, sizeof(ucp_packet_t));
    pkt->type = type;
    return pkt;
}

ucp_packet_t* ucp_packet_init_data(uint32_t seq_no, size_t offset, uint8_t* buf, size_t buf_len) {
    ucp_packet_t* pkt = ucp_packet_init(UCP_PACKET_TYPE_DATA);
    if (pkt) {
        pkt->data_packet.seq_no = seq_no;
        pkt->data_packet.seg_len = buf_len;
        pkt->data_packet.offset = offset;
        memcpy(pkt->data_packet.segment_data, buf, buf_len);
    }
    return pkt;
}

ucp_packet_t* ucp_packet_init_metadata(char* dst_name, size_t dst_len, uint8_t part_index, uint32_t part_size) {
    ucp_packet_t* pkt = ucp_packet_init(UCP_PACKET_TYPE_METADATA);
    if (pkt) {
        pkt->metadata_packet.part_index = part_index;
        pkt->metadata_packet.part_size = part_size;
        (void) dst_len;
        strncpy(pkt->metadata_packet.desination_name, dst_name, sizeof(pkt->metadata_packet.desination_name) - 1);
    }
    return pkt;
}

ucp_packet_t* ucp_packet_init_ctrl(uint32_t seq_no, ucp_flag_t flag) {
    ucp_packet_t* pkt = ucp_packet_init(UCP_PACKET_TYPE_CTRL);
    if (pkt) {
        pkt->ctrl_packet.seq_no = seq_no;
        pkt->ctrl_packet.flag = flag;
    }
    return pkt;
}

void ucp_packet_free(ucp_packet_t* packet) {
    free(packet);
}

static size_t ucp_packet_encode_data(ucp_packet_t* packet, uint8_t *buf, size_t buf_len) {
    if (!packet || !buf || buf_len < (sizeof(ucp_packet_t)))
        return -1;

    if (packet->type != UCP_PACKET_TYPE_DATA)
        return -1;

    ucp_data_packet_t* data_packet = &packet->data_packet;

    buf[0] = packet->type;

    buf[1] = data_packet->flag & 0xFF;

    // Insert Seq_no
    buf[2] = data_packet->seq_no & 0xFF;
    buf[3] = (data_packet->seq_no >> 8) & 0xFF;
    buf[4] = (data_packet->seq_no >> 16) & 0xFF;
    buf[5] = (data_packet->seq_no >> 24) & 0xFF;

    buf[6] = data_packet->offset & 0xFF;
    buf[7] = (data_packet->offset >> 8) & 0xFF;
    buf[8] = (data_packet->offset >> 16) & 0xFF;
    buf[9] = (data_packet->offset >> 24) & 0xFF;

    // Insert Segment_length
    buf[10] = data_packet->seg_len & 0xFF;
    buf[11] = (data_packet->seg_len >> 8) & 0xFF;

    // Insert data
    memcpy(buf + 12, data_packet->segment_data, data_packet->seg_len);

    return 12 + data_packet->seg_len;

}

static void ucp_packet_decode_data(uint8_t *buf, size_t buf_len, ucp_packet_t* packet) {
    if (!packet || !buf || buf_len < (sizeof(ucp_packet_t)))
        return;

    packet->type = buf[0];

    packet->data_packet.flag = buf[1];

    // Insert Seq_no

    packet->data_packet.seq_no = (buf[5] << 24) | (buf[4] << 16) | (buf[3] << 8) | (buf[2]);

    // Insert offset
    packet->data_packet.offset = (buf[9] << 24) | (buf[8] << 16) | (buf[7] << 8) | (buf[6]);

    // fprintf(stderr, "Received Data Pkt: Seq No %d\n", packet->data_packet.seq_no);

    // Insert Segment_length
    packet->data_packet.seg_len = (buf[11] << 8) | (buf[10]);

    // Insert data
    memcpy(packet->data_packet.segment_data, buf + 12, packet->data_packet.seg_len);
    
}

static size_t ucp_packet_encode_ctrl_data(ucp_packet_t* packet, uint8_t *buf, size_t buf_len) {
    if (!packet || !buf || buf_len < 6)
        return -1;

    if (packet->type != UCP_PACKET_TYPE_CTRL)
        return -1;

    ucp_ctrl_packet_t* ctrl_packet = &packet->ctrl_packet;

    buf[0] = packet->type;

    buf[1] = ctrl_packet->flag & 0xFF;

    // Insert Seq_no
    buf[2] = ctrl_packet->seq_no & 0xFF;
    buf[3] = (ctrl_packet->seq_no >> 8) & 0xFF;
    buf[4] = (ctrl_packet->seq_no >> 16) & 0xFF;
    buf[5] = (ctrl_packet->seq_no >> 24) & 0xFF;

    return 6;
}

void ucp_packet_decode_ctrl_data(uint8_t *buf, size_t buf_len, ucp_packet_t* packet) {
    if (!packet || !buf || buf_len < 6)
        return;

    (packet)->type = buf[0];

    // Insert flag
    packet->ctrl_packet.flag = buf[1];

    // Insert Seq_no
    packet->ctrl_packet.seq_no = (buf[5] << 24) | (buf[4] << 16) | (buf[3] << 8) | (buf[2]);
}

static size_t ucp_packet_encode_meta_data(ucp_packet_t* packet, uint8_t *buf, size_t buf_len) {
    if (!packet || !buf || buf_len < (sizeof(ucp_packet_t)))
        return -1;

    if (packet->type != UCP_PACKET_TYPE_METADATA)
        return -1;

    ucp_metadata_packet_t* metadata_packet = &packet->metadata_packet;

    buf[0] = packet->type & 0xFF;

    // Insert Part_index
    buf[1] = metadata_packet->part_index & 0xFF;

    // Insert Part_size
    buf[2] = (metadata_packet->part_size >> 24) & 0xFF;
    buf[3] = (metadata_packet->part_size >> 16) & 0xFF;
    buf[4] = (metadata_packet->part_size >> 8) & 0xFF;
    buf[5] = (metadata_packet->part_size >> 0) & 0xFF;

    // Insert Destination_name
    memcpy(buf + 6, metadata_packet->desination_name, sizeof(metadata_packet->desination_name));
    return 6 + sizeof(metadata_packet->desination_name);
}

void ucp_packet_decode_meta_data(uint8_t *buf, size_t buf_len, ucp_packet_t* packet) {
    if (!packet || !buf || buf_len < (sizeof(ucp_packet_t)))
        return;

    (packet)->type = buf[0];

    // Insert Part_index
    packet->metadata_packet.part_index = buf[1];

    packet->metadata_packet.part_size = (buf[2] << 24 );
    packet->metadata_packet.part_size = (buf[3] << 16 ) | packet->metadata_packet.part_size;
    packet->metadata_packet.part_size = (buf[4] << 8 ) | packet->metadata_packet.part_size;
    packet->metadata_packet.part_size = (buf[5] << 0 ) | packet->metadata_packet.part_size;

    // Insert Destination_name
    memcpy(packet->metadata_packet.desination_name, buf + 6, sizeof(packet->metadata_packet.desination_name));
}

size_t ucp_packet_encode(ucp_packet_t* packet, uint8_t *buf, size_t buf_len) {
    if (!packet) {
        return -1;
    }
    if (packet->type == UCP_PACKET_TYPE_DATA) {
        return ucp_packet_encode_data(packet, buf, buf_len);
    } else if (packet->type == UCP_PACKET_TYPE_CTRL) {
        return ucp_packet_encode_ctrl_data(packet, buf, buf_len);
    } else if (packet->type == UCP_PACKET_TYPE_METADATA) {
        return ucp_packet_encode_meta_data(packet, buf, buf_len);
    }
    return -2;
}

void ucp_packet_decode(uint8_t *buf, size_t buf_len, ucp_packet_t* packet) {
    if (!packet) {
        return;
    }
    if (buf[0] == UCP_PACKET_TYPE_DATA) {
        // fprintf(stderr, "Received Data Pkt\n");
        ucp_packet_decode_data(buf, buf_len, packet);
    } else if (buf[0] == UCP_PACKET_TYPE_METADATA) {
        // fprintf(stderr, "Received <MEta> Pkt\n");
        ucp_packet_decode_meta_data(buf, buf_len, packet);
    } else if (buf[0] == UCP_PACKET_TYPE_CTRL) {
        // fprintf(stderr, "Received Ctrl Pkt\n");
        ucp_packet_decode_ctrl_data(buf, buf_len, packet);
    }
    return;
}

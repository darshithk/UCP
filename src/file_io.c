#include "defines.h"
#include "file_io.h"

#include <unistd.h>
#include <stdlib.h>

file_io_partition_handle_t* file_io_partition_file(char* filepath, int count) {
    char invocation[256] = {0};

    char prefix[10] = "tmp_";

    snprintf(invocation, sizeof(invocation) - 1, "/usr/bin/split -d -n %d %s %s", count, filepath, prefix);

    FILE* fp = popen(invocation, "r");
    if (!fp) {
        perror("popen");
    }

    int ret = pclose(fp);
    if (ret) {
        perror("pclose");
        return NULL;
    }

    file_io_partition_handle_t* handles = (file_io_partition_handle_t*)calloc(count, sizeof(file_io_partition_handle_t));
    if (!handles) {
        perror("calloc");
        return NULL;
    }

    for (uint8_t i = 0; i < count; i++) {
        handles[i].idx = i;
        snprintf(handles[i].filepath, sizeof(handles[i].filepath) - 1, "%s%02d", prefix, i);
        if (access(handles[i].filepath, F_OK) == -1) {
            fprintf(stderr, "File %s does not exist\n", handles[i].filepath);
            return NULL;
        }
        handles[i].fp = fopen(handles[i].filepath, "rb");
        if (!handles[i].fp) {
            perror("fopen");
            return NULL;
        }
        // Get the size of the file
        fseek(handles[i].fp, 0L, SEEK_END);
        handles[i].part_size = ftell(handles[i].fp);
        fseek(handles[i].fp, 0L, SEEK_SET);
        
    }

    return handles;
}

void file_io_partition_release(file_io_partition_handle_t* handle, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        fclose(handle[i].fp);
        remove(handle[i].filepath);
    }
    free(handle);
}

ucp_packet_t* file_io_get_next_packet(file_io_partition_handle_t* handle) {
    uint8_t buffer[UDP_PACKET_DATA_SIZE] = {0};
    size_t offset = ftell(handle->fp);
    size_t size = fread(buffer, 1, UDP_PACKET_DATA_SIZE, handle->fp);
    if (size == 0) {
        return NULL;
    }
    ucp_packet_t* packet = ucp_packet_init_data(handle->last_seq_no++, offset, buffer, size);
    if (offset == 0) {
        packet->data_packet.flag = UCP_FLAG_DATA_START;
    } else if (feof(handle->fp)) {
        // fprintf(stderr, "End of file\n");
        packet->data_packet.flag = UCP_FLAG_DATA_END;
    }
    return packet;
}

ucp_packet_t* file_io_get_next_packet_with_offset(file_io_partition_handle_t* handle, size_t offset) {
    uint8_t buffer[UDP_PACKET_DATA_SIZE] = {0};
    fseek(handle->fp, offset, SEEK_SET);
    size_t size = fread(buffer, 1, UDP_PACKET_DATA_SIZE, handle->fp);
    if (size == 0) {
        return NULL;
    }
    return ucp_packet_init_data(handle->last_seq_no++, offset, buffer, size);
}

ucp_packet_t* file_io_get_next_packet_with_offset_and_size(file_io_partition_handle_t* handle, size_t offset, size_t size) {
    uint8_t buffer[UDP_PACKET_DATA_SIZE] = {0};
    fseek(handle->fp, offset, SEEK_SET);
    size_t read_size = fread(buffer, 1, size, handle->fp);
    if (read_size == 0) {
        return NULL;
    }
    return ucp_packet_init_data(handle->last_seq_no++, offset, buffer, read_size);
}

bool file_io_save_packet(file_io_partition_handle_t* handle, ucp_packet_t* packet) {
    if (fseek(handle->fp, packet->data_packet.offset, SEEK_SET)) {
        return false;
    }
    if (fwrite(packet->data_packet.segment_data, 1, packet->data_packet.seg_len, handle->fp) != packet->data_packet.seg_len) {
        return false;
    }
    if (fflush(handle->fp)) {
        return false;
    }
    return true;
}

bool file_io_open_file_of_size(file_io_partition_handle_t* handle, char* name, size_t size) {

    handle->fp = fopen(name, "wb");
    if (!handle->fp) {
        perror("fopen");
        return false;
    }
    handle->part_size = size;
    // Create a file of size size filled with 0s
    uint8_t buffer[UDP_PACKET_DATA_SIZE] = {0};
    size_t remaining = size;
    while (remaining > 0) {
        size_t write_size = remaining > UDP_PACKET_DATA_SIZE ? UDP_PACKET_DATA_SIZE : remaining;
        if (fwrite(buffer, 1, write_size, handle->fp) != write_size) {
            return false;
        }
        remaining -= write_size;
    }
    return true;
}

bool file_io_merge_file(char* prefix, char* outfile) {
    char invocation[256] = {0};
    #ifdef __APPLE__
    snprintf(invocation, sizeof(invocation) - 1, "/bin/cat %s* > %s", prefix, outfile);
    #else
    snprintf(invocation, sizeof(invocation) - 1, "/usr/bin/cat %s* > %s", prefix, outfile);
    #endif

    // If the file already exists, delete it
    if (access(outfile, F_OK) != -1) {
        remove(outfile);
    }

    FILE* fp = popen(invocation, "r");
    if (!fp) {
        perror("popen");
    }

    int ret = pclose(fp);
    if (ret) {
        perror("pclose");
        return false;
    }
    return true;
}

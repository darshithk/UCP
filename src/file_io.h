#ifndef FILE_IO_H_
#define FILE_IO_H_

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include "ucp_packet.h"

typedef struct __file_io_partition_handle {
    char filepath[255];
    FILE* fp;
    size_t bytes_read;
    uint8_t idx;
    uint32_t last_seq_no;
    uint32_t part_size;
} file_io_partition_handle_t;

file_io_partition_handle_t* file_io_partition_file(char* filepath, int count);

void file_io_partition_release(file_io_partition_handle_t* handle, uint8_t count);

ucp_packet_t* file_io_get_next_packet(file_io_partition_handle_t* handle);

ucp_packet_t* file_io_get_next_packet_with_offset(file_io_partition_handle_t* handle, size_t offset);

ucp_packet_t* file_io_get_next_packet_with_offset_and_size(file_io_partition_handle_t* handle, size_t offset, size_t size);

bool file_io_save_packet(file_io_partition_handle_t* handle, ucp_packet_t* packet);

bool file_io_open_file_of_size(file_io_partition_handle_t* handle, char* name, size_t size);

bool file_io_merge_file(char* prefix, char* outfile);

#endif // FILE_IO_H_

cmake_minimum_required(VERSION 3.1)

project(ucp)
set(CMAKE_C_STANDARD 11)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(SERVER_SOURCE_FILES ${SRC_DIR}/ucp_server.c
                        ${SRC_DIR}/tcp_socket.c
                        ${SRC_DIR}/udp_socket.c
                        ${SRC_DIR}/ucp_packet.c
                        ${SRC_DIR}/file_io.c
                        ${SRC_DIR}/sequencer.c
                        ${SRC_DIR}/linked_list.c)
set(CLIENT_SOURCE_FILES ${SRC_DIR}/ucp_client.c
                        ${SRC_DIR}/tcp_socket.c
                        ${SRC_DIR}/ucp_packet.c
                        ${SRC_DIR}/udp_socket.c
                        ${SRC_DIR}/file_io.c
                        ${SRC_DIR}/linked_list.c)

add_executable(ucp-daemon ${SERVER_SOURCE_FILES})
add_executable(ucp ${CLIENT_SOURCE_FILES})

target_compile_definitions(ucp-daemon PRIVATE -DUCP_SERVER)
target_compile_definitions(ucp PRIVATE -DUCP_CLIENT)

target_compile_options(ucp-daemon PRIVATE -Wall -Wextra -Wpedantic -g)
target_compile_options(ucp PRIVATE -Wall -Wextra -Wpedantic -g)

target_link_libraries(ucp-daemon -pthread)
target_link_libraries(ucp -pthread)

install(TARGETS ucp-daemon DESTINATION /usr/local/bin)
install(TARGETS ucp DESTINATION /usr/local/bin)

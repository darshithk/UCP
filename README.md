# UCP

### ee542-assignment-4

A UDP based file transfer application written in C.

Developed based on the requirements for the lab assignment for EE542.

## Aim
Develop a file transfer application which uses UDP/IP to circumvent TCP/IP congestion control limitations, while ensuring reliable delivery of file packets.

## Application Interface
The application will expose a client and server application. The server application is a daemon, while the client application will be invoked as a CLI similar to the invocation of `scp`.

## BUILDING

We use CMake to build the project binaries.

To get started.

```bash
$ cmake -S . -B build
$ cmake --build build
```

You will have your application binaries generated in the build directory.

To run the Command Line utility.
```bash
$ ./build/ucp
```

To run the receiver daemon
```bash
$ ./build/ucp-server
```

# mdns responder for esp8266

This library implements a simple MDNS client for the esp8266.

## Documentation

See the header file `include/mdns/mdns.h` for documentation.

## Example code

TODO

## Building for esp8266

Some pointers:

1. Get the ESP8266 compiler: https://github.com/pfalcon/esp-open-sdk
2. Get the SDK
3. Unpack SDK somewhere
4. Set up the environment
    ```bash
    export PATH="/path/to/compiler/xtensa-lx106-elf/bin:$PATH"
    export SDK_PATH="$HOME/esp-sdk/ESP8266_RTOS_SDK"
    export BIN_PATH="$HOME/esp-sdk/build"

    export CFLAGS="-I${SDK_PATH}/include -I${SDK_PATH}/extra_include $CFLAGS"
    export LDFLAGS="-L${SDK_PATH}/lib $LDFLAGS"
    ```
5. Switch to the base directory and run `make`
6. Grab the library from `.output/lib/libmdns.a`
7. Grab the headers from `include/*.h`

## Other platforms

The code in `library` is abstracted from the actual hardware by a very thin abstraction layer which is defined in `platform`. To adapt the mdns service to another platform you will have to exchange the Makefiles and supply implementations for the following functions in `libplatform`:

### Types

- `mdnsUDPHandle`: handle for the UDP connection (socket, LWIP `udp_pcb`, etc.)
- `mdnsNetworkBuffer`: platform specific buffer type (probably a linked list, etc.)
- `struct _mdnsStreamBuf`: stream buffer internal state (probably a byte offset and a linked list of buffers)
- `struct ip_addr`: a IPv4 address (the one from LWIP is fine, just import it)

### Networking

- `bool mdns_join_multicast_group(void)`: join the MDNS multicast group
- `bool mdns_leave_multicast_group(void)`: leave the MDNS multicast group
- `mdnsUDPHandle *mdns_listen(mdnsHandle *handle)`: listen to packets from the multicast group and connect
- `uint16_t mdns_send_udp_packet(mdnsHandle *handle, char *data, uint16_t len)`: send UDP payload
- `void mdns_shutdown_socket(mdnsUDPHandle *pcb)`: shutdown a socket

### Buffer handling

- `mdnsStreamBuf *mdns_stream_new(mdnsNetworkBuffer *buffer)`: create a stream buffer for the platform specific response buffers
- `uint8_t mdns_stream_read8(mdnsStreamBuf *buffer)`: read a byte from the buffer
- `void mdns_stream_destroy(mdnsStreamBuf *buffer)`: free stream buffer

## Legal

License: 3 Clause BSD (see LICENSE-BSD.txt)
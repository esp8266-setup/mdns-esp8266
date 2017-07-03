# mdns responder for esp8266

This library implements a simple MDNS client for the esp8266.

## Documentation

See the header file `include/mdns/mdns.h` for documentation.

## Example code

TODO

## Building

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

## Legal

License: 3 Clause BSD (see LICENSE-BSD.txt)
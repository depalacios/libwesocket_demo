# ws_raw - Lightweight WebSocket Server Library

## Overview

`ws_raw` is a minimal WebSocket server library built on top of `libwebsockets` and `libuv`. It provides a simple API for creating WebSocket servers with minimal boilerplate. The library is designed for embedded systems and applications that require efficient, asynchronous WebSocket communication.

## Features

- Simple API for creating WebSocket servers
- Integration with libuv event loop (automatic, no need to call `lws_service` manually)
- Callback-based message handling
- Lightweight, single connection support (last connected client receives messages)
- Text frame support (binary frames not implemented)

## Dependencies

- [libwebsockets](https://libwebsockets.org/) (version 4.0 or higher)
- [libuv](https://libuv.org/) (version 1.0 or higher)

## API Documentation

### Header File: `ws_raw.h`

#### Data Types

- `ws_raw_ctx_t`: Opaque handle for the WebSocket server context.

- `ws_raw_rx_cb_t`: Callback function type for received messages.
    ```c
    typedef void (*ws_raw_rx_cb_t)(ws_raw_ctx_t *ctx, const void *data, size_t len);
    ```
- ws_raw_cfg_t: Configuration structure for server initialization.
    ```c
    typedef struct {
        int port;               // Port to listen on
        ws_raw_rx_cb_t on_rx;   // Callback for received messages
    } ws_raw_cfg_t;
    ```

#### Functions
- Initializes a WebSocket server instance. Returns a pointer to the context on success, or NULL on failure.
    ```c
    ws_raw_ctx_t *ws_raw_init(uv_loop_t *loop, const ws_raw_cfg_t *cfg);
    ```

- Sends data to the connected client. Returns the number of bytes sent, or -1 on error.
    ```c
    int ws_raw_send(ws_raw_ctx_t *ctx, const void *data, size_t len);
    ```

- Destroys the WebSocket server instance and frees resources.
    ```c
    void ws_raw_destroy(ws_raw_ctx_t *ctx);
    ```


### Usage Example
#### Basic Echo Server
```c
#include "ws_raw.h"

static void on_ws_rx(ws_raw_ctx_t *ctx, const void *data, size_t len)
{
    lwsl_user("RX (%zu): %.*s\n", len, (int)len, (const char *)data);
    
    // Echo the received message back to the client
    int ret = ws_raw_send(ctx, data, len);
    if (ret < 0) {
        lwsl_err("Error sending echo\n");
    } else {
        lwsl_user("Echo sent successfully\n");
    }
}

int main(void)
{
    uv_loop_t *loop = uv_default_loop();

    ws_raw_cfg_t cfg = {
        .port = 9000,
        .on_rx = on_ws_rx
    };

    ws_raw_ctx_t *ws = ws_raw_init(loop, &cfg);
    if (!ws) {
        lwsl_user("Error starting WS\n");
        return 1;
    }

    lwsl_user("RAW WS server started on port %d\n", cfg.port);
    lwsl_user("Press Ctrl+C to exit\n");

    uv_run(loop, UV_RUN_DEFAULT);

    ws_raw_destroy(ws);
    uv_loop_close(loop);
    
    return 0;
}
```

## Notes
The server currently supports only one client at a time. The last client to connect will be the recipient of any messages sent by the server.

The library uses the libuv event loop integration provided by libwebsockets, so there's no need to call lws_service manually.

The send function uses a dynamic buffer with LWS_PRE bytes prepended for libwebsockets internal use.

## Limitations
Single client support (the last connected client replaces previous ones).

Text frames only (binary frames are not implemented, but can be added by modifying ws_raw_send to use LWS_WRITE_BINARY).

No authentication or encryption (TLS not implemented).
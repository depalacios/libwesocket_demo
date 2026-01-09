# ws_raw – Lightweight Raw WebSocket Server (C)

ws_raw is a lightweight WebSocket server abstraction built on top of **libwebsockets** and **libuv**.
It provides a simple API for managing multiple WebSocket clients, sending and broadcasting messages,
and handling connection lifecycle events with thread-safe access.

Designed for embedded and Linux systems with a focus on performance and low overhead.

---

## Features

- WebSocket server using libwebsockets
- libuv event loop integration
- Thread-safe client management (pthread mutex)
- Automatic client ID generation
- Per-client user data support
- Optimized static + dynamic TX buffers
- Broadcast and broadcast-except messaging
- Configurable maximum number of clients
- Clean startup and shutdown

---

## Dependencies

- libwebsockets
- libuv
- pthread
- Standard C library

---

## Configuration

```c
typedef struct {
    int port;
    ws_raw_rx_cb_t on_rx;
    ws_raw_connect_cb_t on_connect;
    ws_raw_disconnect_cb_t on_disconnect;
    int max_clients;
} ws_raw_cfg_t;
```

### Fields

- **port**: TCP port to listen on
- **on_rx**: Called when data is received
- **on_connect**: Called when a client connects
- **on_disconnect**: Called when a client disconnects
- **max_clients**: Maximum allowed clients (0 = default)

---

## Lifecycle API

### Initialize

```c
void ws_raw_init(const ws_raw_cfg_t *cfg);
```

Creates the WebSocket context and initializes libuv integration.

---

### Run

```c
void ws_raw_run();
```

Starts the libuv event loop (blocking).

---

### Stop

```c
void ws_raw_stop();
```

Stops the server asynchronously.

---

### Destroy

```c
void ws_raw_destroy();
```

Cleans up all resources, clients, and contexts.

---

## Messaging API

### Send to a Client

```c
int ws_raw_send_to(ws_raw_ctx_t *ctx,
                   ws_raw_client_t *client,
                   const void *data,
                   size_t len);
```

Queues data to be sent to a specific client.

---

### Broadcast

```c
int ws_raw_broadcast(ws_raw_ctx_t *ctx,
                     const void *data,
                     size_t len);
```

Sends data to all connected clients.

---

### Broadcast Except

```c
int ws_raw_broadcast_except(ws_raw_ctx_t *ctx,
                            ws_raw_client_t *exclude,
                            const void *data,
                            size_t len);
```

Sends data to all clients except one.

---

## Client Utilities

```c
int ws_raw_client_count(ws_raw_ctx_t *ctx);
```

Returns number of connected clients.

```c
ws_raw_client_t** ws_raw_get_clients(ws_raw_ctx_t *ctx, int *count);
```

Returns a snapshot list of clients (caller must free).

---

## Client Metadata

```c
const char* ws_raw_client_get_id(ws_raw_client_t *client);
time_t ws_raw_client_get_connect_time(ws_raw_client_t *client);
void ws_raw_client_set_user_data(ws_raw_client_t *client, void *data);
void* ws_raw_client_get_user_data(ws_raw_client_t *client);
```

---

## Thread Safety

- All client operations are mutex-protected
- Safe to send data from other threads
- libwebsockets callbacks synchronized internally

---

## Buffer Strategy

- Static buffer per client for small messages
- Dynamic allocation for larger payloads
- One pending TX per client

---

## Typical Flow

1. Configure `ws_raw_cfg_t`
2. Call `ws_raw_init()`
3. Call `ws_raw_run()`
4. Handle callbacks
5. Call `ws_raw_stop()`
6. Call `ws_raw_destroy()`

---

## License

MIT or user-defined.

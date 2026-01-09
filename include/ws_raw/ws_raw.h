#ifndef WS_RAW_H
#define WS_RAW_H

/* ============================================================================
 * INCLUDES
 * ============================================================================
 */

#include <libwebsockets.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <uv.h>

/* ============================================================================
 * OPAQUE TYPES
 * ============================================================================
 */

/**
 * @brief Opaque server context.
 *
 * The internal structure is hidden from API users.
 */
typedef struct ws_raw_ctx ws_raw_ctx_t;

/**
 * @brief Opaque client handle.
 *
 * Represents a single connected WebSocket client.
 */
typedef struct ws_raw_client ws_raw_client_t;

/* ============================================================================
 * CALLBACK TYPES
 * ============================================================================
 */

/**
 * @brief Callback triggered when data is received from a client.
 *
 * @param ctx    Pointer to the server context.
 * @param client Pointer to the client sending the message.
 * @param data   Pointer to the received payload.
 * @param len    Length of the payload in bytes.
 *
 * @note The data buffer is only valid for the duration of the callback.
 */
typedef void (*ws_raw_rx_cb_t)(ws_raw_ctx_t *ctx, ws_raw_client_t *client, const void *data, size_t len);

/**
 * @brief Callback triggered when a new client successfully connects.
 *
 * @param ctx    Pointer to the server context.
 * @param client Pointer to the newly connected client.
 */
typedef void (*ws_raw_connect_cb_t)(ws_raw_ctx_t *ctx, ws_raw_client_t *client);

/**
 * @brief Callback triggered when a client disconnects.
 *
 * @param ctx    Pointer to the server context.
 * @param client Pointer to the disconnected client.
 */
typedef void (*ws_raw_disconnect_cb_t)(ws_raw_ctx_t *ctx, ws_raw_client_t *client);

/* ============================================================================
 * SERVER CONFIGURATION
 * ============================================================================
 */

/**
 * @brief WebSocket server configuration.
 */
typedef struct {
    int port;                             /**< Listening port (e.g., 9000). */
    ws_raw_rx_cb_t on_rx;                 /**< Mandatory: RX handler. */
    ws_raw_connect_cb_t on_connect;       /**< Optional: connect handler. */
    ws_raw_disconnect_cb_t on_disconnect; /**< Optional: disconnect handler. */
    int max_clients;                      /**< Max clients (0 = unlimited). */
} ws_raw_cfg_t;

/* ============================================================================
 * GLOBALS (ADVANCED / EMBEDDED USE)
 * ============================================================================
 */

/**
 * @brief libuv loop used by the server.
 *
 * Exposed to allow integration with external libuv-based applications.
 */
extern uv_loop_t *loop;

/**
 * @brief Global server instance.
 *
 * Useful for single-instance embedded applications.
 */
extern ws_raw_ctx_t *ws;

/* ============================================================================
 * SERVER LIFECYCLE API
 * ============================================================================
 */

/**
 * @brief Initialize the WebSocket server.
 *
 * This function:
 * - Creates the libwebsockets context
 * - Attaches it to a libuv loop
 * - Applies the provided configuration
 *
 * @param cfg Pointer to server configuration.
 *
 * @note This does NOT start the event loop.
 *       Call ws_raw_run() to begin processing events.
 */
void ws_raw_init(const ws_raw_cfg_t *cfg);

/**
 * @brief Start the server event loop (blocking).
 *
 * This call blocks until ws_raw_stop() is invoked.
 */
void ws_raw_run(void);

/**
 * @brief Request the server to stop.
 *
 * Thread-safe and signal-safe (can be called from SIGINT handlers).
 */
void ws_raw_stop(void);

/**
 * @brief Destroy the server and free all resources.
 *
 * This closes all client connections and destroys the context.
 */
void ws_raw_destroy(void);

/* ============================================================================
 * DATA TRANSMISSION API
 * ============================================================================
 */

/**
 * @brief Send data to the first connected client.
 *
 * @deprecated Provided for backward compatibility.
 *
 * @param ctx  Server context.
 * @param data Pointer to payload.
 * @param len  Payload length in bytes.
 *
 * @return 0 on success, -1 on failure.
 */
int ws_raw_send(ws_raw_ctx_t *ctx, const void *data, size_t len);

/**
 * @brief Send data to a specific client.
 *
 * Uses a hybrid buffering strategy:
 * - < 4 KB  → static zero-allocation buffer
 * - ≥ 4 KB  → temporary heap allocation
 *
 * @param ctx    Server context.
 * @param client Target client.
 * @param data   Payload pointer.
 * @param len    Payload length in bytes.
 *
 * @return 0 on success, -1 if client is invalid or busy.
 */
int ws_raw_send_to( ws_raw_ctx_t *ctx, ws_raw_client_t *client, const void *data, size_t len);

/**
 * @brief Broadcast data to all connected clients.
 *
 * @param ctx  Server context.
 * @param data Payload pointer.
 * @param len  Payload length in bytes.
 *
 * @return Number of clients scheduled to receive the message.
 */
int ws_raw_broadcast(ws_raw_ctx_t *ctx, const void *data, size_t len);

/**
 * @brief Broadcast to all clients except one.
 *
 * Useful for chat-like scenarios.
 *
 * @param ctx            Server context.
 * @param exclude_client Client that should NOT receive the message.
 * @param data           Payload pointer.
 * @param len            Payload length in bytes.
 *
 * @return Number of clients scheduled to receive the message.
 */
int ws_raw_broadcast_except(ws_raw_ctx_t *ctx,ws_raw_client_t *exclude_client,const void *data,size_t len);

/* ============================================================================
 * CLIENT QUERY API
 * ============================================================================
 */

/**
 * @brief Get the number of currently connected clients.
 *
 * @param ctx Server context.
 * @return Client count.
 */
int ws_raw_client_count(ws_raw_ctx_t *ctx);

/**
 * @brief Get a snapshot of connected clients.
 *
 * @param ctx   Server context.
 * @param count Output parameter for number of clients.
 *
 * @return Dynamically allocated array of client pointers.
 *
 * @warning The returned array must be freed by the caller using free().
 */
ws_raw_client_t **ws_raw_get_clients(ws_raw_ctx_t *ctx, int *count);

/* ============================================================================
 * CLIENT ACCESSORS
 * ============================================================================
 */

/**
 * @brief Get the unique identifier of a client.
 */
const char *ws_raw_client_get_id(ws_raw_client_t *client);

/**
 * @brief Get the connection timestamp of a client.
 */
time_t ws_raw_client_get_connect_time(ws_raw_client_t *client);

/**
 * @brief Associate user-defined data with a client.
 *
 * The library does not manage the lifetime of this pointer.
 */
void ws_raw_client_set_user_data(ws_raw_client_t *client, void *user_data);

/**
 * @brief Retrieve user-defined data associated with a client.
 */
void *ws_raw_client_get_user_data(ws_raw_client_t *client);



#endif /* WS_RAW_H */

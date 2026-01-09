/**
 * @file ws_raw.c
 * @brief Raw WebSocket server based on libwebsockets + libuv.
 *
 * This module implements a lightweight, multi-client WebSocket server
 * designed for embedded and Linux environments.
 *
 * Features:
 *  - libwebsockets server with libuv event loop
 *  - Thread-safe client handling
 *  - Optimized TX buffering (static + dynamic)
 *  - Client abstraction with opaque API
 *
 * Intended usage:
 *  - Initialize with ws_raw_init()
 *  - Run with ws_raw_run()
 *  - Stop gracefully with ws_raw_stop()
 *  - Destroy with ws_raw_destroy()
 */

#include "ws_raw.h"

/* ============================================================================
 * GLOBAL OBJECTS
 * ============================================================================
 */

/**
 * @brief Global libuv loop pointer (exposed in public API).
 */
uv_loop_t *loop = NULL;

/**
 * @brief Global WebSocket server context.
 */
ws_raw_ctx_t *ws = NULL;

/* ============================================================================
 * CONSTANTS
 * ============================================================================
 */

#define CLIENT_ID_SIZE        37   /**< Length of generated client ID */
#define MAX_CLIENTS_DEFAULT   100  /**< Default maximum clients */
#define STATIC_BUFFER_SIZE   4096  /**< Static TX buffer size */

/* ============================================================================
 * INTERNAL DATA STRUCTURES
 * ============================================================================
 */

/**
 * @brief Internal representation of a connected WebSocket client.
 *
 * This structure is opaque to API users. It maintains:
 * - libwebsockets session handle
 * - Unique client identifier
 * - Connection timestamp
 * - User-defined data pointer
 * - Optimized transmission buffers
 */
struct ws_raw_client {
    struct lws *wsi;                     /**< libwebsockets session handle */
    char id[CLIENT_ID_SIZE];             /**< Unique client identifier */
    time_t connect_time;                 /**< Connection timestamp (Unix time) */
    void *user_data;                     /**< User-defined data pointer */
    struct ws_raw_client *next;          /**< Linked list pointer to next client */

    /* Optimized TX buffers */
    unsigned char tx_buf_static[STATIC_BUFFER_SIZE]; /**< Static buffer for small messages */
    unsigned char *tx_buf_dynamic;       /**< Dynamic buffer for large messages */
    size_t tx_len;                       /**< Payload length in bytes */
    int tx_pending;                      /**< TX pending flag (0=ready, 1=pending) */
    int using_static;                    /**< Buffer selection flag (1=static, 0=dynamic) */
};

/**
 * @brief Main server context structure.
 *
 * Contains all server state including:
 * - libwebsockets context
 * - Linked list of connected clients
 * - Server configuration
 * - Thread synchronization primitives
 */
struct ws_raw_ctx {
    struct lws_context *context;          /**< libwebsockets server context */
    ws_raw_client_t *clients;             /**< Linked list of connected clients */
    const ws_raw_cfg_t *cfg;              /**< Server configuration (copy) */
    uv_loop_t *loop;                      /**< libuv event loop reference */
    uv_async_t stop_async;                /**< Async stop signal handler */
    int running;                          /**< Server running flag (1=running, 0=stopped) */
    pthread_mutex_t lock;                 /**< Global mutex for thread safety */
    int client_counter;                   /**< Total client counter (for ID generation) */
};

/**
 * @brief Per-session data allocated by libwebsockets.
 *
 * Each WebSocket connection has its own instance of this structure.
 * Linked to both the global context and the specific client.
 */
struct per_session_data {
    ws_raw_ctx_t *global_ctx;             /**< Pointer to global server context */
    ws_raw_client_t *client;              /**< Associated client structure */
};

/* ============================================================================
 * UTILITY FUNCTIONS (INTERNAL)
 * ============================================================================
 */

/**
 * @brief Generate a unique client identifier.
 *
 * Creates a time-based ID string in format "client-TIMESTAMP-COUNTER".
 * Thread-safe using atomic operations.
 *
 * @param buffer Output buffer for ID string
 * @param size   Buffer size (must be at least CLIENT_ID_SIZE)
 */
static void generate_client_id(char *buffer, size_t size) {
    static unsigned int counter = 0;
    time_t now = time(NULL);
    snprintf(buffer, size, "client-%ld-%04d", now, __sync_fetch_and_add(&counter, 1));
}

/**
 * @brief Add a new client to the server's client list.
 *
 * Creates and initializes a new client structure, adds it to the linked list,
 * and invokes the on_connect callback if configured.
 *
 * @param ctx Server context
 * @param wsi libwebsockets session instance
 * @return Pointer to new client structure, or NULL on failure
 */
static ws_raw_client_t* add_client(ws_raw_ctx_t *ctx, struct lws *wsi) {
    pthread_mutex_lock(&ctx->lock);
    
    /* Check client limit */
    int count = 0;
    ws_raw_client_t *tmp = ctx->clients;
    while (tmp) {
        count++;
        tmp = tmp->next;
    }
    
    if (ctx->cfg->max_clients > 0 && count >= ctx->cfg->max_clients) {
        pthread_mutex_unlock(&ctx->lock);
        return NULL;  /* Maximum clients reached */
    }
    
    /* Allocate and initialize client structure */
    ws_raw_client_t *client = calloc(1, sizeof(ws_raw_client_t));
    if (!client) {
        pthread_mutex_unlock(&ctx->lock);
        return NULL;  /* Memory allocation failed */
    }
    
    client->wsi = wsi;
    generate_client_id(client->id, sizeof(client->id));
    client->connect_time = time(NULL);
    client->next = ctx->clients;  /* Insert at head of list */
    client->tx_buf_dynamic = NULL;
    client->tx_pending = 0;
    client->using_static = 1;
    
    ctx->clients = client;
    ctx->client_counter++;
    
    pthread_mutex_unlock(&ctx->lock);
    
    return client;
}

/**
 * @brief Remove a client from the server's client list.
 *
 * Cleans up client resources, invokes on_disconnect callback,
 * and removes the client from the linked list.
 *
 * @param ctx    Server context
 * @param client Client to remove
 */
static void remove_client(ws_raw_ctx_t *ctx, ws_raw_client_t *client) {
    if (!ctx || !client) return;
    
    pthread_mutex_lock(&ctx->lock);
    
    /* Find and remove client from linked list */
    ws_raw_client_t **pp = &ctx->clients;
    while (*pp) {
        if (*pp == client) {
            *pp = client->next;  /* Bypass this node */
            
            /* Free dynamic buffer if allocated */
            if (client->tx_buf_dynamic) {
                free(client->tx_buf_dynamic);
            }
            
            /* Invoke disconnect callback */
            if (ctx->cfg->on_disconnect) {
                ctx->cfg->on_disconnect(ctx, client);
            }
            
            free(client);
            break;
        }
        pp = &(*pp)->next;
    }
    
    pthread_mutex_unlock(&ctx->lock);
}

/* ============================================================================
 * WEBSOCKET CALLBACK HANDLER
 * ============================================================================
 */

/**
 * @brief libwebsockets protocol callback function.
 *
 * Handles all WebSocket events:
 * - Connection establishment
 * - Message reception
 * - Write readiness
 * - Connection closure
 *
 * @param wsi    libwebsockets session instance
 * @param reason Callback reason (event type)
 * @param user   Per-session data pointer
 * @param in     Received data (for LWS_CALLBACK_RECEIVE)
 * @param len    Data length (for LWS_CALLBACK_RECEIVE)
 * @return Always returns 0 (libwebsockets convention)
 */
static int cb_ws(struct lws *wsi, enum lws_callback_reasons reason, 
                 void *user, void *in, size_t len)
{
    struct per_session_data *psd = (struct per_session_data *)user;
    
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            /* New WebSocket connection established */
            psd->global_ctx = (ws_raw_ctx_t *)lws_get_protocol(wsi)->user;
            if (psd->global_ctx) {
                psd->client = add_client(psd->global_ctx, wsi);
                if (psd->client && psd->global_ctx->cfg->on_connect) {
                    psd->global_ctx->cfg->on_connect(psd->global_ctx, psd->client);
                }
            }
            break;

        case LWS_CALLBACK_RECEIVE:
            /* Message received from client */
            if (!psd || !psd->global_ctx || !psd->client)
                break;

            /* Forward to user callback */
            psd->global_ctx->cfg->on_rx(psd->global_ctx, psd->client, in, len);
            break;
        
        case LWS_CALLBACK_SERVER_WRITEABLE:
            /* Socket ready for writing */
            if (!psd || !psd->client)
                break;

            ws_raw_client_t *client = psd->client;

            pthread_mutex_lock(&psd->global_ctx->lock);

            if (client->tx_pending) {
                /* Select appropriate buffer */
                unsigned char *buf = client->using_static ? 
                    client->tx_buf_static : client->tx_buf_dynamic;
                
                /* Write to WebSocket (skip LWS_PRE bytes) */
                lws_write(client->wsi, buf + LWS_PRE, client->tx_len, LWS_WRITE_TEXT);

                /* Free dynamic buffer if used */
                if (!client->using_static && client->tx_buf_dynamic) {
                    free(client->tx_buf_dynamic);
                    client->tx_buf_dynamic = NULL;
                }
                
                client->tx_pending = 0;  /* Mark as sent */
            }

            pthread_mutex_unlock(&psd->global_ctx->lock);
            break;

        case LWS_CALLBACK_CLOSED:
            /* Connection closed */
            if (psd && psd->global_ctx && psd->client) {
                remove_client(psd->global_ctx, psd->client);
                psd->client = NULL;
            }
            break;

        default:
            /* Other events not handled */
            break;
    }
    return 0;
}

/* ============================================================================
 * LWS PROTOCOL DEFINITION
 * ============================================================================
 */

/**
 * @brief libwebsockets protocol array.
 *
 * Defines the WebSocket protocol handler with:
 * - Protocol name
 * - Callback function
 * - Per-session data size
 * - Receive buffer size
 */
static struct lws_protocols protocols[] = {
    {
        .name = "ws-raw",                              /**< Protocol identifier */
        .callback = cb_ws,                             /**< Event callback */
        .per_session_data_size = sizeof(struct per_session_data), /**< Session storage */
        .rx_buffer_size = 16384,                       /**< RX buffer (16KB) */
        .user = NULL,                                  /**< Set during initialization */
    },
    { NULL, NULL, 0, 0 }  /* Sentinel */
};

/* ============================================================================
 * LIBUV INTEGRATION
 * ============================================================================
 */

/**
 * @brief libuv async callback for graceful shutdown.
 *
 * Called when ws_raw_stop() is invoked. Sets the running flag to 0
 * and stops the libuv event loop.
 *
 * @param handle uv_async_t handle containing server context
 */
static void stop_async_cb(uv_async_t* handle) {
    ws_raw_ctx_t *ctx = (ws_raw_ctx_t*)handle->data;
    if (ctx) {
        ctx->running = 0;
        uv_stop(ctx->loop);
    }
}

/* ============================================================================
 * PUBLIC API IMPLEMENTATION
 * ============================================================================
 */

/**
 * @brief Initialize the WebSocket server.
 *
 * Creates server context, initializes libwebsockets with libuv integration,
 * and prepares the event loop.
 *
 * @param cfg Server configuration (will be copied internally)
 */
void ws_raw_init(const ws_raw_cfg_t *cfg)
{
    loop = uv_default_loop();

    /* Allocate server context */
    ws_raw_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx){
        lwsl_user("Error starting WS: allocation failed\n");
        return;
    }

    /* Copy configuration (for thread safety) */
    ws_raw_cfg_t *cfg_copy = malloc(sizeof(ws_raw_cfg_t));
    if (!cfg_copy) {
        free(ctx);
        lwsl_user("Error starting WS: config allocation failed\n");
        return;
    }

    *cfg_copy = *cfg;
    
    /* Apply defaults */
    if (cfg_copy->max_clients <= 0) {
        cfg_copy->max_clients = MAX_CLIENTS_DEFAULT;
    }

    /* Initialize context */
    ctx->cfg = cfg_copy;
    ctx->loop = loop;
    ctx->clients = NULL;
    ctx->client_counter = 0;
    ctx->running = 1;
    
    pthread_mutex_init(&ctx->lock, NULL);

    /* Store context pointer in protocol for callback access */
    protocols[0].user = ctx;

    /* Configure minimal logging */
    lws_set_log_level(LLL_ERR | LLL_WARN, NULL);

    /* libwebsockets server configuration */
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = cfg->port;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_LIBUV;  /* Use libuv event loop */
    
    info.foreign_loops = (void **)&loop;     /* Pass libuv loop */
    info.gid = -1;                           /* Default group */
    info.uid = -1;                           /* Default user */

    /* Create libwebsockets context */
    ctx->context = lws_create_context(&info);
    if (!ctx->context) {
        lwsl_user("Error creating LWS context\n");
        free(cfg_copy);
        free(ctx);
        return;
    }

    /* Initialize async stop handler */
    ctx->stop_async.data = ctx;
    uv_async_init(loop, &ctx->stop_async, stop_async_cb);

    /* Set global reference */
    ws = ctx;
}

/**
 * @brief Start the WebSocket server event loop.
 *
 * Begins processing WebSocket events using libuv's event loop.
 * This function blocks until ws_raw_stop() is called.
 */
void ws_raw_run(){
    if (!ws || !ws->loop) {
        lwsl_user("Cannot run: context not initialized\n");
        return;
    }
    
    /* Run libuv event loop (blocks until stopped) */
    uv_run(loop, UV_RUN_DEFAULT);
}

/**
 * @brief Signal the server to stop gracefully.
 *
 * Sends an async signal to the event loop to initiate shutdown.
 * Returns immediately; actual shutdown occurs in the event loop.
 */
void ws_raw_stop(){
    if (!ws || !ws->loop) {
        lwsl_user("Cannot stop: context not initialized\n");
        return;
    }

    uv_async_send(&ws->stop_async);
}

/**
 * @brief Send data to a specific client.
 *
 * Queues data for transmission to a single client. Uses static buffer
 * for small messages (< STATIC_BUFFER_SIZE) or allocates dynamic buffer
 * for larger messages.
 *
 * @param ctx    Server context
 * @param client Target client
 * @param data   Data to send
 * @param len    Data length in bytes
 * @return 0 on success, -1 on error
 */
int ws_raw_send_to(ws_raw_ctx_t *ctx, ws_raw_client_t *client, 
                   const void *data, size_t len)
{
    if (!ctx || !client || !data || !len)
        return -1;

    pthread_mutex_lock(&ctx->lock);

    /* Check if client already has pending transmission */
    if (client->tx_pending) {
        pthread_mutex_unlock(&ctx->lock);
        return -1;
    }

    /* Select appropriate buffer based on message size */
    if (len <= sizeof(client->tx_buf_static) - LWS_PRE) {
        /* Use static buffer (no allocation) */
        client->using_static = 1;
        memcpy(client->tx_buf_static + LWS_PRE, data, len);
        client->tx_len = len;
    } else {
        /* Allocate dynamic buffer */
        client->using_static = 0;
        client->tx_buf_dynamic = malloc(LWS_PRE + len);
        if (!client->tx_buf_dynamic) {
            pthread_mutex_unlock(&ctx->lock);
            return -1;
        }
        memcpy(client->tx_buf_dynamic + LWS_PRE, data, len);
        client->tx_len = len;
    }

    /* Mark as pending and request write callback */
    client->tx_pending = 1;
    lws_callback_on_writable(client->wsi);

    pthread_mutex_unlock(&ctx->lock);
    return 0;
}

/**
 * @brief Broadcast data to all connected clients.
 *
 * Sends the same data to every connected client. Each client gets
 * its own buffer copy to allow concurrent transmission.
 *
 * @param ctx  Server context
 * @param data Data to broadcast
 * @param len  Data length in bytes
 * @return Always returns 0
 */
int ws_raw_broadcast(ws_raw_ctx_t *ctx, const void *data, size_t len)
{
    if (!ctx || !data || !len)
        return 0;

    pthread_mutex_lock(&ctx->lock);

    ws_raw_client_t *client = ctx->clients;
    while (client) {
        if (!client->tx_pending) {
            /* Try static buffer first */
            if (len <= sizeof(client->tx_buf_static) - LWS_PRE) {
                client->using_static = 1;
                memcpy(client->tx_buf_static + LWS_PRE, data, len);
                client->tx_len = len;
                client->tx_pending = 1;
                lws_callback_on_writable(client->wsi);
            } 
            /* Fall back to dynamic buffer if available */
            else if (!client->tx_buf_dynamic) {
                client->using_static = 0;
                client->tx_buf_dynamic = malloc(LWS_PRE + len);
                if (client->tx_buf_dynamic) {
                    memcpy(client->tx_buf_dynamic + LWS_PRE, data, len);
                    client->tx_len = len;
                    client->tx_pending = 1;
                    lws_callback_on_writable(client->wsi);
                }
            }
        }
        client = client->next;
    }

    pthread_mutex_unlock(&ctx->lock);
    return 0;
}

/**
 * @brief Broadcast data to all clients except one.
 *
 * Similar to ws_raw_broadcast() but excludes a specific client.
 * Useful for echo servers or chat applications.
 *
 * @param ctx            Server context
 * @param exclude_client Client to exclude from broadcast
 * @param data           Data to broadcast
 * @param len            Data length in bytes
 * @return Number of clients scheduled for transmission, or -1 on error
 */
int ws_raw_broadcast_except(ws_raw_ctx_t *ctx, ws_raw_client_t *exclude_client, 
                            const void *data, size_t len)
{
    if (!ctx || !data || len == 0)
        return -1;

    pthread_mutex_lock(&ctx->lock);

    ws_raw_client_t *client = ctx->clients;
    int scheduled = 0;

    while (client) {
        if (client != exclude_client && !client->tx_pending) {
            if (len <= sizeof(client->tx_buf_static) - LWS_PRE) {
                client->using_static = 1;
                memcpy(client->tx_buf_static + LWS_PRE, data, len);
                client->tx_len = len;
                client->tx_pending = 1;
                lws_callback_on_writable(client->wsi);
                scheduled++;
            } else if (!client->tx_buf_dynamic) {
                client->using_static = 0;
                client->tx_buf_dynamic = malloc(LWS_PRE + len);
                if (client->tx_buf_dynamic) {
                    memcpy(client->tx_buf_dynamic + LWS_PRE, data, len);
                    client->tx_len = len;
                    client->tx_pending = 1;
                    lws_callback_on_writable(client->wsi);
                    scheduled++;
                }
            }
        }
        client = client->next;
    }

    pthread_mutex_unlock(&ctx->lock);
    return scheduled;
}

/**
 * @brief Get current number of connected clients.
 *
 * Thread-safe count of active connections.
 *
 * @param ctx Server context
 * @return Number of connected clients
 */
int ws_raw_client_count(ws_raw_ctx_t *ctx)
{
    if (!ctx) return 0;
    
    pthread_mutex_lock(&ctx->lock);
    int count = 0;
    ws_raw_client_t *client = ctx->clients;
    while (client) {
        count++;
        client = client->next;
    }
    pthread_mutex_unlock(&ctx->lock);
    
    return count;
}

/**
 * @brief Get array of all connected clients.
 *
 * Allocates and returns an array of client pointers. Caller must free
 * the returned array when done.
 *
 * @param ctx   Server context
 * @param count Output parameter for array size
 * @return Array of client pointers, or NULL if empty
 */
ws_raw_client_t** ws_raw_get_clients(ws_raw_ctx_t *ctx, int *count)
{
    if (!ctx || !count) return NULL;
    
    pthread_mutex_lock(&ctx->lock);
    
    /* Count clients */
    int client_count = 0;
    ws_raw_client_t *client = ctx->clients;
    while (client) {
        client_count++;
        client = client->next;
    }
    
    if (client_count == 0) {
        pthread_mutex_unlock(&ctx->lock);
        *count = 0;
        return NULL;
    }
    
    /* Allocate client array */
    ws_raw_client_t **client_array = malloc(client_count * sizeof(ws_raw_client_t*));
    if (!client_array) {
        pthread_mutex_unlock(&ctx->lock);
        *count = 0;
        return NULL;
    }
    
    /* Fill array */
    int i = 0;
    client = ctx->clients;
    while (client && i < client_count) {
        client_array[i] = client;
        i++;
        client = client->next;
    }
    
    pthread_mutex_unlock(&ctx->lock);
    
    *count = client_count;
    return client_array;
}

/**
 * @brief Send data to first connected client (legacy compatibility).
 *
 * Simplified version that sends to the first client in the list.
 * For new code, use ws_raw_send_to() instead.
 *
 * @param ctx  Server context
 * @param data Data to send
 * @param len  Data length
 * @return 0 on success, -1 on error
 */
int ws_raw_send(ws_raw_ctx_t *ctx, const void *data, size_t len)
{
    if (!ctx) return -1;
    
    pthread_mutex_lock(&ctx->lock);
    
    if (!ctx->clients) {
        pthread_mutex_unlock(&ctx->lock);
        return -1;
    }
    
    ws_raw_client_t *client = ctx->clients;
    pthread_mutex_unlock(&ctx->lock);
    
    return ws_raw_send_to(ctx, client, data, len);
}

/* ============================================================================
 * CLIENT ACCESSOR FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Get client's unique identifier.
 *
 * @param client Client pointer
 * @return Client ID string (valid for lifetime of client)
 */
const char* ws_raw_client_get_id(ws_raw_client_t *client)
{
    if (!client) return NULL;
    return client->id;
}

/**
 * @brief Get client's connection timestamp.
 *
 * @param client Client pointer
 * @return Unix timestamp when client connected
 */
time_t ws_raw_client_get_connect_time(ws_raw_client_t *client)
{
    if (!client) return 0;
    return client->connect_time;
}

/**
 * @brief Set user-defined data pointer for client.
 *
 * Allows application to associate arbitrary data with a client.
 *
 * @param client   Client pointer
 * @param user_data User data pointer
 */
void ws_raw_client_set_user_data(ws_raw_client_t *client, void *user_data)
{
    if (!client) return;
    client->user_data = user_data;
}

/**
 * @brief Get user-defined data pointer from client.
 *
 * @param client Client pointer
 * @return User data pointer (or NULL if not set)
 */
void* ws_raw_client_get_user_data(ws_raw_client_t *client)
{
    if (!client) return NULL;
    return client->user_data;
}

/* ============================================================================
 * CLEANUP AND SHUTDOWN
 * ============================================================================
 */

/**
 * @brief Destroy server context and release all resources.
 *
 * Performs graceful shutdown:
 * 1. Stops the event loop if running
 * 2. Closes all client connections
 * 3. Frees all allocated memory
 * 4. Destroys libwebsockets context
 *
 * After calling this function, ws_raw_init() must be called again
 * to restart the server.
 */
void ws_raw_destroy()
{
    if (!ws)
        return;

    /* Stop server if still running */
    if (ws->running) {
        ws_raw_stop();
        usleep(100000);  /* Allow 100ms for graceful shutdown */
    }
    
    /* Clean up libuv handle */
    uv_close((uv_handle_t*)&ws->stop_async, NULL);
    
    /* Free all clients */
    pthread_mutex_lock(&ws->lock);
    ws_raw_client_t *client = ws->clients;
    while (client) {
        ws_raw_client_t *next = client->next;
        if (client->tx_buf_dynamic) {
            free(client->tx_buf_dynamic);
        }
        free(client);
        client = next;
    }
    ws->clients = NULL;
    pthread_mutex_unlock(&ws->lock);
    
    /* Destroy mutex */
    pthread_mutex_destroy(&ws->lock);
    
    /* Clean up libwebsockets */
    if (ws->context) {
        lws_context_destroy(ws->context);
    }
    
    /* Free configuration copy */
    if (ws->cfg) {
        free((void*)ws->cfg);
    }
    
    /* Free context */
    free(ws);
    ws = NULL;
}
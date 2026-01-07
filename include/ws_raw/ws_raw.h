#ifndef WS_RAW_H
#define WS_RAW_H

#include <libwebsockets.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <uv.h>

typedef struct ws_raw_ctx ws_raw_ctx_t;

/* Rx callback signature*/
typedef void (*ws_raw_rx_cb_t)(ws_raw_ctx_t *ctx, const void *data, size_t len);

typedef struct {
    int port;
    ws_raw_rx_cb_t on_rx;
} ws_raw_cfg_t;

/* API */
ws_raw_ctx_t *ws_raw_init(uv_loop_t *loop, const ws_raw_cfg_t *cfg);
int ws_raw_send(ws_raw_ctx_t *ctx, const void *data, size_t len);
void ws_raw_destroy(ws_raw_ctx_t *ctx);

#endif
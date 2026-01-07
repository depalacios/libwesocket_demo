#include "ws_raw.h"

struct ws_raw_ctx {
    struct lws_context *context;
    struct lws *client;
    const ws_raw_cfg_t *cfg;
    uv_loop_t *loop;
    // NOTE: We don't need uv_idle_t because LWS integrates automatically
};

// Structure for per-session data
struct per_session_data {
    ws_raw_ctx_t *global_ctx;
};

/* ================= WEBSOCKET CALLBACK ================= */
static int cb_ws(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    struct per_session_data *psd = (struct per_session_data *)user;
    
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            psd->global_ctx = (ws_raw_ctx_t *)lws_get_protocol(wsi)->user;
            if (psd->global_ctx) {
                psd->global_ctx->client = wsi;
                lwsl_user("WebSocket connection established\n");
            }
            break;

        case LWS_CALLBACK_RECEIVE:
            if (psd && psd->global_ctx && psd->global_ctx->cfg && psd->global_ctx->cfg->on_rx) {
                psd->global_ctx->cfg->on_rx(psd->global_ctx, in, len);
            }
            break;

        case LWS_CALLBACK_CLOSED:
            if (psd && psd->global_ctx && psd->global_ctx->client == wsi) {
                psd->global_ctx->client = NULL;
                lwsl_user("WebSocket connection closed\n");
            }
            break;

        default:
            break;
    }
    return 0;
}

/* ================= PROTOCOLS ================= */
static struct lws_protocols protocols[] = {
    {
        .name = "ws-raw",
        .callback = cb_ws,
        .per_session_data_size = sizeof(struct per_session_data),
        .rx_buffer_size = 4096,
        .user = NULL,
    },
    { NULL, NULL, 0, 0 }
};

/* ================= API ================= */
ws_raw_ctx_t *ws_raw_init(uv_loop_t *loop, const ws_raw_cfg_t *cfg)
{
    ws_raw_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        return NULL;

    ctx->cfg = cfg;
    ctx->loop = loop;
    ctx->client = NULL;

    // Set context in protocol
    protocols[0].user = ctx;

    lws_set_log_level(LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE, NULL);

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = cfg->port;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_LIBUV;
    
    // IMPORTANT: Pass libuv loop to LWS
    info.foreign_loops = (void **)&loop;
    
    info.gid = -1;
    info.uid = -1;

    ctx->context = lws_create_context(&info);
    if (!ctx->context) {
        lwsl_user("Error creating LWS context\n");
        free(ctx);
        return NULL;
    }

    lwsl_user("WS server started with libuv integration\n");
    return ctx;
}

/* ================= SEND ================= */
int ws_raw_send(ws_raw_ctx_t *ctx, const void *data, size_t len)
{
    if (!ctx || !ctx->client)
        return -1;

    unsigned char *buf = malloc(LWS_PRE + len);
    if (!buf)
        return -1;

    memcpy(&buf[LWS_PRE], data, len);
    
    int ret = lws_write(ctx->client, &buf[LWS_PRE], len, LWS_WRITE_TEXT);
    free(buf);
    
    return ret;
}

/* ================= DESTROY ================= */
void ws_raw_destroy(ws_raw_ctx_t *ctx)
{
    if (!ctx)
        return;

    if (ctx->context) {
        lws_context_destroy(ctx->context);
    }
    free(ctx);
}
#include <ws_raw.h>

static void on_ws_rx(ws_raw_ctx_t *ctx, const void *data, size_t len)
{
    lwsl_user("RX (%zu): %.*s\n", len, (int)len, (const char *)data);
    
    // Example: echo
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
        .port = 5000,
        .on_rx = on_ws_rx
    };

    ws_raw_ctx_t *ws = ws_raw_init(loop, &cfg);
    if (!ws) {
        lwsl_user("Error starting WS\n");
        return 1;
    }

    lwsl_user("RAW WS server started on port %d\n", cfg.port);
    lwsl_user("Press Ctrl+C to exit\n");

    // IMPORTANT: We don't call lws_service() manually
    // Libwebsockets integrates automatically with libuv's loop
    // thanks to LWS_SERVER_OPTION_LIBUV and foreign_loops
    uv_run(loop, UV_RUN_DEFAULT);

    // Cleanup
    ws_raw_destroy(ws);
    uv_loop_close(loop);
    
    return 0;
}
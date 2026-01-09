#include <ws_raw.h>

static void on_ws_rx(ws_raw_ctx_t *ctx, ws_raw_client_t *client, const void *data, size_t len)
{
    // Solo reenvía el mensaje sin procesamiento adicional
    ws_raw_send_to(ctx, client, data, len);
}

static void on_client_connect(ws_raw_ctx_t *ctx, ws_raw_client_t *client)
{
    // Opcional: sin logging para mejor rendimiento
    // const char *client_id = ws_raw_client_get_id(client);
    // printf("Nuevo cliente conectado: %s\n", client_id);
}

static void on_client_disconnect(ws_raw_ctx_t *ctx, ws_raw_client_t *client)
{
    // Opcional: sin logging para mejor rendimiento
    // const char *client_id = ws_raw_client_get_id(client);
    // printf("Cliente desconectado: %s\n", client_id);
}

int main(void)
{
    // Configurar servidor
    ws_raw_cfg_t cfg = {
        .port = 9000,
        .on_rx = on_ws_rx,
        .on_connect = on_client_connect,
        .on_disconnect = on_client_disconnect,
        .max_clients = 1000  // Aumentado para benchmark
    };

    // Inicializar servidor
    ws_raw_init(&cfg);

    printf("WebSocket Echo Server started on port %d\n", 9000);
    printf("Optimized for benchmarking\n");

    // Ejecutar servidor
    ws_raw_run();

    // Limpieza
    ws_raw_destroy();
    
    return 0;
}
#include <libwebsockets.h>

typedef enum {
    CHANNEL_UNKNOWN = 0,
    CHANNEL_DATA,
    CHANNEL_ECHO
} channel_type_t;

LWS_SS_USER_TYPEDEF
	char						payload[200];
	size_t						size;
	size_t						pos;
	// channel_type_t 				type;
} server_srv_t;

static lws_ss_state_return_t server_srv_rx(void *userobj, const uint8_t *buf, size_t len, int flags);
static lws_ss_state_return_t server_srv_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf, size_t *len, int *flags);
static lws_ss_state_return_t server_srv_state(void *userobj, void *sh, lws_ss_constate_t state, lws_ss_tx_ordinal_t ack);

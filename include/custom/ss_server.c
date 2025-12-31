#include "ss_server.h"

static lws_ss_state_return_t server_srv_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
	server_srv_t *g = (server_srv_t *)userobj;

    /* proteger overflow */
    if (len > sizeof(g->payload))
        return LWSSSSRET_DISCONNECT_ME;

    /* copiar RX → TX */
    memcpy(g->payload, buf, len);

    g->size = len;
    g->pos  = 0;

    /* pedir transmisión inmediata */
    return lws_ss_request_tx_len(lws_ss_from_user(g), (unsigned long)g->size);
}

static lws_ss_state_return_t server_srv_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf, size_t *len, int *flags)
{
	server_srv_t *g = (server_srv_t *)userobj;
	lws_ss_state_return_t r = LWSSSSRET_OK;

	if (g->size == g->pos)
		return LWSSSSRET_TX_DONT_SEND;

	if (*len > g->size - g->pos)
		*len = g->size - g->pos;

	if (!g->pos)
		*flags |= LWSSS_FLAG_SOM;

	memcpy(buf, g->payload + g->pos, *len);
	g->pos += *len;

	if (g->pos != g->size) /* more to do */
		r = lws_ss_request_tx(lws_ss_from_user(g));
	else
		*flags |= LWSSS_FLAG_EOM;

	lwsl_ss_user(lws_ss_from_user(g), "TX %zu, flags 0x%x, r %d", *len, (unsigned int)*flags, (int)r);

	return r;
}

static lws_ss_state_return_t server_srv_state(void *userobj, void *sh, lws_ss_constate_t state, lws_ss_tx_ordinal_t ack)
{
	server_srv_t *g = (server_srv_t *)userobj;
	struct lws_ss_handle *ss = lws_ss_from_user(g);

	switch (state) {
		case LWSSSCS_CREATING:
			/* NO pedir TX en server */
			break;

		case LWSSSCS_SERVER_TXN:
			/* solo aceptar la transacción */
			lws_ss_server_ack(ss, 0);
			break;

		case LWSSSCS_DESTROYING:
			break;

		default:
			break;
	}

	return LWSSSSRET_OK;
}

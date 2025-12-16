#include "ss_server.h"

static lws_ss_state_return_t server_srv_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
	server_srv_t *g = (server_srv_t *)userobj;  	
	char * json_res_str = NULL;
	cJSON * json_response;
	size_t len_to_send;

	char *json_request = (char *)malloc(len + 1);
    if (!json_request) return LWSSSSRET_DISCONNECT_ME;

    memcpy(json_request, buf, len);
    json_request[len] = '\0';

	enum commands received_command = sykoCommandsHandler(cJSON_Parse(json_request));

	free(json_request); 

	switch (received_command){
		case remotegui_device_info: 
			json_response = remotegui_device_info_fnc();			
			break;
		case remotegui_program_vehicle: 
			json_response = remotegui_program_vehicle_fnc();
			break;			
		case get_basic_config: 			
		case get_full_config:			
		case get_available_features: 		
		case remotegui_vehicle_info: 			
		case remotegui_read_dtc: 			
		case remotegui_clear_dtc: 			
		case remotegui_datalog: 			
		case remotegui_user_input:				
		default:
			json_response = unknown_command_fnc();			
			break;
	}	   

	json_res_str = cJSON_PrintUnformatted(json_response);
	len_to_send = strlen(json_res_str);

    memcpy(g->payload, json_res_str, len_to_send);
    g->size = len_to_send;
    g->pos = 0;

	lwsl_user("%d\n", len_to_send);

	free(json_res_str);
    cJSON_Delete(json_response);
   
    return lws_ss_request_tx_len(lws_ss_from_user(g), g->size);
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

	lwsl_user("%s: %p %s, ord 0x%x\n", __func__, g->ss, lws_ss_state_name(state), (unsigned int)ack);

	switch ((int)state) {
		case LWSSSCS_CREATING:
			return lws_ss_request_tx(lws_ss_from_user(g));

		case LWSSSCS_SERVER_TXN:
			/*
			* A transaction is starting on an accepted connection.  Say
			* that we're OK with the transaction, prepare the user
			* object with the response, and request tx to start sending it.
			*/
			lws_ss_server_ack(lws_ss_from_user(g), 0);

			if (lws_ss_set_metadata(lws_ss_from_user(g), "mime", "text/html", 9))
				return LWSSSSRET_DISCONNECT_ME;

			g->size	= (size_t)lws_snprintf(g->payload, sizeof(g->payload), "Hello World: %lu", (unsigned long)lws_now_usecs());
			g->pos = 0;

			return lws_ss_request_tx_len(lws_ss_from_user(g), (unsigned long)g->size);
	}

	return LWSSSSRET_OK;
}

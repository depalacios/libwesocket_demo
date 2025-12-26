#include <libwebsockets.h>
#include <signal.h>
#include <syko_handler.h>
#include <uv.h>
#include <stdio.h>
#include <unistd.h>

#include "include/websockets/lws-uv.h"

extern const lws_ss_info_t ssi_server_srv_t;

static struct lws_context *cx;
static volatile int interrupted = 0;

static int smd_cb(void *opaque, lws_smd_class_t c, lws_usec_t ts, void *buf, size_t len)
{
	if (!(c & LWSSMDCL_SYSTEM_STATE) ||
	    lws_json_simple_strcmp(buf, len, "\"state\":", "OPERATIONAL") ||
		!lws_ss_create(cx, 0, &ssi_server_srv_t, NULL, NULL, NULL, NULL)) 
		return 0;

	lwsl_err("%s: failed to create secure stream\n", __func__);
	lws_default_loop_exit(cx);

	return -1;
}

static void sigint_handler(int sig)
{
	lws_default_loop_exit(cx);
	interrupted = 1;
}

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;	
	uv_loop_t* uv_loop = NULL;	
	int use_libuv = 0;
	
	lws_context_info_defaults(&info, "policy.json");
	lws_cmdline_option_handle_builtin(argc, argv, &info);	
	signal(SIGINT, sigint_handler);

	if(initCanBus()){
		lwsl_user("Socket init fail.\n");
		return 1;
	}
	
	lwsl_user("LWS Secure Streams Server\n");

	info.early_smd_cb		= smd_cb;
	info.early_smd_class_filter	= LWSSMDCL_SYSTEM_STATE;

	// FORZAR uso de libuv si está disponible
	info.options |= LWS_SERVER_OPTION_LIBUV;

	cx = lws_create_context(&info); 

	if (!cx) {
		lwsl_err("LWS init failed\n");
		return 1;
	}

	// Intentar obtener loop de libuv
	uv_loop = lws_uv_getloop(cx, 0);
	
	if (uv_loop) {
		use_libuv = 1;
		lwsl_user("Using libuv event loop\n");
	} else {
		lwsl_user("libuv loop not available, using default event loop\n");
		// NO usar lws_default_loop_exit si no estamos usando libuv
		// Reescribir el handler para no llamar a lws_default_loop_exit
		signal(SIGINT, (void (*)(int))&interrupted);
	}

	int counter = 0;
	while (!interrupted) {
		if (use_libuv && uv_loop) {
			// Usar libuv para el event loop
			uv_run(uv_loop, UV_RUN_NOWAIT);
		}
		
		// Servir WebSockets
		lws_service(cx, 0);
		
		// Mostrar actividad
		if (counter++ % 500 == 0) {
			printf(".");
			fflush(stdout);
		}
		
		// Pausa pequeña
		usleep(1000);
	}

	lws_context_destroy(cx);
	lwsl_user("Server stopped\n");

	return 0;
}
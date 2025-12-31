#include <libwebsockets.h>
#include <ev.h>

extern const lws_ss_info_t ssi_server_srv_t; // Check /include/custom/ss_server.h
static struct lws_context *cx;
struct ev_loop *loop;	

static ev_signal sigint_watcher;
static ev_signal sigterm_watcher;
static ev_timer lws_timer;

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

static void signal_cb(EV_P_ ev_signal *w, int revents)
{
    lwsl_user("Signal %d, closing...\n", w->signum);
    ev_break(EV_A_ EVBREAK_ALL);
}

static void lws_ev_cb(EV_P_ ev_timer *w, int revents)
{
    lws_service(cx, 0);
}

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;	
	
	loop = ev_default_loop(0);

	ev_signal_init(&sigint_watcher, signal_cb, SIGINT);
    ev_signal_start(loop, &sigint_watcher);
    ev_signal_init(&sigterm_watcher, signal_cb, SIGTERM);
    ev_signal_start(loop, &sigterm_watcher);
	
	lws_context_info_defaults(&info, "policy.json");
	lws_cmdline_option_handle_builtin(argc, argv, &info);
	
	lwsl_user("LWS Secure Streams Server\n");

	info.early_smd_cb		= smd_cb;
	info.early_smd_class_filter	= LWSSMDCL_SYSTEM_STATE;

	cx = lws_create_context(&info); 
	if (!cx) {
		lwsl_err("LWS init failed\n");
		return 1;
	}

	ev_timer_init(&lws_timer, lws_ev_cb, 0.0, 0.005);
    ev_timer_start(loop, &lws_timer);

    ev_run(loop, 0);

    /* Cleanup ordenado */
    lws_context_destroy(cx);
    ev_loop_destroy(loop);

	return 0;
}

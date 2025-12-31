#include <libwebsockets.h>
#include <signal.h>
#include <glib.h>
#include <glib-unix.h>

static GMainLoop *glib_loop;

extern const lws_ss_info_t ssi_server_srv_t; // Check /include/custom/ss_server.h

static struct lws_context *cx;
int test_result = 0, multipart;

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

static gboolean glib_sigint_cb(gpointer user_data)
{
    lwsl_notice("SIGINT received, exiting GLib loop\n");

    if (glib_loop)
        g_main_loop_quit(glib_loop);

    return G_SOURCE_REMOVE;
}

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;		
	
	lws_context_info_defaults(&info, "policy.json");
	lws_cmdline_option_handle_builtin(argc, argv, &info);	

	lwsl_user("LWS Secure Streams Server\n");

	info.early_smd_cb		= smd_cb;
	info.early_smd_class_filter	= LWSSMDCL_SYSTEM_STATE;
	info.options |= LWS_SERVER_OPTION_GLIB;

	cx = lws_create_context(&info); 

	if (!cx) {
		lwsl_err("LWS init failed\n");
		return 1;
	}

	glib_loop = g_main_loop_new(NULL, FALSE);

    g_unix_signal_add(SIGINT,  glib_sigint_cb, NULL);
    g_unix_signal_add(SIGTERM, glib_sigint_cb, NULL);

    g_main_loop_run(glib_loop);

    g_main_loop_unref(glib_loop);
    lws_context_destroy(cx);

	return 0;
}

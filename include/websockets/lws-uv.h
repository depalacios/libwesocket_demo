#ifndef _LWS_UV_H
#define _LWS_UV_H

#include <libwebsockets.h>
#include <uv.h>

#ifdef __cplusplus
extern "C" {
#endif

LWS_VISIBLE LWS_EXTERN uv_loop_t * lws_uv_getloop(struct lws_context *context, int tsi);

LWS_VISIBLE LWS_EXTERN void lws_uv_sigint_cfg(struct lws_context *context, int tsi, int use_uv_sigint);

#ifdef __cplusplus
}
#endif

#endif /* _LWS_UV_H */
/*
 * Copyright (C) 5ky1s61ue
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    ngx_array_t  *forward;
    ngx_flag_t    request_body;
} ngx_http_forward_plugin_loc_conf_t;


typedef struct {
    ngx_int_t     status;
    ngx_chain_t   *sr_out_bufs;
    ngx_uint_t    block_flag;
    ngx_uint_t    sr_status;
} ngx_http_forward_plugin_ctx_t;


static ngx_int_t ngx_http_forward_plugin_handler(ngx_http_request_t *r);
static void      ngx_http_forward_plugin_body_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_forward_plugin_handler_internal(ngx_http_request_t *r);
static void      *ngx_http_forward_plugin_create_loc_conf(ngx_conf_t *cf);
static char      *ngx_http_forward_plugin_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static char      *ngx_http_forward_plugin(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_forward_plugin_init(ngx_conf_t *cf);
static ngx_int_t forward_plugin_subrequest_post_handler(ngx_http_request_t *r, void *data, ngx_int_t rc);
static void      forward_plugin_post_handler(ngx_http_request_t *r);


static ngx_command_t  ngx_http_forward_plugin_commands[] = {

    { ngx_string("forward_plugin"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_forward_plugin,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("forward_plugin_request_body"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_forward_plugin_loc_conf_t, request_body),
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_forward_plugin_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_forward_plugin_init,              /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_forward_plugin_create_loc_conf,   /* create location configuration */
    ngx_http_forward_plugin_merge_loc_conf     /* merge location configuration */
};


ngx_module_t  ngx_http_forward_plugin_module = {
    NGX_MODULE_V1,
    &ngx_http_forward_plugin_module_ctx,       /* module context */
    ngx_http_forward_plugin_commands,          /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_http_forward_plugin_handler(ngx_http_request_t *r)
{
    ngx_int_t                    rc;
    ngx_http_forward_plugin_ctx_t       *ctx;
    ngx_http_forward_plugin_loc_conf_t  *lcf;

    if (r != r->main) {
        return NGX_DECLINED;
    }

    lcf = ngx_http_get_module_loc_conf(r, ngx_http_forward_plugin_module);

    if (lcf->forward == NULL) {
        return NGX_DECLINED;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "forward_plugin handler");

    ctx = ngx_http_get_module_ctx(r, ngx_http_forward_plugin_module);

    if (ctx && ctx->sr_status) {
        return NGX_DECLINED;
    }

    if (lcf->request_body) {
        if (ctx) {
            return ctx->status;
        }

        ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_forward_plugin_ctx_t));
        if (ctx == NULL) {
            return NGX_ERROR;
        }

        ctx->status = NGX_DONE;

        ngx_http_set_ctx(r, ctx, ngx_http_forward_plugin_module);

        rc = ngx_http_read_client_request_body(r, ngx_http_forward_plugin_body_handler);
        if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            return rc;
        }

        ngx_http_finalize_request(r, NGX_DONE);
        return NGX_DONE;
    } else {
        ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_forward_plugin_ctx_t));
        if (ctx == NULL) {
            return NGX_ERROR;
        }
        
        ctx->status = NGX_DONE;
        
        ngx_http_set_ctx(r, ctx, ngx_http_forward_plugin_module);
        return ngx_http_forward_plugin_handler_internal(r);
    }
}


static void
ngx_http_forward_plugin_body_handler(ngx_http_request_t *r)
{
    ngx_http_forward_plugin_ctx_t  *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_forward_plugin_module);

    ctx->status = ngx_http_forward_plugin_handler_internal(r);

    r->preserve_body = 1;

    r->write_event_handler = ngx_http_core_run_phases;
    ngx_http_core_run_phases(r);
}


static ngx_int_t
ngx_http_forward_plugin_handler_internal(ngx_http_request_t *r)
{
    ngx_str_t                   *name;
    ngx_uint_t                   i;
    ngx_http_request_t          *sr;
    ngx_http_forward_plugin_loc_conf_t  *lcf;

    lcf = ngx_http_get_module_loc_conf(r, ngx_http_forward_plugin_module);

    name = lcf->forward->elts;
    ngx_http_forward_plugin_ctx_t *ctx = ngx_http_get_module_ctx(r, ngx_http_forward_plugin_module);

    for (i = 0; i < lcf->forward->nelts; i++) {
        ngx_http_post_subrequest_t *psr = ngx_palloc(r->pool, sizeof(ngx_http_post_subrequest_t));
        if (psr == NULL){
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        psr->handler=forward_plugin_subrequest_post_handler;
        psr->data = ctx;
        if (ngx_http_subrequest(r, &name[i], &r->args, &sr, psr, NGX_HTTP_SUBREQUEST_WAITED) != NGX_OK)
        {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        sr->header_only = 1;
        sr->method = r->method;
        sr->method_name = r->method_name;
    }
    return NGX_AGAIN;
}


static void *
ngx_http_forward_plugin_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_forward_plugin_loc_conf_t  *lcf;

    lcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_forward_plugin_loc_conf_t));
    if (lcf == NULL) {
        return NULL;
    }

    lcf->forward = NGX_CONF_UNSET_PTR;
    lcf->request_body = NGX_CONF_UNSET;

    return lcf;
}


static char *
ngx_http_forward_plugin_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_forward_plugin_loc_conf_t *prev = parent;
    ngx_http_forward_plugin_loc_conf_t *conf = child;

    ngx_conf_merge_ptr_value(conf->forward, prev->forward, NULL);
    ngx_conf_merge_value(conf->request_body, prev->request_body, 1);

    return NGX_CONF_OK;
}


static char *
ngx_http_forward_plugin(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_forward_plugin_loc_conf_t *lcf = conf;

    ngx_str_t  *value, *s;

    value = cf->args->elts;

    if (ngx_strcmp(value[1].data, "off") == 0) {
        if (lcf->forward != NGX_CONF_UNSET_PTR) {
            return "is duplicate";
        }

        lcf->forward = NULL;
        return NGX_CONF_OK;
    }

    if (lcf->forward == NULL) {
        return "is duplicate";
    }

    if (lcf->forward == NGX_CONF_UNSET_PTR) {
        lcf->forward = ngx_array_create(cf->pool, 4, sizeof(ngx_str_t));
        if (lcf->forward == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    s = ngx_array_push(lcf->forward);
    if (s == NULL) {
        return NGX_CONF_ERROR;
    }

    *s = value[1];

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_forward_plugin_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_PREACCESS_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_forward_plugin_handler;

    return NGX_OK;
}


static ngx_int_t forward_plugin_subrequest_post_handler(ngx_http_request_t *r, void *data, ngx_int_t rc){
    ngx_http_forward_plugin_ctx_t   *parent_ctx = data;
    parent_ctx->sr_status = r->headers_out.status;
    if (r->headers_out.status == 418) {
        parent_ctx->block_flag = 1;
        ngx_http_request_t *pr = r->parent;
        pr->headers_out = r->headers_out;
        pr->write_event_handler = forward_plugin_post_handler;

        ngx_buf_t *resp_buf = &r->upstream->buffer;
        ngx_uint_t body_len = resp_buf->last - resp_buf->pos;
        ngx_buf_t *temp_buf = ngx_create_temp_buf(pr->pool, body_len);
        ngx_memcpy(temp_buf->pos, resp_buf->pos, body_len);
        temp_buf->last = temp_buf->pos + body_len;
        temp_buf->last_buf = 1;
        parent_ctx->sr_out_bufs = ngx_alloc_chain_link(pr->pool);
        parent_ctx->sr_out_bufs->buf = temp_buf;
        parent_ctx->sr_out_bufs->next = NULL;
    }
    return NGX_OK;
}


static void forward_plugin_post_handler(ngx_http_request_t *r){
    ngx_http_forward_plugin_ctx_t *ctx = ngx_http_get_module_ctx(r, ngx_http_forward_plugin_module);
    if (ctx->block_flag) {
        r->connection->buffered |= NGX_HTTP_WRITE_BUFFERED;
        ngx_int_t ret = ngx_http_send_header(r);
        ret = ngx_http_output_filter(r, ctx->sr_out_bufs);
        ngx_http_finalize_request(r, ret);
    } else {
        ngx_http_finalize_request(r, r->headers_out.status);
    }
}

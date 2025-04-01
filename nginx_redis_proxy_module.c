#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <hiredis.h>

#define NGX_HTTP_HELLO_WORLD "Hello, World!\n"

static char *ngx_http_hello_world(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_hello_world_handler(ngx_http_request_t *r);

static ngx_command_t ngx_http_hello_world_commands[] = {
    { 
        ngx_string("hello_world"),
        NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
        ngx_http_hello_world,
        0,
        0,
        NULL
    },
    ngx_null_command
};


static ngx_http_module_t ngx_http_hello_world_module_ctx = {
    NULL,                              /* preconfiguration */
    NULL,                              /* postconfiguration */
    
    NULL,                              /* create main configuration */
    NULL,                              /* init main configuration */

    NULL,                              /* create server configuration */
    NULL,                              /* merge server configuration */

    NULL,                              /* create location configuration */
    NULL                               /* merge location configuration */
};

ngx_module_t ngx_http_hello_world_module = {
    NGX_MODULE_V1,
    &ngx_http_hello_world_module_ctx, /* module context */
    ngx_http_hello_world_commands,    /* module directives */
    NGX_HTTP_MODULE,                  /* module type */
    NULL,                             /* init master */
    NULL,                             /* init module */
    NULL,                             /* init process */
    NULL,                             /* init thread */
    NULL,                             /* exit thread */
    NULL,                             /* exit process */
    NULL,                             /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_int_t ngx_http_hello_world_handler(ngx_http_request_t *r)
{
    ngx_int_t                    rc;
    ngx_chain_t                  out;
    ngx_buf_t                   *b;
    ngx_str_t                    body = ngx_string(NGX_HTTP_HELLO_WORLD);

    redis_proxy_ctx_t *ctx;
    u_char *p;
    redisContext *c;
    redisReply *reply;

    ngx_str_t key, value;
    key.len = r->headers_in.host->value.len + r->uri.len;
    key.data = ngx_palloc(r->pool, key.len);
    ngx_snprintf(key.data, key.len, "%V%V", &r->headers_in.host->value, &r->uri);



    if (r->method != NGX_HTTP_GET && r->method != NGX_HTTP_HEAD) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    if (r->headers_in.if_modified_since) {
        return NGX_HTTP_NOT_MODIFIED;
    }

    b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    c = redisConnect("127.0.0.1", 6379);
    if (c->err) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Redis error: %s", c->errstr);
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    reply = redisCommand(c, "GET %s", key.data);
    if (!reply || reply->type == REDIS_REPLY_NIL) {
        ngx_http_finalize_request(r, NGX_HTTP_NOT_FOUND);
        return NGX_DECLINED;
    }

    value.data = ngx_string((u_char *)reply->str);
    value.len = ngx_strlen(value.data);


    b->pos      = value.data;
    b->last     = b->pos + value.len;
    b->memory   = 1;
    b->last_buf = 1;
    out.buf     = b;
    out.next    = NULL;

    ngx_str_set(&r->headers_out.content_type, "text/plain");
    r->headers_out.status            = NGX_HTTP_OK;
    r->headers_out.content_length_n  = value.len;

    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    return ngx_http_output_filter(r, &out);
}
 
static char *ngx_http_hello_world(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_hello_world_handler;

    return NGX_CONF_OK;
}

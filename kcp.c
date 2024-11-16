#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_kcp.h"
#include "ikcp.h"

static int le_kcp;

typedef struct {
    ikcpcb *kcp;
    zval output_callback;
} php_kcp_t;

ZEND_BEGIN_ARG_INFO_EX(arginfo_kcp_create, 0, 0, 2)
    ZEND_ARG_INFO(0, conv)
    ZEND_ARG_INFO(0, user)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_kcp_set_output_callback, 0, 0, 2)
    ZEND_ARG_INFO(0, kcp)
    ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_kcp_release, 0, 0, 1)
    ZEND_ARG_INFO(0, kcp)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_kcp_update, 0, 0, 2)
    ZEND_ARG_INFO(0, kcp)
    ZEND_ARG_INFO(0, current)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_kcp_input, 0, 0, 2)
    ZEND_ARG_INFO(0, kcp)
    ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_kcp_send, 0, 0, 2)
    ZEND_ARG_INFO(0, kcp)
    ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_kcp_flush, 0, 0, 1)
    ZEND_ARG_INFO(0, kcp)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_kcp_recv, 0, 0, 2)
    ZEND_ARG_INFO(0, kcp)
    ZEND_ARG_INFO(0, maxsize)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_kcp_nodelay, 0, 0, 5)
    ZEND_ARG_INFO(0, kcp)
    ZEND_ARG_INFO(0, nodelay)
    ZEND_ARG_INFO(0, interval)
    ZEND_ARG_INFO(0, resend)
    ZEND_ARG_INFO(0, nc)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_kcp_wndsize, 0, 0, 3)
    ZEND_ARG_INFO(0, kcp)
    ZEND_ARG_INFO(0, sndwnd)
    ZEND_ARG_INFO(0, rcvwnd)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_kcp_setmtu, 0, 0, 2)
    ZEND_ARG_INFO(0, kcp)
    ZEND_ARG_INFO(0, mtu)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_kcp_set_rx_minrto, 0, 0, 2)
    ZEND_ARG_INFO(0, kcp)
    ZEND_ARG_INFO(0, rx_minrto)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_kcp_waitsnd, 0, 0, 1)
    ZEND_ARG_INFO(0, kcp)
ZEND_END_ARG_INFO()

PHP_FUNCTION(kcp_create)
{
    long conv;
    zval *zuser;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "lz", &conv, &zuser) == FAILURE) {
        RETURN_FALSE;
    }

    php_kcp_t *kcp_res;
    kcp_res = emalloc(sizeof(php_kcp_t));
    kcp_res->kcp = ikcp_create(conv, Z_RES_P(zuser));
    
    zend_resource *res = zend_register_resource(kcp_res, le_kcp);
    RETURN_RES(res);
}
// udp_output function to be called by KCP
int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    php_printf("udp_output888: %s\n", buf);

    // Retrieve the custom data structure
    php_kcp_t *kcp_res = (php_kcp_t *)user;

    if (kcp_res == NULL || &kcp_res->output_callback == NULL) {
        return -1; // Error if callback is not set
    }

    // Prepare arguments to call the PHP callback
    zval params[2];
    ZVAL_STRINGL(&params[0], buf, len); // Convert the buffer to a PHP string
    ZVAL_LONG(&params[1], len);         // Length as a second argument

    // Call the PHP callback
    zval retval;
    if (call_user_function(EG(function_table), NULL, &kcp_res->output_callback, &retval, 2, params) == FAILURE) {
        php_error_docref(NULL, E_WARNING, "Failed to call output callback");
        return -1;
    }

    php_printf("udp_output11: %s\n", buf);



    // Clean up
    zval_ptr_dtor(&params[0]);
    zval_ptr_dtor(&retval);

    php_printf("udp_output222: %s\n", buf);

    return len; // Return the length of the data sent
}
PHP_FUNCTION(kcp_set_output_callback)
{
    zval *zkcp;
    zval *callback;

    // Parse parameters: zkcp is the KCP resource, callback is the PHP function
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rz", &zkcp, &callback) == FAILURE) {
        RETURN_FALSE;
    }

    // Fetch the KCP resource
    php_kcp_t *kcp_res = (php_kcp_t *)zend_fetch_resource(Z_RES_P(zkcp), "KCP Resource", le_kcp);
    if (!kcp_res) {
        RETURN_FALSE;
    }

    // Check if the callback is callable
    if (!zend_is_callable(callback, 0, NULL)) {
        php_error_docref(NULL, E_WARNING, "Invalid callback provided. Must be a callable.");
        RETURN_FALSE;
    }

    // Release the previous callback if it exists
    if (&kcp_res->output_callback) {
        zval_ptr_dtor(&kcp_res->output_callback);
        // kcp_res->output_callback = NULL;
    }

    // Store the new callback

    ZVAL_COPY(&kcp_res->output_callback, callback);

    // Set the udp_output function as the output callback for KCP
    kcp_res->kcp->output = udp_output;
    kcp_res->kcp->user = kcp_res; // Set the user pointer to our custom structure

    RETURN_TRUE;
}


PHP_FUNCTION(kcp_release)
{
    zval *zkcp;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &zkcp) == FAILURE) {
        RETURN_FALSE;
    }

    php_kcp_t *kcp_res = (php_kcp_t *)zend_fetch_resource(Z_RES_P(zkcp), "KCP Resource", le_kcp);
    if (!kcp_res) {
        RETURN_FALSE;
    }

    ikcp_release(kcp_res->kcp);
    zend_list_close(Z_RES_P(zkcp));
    RETURN_TRUE;
}

PHP_FUNCTION(kcp_update)
{
    zval *zkcp;
    long current;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rl", &zkcp, &current) == FAILURE) {
        RETURN_FALSE;
    }

    php_kcp_t *kcp_res = (php_kcp_t *)zend_fetch_resource(Z_RES_P(zkcp), "KCP Resource", le_kcp);
    if (!kcp_res) {
        RETURN_FALSE;
    }

    ikcp_update(kcp_res->kcp, current);
    RETURN_TRUE;
}

PHP_FUNCTION(kcp_input)
{
    zval *zkcp;
    char *data;
    size_t data_len;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs", &zkcp, &data, &data_len) == FAILURE) {
        RETURN_FALSE;
    }

    php_kcp_t *kcp_res = (php_kcp_t *)zend_fetch_resource(Z_RES_P(zkcp), "KCP Resource", le_kcp);
    if (!kcp_res) {
        RETURN_FALSE;
    }

    ikcp_input(kcp_res->kcp, data, data_len);
    RETURN_TRUE;
}

PHP_FUNCTION(kcp_send)
{
    zval *zkcp;
    char *data;
    size_t data_len;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs", &zkcp, &data, &data_len) == FAILURE) {
        RETURN_FALSE;
    }

    php_kcp_t *kcp_res = (php_kcp_t *)zend_fetch_resource(Z_RES_P(zkcp), "KCP Resource", le_kcp);
    if (!kcp_res) {
        RETURN_FALSE;
    }

    int ret = ikcp_send(kcp_res->kcp, data, data_len);
    RETURN_LONG(ret);
}

PHP_FUNCTION(kcp_flush)
{
    zval *zkcp;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &zkcp) == FAILURE) {
        RETURN_FALSE;
    }

    php_kcp_t *kcp_res = (php_kcp_t *)zend_fetch_resource(Z_RES_P(zkcp), "KCP Resource", le_kcp);
    if (!kcp_res) {
        RETURN_FALSE;
    }

    ikcp_flush(kcp_res->kcp);
    RETURN_TRUE;
}

PHP_FUNCTION(kcp_recv)
{
    zval *zkcp;
    long maxsize;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rl", &zkcp, &maxsize) == FAILURE) {
        RETURN_FALSE;
    }

    php_kcp_t *kcp_res = (php_kcp_t *)zend_fetch_resource(Z_RES_P(zkcp), "KCP Resource", le_kcp);
    if (!kcp_res) {
        RETURN_FALSE;
    }

    char *buffer = emalloc(maxsize);
    int ret = ikcp_recv(kcp_res->kcp, buffer, maxsize);
    if (ret < 0) {
        efree(buffer);
        RETURN_FALSE;
    }

    RETVAL_STRINGL(buffer, ret);
    efree(buffer);
}

PHP_FUNCTION(kcp_nodelay)
{
    zval *zkcp;
    long nodelay;
    long interval;
    long resend;
    long nc;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rllll", &zkcp, &nodelay, &interval, &resend, &nc) == FAILURE) {
        RETURN_FALSE;
    }

    php_kcp_t *kcp_res = (php_kcp_t *)zend_fetch_resource(Z_RES_P(zkcp), "KCP Resource", le_kcp);
    if (!kcp_res) {
        RETURN_FALSE;
    }

    int ret = ikcp_nodelay(kcp_res->kcp, nodelay, interval, resend, nc);
    RETURN_LONG(ret);
}

PHP_FUNCTION(kcp_wndsize)
{
    zval *zkcp;
    long sndwnd;
    long rcvwnd;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rll", &zkcp, &sndwnd, &rcvwnd) == FAILURE) {
        RETURN_FALSE;
    }

    php_kcp_t *kcp_res = (php_kcp_t *)zend_fetch_resource(Z_RES_P(zkcp), "KCP Resource", le_kcp);
    if (!kcp_res) {
        RETURN_FALSE;
    }

    int ret = ikcp_wndsize(kcp_res->kcp, sndwnd, rcvwnd);
    RETURN_LONG(ret);
}

PHP_FUNCTION(kcp_setmtu)
{
    zval *zkcp;
    long mtu;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rl", &zkcp, &mtu) == FAILURE) {
        RETURN_FALSE;
    }

    php_kcp_t *kcp_res = (php_kcp_t *)zend_fetch_resource(Z_RES_P(zkcp), "KCP Resource", le_kcp);
    if (!kcp_res) {
        RETURN_FALSE;
    }

    int ret = ikcp_setmtu(kcp_res->kcp, mtu);
    RETURN_LONG(ret);
}

PHP_FUNCTION(kcp_set_rx_minrto)
{
    zval *zkcp;
    long rx_minrto;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rl", &zkcp, &rx_minrto) == FAILURE) {
        RETURN_FALSE;
    }

    php_kcp_t *kcp_res = (php_kcp_t *)zend_fetch_resource(Z_RES_P(zkcp), "KCP Resource", le_kcp);
    if (!kcp_res) {
        RETURN_FALSE;
    }

    kcp_res->kcp->rx_minrto = rx_minrto;
    RETURN_TRUE;
}

PHP_FUNCTION(kcp_waitsnd)
{
    zval *zkcp;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &zkcp) == FAILURE) {
        RETURN_FALSE;
    }

    php_kcp_t *kcp_res = (php_kcp_t *)zend_fetch_resource(Z_RES_P(zkcp), "KCP Resource", le_kcp);
    if (!kcp_res) {
        RETURN_FALSE;
    }

    int ret = ikcp_waitsnd(kcp_res->kcp);
    RETURN_LONG(ret);
}


const zend_function_entry kcp_functions[] = {
    PHP_FE(kcp_create, arginfo_kcp_create)
    PHP_FE(kcp_set_output_callback, arginfo_kcp_set_output_callback)
    PHP_FE(kcp_release, arginfo_kcp_release)
    PHP_FE(kcp_update, arginfo_kcp_update)
    PHP_FE(kcp_input, arginfo_kcp_input)
    PHP_FE(kcp_send, arginfo_kcp_send)
    PHP_FE(kcp_flush, arginfo_kcp_flush)
    PHP_FE(kcp_recv, arginfo_kcp_recv)
    PHP_FE(kcp_nodelay, arginfo_kcp_nodelay)
    PHP_FE(kcp_wndsize, arginfo_kcp_wndsize)
    PHP_FE(kcp_setmtu, arginfo_kcp_setmtu)
    PHP_FE(kcp_set_rx_minrto, arginfo_kcp_set_rx_minrto)
    PHP_FE(kcp_waitsnd, arginfo_kcp_waitsnd)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(kcp)
{
    le_kcp = zend_register_list_destructors_ex(NULL, NULL, "KCP Resource", module_number);
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(kcp)
{
    return SUCCESS;
}

PHP_RINIT_FUNCTION(kcp)
{
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(kcp)
{
    return SUCCESS;
}

PHP_MINFO_FUNCTION(kcp)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "kcp support", "enabled");
    php_info_print_table_end();
}

zend_module_entry kcp_module_entry = {
    STANDARD_MODULE_HEADER,
    "kcp",
    kcp_functions,
    PHP_MINIT(kcp),
    PHP_MSHUTDOWN(kcp),
    PHP_RINIT(kcp),
    PHP_RSHUTDOWN(kcp),
    PHP_MINFO(kcp),
    PHP_KCP_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_KCP
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(kcp)
#endif
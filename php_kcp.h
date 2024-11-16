#ifndef PHP_KCP_H
#define PHP_KCP_H

extern zend_module_entry kcp_module_entry;
#define phpext_kcp_ptr &kcp_module_entry

#define PHP_KCP_VERSION "0.1.0"

#ifdef PHP_WIN32
#	define PHP_KCP_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_KCP_API __attribute__ ((visibility("default")))
#else
#	define PHP_KCP_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

// PHP_MINIT_FUNCTION(kcp);
// PHP_MSHUTDOWN_FUNCTION(kcp);
// PHP_RINIT_FUNCTION(kcp);
// PHP_RSHUTDOWN_FUNCTION(kcp);
// PHP_MINFO_FUNCTION(kcp);

// PHP_FUNCTION(kcp_create);
// PHP_FUNCTION(kcp_release);
// PHP_FUNCTION(kcp_update);
// PHP_FUNCTION(kcp_input);
// PHP_FUNCTION(kcp_send);
// PHP_FUNCTION(kcp_recv);

#endif	/* PHP_KCP_H */
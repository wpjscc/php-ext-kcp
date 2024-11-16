PHP_ARG_ENABLE(kcp, whether to enable kcp support,
[  --enable-kcp           Enable kcp support])

if test "$PHP_KCP" != "no"; then
  PHP_NEW_EXTENSION(kcp, kcp.c ikcp.c, $ext_shared)
fi
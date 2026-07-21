dnl config.m4 for extension rasplab (Lab 2)
dnl 已寫好，不需修改。

PHP_ARG_ENABLE([rasplab],
  [whether to enable rasplab support],
  [AS_HELP_STRING([--enable-rasplab], [Enable rasplab])],
  [no])

if test "$PHP_RASPLAB" != "no"; then
  AC_DEFINE(HAVE_RASPLAB, 1, [ rasplab enabled ])
  PHP_NEW_EXTENSION(rasplab, rasplab.c, $ext_shared)
fi

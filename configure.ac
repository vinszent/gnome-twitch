AC_INIT(GNOME Twitch, 0.0.1, ippytraxx@installgentoo.org, gnome-twitch, https://github.com/Ippytraxx/gnome-twitch)

AM_INIT_AUTOMAKE(-Wall -Wno-portability foreign)

AC_PROG_CC

IT_PROG_INTLTOOL(0.50.0)
AC_SUBST(LOCALE_DIR, $prefix/share/locale)
AC_SUBST(GETTEXT_PACKAGE, AC_PACKAGE_TARNAME)

AC_PATH_PROG(GLIB_COMPILE_RESOURCES, glib-compile-resources)

PKG_CHECK_MODULES(APP, gtk+-3.0 >= 3.16 libsoup-2.4 json-glib-1.0 gstreamer-1.0 gstreamer-video-1.0)

git_ver=`./print_git_version.sh`
if test x"$VCS_TAG" = x; then
    AC_SUBST(VCS_TAG, $git_ver)
else
    AC_SUBST(VCS_TAG, $version)
fi

AC_CONFIG_FILES(Makefile
                src/Makefile
                po/Makefile.in
                data/Makefile
                src/config.h
                src/version.h)

AC_OUTPUT

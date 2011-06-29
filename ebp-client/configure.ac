AC_PREREQ(2.61)

# package version number (not shared library version)
# odd micro numbers indicate in-progress development
# even micro numbers indicate released versions
m4_define(ebp_version_major, 0)
m4_define(ebp_version_minor, 1)
m4_define(ebp_version_micro, 0)

m4_define([ebp_version],
          [ebp_version_major.ebp_version_minor.ebp_version_micro])
m4_define([ebp_api_version],
          [ebp_version_major.ebp_version_minor])

AC_INIT(ebp, ebp_version)


AC_SUBST(LDFLAGS)


AC_SUBST(LIBS)

AC_SUBST(LDFLAGS)


AC_SUBST(CFLAGS)


AC_SUBST(CXXFLAGS)

AC_SUBST(includedir)

AM_INIT_AUTOMAKE(ebp, ebp_version)
AC_CONFIG_HEADERS(config.h)
#AC_CONFIG_SRCDIR(libebp/ebp.cpp)

# Check for programs
AC_PROG_LIBTOOL
AC_PROG_CC
AC_PROG_INSTALL

# enable pkg-config
PKG_PROG_PKG_CONFIG

# Check for function availability
# AC_CHECK_FUNCS([memset])

# all libraries listed are associated with "EBP" tag
PKG_CHECK_MODULES(EBP,[mx-1.0 glib-2.0 gthread-2.0])

AC_SUBST(EBP_CFLAGS)
AC_SUBST(EBP_LIBS)



# i18n
ALL_LINGUAS="en_US"
AC_SUBST(ALL_LINGUAS)
AM_GLIB_GNU_GETTEXT
AC_DEFINE(GETTEXT_PACKAGE, ["ebp"], [Package name for gettext])


desktopentrydir=${datadir}/applications
AC_SUBST(desktopentrydir)

icondir=${datadir}/icons/hicolor/32x32/apps
AC_SUBST(icondir)

# Output files
AC_OUTPUT([Makefile
src/Makefile
src/ebp.desktop
])

echo ""
echo "   ebp $VERSION"
echo "   ====================="
echo "   Documentation: ${enable_gtk_doc}"
echo ""
echo "   To build the project, run \"make\""
echo ""


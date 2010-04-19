
bin_PROGRAMS = ebp
ebp_SOURCES = main.c

desktopentry_DATA = ebp.desktop
icon_DATA = ebp32.png

#see gcc man page for description: http://www.google.com/search?q=man+gcc
AM_CFLAGS = @EBP_CFLAGS@ -Wall -Werror -g3 -O0 -ggdb -DPKGDATADIR="\"$(pkgdatadir)\""
AM_LDFLAGS = @EBP_LIBS@ 


#Example for copying directory to project data directory on "make install"
#install-data-hook:
#	$(MKDIR_P) ${pkgdatadir}
#	cp -R <dir>/* ${pkgdatadir}

EXTRA_DIST = ebp.conf

CLEANFILES = *~ ebp.desktop


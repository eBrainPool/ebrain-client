## Process this file with automake to produce Makefile.in

## Created by Anjuta



AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	$(EBP_CFLAGS)

AM_CFLAGS =\
	 -Wall\
	 -g

bin_PROGRAMS = ebp

ebp_SOURCES = \
	main.c \
	avahi-discovery.c \
	utils.c

ebp_LDFLAGS = \
	-Wl,--export-dynamic

ebp_LDADD = $(EBP_LIBS)


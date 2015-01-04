AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4

# libebpcomm is g++ need to force libtool to link with g++
CXXLINK = $(LIBTOOL) --tag=CXX --mode=link $(CXXLD) $(AM_CXXFLAGS) $(CXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@

THRIFTLIB_DIR = /home/jeetu/utils/thrift/thrift/lib/cpp/.libs/

EBPPACKAGE = `pkg-config --cflags --libs gtk+-3.0 glib-2.0 avahi-client gtk+-x11-3.0`


AM_CPPFLAGS = \
	-D_REENTRANT -pthread \
	-Debp_LOCALEDIR=$(prefix)/locale \
	$(EBPPACKAGE) 

AM_CXXFLAGS = @AM_CXXFLAGS@

#AM_CFLAGS =\
#	 -Wall\
#	 -g

bin_PROGRAMS = ebp

ebp_SOURCES = \
	main.c \
	avahi-discovery.c \
	utils.c 


ebp_LDFLAGS = -pthread -lavahi-common -lavahi-client -lgtk-3 -lgdk-3 -latk-1.0 -lgio-2.0 -lpangocairo-1.0 -lgdk_pixbuf-2.0 -lcairo-gobject -lpango-1.0 -lcairo -lgobject-2.0 -lgthread-2.0 -lglib-2.0 -lstdc++

ebp_LDADD = $(top_builddir)/src/thriftcomm/libebpcomm.a $(THRIFTLIB_DIR)libthrift.a $(AM_LDFLAGS)

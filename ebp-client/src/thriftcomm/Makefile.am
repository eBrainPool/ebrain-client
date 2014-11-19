AUTOMAKE_OPTIONS = subdir-objects
#ACLOCAL_AMFLAGS = -I m4

#THRIFT_DIR = 
    
AM_CPPFLAGS = -static -I$(top_srcdir)/thriftcomm -I$(THRIFT_DIR)lib/cpp/src/

AM_CXXFLAGS = -DHAVE_INTTYPES_H -DHAVE_NETINET_IN_H -I${THRIFT_DIR} -I$(includedir)/boost @AM_CXXFLAGS@

lib_LIBRARIES = libebpcomm.a

#libebpcomm_la_LIBADD = $(THRIFT_DIR)lib/cpp/libthrift.la

#libebpcomm_a_LIBADD = $(THRIFTLIB_DIR)libthrift.a

libebpcomm_a_SOURCES = \
		       announcelist_server.cpp CppClient.cpp gen-cpp/announcelist.cpp gen-cpp/announcelist.h gen-cpp/ebp_proto_constants.cpp gen-cpp/ebp_proto_contstants.h gen-cpp/ebp_proto_types.cpp gen-cpp/ebp_proto_types.h

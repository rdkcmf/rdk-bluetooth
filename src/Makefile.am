SUBDIRS = 

INCLUDE_FILES = $(DBUS_CFLAGS)

BTRCORE_IFCE_SRCS = bt-ifce/btrCore_dbus_bluez4.c
BTRCORE_IFCE_INC = -I${top_srcdir}/include/bt-ifce

AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/include $(BTRCORE_IFCE_INC)
lib_LTLIBRARIES = libbtrCore.la
libbtrCore_la_SOURCES = btrCore.c btrCore_avMedia.c $(BTRCORE_IFCE_SRCS)
libbtrCore_la_CFLAGS = $(INCLUDE_FILES) $(GLIB_CFLAGS)
libbtrCore_la_LIBADD = $(DBUS_LIBS) $(GLIB_LIBS)

# Explicitly declare the header file U want to expose out of libbtrCore.so her
include_HEADERS = $(top_srcdir)/include/btrCore.h
include_HEADERS += $(top_srcdir)/include/btrCoreTypes.h
include_HEADERS += $(top_srcdir)/include/btrCore_service.h
include_HEADERS += $(top_srcdir)/include/btrCore_avMedia.h

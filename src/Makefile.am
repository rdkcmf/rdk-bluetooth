SUBDIRS = 

INCLUDE_FILES = $(DBUS_CFLAGS)

AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/include
lib_LTLIBRARIES = libbtrCore.la
libbtrCore_la_SOURCES = btrCore.c btrCore_dbusBluez.c
libbtrCore_la_CFLAGS = $(INCLUDE_FILES)

# Explicitly declare the header file U want to expose out of libbtrCore.so her
include_HEADERS = $(top_srcdir)/include/btrCore.h
include_HEADERS += $(top_srcdir)/include/btrCore_service.h
include_HEADERS += $(top_srcdir)/include/btrCore_avMedia.h

SUBDIRS = 

INCLUDE_FILES = $(DBUS_CFLAGS)

AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/include
lib_LTLIBRARIES = libbtrCore.la
libbtrCore_la_SOURCES = btr_core.c
libbtrCore_la_CFLAGS = $(INCLUDE_FILES)

include_HEADERS = $(top_srcdir)/include/*.h

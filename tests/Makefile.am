SUBDIRS = 

INCLUDE_FILES = $(DBUS_CFLAGS)

AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/include
bin_PROGRAMS = btrCoreTest
btrCoreTest_SOURCES = btr_test.c
btrCoreTest_CFLAGS = $(INCLUDE_FILES)
btrCoreTest_LDADD = $(top_builddir)/src/libbtrCore.la $(DBUS_LIBS)

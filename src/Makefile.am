##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2016 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
SUBDIRS = 

INCLUDE_FILES = $(DBUS_CFLAGS)

if BTR_IFCE_DBUS_BLUEZ4
BTRCORE_IFCE_SRCS = bt-ifce/btrCore_dbus_bluez4.c
else !BTR_IFCE_DBUS_BLUEZ4
if BTR_IFCE_DBUS_BLUEZ5
BTRCORE_IFCE_SRCS = bt-ifce/btrCore_dbus_bluez5.c
endif
endif


BTRCORE_IFCE_INC = -I${top_srcdir}/include/bt-ifce

AM_CPPFLAGS = -Wall -Werror -Wno-error=maybe-uninitialized -I${top_srcdir}/include $(BTRCORE_IFCE_INC)
lib_LTLIBRARIES = libbtrCore.la
libbtrCore_la_SOURCES = btrCore.c btrCore_avMedia.c $(BTRCORE_IFCE_SRCS)
libbtrCore_la_CFLAGS = $(INCLUDE_FILES) $(GLIB_CFLAGS)
libbtrCore_la_LIBADD = $(DBUS_LIBS) $(GLIB_LIBS)

# Explicitly declare the header file U want to expose out of libbtrCore.so her
include_HEADERS = $(top_srcdir)/include/btrCore.h
include_HEADERS += $(top_srcdir)/include/btrCoreTypes.h
include_HEADERS += $(top_srcdir)/include/btrCore_service.h
include_HEADERS += $(top_srcdir)/include/btrCore_avMedia.h

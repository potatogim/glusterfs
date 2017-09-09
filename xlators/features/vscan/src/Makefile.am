xlator_LTLIBRARIES = vscan.la
xlatordir = $(libdir)/glusterfs/$(PACKAGE_VERSION)/xlator/features

vscan_la_LDFLAGS = -module $(GF_XLATOR_DEFAULT_LDFLAGS)

AM_CPPFLAGS = $(GF_CPPFLAGS) \
	-I$(top_srcdir)/libglusterfs/src \
	-I$(top_srcdir)/rpc/xdr/src/ \
	-I$(top_builddir)/rpc/xdr/src/ \
	-I$(top_srcdir)/rpc/rpc-lib/src \
	-I$(CONTRIBDIR)/timer-wheel \
	-I./stub

vscan_la_SOURCES = vscan.c stub/stub.c
vscan_la_LIBADD = $(top_builddir)/libglusterfs/src/libglusterfs.la \
	$(top_builddir)/xlators/features/changelog/lib/src/libgfchangelog.la

noinst_HEADERS = vscan.h vscan-mem-types.h vscan-messages.h

AM_CFLAGS = -Wall $(GF_CFLAGS)

CLEANFILES =


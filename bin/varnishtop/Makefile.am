#

AM_CPPFLAGS = -I$(top_srcdir)/include

bin_PROGRAMS = varnishtop

dist_man_MANS = varnishtop.1

varnishtop_SOURCES = varnishtop.c \
	$(top_builddir)/lib/libvarnish/vas.c \
	$(top_builddir)/lib/libvarnish/version.c

varnishtop_LDADD = \
	$(top_builddir)/lib/libvarnishcompat/libvarnishcompat.la \
	$(top_builddir)/lib/libvarnishapi/libvarnishapi.la \
	@CURSES_LIB@ ${RT_LIBS} ${LIBM} ${PTHREAD_LIBS}

varnishtop.1: $(top_srcdir)/doc/sphinx/reference/varnishtop.rst
if HAVE_RST2MAN
	${RST2MAN} $? $@
else
	@echo "========================================"
	@echo "You need rst2man installed to make dist"
	@echo "========================================"
	@false
endif

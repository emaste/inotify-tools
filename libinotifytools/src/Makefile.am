SUBDIRS = inotifytools

lib_LTLIBRARIES = libinotifytools.la
libinotifytools_la_SOURCES = inotifytools.c inotifytools_p.h redblack.c redblack.h stats.c stats.h
libinotifytools_la_CFLAGS = -I$(srcdir)/inotifytools
libinotifytools_la_LDFLAGS = -version-info 4:1:4

check_PROGRAMS = test
test_SOURCES = test.c
test_LDADD = libinotifytools.la

TESTS = test

EXTRA_DIST = example.c Doxyfile

nobase_include_HEADERS = inotifytools/inotifytools.h inotifytools/inotify-nosys.h inotifytools/inotify.h
nobase_include_HEADERS += inotifytools/fanotify-dfid-name.h inotifytools/fanotify.h

AM_CFLAGS = -std=c99 -Wall -Wextra -Wshadow -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Werror

if DOXYGEN_ENABLE

doc/html/*: inotifytools.c Doxyfile
	$(DOXYGEN)

shareddocdir = $(datadir)/doc
doc_DATA = doc/html/*

clean-local:
	rm -rf 'doc'

uninstall-hook:
	rm -rf '$(DESTDIR)/$(shareddocdir)/libinotifytools'

endif DOXYGEN_ENABLE

dist-hook:
	rm $(distdir)/inotifytools/inotify.h

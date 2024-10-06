SUBDIRS = lib src man
EXTRA_DIST = m4/gnulib-cache.m4 $(top_srcdir)/.version man
BUILT_SOURCES = $(top_srcdir)/.version

$(top_srcdir)/.version:
	echo '$(VERSION)' > $@-t
	mv $@-t $@

dist-hook:
	echo '$(VERSION)' > $(distdir)/.tarball-version
	$(top_srcdir)/build-aux/gitlog-to-changelog $(top_srcdir) > $(distdir)/ChangeLog


## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

if INTERNAL_LUA
  MAYBE_LUA = lua
endif

SUBDIRS = $(MAYBE_LUA) src

# man pages are not considered to be sources, so need to add "dist_"
# prefix to ensure they are added to the distribution.
dist_man_MANS = sox.1 soxexam.1 libst.3
dist_pkgdata_DATA = sox.txt soxexam.txt libst.txt

# Rule for making plain text man pages
%.txt: %.1
	tbl $(srcdir)/$< | nroff -man -Tascii | col -b > $@

# Rule for making PDF man pages
%.pdf: %.1
	tbl $(srcdir)/$< | groff -man -Tps | ps2pdf - $@

pdf: sox.pdf soxexam.pdf

# Automatically update libtool script if it gets out-of-date
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

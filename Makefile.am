## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

if INTERNAL_LUA
  MAYBE_LUA = lua
endif

SUBDIRS = $(MAYBE_LUA) src

# man pages are not considered to be sources, so need to add "dist_"
# prefix to ensure they are added to the distribution.
dist_man_MANS = sox.1 soxexam.7 soxlua.7 libst.3 play.1 rec.1
dist_pkgdata_DATA = sox.txt soxexam.txt soxlua.txt libst.txt

play.1 rec.1: sox.1
	$(RM) $@ && $(LN_S) $< $@

# Rule for making plain text man pages
%.txt: %.[1-9]
	tbl $(srcdir)/$< | nroff -man -Tascii | col -b > $@

# Rule for making PDF man pages
%.pdf: %.[1-9]
	tbl $(srcdir)/$< | groff -man -Tps | ps2pdf - $@

pdf: sox.pdf soxexam.pdf libst.pdf

install-exec-hook:
	cd $(DESTDIR)$(bindir) && $(RM) play.1 && $(LN_S) sox.1 play.1
	cd $(DESTDIR)$(bindir) && $(RM) rec.1 && $(LN_S) sox.1 rec.1

# Automatically update libtool script if it gets out-of-date
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

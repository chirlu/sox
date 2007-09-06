## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = lpc10 libgsm src
if HAVE_LIBLTDL
SUBDIRS += libltdl
endif
DIST_SUBDIRS = libltdl lpc10 libgsm src

RM = rm -f

# man pages are not considered to be sources, so need to add "dist_"
# prefix to ensure they are added to the distribution.
dist_man_MANS = sox.1 soxeffect.7 soxformat.7 soxexam.7 libsox.3
EXTRA_DIST = sox.txt soxeffect.txt soxformat.txt soxexam.txt libsox.txt

play.1 rec.1: sox.1
	$(RM) $@ && $(LN_S) $< $@

# Rule for making text man pages
.1.txt .3.txt .7.txt:
	tbl $(srcdir)/$< | nroff -man | col -b > $@

txt: sox.txt soxeffect.txt soxformat.txt soxexam.txt libsox.txt

# Rule for making PDF man pages
.1.pdf .3.pdf .7.pdf:
	tbl $(srcdir)/$< | groff -man -Tps | ps2pdf - $@

pdf: sox.pdf soxeffect.pdf soxformat.pdf soxexam.pdf libsox.pdf

install-data-hook:
	cd $(DESTDIR)$(mandir)/man1 && $(RM) play.1 && $(LN_S) sox.1 play.1
	cd $(DESTDIR)$(mandir)/man1 && $(RM) rec.1 && $(LN_S) sox.1 rec.1

# Automatically update libtool script if it gets out-of-date
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

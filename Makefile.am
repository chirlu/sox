## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = lpc10 libgsm src
DIST_SUBDIRS = lpc10 libgsm src msvc9 msvc10

RM = rm -f

# man pages are not considered to be sources, so need to add "dist_"
# prefix to ensure they are added to the distribution.
dist_man_MANS = sox.1 soxi.1 soxformat.7 libsox.3
EXTRA_DIST = sox.txt soxi.txt soxformat.txt libsox.txt \
	     CMakeLists.txt sox.pc.in LICENSE.GPL LICENSE.LGPL \
	     cygbuild README.win32 README.osx scripts/batch-example.bat \
	     README.sh FEATURES.in

if HAVE_PKGCONFIG
pkgconfigdir = @PKGCONFIGDIR@
pkgconfig_DATA = sox.pc
endif

play.1 rec.1: sox.1
	$(RM) $@ && $(LN_S) $< $@

README: README.sh FEATURES.in
	./README.sh

# Rule for making text man pages.  Uses GNU nroff's -c so not portable.
.1.txt:
	tbl $(srcdir)/$< | nroff -man -c | sed "s/.//g" > $@
.3.txt:
	tbl $(srcdir)/$< | nroff -man -c | sed "s/.//g" > $@
.7.txt:
	tbl $(srcdir)/$< | nroff -man -c | sed "s/.//g" > $@

DOCTXT = sox.txt soxi.txt soxformat.txt libsox.txt README
txt: $(DOCTXT)

# Rule for making PDF man pages
.1.pdf:
	pdfroff -t -man -Tps $(srcdir)/$< > $@
.3.pdf:
	pdfroff -t -man -Tps $(srcdir)/$< > $@
.7.pdf:
	pdfroff -t -man -Tps $(srcdir)/$< > $@

DOCPDF = sox.pdf soxi.pdf soxformat.pdf libsox.pdf
pdf: $(DOCPDF)

install-pdf: pdf
	@$(NORMAL_INSTALL)
	test -z "$(pdfdir)" || $(MKDIR_P) "$(DESTDIR)$(pdfdir)"
	@list='$(DOCPDF)'; for p in $$list; do \
	  if test -f "$$p"; then d=; else d="$(srcdir)/"; fi; \
	  f=$(am__strip_dir) \
	  echo " $(INSTALL_DATA) '$$d$$p' '$(DESTDIR)$(pdfdir)/$$f'"; \
	  $(INSTALL_DATA) "$$d$$p" "$(DESTDIR)$(pdfdir)/$$f"; \
	  done

uninstall-pdf:
	@$(NORMAL_UNINSTALL)
	@list='$(DOCPDF)'; for p in $$list; do \
	  f=$(am__strip_dir) \
	  echo " $(RM) '$(DESTDIR)$(pdfdir)/$$f'"; \
	  $(RM) "$(DESTDIR)$(pdfdir)/$$f"; \
	  done

distclean-local:
	$(RM) mingw32-config.cache
	$(RM) -r -f soxpng

# Rule for making HTML man pages
#
# Get rid on unneeded Content-Type, man directory and manpage index in extension.
# e.g. ../man1/soxi.1.html becomes ../soxi.html
# Return to Main Contents and link to man2html are also modified.
FIXHTML = sed 's,<head>,&<link rel="stylesheet" href="http://sox.sourceforge.net/pub/skins/sox/sox.css" type="text/css"></link>,' 

.1.html:
	groff -t -man -Thtml -P -Dsoxpng $(srcdir)/$< | $(FIXHTML) > $@
.3.html:
	groff -t -man -Thtml -P -Dsoxpng $(srcdir)/$< | $(FIXHTML) > $@
.7.html:
	groff -t -man -Thtml -P -Dsoxpng $(srcdir)/$< | $(FIXHTML) > $@

DOCHTML = sox.html soxi.html soxformat.html libsox.html
html: $(DOCHTML)

install-html: html 	 
	@$(NORMAL_INSTALL) 	 
	test -z "$(htmldir)/soxpng" || $(MKDIR_P) "$(DESTDIR)$(htmldir)/soxpng"
	@list='$(DOCHTML)'; for p in $$list; do \
	if test -f "$$p"; then d=; else d="$(srcdir)/"; fi; \
	f=$(am__strip_dir) \
	echo " $(INSTALL_DATA) '$$d$$p' '$(DESTDIR)$(htmldir)//$$f'"; \
	$(INSTALL_DATA) "$$d$$p" "$(DESTDIR)$(htmldir)/$$f"; \
	done; \
	@for p in soxpng/*; do \
	if test -f "$$p"; then d=; else d="$(srcdir)"; fi; \
	f=$(am__strip_dir) \
	echo " $(INSTALL_DATA) '$$d$$p' '$(DESTDIR)$(htmldir)/soxpng/$$f'"; \
	$(INSTALL_DATA) "$$d$$p" "$(DESTDIR)$(htmldir)/soxpng/$$f"; \
	done

uninstall-html: 	 
	@$(NORMAL_UNINSTALL)
	@list='$(DOCHTML)'; for p in $$list; do \
	f=$(am__strip_dir) \
	echo " $(RM) '$(DESTDIR)$(htmldir)/$$f'"; \
	$(RM) "$(DESTDIR)$(htmldir)/$$f"; \
	done; \
	$(RM) -fr "$(DESTDIR)$(htmldir)/soxpng";

DISTCLEANFILES = $(DOCHTML) $(DOCPDF)
MAINTAINERCLEANFILES = $(DOCTXT)

install-data-hook:
	cd $(DESTDIR)$(mandir)/man1 && $(RM) play.1 && $(LN_S) sox.1 play.1
	cd $(DESTDIR)$(mandir)/man1 && $(RM) rec.1 && $(LN_S) sox.1 rec.1
	cd $(DESTDIR)$(mandir)/man7 && $(RM) soxeffect.7 && $(LN_S) ../man1/sox.1 soxeffect.7

uninstall-hook:
	$(RM) $(DESTDIR)$(mandir)/man1/play.1
	$(RM) $(DESTDIR)$(mandir)/man1/rec.1
	$(RM) $(DESTDIR)$(mandir)/man7/soxeffect.7

# Automatically update libtool script if it gets out-of-date
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

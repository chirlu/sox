## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

if INTERNAL_LUA
  MAYBE_LUA = src/lua
endif

SUBDIRS = $(MAYBE_LUA) src

# man pages are not considered to be sources, so need to add "dist_"
# prefix to ensure they are added to the distribution.
dist_man_MANS = sox.1 soxexam.1 play.1 libst.3
dist_pkgdata_DATA = sox.txt soxexam.txt

# Rule for making plain text man pages
%.txt: %.1
	nroff -man $(srcdir)/$< | col -b > $@

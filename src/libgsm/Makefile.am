man_MANS = gsm.3 gsm_options.3

# Define SASR if >> is a signed arithmetic shift (-1 >> 1 == -1)
SASR	= -DSASR

# Build macros.

CFLAGS	= @CFLAGS@ @DEFS@ $(SASR) -I$(top_srcdir)/libgsm

# Objects.

lib_LIBRARIES = libgsm.a
libgsm_a_SOURCES = add.c code.c decode.c long_term.c lpc.c preprocess.c	\
		rpe.c gsm_destroy.c gsm_decode.c gsm_encode.c gsm_create.c \
		gsm_option.c short_term.c table.c

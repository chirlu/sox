man_MANS = gsm.3 gsm_option.3

# Define SASR if >> is a signed on signed types (-1 >> 1 == -1)
SASR	= -DSASR

AM_CFLAGS = $(SASR)

lib_LIBRARIES = libgsm.a
libgsm_a_SOURCES = add.c code.c decode.c long_term.c lpc.c preprocess.c	\
		rpe.c gsm_destroy.c gsm_decode.c gsm_encode.c gsm_create.c \
		gsm_option.c short_term.c table.c

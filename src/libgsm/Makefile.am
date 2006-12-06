######### Define SASR if >> is a signed arithmetic shift (-1 >> 1 == -1)
SASR	= -DSASR

######### Define this if your host multiplies floats faster than integers,
######### e.g. on a SPARCstation.
MULHACK = -DUSE_FLOAT_MUL

# Build macros.

CFLAGS	= @CFLAGS@ @DEFS@ $(SASR) $(MULHAC) -I$(top_srcdir)/gsm

# Objects.

lib_LIBRARIES = libgsm.a
libgsm_a_SOURCES = add.o code.o decode.o long_term.o lpc.o preprocess.o	\
		rpe.o gsm_destroy.o gsm_decode.o gsm_encode.o gsm_create.o \
		gsm_option.o short_term.o table.o

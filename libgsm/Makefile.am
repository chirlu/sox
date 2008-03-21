SASR    = -DSASR
######### Define SASR if >> is a signed arithmetic shift (-1 >> 1 == -1)

MULHACK = -DUSE_FLOAT_MUL
######### Define this if your host multiplies floats faster than integers,
######### e.g. on a SPARCstation.

FAST    = -DFAST
######### Define together with USE_FLOAT_MUL to enable the GSM library's
######### approximation option for incorrect, but good-enough results.

# LTP_CUT       = -DLTP_CUT
LTP_CUT =
######### Define to enable the GSM library's long-term correlation 
######### approximation option---faster, but worse; works for
######### both integer and floating point multiplications.
######### This flag is still in the experimental stage.

WAV49   = -DWAV49
#WAV49  =
######### Define to enable the GSM library's option to pack GSM frames 
######### in the style used by the WAV #49 format.  If you want to write
######### a tool that produces .WAV files which contain GSM-encoded data,
######### define this, and read about the GSM_OPT_WAV49 option in the
######### manual page on gsm_option(3).

AM_CFLAGS = $(SASR) $(MULHAC) $(FAST) $(LTP_CUT) $(WAV49) 

if EXTERNAL_GSM
EXTRA_DIST = add.c code.c decode.c long_term.c lpc.c preprocess.c \
	     rpe.c gsm_destroy.c gsm_decode.c gsm_encode.c gsm_create.c \
	     gsm_option.c short_term.c table.c private.h gsm.h CMakeLists.txt \
	     aliases.h
else
noinst_LTLIBRARIES = libgsm.la
noinst_HEADERS = gsm.h
libgsm_la_SOURCES = add.c code.c decode.c long_term.c lpc.c preprocess.c \
		rpe.c gsm_destroy.c gsm_decode.c gsm_encode.c gsm_create.c \
		gsm_option.c short_term.c table.c private.h aliases.h
EXTRA_DIST = CMakeLists.txt
endif

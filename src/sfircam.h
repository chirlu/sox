/*                              SFHEADER.H                              */

/* definitions and structures needed for manipulating soundfiles.
 */

#define SIZEOF_HEADER 1024
#define SF_BUFSIZE      (16*1024) /* used only in play */
#define SF_MAXCHAN      4
#define MAXCOMM 512
#define MINCOMM 256

#define SF_MAGIC1 0144
#define SF_MAGIC2 0243

/* Definition of SF_MACHINE and SF_MAGIC
 *
 * Note that SF_MAGIC always has SF_MAGIC1 as its first byte, SF_MAGIC2 as its
 * second, SF_MACHINE as its third, and zero as its fourth.  Separate define's
 * are needed because byte order is different on different machines.
 */
#define SF_VAX 1
#define SF_SUN 2
#define SF_MIPS 3
#define SF_NEXT 4
#ifdef vax
#define SF_MACHINE SF_VAX
#define SF_MAGIC ((uint32_t)(SF_MAGIC1 | SF_MAGIC2 << 8 | SF_MACHINE << 16))
#endif
#ifdef sun
#define SF_MACHINE SF_SUN
#define SF_MAGIC ((uint32_t)(SF_MAGIC1 << 24 | SF_MAGIC2 << 16 | SF_MACHINE << 8))
#endif
#ifdef mips
#define SF_MACHINE SF_MIPS
#define SF_MAGIC ((uint32_t)(SF_MAGIC1 | SF_MAGIC2 << 8 | SF_MACHINE << 16))
#endif
#ifdef NeXT
#define SF_MACHINE SF_NEXT
#define SF_MAGIC ((uint32_t)(SF_MAGIC1 << 24 | SF_MAGIC2 << 16 | SF_MACHINE << 8))
#endif


/* Packing modes, as stored in the SFHEADER.sf_packmode field
 *
 * For each packing mode, the lower-order short is the number of bytes per
 * sample, and for backward compatibility, SF_SHORT and SF_FLOAT have
 * high-order short = 0 so overall they're the bytes per sample, but that's not
 * true for all SF_'s.  Thus while the "sfclass" macro still returns a unique
 * ID for each packing mode, the new "sfsamplesize" macro should be used to get
 * the bytes per sample.
 *
 * Note that SF_X == SFMT_X in most, but not all, cases, because MIT changed
 * SFMT_FLOAT and we kept SF_FLOAT for compatibility with existing sound files.
 *
 * Possible values of sf_packmode:
 */
#define SF_CHAR  ((uint32_t) sizeof(char))
#define SF_ALAW  ((uint32_t) sizeof(char) | 0x10000)
#define SF_ULAW  ((uint32_t) sizeof(char) | 0x20000)
#define SF_SHORT ((uint32_t) sizeof(short))
#define SF_LONG  ((uint32_t) sizeof(long) | 0x40000)
#define SF_FLOAT ((uint32_t) sizeof(float))

/* For marking data after fixed section of soundfile header -- see man (3carl)
 * sfcodes and defintions of SFCODE and related structures, below.
 */
#define SF_END 0            /* Meaning no more information */
#define SF_MAXAMP 1         /* Meaning maxamp follows */
#define SF_COMMENT 2        /* code for "comment line" */
#define SF_PVDATA      3
#define SF_AUDIOENCOD  4
#define SF_CODMAX      4

/*
 * DEFINITION OF SFHEADER FORMAT
 *
 * The first four bytes are the magic information for the sound file.  They
 * can be accessed, via a union, either as a structure of four unsigned bytes
 * sf_magic1, sf_magic2, sf_machine, sf_param, or as the single long sf_magic.
 * sf_magic is for backward compatibility; it should be SF_MAGIC as defined
 * above.
 */
struct sfinfo {
    union magic_union {
        struct {
            unsigned char sf_magic1;  /* byte 1 of magic */
            unsigned char sf_magic2;  /* 2 */
            unsigned char sf_machine; /* 3 */
            unsigned char sf_param;   /* 4 */
        } _magic_bytes;
        uint32_t sf_magic;            /* magic as a 4-byte long */
    } magic_union;
    float     sf_srate;
    uint32_t          sf_chans;
    uint32_t          sf_packmode;
    char      sf_codes;
};

typedef union sfheader {
        struct sfinfo sfinfo;
        char    filler[SIZEOF_HEADER];
} SFHEADER;

/*
 * Definition of SFCODE and related data structs
 *
 * Two routines in libbicsf/sfcodes.c, getsfcode() and putsfcode()
 * are used to insert additionnal information into a header
 * or to retreive such information. See man sfcodes.
 *
 * 10/90 pw
 *      These routines are now part of libcarl/sfcodes.c
 */

typedef struct sfcode {
        short   code;
        short   bsize;
} SFCODE;

typedef struct Sfmaxamp {
        float   value[SF_MAXCHAN];
        uint32_t samploc[SF_MAXCHAN];
        uint32_t timetag;
} SFMAXAMP;

typedef struct sfcomment {
        char    comment[MAXCOMM];
} SFCOMMENT;

typedef struct {                  /* this code written by pvanal */
        short   frameSize;
        short   frameIncr;
} SFPVDATA;

typedef struct {                  /*     ditto                    */
        short   encoding;
        short   grouping;
} SFAUDIOENCOD;

/*
 * DEFINITION OF MACROS TO GET HEADER INFO
 *     x is a pointer to SFHEADER
 *
 * For backward compatibility in MIT Csound code, sfmagic(x) still provides
 * access to the first long of SFHEADER x.  It can be compared to SF_MAGIC,
 * which is defined machine-dependently (above) to always be the right four
 * bytes in the right order.
 *
 * sfclass(x) returns one of SF_SHORT, SF_FLOAT etc. defined above, while
 * sfsamplesize(x) returns just the bytes per object, the lower-order short of
 * sf_packmode.
 */
#define sfmagic(x) ((x)->sfinfo.magic_union.sf_magic)
#define sfmagic1(x) ((x)->sfinfo.magic_union._magic_bytes.sf_magic1)
#define sfmagic2(x) ((x)->sfinfo.magic_union._magic_bytes.sf_magic2)
#define sfmachine(x) ((x)->sfinfo.magic_union._magic_bytes.sf_machine)
#define sfparam(x) ((x)->sfinfo.magic_union._magic_bytes.sf_param)
#define sfsrate(x) ((x)->sfinfo.sf_srate)
#define sfchans(x) ((x)->sfinfo.sf_chans)
#define sfclass(x) ((x)->sfinfo.sf_packmode)
#define sfsamplesize(x) ((size_t) ((x)->sfinfo.sf_packmode & 0xFFFF))
#define sfbsize(x) ((x)->st_size - sizeof(SFHEADER))
#define sfcodes(x) ((x)->sfinfo.sf_codes)

/*
 * Macros for testing soundfiles
 */
/* True if soundfile and good arch */
#define ismagic(x) ((sfmagic1(x) == SF_MAGIC1) && \
        (sfmagic2(x) == SF_MAGIC2) && \
        (sfmachine(x) == SF_MACHINE))

/* True if soundfile */
#define isforeignmagic(x) ((sfmagic1(x) == SF_MAGIC1) && \
        (sfmagic2(x) == SF_MAGIC2))

/* True if soundfile */
#define issoundfile(x)  ((sfmagic1(x) == SF_MAGIC1) && \
        (sfmagic2(x) == SF_MAGIC2))

/* True if soundfile and foreign arch */
#define isforeignsoundfile(x) ((sfmagic1(x) == SF_MAGIC1) && \
        (sfmagic2(x) == SF_MAGIC2) && \
        (sfmachine(x) != SF_MACHINE))

/* True if foreign arch */
#define isforeign(x) (sfmachine(x) != SF_MACHINE)


/*
 * The macros for opening soundfiles have been rewritten as C routines.
 * In order to preserve compatibility, we supply the following new macros
 */

#define readopensf(name,fd,sfh,sfst,prog,result) \
        result = (fd = openrosf(name, &sfh, &sfst, prog)) < 0 ? fd : 0;

#define freadopensf(name,fp,sfh,sfst,prog,result) \
        result = fopenrosf(name, &fp, &sfh, &sfst, prog);

#define wropensf(name,fd,sfh,prog,result) \
        result = (fd = openwosf(name, &sfh, prog)) < 0 ? fd : 0;

#define rdwropensf(name,fd,sfh,sfst,prog,result) \
        result = (fd = openrwsf(name, &sfh, &sfst, prog)) < 0 ? fd : 0;


/*
 * Definition of macro to get MAXAMP and COMMENT info
 *
 * sfm is ptr to SFMAXAMP
 * sfst is the address of a stat struct
 */

#define sfmaxamp(mptr,chan) (mptr)->value[chan]
#define sfmaxamploc(mptr,chan) (mptr)->samploc[chan]
#define sfmaxamptime(x) (x)->timetag
#define ismaxampgood(x,s) (sfmaxamptime(x) >= (s)->st_mtime)
#define sfcomm(x,n) (x)->comment[n]


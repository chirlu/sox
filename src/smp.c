/*
 * libSoX SampleVision file format driver.
 * Output is always in little-endian (80x86/VAX) order.
 * 
 * Derived from: libSoX skeleton handler file.
 *
 * Add: Loop point verbose info.  It's a start, anyway.
 */

/*
 * June 30, 1992
 * Copyright 1992 Leigh Smith And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Leigh Smith And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

#include "sox_i.h"
#include <string.h>
#include <errno.h>

#define NAMELEN    30           /* Size of Samplevision name */
#define COMMENTLEN 60           /* Size of Samplevision comment, not shared */
#define MIDI_UNITY 60           /* MIDI note number to play sample at unity */
#define MARKERLEN  10           /* Size of Marker name */

/* The header preceeding the sample data */
struct smpheader {
        char Id[18];            /* File identifier */
        char version[4];        /* File version */
        char comments[COMMENTLEN];      /* User comments */
        char name[NAMELEN + 1]; /* Sample Name, left justified */
};
#define HEADERSIZE (sizeof(struct smpheader) - 1)       /* -1 for name's \0 */

/* Samplevision loop definition structure */
struct loop {
        uint32_t start; /* Sample count into sample data, not byte count */
        uint32_t end;   /* end point */
        char type;   /* 0 = loop off, 1 = forward, 2 = forw/back */
        short count;         /* No of times to loop */
};

/* Samplevision marker definition structure */
struct marker {
        char name[MARKERLEN + 1]; /* Ascii Marker name */
        uint32_t position;        /* Sample Number, not byte number */
};

/* The trailer following the sample data */
struct smptrailer {
        struct loop loops[8];           /* loops */
        struct marker markers[8];       /* markers */
        char MIDInote;                  /* for unity pitch playback */
        uint32_t rate;                  /* in hertz */
        uint32_t SMPTEoffset;           /* in subframes - huh? */
        uint32_t CycleSize;             /* sample count in one cycle of the */
                                        /* sampled sound -1 if unknown */
};

/* Private data for SMP file */
typedef struct smpstuff {
  uint32_t NoOfSamps;           /* Sample data count in words */
  sox_size_t dataStart;
  /* comment memory resides in private data because it's small */
  char comment[COMMENTLEN + NAMELEN + 3];
} *smp_t;

char *SVmagic = "SOUND SAMPLE DATA ", *SVvers = "2.1 ";

/*
 * Read the SampleVision trailer structure.
 * Returns 1 if everything was read ok, 0 if there was an error.
 */
static int readtrailer(ft_t ft, struct smptrailer *trailer)
{
        int i;
        int16_t trash16;

        sox_readw(ft, (unsigned short *)&trash16); /* read reserved word */
        for(i = 0; i < 8; i++) {        /* read the 8 loops */
                sox_readdw(ft, &(trailer->loops[i].start));
                ft->loops[i].start = trailer->loops[i].start;
                sox_readdw(ft, &(trailer->loops[i].end));
                ft->loops[i].length = 
                        trailer->loops[i].end - trailer->loops[i].start;
                sox_readb(ft, (unsigned char *)&(trailer->loops[i].type));
                ft->loops[i].type = trailer->loops[i].type;
                sox_readw(ft, (unsigned short *)&(trailer->loops[i].count));
                ft->loops[i].count = trailer->loops[i].count;
        }
        for(i = 0; i < 8; i++) {        /* read the 8 markers */
                if (sox_readbuf(ft, trailer->markers[i].name, MARKERLEN) != MARKERLEN)
                {
                    sox_fail_errno(ft,SOX_EHDR,"EOF in SMP");
                    return(SOX_EOF);
                }
                trailer->markers[i].name[MARKERLEN] = 0;
                sox_readdw(ft, &(trailer->markers[i].position));
        }
        sox_readb(ft, (unsigned char *)&(trailer->MIDInote));
        sox_readdw(ft, &(trailer->rate));
        sox_readdw(ft, &(trailer->SMPTEoffset));
        sox_readdw(ft, &(trailer->CycleSize));
        return(SOX_SUCCESS);
}

/*
 * set the trailer data - loops and markers, to reasonably benign values
 */
static void settrailer(ft_t ft, struct smptrailer *trailer, sox_rate_t rate)
{
        int i;

        for(i = 0; i < 8; i++) {        /* copy the 8 loops */
            if (ft->loops[i].type != 0) {
                trailer->loops[i].start = ft->loops[i].start;
                /* to mark it as not set */
                trailer->loops[i].end = ft->loops[i].start + ft->loops[i].length;
                trailer->loops[i].type = ft->loops[i].type;
                trailer->loops[i].count = ft->loops[i].count;   
            } else {
                /* set first loop start as FFFFFFFF */
                trailer->loops[i].start = ~0u;   
                /* to mark it as not set */
                trailer->loops[i].end = 0;      
                trailer->loops[i].type = 0;
                trailer->loops[i].count = 0;
            }
        }
        for(i = 0; i < 8; i++) {        /* write the 8 markers */
                strcpy(trailer->markers[i].name, "          ");
                trailer->markers[i].position = ~0u;
        }
        trailer->MIDInote = MIDI_UNITY;         /* Unity play back */
        trailer->rate = rate;
        trailer->SMPTEoffset = 0;
        trailer->CycleSize = ~0u;
}

/*
 * Write the SampleVision trailer structure.
 * Returns 1 if everything was written ok, 0 if there was an error.
 */
static int writetrailer(ft_t ft, struct smptrailer *trailer)
{
        int i;

        sox_writew(ft, 0);                       /* write the reserved word */
        for(i = 0; i < 8; i++) {        /* write the 8 loops */
                sox_writedw(ft, trailer->loops[i].start);
                sox_writedw(ft, trailer->loops[i].end);
                sox_writeb(ft, trailer->loops[i].type);
                sox_writew(ft, trailer->loops[i].count);
        }
        for(i = 0; i < 8; i++) {        /* write the 8 markers */
                if (sox_writes(ft, trailer->markers[i].name) == SOX_EOF)
                {
                    sox_fail_errno(ft,SOX_EHDR,"EOF in SMP");
                    return(SOX_EOF);
                }
                sox_writedw(ft, trailer->markers[i].position);
        }
        sox_writeb(ft, trailer->MIDInote);
        sox_writedw(ft, trailer->rate);
        sox_writedw(ft, trailer->SMPTEoffset);
        sox_writedw(ft, trailer->CycleSize);
        return(SOX_SUCCESS);
}

static int sox_smpseek(ft_t ft, sox_size_t offset) 
{
    sox_size_t new_offset, channel_block, alignment;
    smp_t smp = (smp_t) ft->priv;

    new_offset = offset * ft->signal.size;
    /* Make sure request aligns to a channel block (ie left+right) */
    channel_block = ft->signal.channels * ft->signal.size;
    alignment = new_offset % channel_block;
    /* Most common mistaken is to compute something like
     * "skip everthing upto and including this sample" so
     * advance to next sample block in this case.
     */
    if (alignment != 0)
        new_offset += (channel_block - alignment);
    new_offset += smp->dataStart;

    ft->sox_errno = sox_seeki(ft, new_offset, SEEK_SET);

    if( ft->sox_errno == SOX_SUCCESS )
        smp->NoOfSamps = ft->length - (new_offset / ft->signal.size);

    return(ft->sox_errno);
}
/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *      Find out sampling rate, 
 *      size and encoding of samples, 
 *      mono/stereo/quad.
 */
static int sox_smpstartread(ft_t ft) 
{
        smp_t smp = (smp_t) ft->priv;
        int i;
        int namelen, commentlen;
        sox_size_t samplestart;
        struct smpheader header;
        struct smptrailer trailer;

        /* If you need to seek around the input file. */
        if (! ft->seekable)
        {
                sox_fail_errno(ft,SOX_EOF,"SMP input file must be a file, not a pipe");
                return(SOX_EOF);
        }

        /* Read SampleVision header */
        if (sox_readbuf(ft, (char *)&header, HEADERSIZE) != HEADERSIZE)
        {
                sox_fail_errno(ft,SOX_EHDR,"unexpected EOF in SMP header");
                return(SOX_EOF);
        }
        if (strncmp(header.Id, SVmagic, 17) != 0)
        {
                sox_fail_errno(ft,SOX_EHDR,"SMP header does not begin with magic word %s", SVmagic);
                return(SOX_EOF);
        }
        if (strncmp(header.version, SVvers, 4) != 0)
        {
                sox_fail_errno(ft,SOX_EHDR,"SMP header is not version %s", SVvers);
                return(SOX_EOF);
        }

        /* Format the sample name and comments to a single comment */
        /* string. We decrement the counters till we encounter non */
        /* padding space chars, so the *lengths* are low by one */
        for (namelen = NAMELEN-1;
            namelen >= 0 && header.name[namelen] == ' '; namelen--)
          ;
        for (commentlen = COMMENTLEN-1;
            commentlen >= 0 && header.comments[commentlen] == ' '; commentlen--)
          ;
        sprintf(smp->comment, "%.*s: %.*s", namelen+1, header.name,
                commentlen+1, header.comments);
        ft->comment = smp->comment;

        sox_report("SampleVision file name and comments: %s", ft->comment);
        /* Extract out the sample size (always intel format) */
        sox_readdw(ft, &(smp->NoOfSamps));
        /* mark the start of the sample data */
        samplestart = sox_tell(ft);

        /* seek from the current position (the start of sample data) by */
        /* NoOfSamps * sizeof(int16_t) */
        if (sox_seeki(ft, smp->NoOfSamps * 2, 1) == -1)
        {
                sox_fail_errno(ft,errno,"SMP unable to seek to trailer");
                return(SOX_EOF);
        }
        if (readtrailer(ft, &trailer))
        {
                sox_fail_errno(ft,SOX_EHDR,"unexpected EOF in SMP trailer");
                return(SOX_EOF);
        }

        /* seek back to the beginning of the data */
        if (sox_seeki(ft, samplestart, 0) == -1) 
        {
                sox_fail_errno(ft,errno,"SMP unable to seek back to start of sample data");
                return(SOX_EOF);
        }

        ft->signal.rate = (int) trailer.rate;
        ft->signal.size = SOX_SIZE_16BIT;
        ft->signal.encoding = SOX_ENCODING_SIGN2;
        ft->signal.channels = 1;
        smp->dataStart = samplestart;
        ft->length = smp->NoOfSamps;

        sox_report("SampleVision trailer:");
        for(i = 0; i < 8; i++) if (1 || trailer.loops[i].count) {
                sox_report("Loop %d: start: %6d", i, trailer.loops[i].start);
                sox_report(" end:   %6d", trailer.loops[i].end);
                sox_report(" count: %6d", trailer.loops[i].count);
                switch(trailer.loops[i].type) {
                    case 0: sox_report("type:  off"); break;
                    case 1: sox_report("type:  forward"); break;
                    case 2: sox_report("type:  forward/backward"); break;
                }
        }
        sox_report("MIDI Note number: %d", trailer.MIDInote);

        ft->instr.nloops = 0;
        for(i = 0; i < 8; i++) 
                if (trailer.loops[i].type) 
                        ft->instr.nloops++;
        for(i = 0; i < ft->instr.nloops; i++) {
                ft->loops[i].type = trailer.loops[i].type;
                ft->loops[i].count = trailer.loops[i].count;
                ft->loops[i].start = trailer.loops[i].start;
                ft->loops[i].length = trailer.loops[i].end 
                        - trailer.loops[i].start;
        }
        ft->instr.MIDIlow = ft->instr.MIDIhi =
                ft->instr.MIDInote = trailer.MIDInote;
        if (ft->instr.nloops > 0)
                ft->instr.loopmode = SOX_LOOP_8;
        else
                ft->instr.loopmode = SOX_LOOP_NONE;

        return(SOX_SUCCESS);
}

/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */
static sox_size_t sox_smpread(ft_t ft, sox_sample_t *buf, sox_size_t len) 
{
        smp_t smp = (smp_t) ft->priv;
        unsigned short datum;
        sox_size_t done = 0;
        
        for(; done < len && smp->NoOfSamps; done++, smp->NoOfSamps--) {
                sox_readw(ft, &datum);
                /* scale signed up to long's range */
                *buf++ = SOX_SIGNED_WORD_TO_SAMPLE(datum,);
        }
        return done;
}

static int sox_smpstartwrite(ft_t ft) 
{
        smp_t smp = (smp_t) ft->priv;
        struct smpheader header;

        /* If you have to seek around the output file */
        if (! ft->seekable)
        {
                sox_fail_errno(ft,SOX_EOF,"Output .smp file must be a file, not a pipe");
                return(SOX_EOF);
        }

        /* If your format specifies any of the following info. */
        ft->signal.size = SOX_SIZE_16BIT;
        ft->signal.encoding = SOX_ENCODING_SIGN2;
        ft->signal.channels = 1;

        strcpy(header.Id, SVmagic);
        strcpy(header.version, SVvers);
        sprintf(header.comments, "%-*s", COMMENTLEN, "Converted using Sox.");
        sprintf(header.name, "%-*.*s", NAMELEN, NAMELEN, ft->comment);

        /* Write file header */
        if(sox_writebuf(ft, &header, HEADERSIZE) != HEADERSIZE)
        {
            sox_fail_errno(ft,errno,"SMP: Can't write header completely");
            return(SOX_EOF);
        }
        sox_writedw(ft, 0);      /* write as zero length for now, update later */
        smp->NoOfSamps = 0;

        return(SOX_SUCCESS);
}

static sox_size_t sox_smpwrite(ft_t ft, const sox_sample_t *buf, sox_size_t len) 
{
        smp_t smp = (smp_t) ft->priv;
        int datum;
        sox_size_t done = 0;

        while(done < len) {
                datum = (int) SOX_SAMPLE_TO_SIGNED_WORD(*buf++, ft->clips);
                sox_writew(ft, datum);
                smp->NoOfSamps++;
                done++;
        }

        return(done);
}

static int sox_smpstopwrite(ft_t ft) 
{
        smp_t smp = (smp_t) ft->priv;
        struct smptrailer trailer;

        /* Assign the trailer data */
        settrailer(ft, &trailer, ft->signal.rate);
        writetrailer(ft, &trailer);
        if (sox_seeki(ft, 112, 0) == -1)
        {
                sox_fail_errno(ft,errno,"SMP unable to seek back to save size");
                return(SOX_EOF);
        }
        sox_writedw(ft, smp->NoOfSamps);

        return(SOX_SUCCESS);
}

/* SampleVision sound */
static const char *smpnames[] = {
  "smp",
  NULL,
};

static sox_format_t sox_smp_format = {
  smpnames,
  NULL,
  SOX_FILE_LOOPS | SOX_FILE_SEEK | SOX_FILE_LIT_END,
  sox_smpstartread,
  sox_smpread,
  sox_format_nothing,
  sox_smpstartwrite,
  sox_smpwrite,
  sox_smpstopwrite,
  sox_smpseek
};

const sox_format_t *sox_smp_format_fn(void)
{
    return &sox_smp_format;
}

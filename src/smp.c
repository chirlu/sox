/*
 * June 30, 1992
 * Copyright 1992 Leigh Smith And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Leigh Smith And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools SampleVision file format driver.
 * Output is always in little-endian (80x86/VAX) order.
 * 
 * Derived from: Sound Tools skeleton handler file.
 *
 * Add: Loop point verbose info.  It's a start, anyway.
 */

#include "st_i.h"
#include <string.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

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
  st_size_t dataStart;
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

        st_readw(ft, (unsigned short *)&trash16); /* read reserved word */
        for(i = 0; i < 8; i++) {        /* read the 8 loops */
                st_readdw(ft, &(trailer->loops[i].start));
                ft->loops[i].start = trailer->loops[i].start;
                st_readdw(ft, &(trailer->loops[i].end));
                ft->loops[i].length = 
                        trailer->loops[i].end - trailer->loops[i].start;
                st_readb(ft, (unsigned char *)&(trailer->loops[i].type));
                ft->loops[i].type = trailer->loops[i].type;
                st_readw(ft, (unsigned short *)&(trailer->loops[i].count));
                ft->loops[i].count = trailer->loops[i].count;
        }
        for(i = 0; i < 8; i++) {        /* read the 8 markers */
                if (st_readbuf(ft, trailer->markers[i].name, 1, MARKERLEN) != 10)
                {
                    st_fail_errno(ft,ST_EHDR,"EOF in SMP");
                    return(ST_EOF);
                }
                trailer->markers[i].name[MARKERLEN] = 0;
                st_readdw(ft, &(trailer->markers[i].position));
        }
        st_readb(ft, (unsigned char *)&(trailer->MIDInote));
        st_readdw(ft, &(trailer->rate));
        st_readdw(ft, &(trailer->SMPTEoffset));
        st_readdw(ft, &(trailer->CycleSize));
        return(ST_SUCCESS);
}

/*
 * set the trailer data - loops and markers, to reasonably benign values
 */
static void settrailer(ft_t ft, struct smptrailer *trailer, st_rate_t rate)
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
                trailer->loops[i].start = ~0;   
                /* to mark it as not set */
                trailer->loops[i].end = 0;      
                trailer->loops[i].type = 0;
                trailer->loops[i].count = 0;
            }
        }
        for(i = 0; i < 8; i++) {        /* write the 8 markers */
                strcpy(trailer->markers[i].name, "          ");
                trailer->markers[i].position = ~0;
        }
        trailer->MIDInote = MIDI_UNITY;         /* Unity play back */
        trailer->rate = rate;
        trailer->SMPTEoffset = 0;
        trailer->CycleSize = -1;
}

/*
 * Write the SampleVision trailer structure.
 * Returns 1 if everything was written ok, 0 if there was an error.
 */
static int writetrailer(ft_t ft, struct smptrailer *trailer)
{
        int i;

        st_writew(ft, 0);                       /* write the reserved word */
        for(i = 0; i < 8; i++) {        /* write the 8 loops */
                st_writedw(ft, trailer->loops[i].start);
                st_writedw(ft, trailer->loops[i].end);
                st_writeb(ft, trailer->loops[i].type);
                st_writew(ft, trailer->loops[i].count);
        }
        for(i = 0; i < 8; i++) {        /* write the 8 markers */
                if (st_writes(ft, trailer->markers[i].name) == ST_EOF)
                {
                    st_fail_errno(ft,ST_EHDR,"EOF in SMP");
                    return(ST_EOF);
                }
                st_writedw(ft, trailer->markers[i].position);
        }
        st_writeb(ft, trailer->MIDInote);
        st_writedw(ft, trailer->rate);
        st_writedw(ft, trailer->SMPTEoffset);
        st_writedw(ft, trailer->CycleSize);
        return(ST_SUCCESS);
}

int st_smpseek(ft_t ft, st_size_t offset) 
{
    int new_offset, channel_block, alignment;
    smp_t smp = (smp_t) ft->priv;

    new_offset = offset * ft->info.size;
    /* Make sure request aligns to a channel block (ie left+right) */
    channel_block = ft->info.channels * ft->info.size;
    alignment = new_offset % channel_block;
    /* Most common mistaken is to compute something like
     * "skip everthing upto and including this sample" so
     * advance to next sample block in this case.
     */
    if (alignment != 0)
        new_offset += (channel_block - alignment);
    new_offset += smp->dataStart;

    ft->st_errno = st_seeki(ft, new_offset, SEEK_SET);

    if( ft->st_errno == ST_SUCCESS )
        smp->NoOfSamps = ft->length - (new_offset / ft->info.size);

    return(ft->st_errno);
}
/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *      Find out sampling rate, 
 *      size and encoding of samples, 
 *      mono/stereo/quad.
 */
int st_smpstartread(ft_t ft) 
{
        smp_t smp = (smp_t) ft->priv;
        int i;
        int namelen, commentlen;
        long samplestart;
        struct smpheader header;
        struct smptrailer trailer;

        /* SMP is in Little Endian format.  Swap whats read in on */
        /* Big Endian machines.                                   */
        if (ST_IS_BIGENDIAN)
        {
                ft->swap = ft->swap ? 0 : 1;
        }

        /* If you need to seek around the input file. */
        if (! ft->seekable)
        {
                st_fail_errno(ft,ST_EOF,"SMP input file must be a file, not a pipe");
                return(ST_EOF);
        }

        /* Read SampleVision header */
        if (st_readbuf(ft, (char *)&header, 1, HEADERSIZE) != HEADERSIZE)
        {
                st_fail_errno(ft,ST_EHDR,"unexpected EOF in SMP header");
                return(ST_EOF);
        }
        if (strncmp(header.Id, SVmagic, 17) != 0)
        {
                st_fail_errno(ft,ST_EHDR,"SMP header does not begin with magic word %s\n", SVmagic);
                return(ST_EOF);
        }
        if (strncmp(header.version, SVvers, 4) != 0)
        {
                st_fail_errno(ft,ST_EHDR,"SMP header is not version %s\n", SVvers);
                return(ST_EOF);
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

        st_report("SampleVision file name and comments: %s", ft->comment);
        /* Extract out the sample size (always intel format) */
        st_readdw(ft, &(smp->NoOfSamps));
        /* mark the start of the sample data */
        samplestart = st_tell(ft);

        /* seek from the current position (the start of sample data) by */
        /* NoOfSamps * sizeof(int16_t) */
        if (st_seeki(ft, smp->NoOfSamps * 2L, 1) == -1)
        {
                st_fail_errno(ft,errno,"SMP unable to seek to trailer");
                return(ST_EOF);
        }
        if (readtrailer(ft, &trailer))
        {
                st_fail_errno(ft,ST_EHDR,"unexpected EOF in SMP trailer");
                return(ST_EOF);
        }

        /* seek back to the beginning of the data */
        if (st_seeki(ft, samplestart, 0) == -1) 
        {
                st_fail_errno(ft,errno,"SMP unable to seek back to start of sample data");
                return(ST_EOF);
        }

        ft->info.rate = (int) trailer.rate;
        ft->info.size = ST_SIZE_WORD;
        ft->info.encoding = ST_ENCODING_SIGN2;
        ft->info.channels = 1;
        smp->dataStart = samplestart;
        ft->length = smp->NoOfSamps;

        st_report("SampleVision trailer:\n");
        for(i = 0; i < 8; i++) if (1 || trailer.loops[i].count) {
#ifdef __alpha__
                st_report("Loop %d: start: %6d", i, trailer.loops[i].start);
                st_report(" end:   %6d", trailer.loops[i].end);
#else
                st_report("Loop %d: start: %6ld", i, trailer.loops[i].start);
                st_report(" end:   %6ld", trailer.loops[i].end);
#endif
                st_report(" count: %6d", trailer.loops[i].count);
                switch(trailer.loops[i].type) {
                    case 0: st_report("type:  off\n"); break;
                    case 1: st_report("type:  forward\n"); break;
                    case 2: st_report("type:  forward/backward\n"); break;
                }
        }
        st_report("MIDI Note number: %d\n\n", trailer.MIDInote);

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
                ft->instr.loopmode = ST_LOOP_8;
        else
                ft->instr.loopmode = ST_LOOP_NONE;

        return(ST_SUCCESS);
}

/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */
st_ssize_t st_smpread(ft_t ft, st_sample_t *buf, st_ssize_t len) 
{
        smp_t smp = (smp_t) ft->priv;
        unsigned short datum;
        int done = 0;
        
        for(; done < len && smp->NoOfSamps; done++, smp->NoOfSamps--) {
                st_readw(ft, &datum);
                /* scale signed up to long's range */
                *buf++ = ST_SIGNED_WORD_TO_SAMPLE(datum);
        }
        return done;
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int st_smpstopread(ft_t ft) 
{
    return(ST_SUCCESS);
}

int st_smpstartwrite(ft_t ft) 
{
        smp_t smp = (smp_t) ft->priv;
        struct smpheader header;

        /* SMP is in Little Endian format.  Swap whats read in on */
        /* Big Endian machines.                                   */
        if (ST_IS_BIGENDIAN)
        {
                ft->swap = ft->swap ? 0 : 1;
        }

        /* If you have to seek around the output file */
        if (! ft->seekable)
        {
                st_fail_errno(ft,ST_EOF,"Output .smp file must be a file, not a pipe");
                return(ST_EOF);
        }

        /* If your format specifies any of the following info. */
        ft->info.size = ST_SIZE_WORD;
        ft->info.encoding = ST_ENCODING_SIGN2;
        ft->info.channels = 1;

        strcpy(header.Id, SVmagic);
        strcpy(header.version, SVvers);
        sprintf(header.comments, "%-*s", COMMENTLEN, "Converted using Sox.");
        sprintf(header.name, "%-*.*s", NAMELEN, NAMELEN, ft->comment);

        /* Write file header */
        if(st_writebuf(ft, &header, 1, HEADERSIZE) != HEADERSIZE)
        {
            st_fail_errno(ft,errno,"SMP: Can't write header completely");
            return(ST_EOF);
        }
        st_writedw(ft, 0);      /* write as zero length for now, update later */
        smp->NoOfSamps = 0;

        return(ST_SUCCESS);
}

st_ssize_t st_smpwrite(ft_t ft, st_sample_t *buf, st_ssize_t len) 
{
        smp_t smp = (smp_t) ft->priv;
        register int datum;
        st_ssize_t done = 0;

        while(done < len) {
                datum = (int) ST_SAMPLE_TO_SIGNED_WORD(*buf++);
                st_writew(ft, datum);
                smp->NoOfSamps++;
                done++;
        }

        return(done);
}

int st_smpstopwrite(ft_t ft) 
{
        smp_t smp = (smp_t) ft->priv;
        struct smptrailer trailer;

        /* Assign the trailer data */
        settrailer(ft, &trailer, ft->info.rate);
        writetrailer(ft, &trailer);
        if (st_seeki(ft, 112, 0) == -1)
        {
                st_fail_errno(ft,errno,"SMP unable to seek back to save size");
                return(ST_EOF);
        }
        st_writedw(ft, smp->NoOfSamps);

        return(ST_SUCCESS);
}

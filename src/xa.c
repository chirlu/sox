/*
 * xa.c  Support for Maxis .XA file format
 * 
 *      Copyright (C) 2006 Dwayne C. Litzenberger <dlitz@dlitz.net>
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Library General Public
 *   License as published by the Free Software Foundation; either
 *   version 2 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA  02110-1301
 *   USA
 *
 */

/* Thanks to Valery V. Anisimovsky <samael@avn.mccme.ru> for the 
 * "Maxis XA Audio File Format Description", dated 5-01-2002. */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>             /* For SEEK_* defines if not found in stdio */
#endif

#include "st_i.h"

#define HNIBBLE(byte) (((byte) >> 4) & 0xf)
#define LNIBBLE(byte) ((byte) & 0xf)

/* .xa file header */
typedef struct {
    char magic[4];  /* "XA\0\0", "XAI\0" (sound/speech), or "XAJ\0" (music) */
    uint32_t outSize;       /* decompressed size of the stream (in bytes) */

    /* WAVEFORMATEX structure for the decompressed data */
    uint16_t tag;           /* 0x0001 - PCM data */
    uint16_t channels;      /* number of channels */
    uint32_t sampleRate;    /* sample rate (samples/sec) */
    uint32_t avgByteRate;   /* sampleRate * align */
    uint16_t align;         /* bits / 8 * channels */
    uint16_t bits;          /* 8 or 16 */
} xa_header_t;

typedef struct {
    int32_t curSample;      /* current sample */
    int32_t prevSample;     /* previous sample */
    int32_t c1;
    int32_t c2;
    unsigned int shift;
} xa_state_t;

/* Private data for .xa file */
typedef struct xastuff {
    xa_header_t header;
    xa_state_t *state;
    unsigned int blockSize;
    unsigned int bufPos;    /* position within the current block */
    unsigned char *buf;     /* buffer for the current block */
    unsigned int bytesDecoded;  /* number of decompressed bytes read */
} *xa_t;

/* coefficients for EA ADPCM */
static int32_t EA_ADPCM_Table[]= {
    0, 240,  460,  392,
    0,   0, -208, -220,
    0,   1,    3,    4,
    7,   8,   10,   11,
    0,  -1,   -3,   -4
};

/* Clip sample to 16 bits */
static inline int32_t clip16(int32_t sample)
{
    if (sample > 32767) {
        return 32767;
    } else if (sample < -32768) {
        return -32768;
    } else {
        return sample;
    }
}

static int st_xastartread(ft_t ft)
{
    xa_t xa = (xa_t) ft->priv;
    char *magic = xa->header.magic;

    /* Check for the magic value */
    if (st_readbuf(ft, xa->header.magic, 1, 4) != 4 ||
        (memcmp("XA\0\0", xa->header.magic, 4) != 0 &&
         memcmp("XAI\0", xa->header.magic, 4) != 0 &&
         memcmp("XAJ\0", xa->header.magic, 4) != 0))
    {
        st_fail_errno(ft, ST_EHDR, "XA: Header not found");
        return ST_EOF;
    }
    
    /* Read the rest of the header */
    if (st_readdw(ft, &xa->header.outSize) != ST_SUCCESS) return ST_EOF;
    if (st_readw(ft, &xa->header.tag) != ST_SUCCESS) return ST_EOF;
    if (st_readw(ft, &xa->header.channels) != ST_SUCCESS) return ST_EOF;
    if (st_readdw(ft, &xa->header.sampleRate) != ST_SUCCESS) return ST_EOF;
    if (st_readdw(ft, &xa->header.avgByteRate) != ST_SUCCESS) return ST_EOF;
    if (st_readw(ft, &xa->header.align) != ST_SUCCESS) return ST_EOF;
    if (st_readw(ft, &xa->header.bits) != ST_SUCCESS) return ST_EOF;

    /* Output the data from the header */
    st_debug("XA Header:");
    st_debug(" szID:          %02x %02x %02x %02x  |%c%c%c%c|",
        magic[0], magic[1], magic[2], magic[3],
        (magic[0] >= 0x20 && magic[0] <= 0x7e) ? magic[0] : '.',
        (magic[1] >= 0x20 && magic[1] <= 0x7e) ? magic[1] : '.',
        (magic[2] >= 0x20 && magic[2] <= 0x7e) ? magic[2] : '.',
        (magic[3] >= 0x20 && magic[3] <= 0x7e) ? magic[3] : '.');
    st_debug(" dwOutSize:     %u", xa->header.outSize);
    st_debug(" wTag:          0x%04x", xa->header.tag);
    st_debug(" wChannels:     %u", xa->header.channels);
    st_debug(" dwSampleRate:  %u", xa->header.sampleRate);
    st_debug(" dwAvgByteRate: %u", xa->header.avgByteRate);
    st_debug(" wAlign:        %u", xa->header.align);
    st_debug(" wBits:         %u", xa->header.bits);

    /* Populate the st_soundstream structure */
    ft->signal.encoding = ST_ENCODING_SIGN2;
    
    if (ft->signal.size == -1 || ft->signal.size == (xa->header.bits >> 3)) {
        ft->signal.size = xa->header.bits >> 3;
    } else {
        st_report("User options overriding size read in .xa header");
    }
    
    if (ft->signal.channels == 0 || ft->signal.channels == xa->header.channels) {
        ft->signal.channels = xa->header.channels;
    } else {
        st_report("User options overriding channels read in .xa header");
    }
    
    if (ft->signal.rate == 0 || ft->signal.rate == xa->header.sampleRate) {
        ft->signal.rate = xa->header.sampleRate;
    } else {
        st_report("User options overriding rate read in .xa header");
    }
    
    /* Check for supported formats */
    if (ft->signal.size != 2) {
        st_fail_errno(ft, ST_EFMT, "%d-bit sample resolution not supported.",
            ft->signal.size << 3);
        return ST_EOF;
    }
    
    /* Validate the header */
    if (xa->header.bits != ft->signal.size << 3) {
        st_report("Invalid sample resolution %d bits.  Assuming %d bits.",
            xa->header.bits, ft->signal.size << 3);
        xa->header.bits = ft->signal.size << 3;
    }
    if (xa->header.align != ft->signal.size * xa->header.channels) {
        st_report("Invalid sample alignment value %d.  Assuming %d.",
            xa->header.align, ft->signal.size * xa->header.channels);
        xa->header.align = ft->signal.size * xa->header.channels;
    }
    if (xa->header.avgByteRate != (xa->header.align * xa->header.sampleRate)) {
        st_report("Invalid dwAvgByteRate value %d.  Assuming %d.",
            xa->header.avgByteRate, xa->header.align * xa->header.sampleRate);
        xa->header.avgByteRate = xa->header.align * xa->header.sampleRate;
    }

    /* Set up the block buffer */
    xa->blockSize = ft->signal.channels * 0xf;
    xa->bufPos = xa->blockSize;

    /* Allocate memory for the block buffer */
    xa->buf = (unsigned char *)xcalloc(1, xa->blockSize);
    
    /* Allocate memory for the state */
    xa->state = (xa_state_t *)xcalloc(sizeof(xa_state_t), ft->signal.channels);
    
    /* Final initialization */
    xa->bytesDecoded = 0;
    
    return ST_SUCCESS;
}

/* 
 * Read up to len samples from a file, converted to signed longs.
 * Return the number of samples read.
 */
static st_size_t st_xaread(ft_t ft, st_sample_t *buf, st_size_t len)
{
    xa_t xa = (xa_t) ft->priv;
    int32_t sample;
    unsigned char inByte;
    size_t i, done, bytes;

    ft->st_errno = ST_SUCCESS;
    done = 0;
    while (done < len) {
        if (xa->bufPos >= xa->blockSize) {
            /* Read the next block */
            bytes = st_readbuf(ft, xa->buf, 1, xa->blockSize);
            if (bytes < xa->blockSize) {
                if (st_eof(ft)) {
                    if (done > 0) {
                        return done;
                    }
                    st_fail_errno(ft,ST_EOF,"Premature EOF on .xa input file");
                    return ST_EOF;
                } else {
                    /* error */
                    st_fail_errno(ft,ST_EOF,"read error on input stream");
                    return ST_EOF;
                }
            }
            xa->bufPos = 0;
            
            for (i = 0; i < ft->signal.channels; i++) {
                inByte = xa->buf[i];
                xa->state[i].c1 = EA_ADPCM_Table[HNIBBLE(inByte)];
                xa->state[i].c2 = EA_ADPCM_Table[HNIBBLE(inByte) + 4];
                xa->state[i].shift = LNIBBLE(inByte) + 8;
            }
            xa->bufPos += ft->signal.channels;
        } else {
            /* Process the block */
            for (i = 0; i < ft->signal.channels && done < len; i++) {
                /* high nibble */
                sample = HNIBBLE(xa->buf[xa->bufPos+i]);
                sample = (sample << 28) >> xa->state[i].shift;
                sample = (sample +
                          xa->state[i].curSample * xa->state[i].c1 +
                          xa->state[i].prevSample * xa->state[i].c2 + 0x80) >> 8;
                sample = clip16(sample);
                xa->state[i].prevSample = xa->state[i].curSample;
                xa->state[i].curSample = sample;
                
                buf[done++] = ST_SIGNED_WORD_TO_SAMPLE(sample,);
                xa->bytesDecoded += ft->signal.size;
            }
            for (i = 0; i < ft->signal.channels && done < len; i++) {
                /* low nibble */
                sample = LNIBBLE(xa->buf[xa->bufPos+i]);
                sample = (sample << 28) >> xa->state[i].shift;
                sample = (sample +
                          xa->state[i].curSample * xa->state[i].c1 +
                          xa->state[i].prevSample * xa->state[i].c2 + 0x80) >> 8;
                sample = clip16(sample);
                xa->state[i].prevSample = xa->state[i].curSample;
                xa->state[i].curSample = sample;
                
                buf[done++] = ST_SIGNED_WORD_TO_SAMPLE(sample,);
                xa->bytesDecoded += ft->signal.size;
            }

            xa->bufPos += ft->signal.channels;
        }
    }
    if (done == 0) {
        return ST_EOF;
    }
    return done;
}

static int st_xastopread(ft_t ft)
{
    xa_t xa = (xa_t) ft->priv;

    ft->st_errno = ST_SUCCESS;

    /* Free memory */
    free(xa->buf);
    xa->buf = NULL;
    free(xa->state);
    xa->state = NULL;
    
    return ST_SUCCESS;
}

static int st_xastartwrite(ft_t ft)
{
    st_fail_errno(ft, ST_ENOTSUP, ".XA writing not supported");
    return ST_EOF;
}

static st_size_t st_xawrite(ft_t ft, const st_sample_t *buf UNUSED, st_size_t len UNUSED)
{
    st_fail_errno(ft, ST_ENOTSUP, ".XA writing not supported");
    return ST_EOF;
}

static int st_xastopwrite(ft_t ft)
{
    st_fail_errno(ft, ST_ENOTSUP, ".XA writing not supported");
    return ST_EOF;
}

static int st_xaseek(ft_t ft, st_size_t offset)
{
    return st_format_nothing_seek(ft, offset);
}

/* Maxis .xa */
static const char *xanames[] = {
    "xa",
    NULL
};

st_format_t st_xa_format = {
  xanames,
  NULL,
  ST_FILE_LIT_END,
  st_xastartread,
  st_xaread,
  st_xastopread,
  st_xastartwrite,
  st_xawrite,
  st_xastopwrite,
  st_xaseek
};

const st_format_t *st_xa_format_fn(void)
{
  return &st_xa_format;
}

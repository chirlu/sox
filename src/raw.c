/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

/*
 * Sound Tools raw format file.
 *
 * Includes .ub, .uw, .sb, .sw, and .ul formats at end
 */

/*
 * Notes: most of the headerless formats set their handlers to raw
 * in their startread/write routines.
 *
 */

#include "st_i.h"
#include "g711.h"

#include <string.h>
#include <stdlib.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifndef HAVE_MEMMOVE
#define memmove(dest, src, len) bcopy((src), (dest), (len))
#endif

#define MAXWSPEED 1

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

/* Lookup table to reverse the bit order of a byte. ie MSB become LSB */
unsigned char cswap[256] = {
  0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0,
  0x30, 0xB0, 0x70, 0xF0, 0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 
  0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, 0x04, 0x84, 0x44, 0xC4, 
  0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
  0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC,
  0x3C, 0xBC, 0x7C, 0xFC, 0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 
  0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2, 0x0A, 0x8A, 0x4A, 0xCA, 
  0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
  0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6,
  0x36, 0xB6, 0x76, 0xF6, 0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 
  0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE, 0x01, 0x81, 0x41, 0xC1, 
  0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
  0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9,
  0x39, 0xB9, 0x79, 0xF9, 0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 
  0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5, 0x0D, 0x8D, 0x4D, 0xCD, 
  0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
  0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3,
  0x33, 0xB3, 0x73, 0xF3, 0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 
  0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB, 0x07, 0x87, 0x47, 0xC7, 
  0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
  0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF,
  0x3F, 0xBF, 0x7F, 0xFF
};

#define ST_ULAW_BYTE_TO_SAMPLE(d) ((st_sample_t)(st_ulaw2linear16(d)) << 16)
#define ST_ALAW_BYTE_TO_SAMPLE(d) ((st_sample_t)(st_alaw2linear16(d)) << 16)
#define ST_SAMPLE_TO_ULAW_BYTE(d) (st_14linear2ulaw((int16_t)((d) >> 18)))
#define ST_SAMPLE_TO_ALAW_BYTE(d) (st_13linear2alaw((int16_t)((d) >> 19)))

/* Some hardware sends MSB last. These account for that */
#define ST_INVERT_ULAW_BYTE_TO_SAMPLE(d) \
    ((st_sample_t)(st_ulaw2linear16(cswap[d])) << 16)
#define ST_INVERT_ALAW_BYTE_TO_SAMPLE(d) \
    ((st_sample_t)(st_alaw2linear16(cswap[d])) << 16)
#define ST_SAMPLE_TO_INVERT_ULAW_BYTE(d) \
    (cswap[st_14linear2ulaw((int16_t)((d) >> 18))])
#define ST_SAMPLE_TO_INVERT_ALAW_BYTE(d) \
    (cswap[st_13linear2alaw((int16_t)((d) >> 19))])

static void rawdefaults(ft_t ft);

int st_rawseek(ft_t ft, st_size_t offset)
{
    st_size_t new_offset, channel_block, alignment;

    switch(ft->info.size) {
        case ST_SIZE_BYTE:
        case ST_SIZE_WORD:
        case ST_SIZE_DWORD:
        case ST_SIZE_DDWORD:
            break;
        default:
            st_fail_errno(ft,ST_ENOTSUP,"Can't seek this data size");
            return ft->st_errno;
    }

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

    ft->st_errno = st_seeki(ft, new_offset, SEEK_SET);

    return ft->st_errno;
}

int st_rawstartread(ft_t ft)
{
    ft->file.buf = (char *)malloc(ST_BUFSIZ);
    if (!ft->file.buf)
    {
        st_fail_errno(ft,ST_ENOMEM,"Unable to alloc resources");
        return ST_EOF;
    }
    ft->file.size = ST_BUFSIZ;
    ft->file.count = 0;
    ft->file.pos = 0;
    ft->file.eof = 0;

    return ST_SUCCESS;
}

int st_rawstartwrite(ft_t ft)
{
    ft->file.buf = (char *)malloc(ST_BUFSIZ);
    if (!ft->file.buf)
    {
        st_fail_errno(ft,ST_ENOMEM,"Unable to alloc resources");
        return ST_EOF;
    }
    ft->file.size = ST_BUFSIZ;
    ft->file.pos = 0;
    ft->file.eof = 0;

    return ST_SUCCESS;
}

void st_ub_read_buf(st_sample_t *buf1, char *buf2, st_ssize_t len, char swap)
{
    while (len)
    {
        uint8_t datum;

        datum = *((unsigned char *)buf2);
        buf2++;

        *buf1++ = ST_UNSIGNED_BYTE_TO_SAMPLE(datum);
        len--;
    }
}

void st_sb_read_buf(st_sample_t *buf1, char *buf2, st_ssize_t len, char swap)
{
    while (len)
    {
        int8_t datum;

        datum = *((int8_t *)buf2);
        buf2++;

        *buf1++ = ST_SIGNED_BYTE_TO_SAMPLE(datum);
        len--;
    }
}

void st_ulaw_read_buf(st_sample_t *buf1, char *buf2, st_ssize_t len,
                      char swap)
{
    while (len)
    {
        uint8_t datum;

        datum = *((uint8_t *)buf2);
        buf2++;

        *buf1++ = ST_ULAW_BYTE_TO_SAMPLE(datum);
        len--;
    }
}

void st_alaw_read_buf(st_sample_t *buf1, char *buf2, st_ssize_t len,
                      char swap)
{
    while (len)
    {
        uint8_t datum;

        datum = *((uint8_t *)buf2);
        buf2++;

        *buf1++ = ST_ALAW_BYTE_TO_SAMPLE(datum);
        len--;
    }
}

void st_inv_ulaw_read_buf(st_sample_t *buf1, char *buf2, st_ssize_t len,
                          char swap)
{
    while (len)
    {
        uint8_t datum;

        datum = *((uint8_t *)buf2);
        buf2++;

        *buf1++ = ST_INVERT_ULAW_BYTE_TO_SAMPLE(datum);
        len--;
    }
}

void st_inv_alaw_read_buf(st_sample_t *buf1, char *buf2, st_ssize_t len,
                          char swap)
{
    while (len)
    {
        uint8_t datum;

        datum = *((uint8_t *)buf2);
        buf2++;

        *buf1++ = ST_INVERT_ALAW_BYTE_TO_SAMPLE(datum);
        len--;
    }
}


void st_uw_read_buf(st_sample_t *buf1, char *buf2, st_ssize_t len, char swap)
{
    while (len)
    {
        uint16_t datum;

        datum = *((uint16_t *)buf2);
        buf2++; buf2++;
        if (swap)
            datum = st_swapw(datum);

        *buf1++ = ST_UNSIGNED_WORD_TO_SAMPLE(datum);
        len--;
    }
}

void st_sw_read_buf(st_sample_t *buf1, char *buf2, st_ssize_t len, char swap)
{
    while (len)
    {
        int16_t datum;

        datum = *((int16_t *)buf2);
        buf2++; buf2++;
        if (swap)
            datum = st_swapw(datum);

        *buf1++ = ST_SIGNED_WORD_TO_SAMPLE(datum);
        len--;
    }
}

void st_udw_read_buf(st_sample_t *buf1, char *buf2, st_ssize_t len, char swap)
{
    while (len)
    {
        uint32_t datum;

        datum = *((uint32_t *)buf2);
        buf2++; buf2++; buf2++; buf2++;
        if (swap)
            datum = st_swapdw(datum);

        *buf1++ = ST_UNSIGNED_DWORD_TO_SAMPLE(datum);
        len--;
    }
}

void st_dw_read_buf(st_sample_t *buf1, char *buf2, st_ssize_t len, char swap)
{
    while (len)
    {
        int32_t datum;

        datum = *((int32_t *)buf2);
        buf2++; buf2++; buf2++; buf2++;
        if (swap)
            datum = st_swapdw(datum);

        *buf1++ = ST_SIGNED_DWORD_TO_SAMPLE(datum);
        len--;
    }
}

void st_f32_read_buf(st_sample_t *buf1, char *buf2, st_ssize_t len, char swap)
{
    while (len)
    {
        float datum;

        datum = *((float *)buf2);
        buf2++; buf2++; buf2++; buf2++;
        if (swap)
            datum = st_swapf(datum);

        *buf1++ = ST_FLOAT_DWORD_TO_SAMPLE(datum);
        len--;
    }
}

void st_f64_read_buf(st_sample_t *buf1, char *buf2, st_ssize_t len, char swap)
{
    while (len)
    {
        double datum;

        datum = *((double *)buf2);
        buf2++; buf2++; buf2++; buf2++;
        buf2++; buf2++; buf2++; buf2++;
        if (swap)
            datum = st_swapd(datum);

        *buf1++ = ST_FLOAT_DDWORD_TO_SAMPLE(datum);
        len--;
    }
}

/* Reads a buffer of different data types into SoX's internal buffer
 * format.
 */
/* FIXME:  This function adds buffering on top of stdio's buffering.
 * Mixing st_rawreads's and freads or fgetc or even SoX's other util
 * functions will cause a loss of data!  Need to have sox implement
 * a consistent buffering protocol.
 */
st_ssize_t st_rawread(ft_t ft, st_sample_t *buf, st_ssize_t nsamp)
{
    st_ssize_t len, done = 0;
    void (*read_buf)(st_sample_t *, char *, st_ssize_t, char) = 0;
    size_t i;

    if (nsamp < 0)
    {
        st_fail_errno(ft,ST_EINVAL,"st_rawread requires positive sizes");
        return ST_EOF;
    }

    switch(ft->info.size) {
        case ST_SIZE_BYTE:
            switch(ft->info.encoding)
            {
                case ST_ENCODING_SIGN2:
                    read_buf = st_sb_read_buf;
                    break;
                case ST_ENCODING_UNSIGNED:
                    read_buf = st_ub_read_buf;
                    break;
                case ST_ENCODING_ULAW:
                    read_buf = st_ulaw_read_buf;
                    break;
                case ST_ENCODING_ALAW:
                    read_buf = st_alaw_read_buf;
                    break;
                case ST_ENCODING_INV_ULAW:
                    read_buf = st_inv_ulaw_read_buf;
                    break;
                case ST_ENCODING_INV_ALAW:
                    read_buf = st_inv_alaw_read_buf;
                    break;
                default:
                    st_fail_errno(ft,ST_EFMT,"Do not support this encoding for this data size");
                    return ST_EOF;
            }
            break;

        case ST_SIZE_WORD:
            switch(ft->info.encoding)
            {
                case ST_ENCODING_SIGN2:
                    read_buf = st_sw_read_buf;
                    break;
                case ST_ENCODING_UNSIGNED:
                    read_buf = st_uw_read_buf;
                    break;
                default:
                    st_fail_errno(ft,ST_EFMT,"Do not support this encoding for this data size");
                    return ST_EOF;
            }
            break;

        case ST_SIZE_DWORD:
            switch(ft->info.encoding)
            {
                case ST_ENCODING_SIGN2:
                    read_buf = st_dw_read_buf;
                    break;
                case ST_ENCODING_UNSIGNED:
                    read_buf = st_udw_read_buf;
                    break;
                case ST_ENCODING_FLOAT:
                    read_buf = st_f32_read_buf;
                    break;
                default:
                    st_fail_errno(ft,ST_EFMT,"Do not support this encoding for this data size");
                    return ST_EOF;
            }
            break;

        case ST_SIZE_DDWORD:
            switch(ft->info.encoding)
            {
                case ST_ENCODING_FLOAT:
                    read_buf = st_f64_read_buf;
                    break;
                default:
                    st_fail_errno(ft,ST_EFMT,"Do not support this encoding for this data size");
            }
            break;

        default:
            st_fail_errno(ft,ST_EFMT,"Do not support this data size for this handler");
            return ST_EOF;
    }


    len = MIN((st_size_t)nsamp,(ft->file.count-ft->file.pos)/ft->info.size);
    if (len)
    {
        read_buf(buf + done, ft->file.buf + ft->file.pos, len, ft->swap);
        ft->file.pos += (len*ft->info.size);
        done += len;
    }

    while (done < nsamp)
    {
        /* See if there is not enough data in buffer for any more reads
         * or if there is no data in the buffer at all.
         * If not then shift any remaining data down to the beginning
         * and attempt to fill up the rest of the buffer.
         */
        if (!ft->file.eof && (ft->file.count == 0 ||
                              ft->file.pos >= (ft->file.count-ft->info.size+1)))
        {
            for (i = 0; i < (ft->file.count-ft->file.pos); i++)
                ft->file.buf[i] = ft->file.buf[ft->file.pos+i];

            i = ft->file.count-ft->file.pos;
            ft->file.pos = 0;

            ft->file.count = st_readbuf(ft, ft->file.buf+i, 1, ft->file.size-i);
            if (ft->file.count != ft->file.size-i || ft->file.count == 0)
            {
                ft->file.eof = 1;
            }
            ft->file.count += i;
        }

        len = MIN((st_size_t)nsamp - done,(ft->file.count-ft->file.pos)/ft->info.size);
        if (len)
        {
            read_buf(buf + done, ft->file.buf + ft->file.pos, len, ft->swap);
            ft->file.pos += (len*ft->info.size);
            done += len;
        }
        if (ft->file.eof)
            break;
    }

    if (done == 0 && ft->file.eof)
        return ST_EOF;

    return done;
}

int st_rawstopread(ft_t ft)
{
        free(ft->file.buf);

        return ST_SUCCESS;
}

void st_ub_write_buf(char* buf1, st_sample_t *buf2, st_ssize_t len, char swap)
{
    while (len)
    {
        *(uint8_t *)buf1++ = ST_SAMPLE_TO_UNSIGNED_BYTE(*buf2++);
        len--;
    }
}

void st_sb_write_buf(char *buf1, st_sample_t *buf2, st_ssize_t len, char swap)
{
    while (len)
    {
        *(int8_t *)buf1++ = ST_SAMPLE_TO_SIGNED_BYTE(*buf2++);
        len--;
    }
}

void st_ulaw_write_buf(char *buf1, st_sample_t *buf2, st_ssize_t len,
                       char swap)
{
    while (len)
    {
        *(uint8_t *)buf1++ = ST_SAMPLE_TO_ULAW_BYTE(*buf2++);
        len--;
    }
}

void st_alaw_write_buf(char *buf1, st_sample_t *buf2, st_ssize_t len,
                       char swap)
{
    while (len)
    {
        *(uint8_t *)buf1++ = ST_SAMPLE_TO_ALAW_BYTE(*buf2++);
        len--;
    }
}

void st_inv_ulaw_write_buf(char *buf1, st_sample_t *buf2, st_ssize_t len,
                           char swap)
{
    while (len)
    {
        *(uint8_t *)buf1++ = ST_SAMPLE_TO_INVERT_ULAW_BYTE(*buf2++);
        len--;
    }
}

void st_inv_alaw_write_buf(char *buf1, st_sample_t *buf2, st_ssize_t len,
                           char swap)
{
    while (len)
    {
        *(uint8_t *)buf1++ = ST_SAMPLE_TO_INVERT_ALAW_BYTE(*buf2++);
        len--;
    }
}

void st_uw_write_buf(char *buf1, st_sample_t *buf2, st_ssize_t len, char swap)
{
    while (len)
    {
        uint16_t datum;

        datum = ST_SAMPLE_TO_UNSIGNED_WORD(*buf2++);
        if (swap)
            datum = st_swapw(datum);
        *(uint16_t *)buf1 = datum;
        buf1++; buf1++;

        len--;
    }
}

void st_sw_write_buf(char *buf1, st_sample_t *buf2, st_ssize_t len, char swap)
{
    while (len)
    {
        int16_t datum;

        datum = ST_SAMPLE_TO_SIGNED_WORD(*buf2++);
        if (swap)
            datum = st_swapw(datum);
        *(int16_t *)buf1 = datum;
        buf1++; buf1++;

        len--;
    }
}

void st_udw_write_buf(char *buf1, st_sample_t *buf2, st_ssize_t len, char swap)
{
    while (len)
    {
        uint32_t datum;

        datum = ST_SAMPLE_TO_UNSIGNED_DWORD(*buf2++);
        if (swap)
            datum = st_swapdw(datum);
        *(uint32_t *)buf1 = datum;
        buf1++; buf1++; buf1++; buf1++;

        len--;
    }
}

void st_dw_write_buf(char *buf1, st_sample_t *buf2, st_ssize_t len, char swap)
{
    while (len)
    {
        int32_t datum;

        datum = ST_SAMPLE_TO_SIGNED_DWORD(*buf2++);
        if (swap)
            datum = st_swapdw(datum);
        *(int32_t *)buf1 = datum;
        buf1++; buf1++; buf1++; buf1++;

        len--;
    }
}

void st_f32_write_buf(char *buf1, st_sample_t *buf2, st_ssize_t len, char swap)
{
    while (len)
    {
        float datum;

        datum = ST_SAMPLE_TO_FLOAT_DWORD(*buf2++);
        if (swap)
            datum = st_swapf(datum);
        *(float *)buf1 = datum;
        buf1++; buf1++; buf1++; buf1++;

        len--;
    }
}

void st_f64_write_buf(char *buf1, st_sample_t *buf2, st_ssize_t len, char swap)
{
    while (len)
    {
        double datum;

        datum = ST_SAMPLE_TO_FLOAT_DDWORD(*buf2++);
        if (swap)
            datum = st_swapf(datum);
        *(double *)buf1 = datum;
        buf1++; buf1++; buf1++; buf1++;
        buf1++; buf1++; buf1++; buf1++;

        len--;
    }
}


static void writeflush(ft_t ft)
{
        if (st_writebuf(ft, ft->file.buf, 1, ft->file.pos) != ft->file.pos)
        {
            ft->file.eof = ST_EOF;
        }
        ft->file.pos = 0;
}


/* Writes SoX's internal buffer format to buffer of various data types.
 */
/* FIXME:  This function adds buffering on top of stdio's buffering.
 * Mixing st_rawwrites's and fwrites or even SoX's other util
 * functions will cause a loss of data!  Need to have sox implement
 * a consistent buffering protocol.
 */
st_ssize_t st_rawwrite(ft_t ft, st_sample_t *buf, st_ssize_t nsamp)
{
    st_ssize_t len, done = 0;
    void (*write_buf)(char *, st_sample_t *, st_ssize_t, char) = 0;

    switch(ft->info.size) {
        case ST_SIZE_BYTE:
            switch(ft->info.encoding)
            {
                case ST_ENCODING_SIGN2:
                    write_buf = st_sb_write_buf;
                    break;
                case ST_ENCODING_UNSIGNED:
                    write_buf = st_ub_write_buf;
                    break;
                case ST_ENCODING_ULAW:
                    write_buf = st_ulaw_write_buf;
                    break;
                case ST_ENCODING_ALAW:
                    write_buf = st_alaw_write_buf;
                    break;
                case ST_ENCODING_INV_ULAW:
                    write_buf = st_inv_ulaw_write_buf;
                    break;
                case ST_ENCODING_INV_ALAW:
                    write_buf = st_inv_alaw_write_buf;
                    break;
                default:
                    st_fail_errno(ft,ST_EFMT,"Do not support this encoding for this data size");
                    return ST_EOF;
            }
            break;

        case ST_SIZE_WORD:
            switch(ft->info.encoding)
            {
                case ST_ENCODING_SIGN2:
                    write_buf = st_sw_write_buf;
                    break;
                case ST_ENCODING_UNSIGNED:
                    write_buf = st_uw_write_buf;
                    break;
                default:
                    st_fail_errno(ft,ST_EFMT,"Do not support this encoding for this data size");
                    return ST_EOF;
            }
            break;

        case ST_SIZE_DWORD:
            switch(ft->info.encoding)
            {
                case ST_ENCODING_SIGN2:
                    write_buf = st_dw_write_buf;
                    break;
                case ST_ENCODING_UNSIGNED:
                    write_buf = st_udw_write_buf;
                    break;
                case ST_ENCODING_FLOAT:
                    write_buf = st_f32_write_buf;
                    break;
                default:
                    st_fail_errno(ft,ST_EFMT,"Do not support this encoding for this data size");
                    return ST_EOF;
            }
            break;

        case ST_SIZE_DDWORD:
            switch(ft->info.encoding)
            {
                case ST_ENCODING_FLOAT:
                    write_buf = st_f64_write_buf;
                    break;
                default:
                    st_fail_errno(ft,ST_EFMT,"Do not support this encoding for this data size");
                    return ST_EOF;
            }
            break;

        default:
            st_fail_errno(ft,ST_EFMT,"Do not support this data size for this handler");
            return ST_EOF;
    }

    while (done < nsamp && !ft->file.eof)
    {
        if (ft->file.pos > (ft->file.size-ft->info.size))
        {
            writeflush(ft);
        }

        len = MIN(nsamp-done,(ft->file.size-ft->file.pos)/ft->info.size);
        if (len)
        {
            write_buf(ft->file.buf + ft->file.pos, buf+done, len, ft->swap);
            ft->file.pos += (len*ft->info.size);
            done += len;
        }
    }
    return done;
}

int st_rawstopwrite(ft_t ft)
{
        writeflush(ft);
        free(ft->file.buf);
        return ST_SUCCESS;
}

/*
* Set parameters to the fixed parameters known for this format,
* and change format to raw format.
*/

#define STARTREAD(NAME,SIZE,STYLE) \
int NAME(ft_t ft) \
{ \
        ft->info.size = SIZE; \
        ft->info.encoding = STYLE; \
        rawdefaults(ft); \
        return st_rawstartread(ft); \
}

#define STARTWRITE(NAME,SIZE,STYLE)\
int NAME(ft_t ft) \
{ \
        ft->info.size = SIZE; \
        ft->info.encoding = STYLE; \
        rawdefaults(ft); \
        return st_rawstartwrite(ft); \
}

STARTREAD(st_sbstartread,ST_SIZE_BYTE,ST_ENCODING_SIGN2)
STARTWRITE(st_sbstartwrite,ST_SIZE_BYTE,ST_ENCODING_SIGN2)

STARTREAD(st_ubstartread,ST_SIZE_BYTE,ST_ENCODING_UNSIGNED)
STARTWRITE(st_ubstartwrite,ST_SIZE_BYTE,ST_ENCODING_UNSIGNED)

STARTREAD(st_uwstartread,ST_SIZE_WORD,ST_ENCODING_UNSIGNED)
STARTWRITE(st_uwstartwrite,ST_SIZE_WORD,ST_ENCODING_UNSIGNED)

STARTREAD(st_swstartread,ST_SIZE_WORD,ST_ENCODING_SIGN2)
STARTWRITE(st_swstartwrite,ST_SIZE_WORD,ST_ENCODING_SIGN2)

STARTREAD(st_slstartread,ST_SIZE_DWORD,ST_ENCODING_SIGN2)
STARTWRITE(st_slstartwrite,ST_SIZE_DWORD,ST_ENCODING_SIGN2)

STARTREAD(st_ulstartread,ST_SIZE_BYTE,ST_ENCODING_ULAW)
STARTWRITE(st_ulstartwrite,ST_SIZE_BYTE,ST_ENCODING_ULAW)

STARTREAD(st_alstartread,ST_SIZE_BYTE,ST_ENCODING_ALAW)
STARTWRITE(st_alstartwrite,ST_SIZE_BYTE,ST_ENCODING_ALAW)

STARTREAD(st_lustartread,ST_SIZE_BYTE,ST_ENCODING_INV_ULAW)
STARTWRITE(st_lustartwrite,ST_SIZE_BYTE,ST_ENCODING_INV_ULAW)

STARTREAD(st_lastartread,ST_SIZE_BYTE,ST_ENCODING_INV_ALAW)
STARTWRITE(st_lastartwrite,ST_SIZE_BYTE,ST_ENCODING_INV_ALAW)

void rawdefaults(ft_t ft)
{
        if (ft->info.rate == 0)
                ft->info.rate = 8000;
        if (ft->info.channels == -1)
                ft->info.channels = 1;
}

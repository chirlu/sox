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

#define ST_ULAW_BYTE_TO_SAMPLE(d) ((st_sample_t)(st_ulaw2linear16(d)) << 16)
#define ST_ALAW_BYTE_TO_SAMPLE(d) ((st_sample_t)(st_alaw2linear16(d)) << 16)
#define ST_SAMPLE_TO_ULAW_BYTE(d) (st_14linear2ulaw((int16_t)((d) >> 18)))
#define ST_SAMPLE_TO_ALAW_BYTE(d) (st_13linear2alaw((int16_t)((d) >> 19)))

static void rawdefaults(ft_t ft);

int st_rawseek(ft_t ft, st_size_t offset)
{
    switch(ft->info.size) {
        case ST_SIZE_BYTE:
        case ST_SIZE_WORD:
        case ST_SIZE_DWORD:
        case ST_SIZE_DDWORD:
            break;
        default:
            st_fail_errno(ft,ST_ENOTSUP,"Can't seek this data size");
            return(ft->st_errno);
    }

    ft->st_errno = st_seek(ft,offset*ft->info.size,SEEK_SET);

    return(ft->st_errno);
}

int st_rawstartread(ft_t ft)
{
    ft->file.buf = malloc(BUFSIZ);
    if (!ft->file.buf)
    {
        st_fail_errno(ft,ST_ENOMEM,"Unable to alloc resources");
        return(ST_EOF);
    }
    ft->file.size = BUFSIZ;
    ft->file.count = 0;
    ft->file.pos = 0;
    ft->file.eof = 0;

    return(ST_SUCCESS);
}

int st_rawstartwrite(ft_t ft)
{
    ft->file.buf = malloc(BUFSIZ);
    if (!ft->file.buf)
    {
        st_fail_errno(ft,ST_ENOMEM,"Unable to alloc resources");
        return(ST_EOF);
    }
    ft->file.size = BUFSIZ;
    ft->file.pos = 0;
    ft->file.eof = 0;

    return(ST_SUCCESS);
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
    int i;

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
                default:
                    st_fail_errno(ft,ST_EFMT,"Do not support this encoding for this data size");
                    return(0);
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
                    return(0);
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
                    return(0);
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
            return (0);
    }


    len = MIN(nsamp,(ft->file.count-ft->file.pos)/ft->info.size);
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

            ft->file.count = fread(ft->file.buf+i, 1, ft->file.size-i, ft->fp) ;
            if (ft->file.count == 0)
            {
                ft->file.eof = 1;
            }
            ft->file.count += i;
        }

        len = MIN(nsamp - done,(ft->file.count-ft->file.pos)/ft->info.size);
        if (len)
        {
            read_buf(buf + done, ft->file.buf + ft->file.pos, len, ft->swap);
            ft->file.pos += (len*ft->info.size);
            done += len;
        }
        if (ft->file.eof)
            break;
    }
    return done;
}

int st_rawstopread(ft_t ft)
{
        free(ft->file.buf);

        return(ST_SUCCESS);
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
        if (fwrite(ft->file.buf, 1, ft->file.pos, ft->fp) != ft->file.pos)
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
                default:
                    st_fail_errno(ft,ST_EFMT,"Do not support this encoding for this data size");
                    return(0);
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
                    return(0);
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
                    return(0);
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
            }
            break;

        default:
            st_fail_errno(ft,ST_EFMT,"Do not support this data size for this handler");
            return (0);
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
        return(ST_SUCCESS);
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

void rawdefaults(ft_t ft)
{
        if (ft->info.rate == 0)
                ft->info.rate = 8000;
        if (ft->info.channels == -1)
                ft->info.channels = 1;
}

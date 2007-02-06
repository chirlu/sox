/*
 *      CVSD (Continuously Variable Slope Delta modulation)
 *      conversion routines
 *
 *      The CVSD format is described in the MIL Std 188 113, which is
 *      available from http://bbs.itsi.disa.mil:5580/T3564
 *
 *      Copyright (C) 1996  
 *      Thomas Sailer (sailer@ife.ee.ethz.ch) (HB9JNX/AE4WA)
 *      Swiss Federal Institute of Technology, Electronics Lab
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Change History:
 *
 * June 1, 1998 - Chris Bagwell (cbagwell@sprynet.com)
 *   Fixed compile warnings reported by Kjetil Torgrim Homme
 *   <kjetilho@ifi.uio.no>
 *
 *
 */

/* ---------------------------------------------------------------------- */

#include "st_i.h"

#include <math.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>     /* For SEEK_* defines if not found in stdio */
#endif

#include "cvsdfilt.h"

/* ---------------------------------------------------------------------- */
/*
 * private data structures
 */

struct cvsd_common_state {
        unsigned overload;
        float mla_int;
        float mla_tc0;
        float mla_tc1;
        unsigned phase;
        unsigned phase_inc;
        float v_min, v_max;
};

struct cvsd_decode_state {
        float output_filter[DEC_FILTERLEN];
};

struct cvsd_encode_state {
        float recon_int;
        float input_filter[ENC_FILTERLEN];
};

struct cvsdpriv {
        struct cvsd_common_state com;
        union {
                struct cvsd_decode_state dec;
                struct cvsd_encode_state enc;
        } c;
        struct {
                unsigned char shreg;
                unsigned mask;
                unsigned cnt;
        } bit;
        unsigned bytes_written;
        unsigned cvsd_rate;
};

static int debug_count = 0;

/* ---------------------------------------------------------------------- */

static float float_conv(float *fp1, float *fp2,int n)
{
        float res = 0;
        for(; n > 0; n--)
                res += (*fp1++) * (*fp2++);
        return res;
}

/* ---------------------------------------------------------------------- */
/*
 * some remarks about the implementation of the CVSD decoder
 * the principal integrator is integrated into the output filter
 * to achieve this, the coefficients of the output filter are multiplied
 * with (1/(1-1/z)) in the initialisation code.
 * the output filter must have a sharp zero at f=0 (i.e. the sum of the
 * filter parameters must be zero). This prevents an accumulation of
 * DC voltage at the principal integration.
 */
/* ---------------------------------------------------------------------- */

static void cvsdstartcommon(ft_t ft)
{
        struct cvsdpriv *p = (struct cvsdpriv *) ft->priv;
        
        p->cvsd_rate = (ft->signal.rate <= 24000) ? 16000 : 32000;
        ft->signal.rate = 8000;
        ft->signal.channels = 1;
        ft->signal.size = ST_SIZE_16BIT; /* make output format default to words */
        ft->signal.encoding = ST_ENCODING_SIGN2;
        /*
         * initialize the decoder
         */
        p->com.overload = 0x5;
        p->com.mla_int = 0;
        /*
         * timeconst = (1/e)^(200 / SR) = exp(-200/SR)
         * SR is the sampling rate
         */
        p->com.mla_tc0 = exp((-200.0)/((float)(p->cvsd_rate)));
        /*
         * phase_inc = 32000 / SR
         */
        p->com.phase_inc = 32000 / p->cvsd_rate;
        /*
         * initialize bit shift register
         */
        p->bit.shreg = p->bit.cnt = 0;
        p->bit.mask = 1;
        /*
         * count the bytes written
         */
        p->bytes_written = 0;
        p->com.v_min = 1;
        p->com.v_max = -1;
        st_report("cvsd: bit rate %dbit/s, bits from %s", p->cvsd_rate,
               ft->signal.reverse_bits ? "msb to lsb" : "lsb to msb");
}

/* ---------------------------------------------------------------------- */

static int st_cvsdstartread(ft_t ft) 
{
        struct cvsdpriv *p = (struct cvsdpriv *) ft->priv;
        float *fp1;
        int i;

        cvsdstartcommon(ft);

        p->com.mla_tc1 = 0.1 * (1 - p->com.mla_tc0);
        p->com.phase = 0;
        /*
         * initialize the output filter coeffs (i.e. multiply
         * the coeffs with (1/(1-1/z)) to achieve integration
         * this is now done in the filter parameter generation utility
         */
        /*
         * zero the filter 
         */
        for(fp1 = p->c.dec.output_filter, i = DEC_FILTERLEN; i > 0; i--)
                *fp1++ = 0;

        return (ST_SUCCESS);
}

/* ---------------------------------------------------------------------- */

static int st_cvsdstartwrite(ft_t ft) 
{
        struct cvsdpriv *p = (struct cvsdpriv *) ft->priv;
        float *fp1;
        int i;

        cvsdstartcommon(ft);

        p->com.mla_tc1 = 0.1 * (1 - p->com.mla_tc0);
        p->com.phase = 4;
        /*
         * zero the filter 
         */
        for(fp1 = p->c.enc.input_filter, i = ENC_FILTERLEN; i > 0; i--)
                *fp1++ = 0;
        p->c.enc.recon_int = 0;

        return(ST_SUCCESS);
}

/* ---------------------------------------------------------------------- */

static int st_cvsdstopwrite(ft_t ft)
{
        struct cvsdpriv *p = (struct cvsdpriv *) ft->priv;

        if (p->bit.cnt) {
                st_writeb(ft, p->bit.shreg);
                p->bytes_written++;
        }
        st_debug("cvsd: min slope %f, max slope %f", 
               p->com.v_min, p->com.v_max);     

        return (ST_SUCCESS);
}

/* ---------------------------------------------------------------------- */

static int st_cvsdstopread(ft_t ft)
{
        struct cvsdpriv *p = (struct cvsdpriv *) ft->priv;

        st_debug("cvsd: min value %f, max value %f", 
               p->com.v_min, p->com.v_max);

        return(ST_SUCCESS);
}

/* ---------------------------------------------------------------------- */

static st_size_t st_cvsdread(ft_t ft, st_sample_t *buf, st_size_t nsamp) 
{
        struct cvsdpriv *p = (struct cvsdpriv *) ft->priv;
        st_size_t done = 0;
        float oval;
        
        while (done < nsamp) {
                if (!p->bit.cnt) {
                        if (st_readb(ft, &(p->bit.shreg)) == ST_EOF)
                                return done;
                        p->bit.cnt = 8;
                        p->bit.mask = 1;
                }
                /*
                 * handle one bit
                 */
                p->bit.cnt--;
                p->com.overload = ((p->com.overload << 1) | 
                                   (!!(p->bit.shreg & p->bit.mask))) & 7;
                p->bit.mask <<= 1;
                p->com.mla_int *= p->com.mla_tc0;
                if ((p->com.overload == 0) || (p->com.overload == 7))
                        p->com.mla_int += p->com.mla_tc1;
                memmove(p->c.dec.output_filter+1, p->c.dec.output_filter,
                        sizeof(p->c.dec.output_filter)-sizeof(float));
                if (p->com.overload & 1)
                        p->c.dec.output_filter[0] = p->com.mla_int;
                else
                        p->c.dec.output_filter[0] = -p->com.mla_int;
                /*
                 * check if the next output is due
                 */
                p->com.phase += p->com.phase_inc;
                if (p->com.phase >= 4) {
                        oval = float_conv(p->c.dec.output_filter, 
                                          (p->cvsd_rate < 24000) ? 
                                          dec_filter_16 : dec_filter_32, 
                                          DEC_FILTERLEN);
                        st_debug_more("input %d %f\n", debug_count, p->com.mla_int);
                        st_debug_more("recon %d %f\n", debug_count, oval);
                        debug_count++;

                        if (oval > p->com.v_max)
                                p->com.v_max = oval;
                        if (oval < p->com.v_min)
                                p->com.v_min = oval;
                        *buf++ = (oval * ((float)ST_SAMPLE_MAX));
                        done++;
                }
                p->com.phase &= 3;
        }
        return done;
}

/* ---------------------------------------------------------------------- */

static st_size_t st_cvsdwrite(ft_t ft, const st_sample_t *buf, st_size_t nsamp) 
{
        struct cvsdpriv *p = (struct cvsdpriv *) ft->priv;
        st_size_t done = 0;
        float inval;

        for(;;) {
                /*
                 * check if the next input is due
                 */
                if (p->com.phase >= 4) {
                        if (done >= nsamp)
                                return done;
                        memmove(p->c.enc.input_filter+1, p->c.enc.input_filter,
                                sizeof(p->c.enc.input_filter)-sizeof(float));
                        p->c.enc.input_filter[0] = (*buf++) / 
                                ((float)ST_SAMPLE_MAX);
                        done++;
                }
                p->com.phase &= 3;
                /* insert input filter here! */
                inval = float_conv(p->c.enc.input_filter, 
                                   (p->cvsd_rate < 24000) ? 
                                   (enc_filter_16[(p->com.phase >= 2)]) : 
                                   (enc_filter_32[p->com.phase]), 
                                   ENC_FILTERLEN);
                /*
                 * encode one bit
                 */
                p->com.overload = (((p->com.overload << 1) |
                                    (inval >  p->c.enc.recon_int)) & 7);
                p->com.mla_int *= p->com.mla_tc0;
                if ((p->com.overload == 0) || (p->com.overload == 7))
                        p->com.mla_int += p->com.mla_tc1;
                if (p->com.mla_int > p->com.v_max)
                        p->com.v_max = p->com.mla_int;
                if (p->com.mla_int < p->com.v_min)
                        p->com.v_min = p->com.mla_int;
                if (p->com.overload & 1) {
                        p->c.enc.recon_int += p->com.mla_int;
                        p->bit.shreg |= p->bit.mask;
                } else
                        p->c.enc.recon_int -= p->com.mla_int;
                if ((++(p->bit.cnt)) >= 8) {
                        st_writeb(ft, p->bit.shreg);
                        p->bytes_written++;
                        p->bit.shreg = p->bit.cnt = 0;
                        p->bit.mask = 1;
                } else
                        p->bit.mask <<= 1;
                p->com.phase += p->com.phase_inc;
                st_debug_more("input %d %f\n", debug_count, inval);
                st_debug_more("recon %d %f\n", debug_count, p->c.enc.recon_int);
                debug_count++;
        }
}

/* ---------------------------------------------------------------------- */
/*
 * DVMS file header
 */
struct dvms_header {
        char          Filename[14];
        unsigned      Id;
        unsigned      State;
        time_t        Unixtime;
        unsigned      Usender;
        unsigned      Ureceiver;
        st_size_t     Length;
        unsigned      Srate;
        unsigned      Days;
        unsigned      Custom1;
        unsigned      Custom2;
        char          Info[16];
        char          extend[64];
        unsigned      Crc;
};

#define DVMS_HEADER_LEN 120

/* ---------------------------------------------------------------------- */

static int dvms_read_header(ft_t ft, struct dvms_header *hdr)
{
        unsigned char hdrbuf[DVMS_HEADER_LEN];
        unsigned char *pch = hdrbuf;
        int i;
        unsigned sum;

        if (st_readbuf(ft, hdrbuf, sizeof(hdrbuf), 1) != 1)
        {
                return (ST_EOF);
        }
        for(i = sizeof(hdrbuf), sum = 0; i > /*2*/3; i--) /* Deti bug */
                sum += *pch++;
        pch = hdrbuf;
        memcpy(hdr->Filename, pch, sizeof(hdr->Filename));
        pch += sizeof(hdr->Filename);
        hdr->Id = get16_le(&pch);
        hdr->State = get16_le(&pch);
        hdr->Unixtime = get32_le(&pch);
        hdr->Usender = get16_le(&pch);
        hdr->Ureceiver = get16_le(&pch);
        hdr->Length = get32_le(&pch);
        hdr->Srate = get16_le(&pch);
        hdr->Days = get16_le(&pch);
        hdr->Custom1 = get16_le(&pch);
        hdr->Custom2 = get16_le(&pch);
        memcpy(hdr->Info, pch, sizeof(hdr->Info));
        pch += sizeof(hdr->Info);
        memcpy(hdr->extend, pch, sizeof(hdr->extend));
        pch += sizeof(hdr->extend);
        hdr->Crc = get16_le(&pch);
        if (sum != hdr->Crc) 
        {
                st_report("DVMS header checksum error, read %u, calculated %u",
                     hdr->Crc, sum);
                return (ST_EOF);
        }
        return (ST_SUCCESS);
}

/* ---------------------------------------------------------------------- */

/*
 * note! file must be seekable
 */
static int dvms_write_header(ft_t ft, struct dvms_header *hdr)
{
        unsigned char hdrbuf[DVMS_HEADER_LEN];
        unsigned char *pch = hdrbuf;
        unsigned char *pchs = hdrbuf;
        int i;
        unsigned sum;

        memcpy(pch, hdr->Filename, sizeof(hdr->Filename));
        pch += sizeof(hdr->Filename);
        put16_le(&pch, hdr->Id);
        put16_le(&pch, hdr->State);
        put32_le(&pch, hdr->Unixtime);
        put16_le(&pch, hdr->Usender);
        put16_le(&pch, hdr->Ureceiver);
        put32_le(&pch, hdr->Length);
        put16_le(&pch, hdr->Srate);
        put16_le(&pch, hdr->Days);
        put16_le(&pch, hdr->Custom1);
        put16_le(&pch, hdr->Custom2);
        memcpy(pch, hdr->Info, sizeof(hdr->Info));
        pch += sizeof(hdr->Info);
        memcpy(pch, hdr->extend, sizeof(hdr->extend));
        pch += sizeof(hdr->extend);
        for(i = sizeof(hdrbuf), sum = 0; i > /*2*/3; i--) /* Deti bug */
                sum += *pchs++;
        hdr->Crc = sum;
        put16_le(&pch, hdr->Crc);
        if (st_seeki(ft, 0, SEEK_SET) < 0)
        {
                st_report("seek failed\n: %s",strerror(errno));
                return (ST_EOF);
        }
        if (st_writebuf(ft, hdrbuf, sizeof(hdrbuf), 1) != 1)
        {
                st_report("%s",strerror(errno));
                return (ST_EOF);
        }
        return (ST_SUCCESS);
}

/* ---------------------------------------------------------------------- */

static void make_dvms_hdr(ft_t ft, struct dvms_header *hdr)
{
        struct cvsdpriv *p = (struct cvsdpriv *) ft->priv;
        size_t len;

        memset(hdr->Filename, 0, sizeof(hdr->Filename));
        len = strlen(ft->filename);
        if (len >= sizeof(hdr->Filename))
                len = sizeof(hdr->Filename)-1;
        memcpy(hdr->Filename, ft->filename, len);
        hdr->Id = hdr->State = 0;
        hdr->Unixtime = time(NULL);
        hdr->Usender = hdr->Ureceiver = 0;
        hdr->Length = p->bytes_written;
        hdr->Srate = p->cvsd_rate/100;
        hdr->Days = hdr->Custom1 = hdr->Custom2 = 0;
        memset(hdr->Info, 0, sizeof(hdr->Info));
        len = strlen(ft->comment);
        if (len >= sizeof(hdr->Info))
                len = sizeof(hdr->Info)-1;
        memcpy(hdr->Info, ft->comment, len);
        memset(hdr->extend, 0, sizeof(hdr->extend));
}

/* ---------------------------------------------------------------------- */

static int st_dvmsstartread(ft_t ft) 
{
        struct dvms_header hdr;
        int rc;

        rc = dvms_read_header(ft, &hdr);
        if (rc){
            st_fail_errno(ft,ST_EHDR,"unable to read DVMS header");
            return rc;
        }

        st_debug("DVMS header of source file \"%s\":");
        st_debug("  filename  \"%.14s\"",ft->filename);
        st_debug("  id        0x%x", hdr.Filename);
        st_debug("  state     0x%x", hdr.Id, hdr.State);
        st_debug("  time      %s",ctime(&hdr.Unixtime)); /* ctime generates lf */
        st_debug("  usender   %u", hdr.Usender);
        st_debug("  ureceiver %u", hdr.Ureceiver);
        st_debug("  length    %u", hdr.Length);
        st_debug("  srate     %u", hdr.Srate);
        st_debug("  days      %u", hdr.Days);
        st_debug("  custom1   %u", hdr.Custom1);
        st_debug("  custom2   %u", hdr.Custom2);
        st_debug("  info      \"%.16s\"", hdr.Info);
        ft->signal.rate = (hdr.Srate < 240) ? 16000 : 32000;
        st_debug("DVMS rate %dbit/s using %dbit/s deviation %d%%", 
               hdr.Srate*100, ft->signal.rate, 
               ((ft->signal.rate - hdr.Srate*100) * 100) / ft->signal.rate);
        rc = st_cvsdstartread(ft);
        if (rc)
            return rc;

        return(ST_SUCCESS);
}

/* ---------------------------------------------------------------------- */

static int st_dvmsstartwrite(ft_t ft) 
{
        struct dvms_header hdr;
        int rc;
        
        rc = st_cvsdstartwrite(ft);
        if (rc)
            return rc;

        make_dvms_hdr(ft, &hdr);
        rc = dvms_write_header(ft, &hdr);
        if (rc){
                st_fail_errno(ft,rc,"cannot write DVMS header");
            return rc;
        }

        if (!ft->seekable)
               st_warn("Length in output .DVMS header will wrong since can't seek to fix it");

        return(ST_SUCCESS);
}

/* ---------------------------------------------------------------------- */

static int st_dvmsstopwrite(ft_t ft)
{
        struct dvms_header hdr;
        int rc;
        
        st_cvsdstopwrite(ft);
        if (!ft->seekable)
        {
            st_warn("File not seekable");
            return (ST_EOF);
        }
        if (st_seeki(ft, 0, 0) != 0)
        {
                st_fail_errno(ft,errno,"Can't rewind output file to rewrite DVMS header.");
                return(ST_EOF);
        }
        make_dvms_hdr(ft, &hdr);
        rc = dvms_write_header(ft, &hdr);
        if(rc){
            st_fail_errno(ft,rc,"cannot write DVMS header");
            return rc;
        }       
        return rc;
}

/* ---------------------------------------------------------------------- */

/* Cont. Variable Slope Delta */
static const char *cvsdnames[] = {
  "cvs",
  "cvsd",
  NULL
};

static st_format_t st_cvsd_format = {
  cvsdnames,
  NULL,
  0,
  st_cvsdstartread,
  st_cvsdread,
  st_cvsdstopread,
  st_cvsdstartwrite,
  st_cvsdwrite,
  st_cvsdstopwrite,
  st_format_nothing_seek
};

const st_format_t *st_cvsd_format_fn(void)
{
    return &st_cvsd_format;
}
/* Cont. Variable Solot Delta */
static const char *dvmsnames[] = {
  "vms",
  "dvms",
  NULL
};

static st_format_t st_dvms_format = {
  dvmsnames,
  NULL,
  0,
  st_dvmsstartread,
  st_cvsdread,
  st_cvsdstopread,
  st_dvmsstartwrite,
  st_cvsdwrite,
  st_dvmsstopwrite,
  st_format_nothing_seek
};

const st_format_t *st_dvms_format_fn(void)
{
    return &st_dvms_format;
}

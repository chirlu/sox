/*      libSoX CVSD (Continuously Variable Slope Delta modulation)
 *      conversion routines
 *
 *      The CVSD format is described in the MIL Std 188 113, which is
 *      available from http://bbs.itsi.disa.mil:5580/T3564
 *
 *      Copyright (C) 1996
 *      Thomas Sailer (sailer@ife.ee.ethz.ch) (HB9JNX/AE4WA)
 *      Swiss Federal Institute of Technology, Electronics Lab
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Change History:
 *
 * June 1, 1998 - Chris Bagwell (cbagwell@sprynet.com)
 *   Fixed compile warnings reported by Kjetil Torgrim Homme
 *   <kjetilho@ifi.uio.no>
 */

#include "sox_i.h"
#include "cvsd.h"
#include "cvsdfilt.h"

#include <string.h>
#include <time.h>

/* ---------------------------------------------------------------------- */
/*
 * private data structures
 */

typedef cvsd_priv_t priv_t;

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

static void cvsdstartcommon(sox_format_t * ft)
{
        priv_t *p = (priv_t *) ft->priv;

        p->cvsd_rate = (ft->signal.rate <= 24000) ? 16000 : 32000;
        ft->signal.rate = 8000;
        ft->signal.channels = 1;
        lsx_rawstart(ft, sox_true, sox_false, sox_true, SOX_ENCODING_CVSD, 1);
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
        sox_report("cvsd: bit rate %dbit/s, bits from %s", p->cvsd_rate,
               ft->encoding.reverse_bits ? "msb to lsb" : "lsb to msb");
}

/* ---------------------------------------------------------------------- */

int sox_cvsdstartread(sox_format_t * ft)
{
        priv_t *p = (priv_t *) ft->priv;
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
        for(fp1 = p->c.dec.output_filter, i = CVSD_DEC_FILTERLEN; i > 0; i--)
                *fp1++ = 0;

        return (SOX_SUCCESS);
}

/* ---------------------------------------------------------------------- */

int sox_cvsdstartwrite(sox_format_t * ft)
{
        priv_t *p = (priv_t *) ft->priv;
        float *fp1;
        int i;

        cvsdstartcommon(ft);

        p->com.mla_tc1 = 0.1 * (1 - p->com.mla_tc0);
        p->com.phase = 4;
        /*
         * zero the filter
         */
        for(fp1 = p->c.enc.input_filter, i = CVSD_ENC_FILTERLEN; i > 0; i--)
                *fp1++ = 0;
        p->c.enc.recon_int = 0;

        return(SOX_SUCCESS);
}

/* ---------------------------------------------------------------------- */

int sox_cvsdstopwrite(sox_format_t * ft)
{
        priv_t *p = (priv_t *) ft->priv;

        if (p->bit.cnt) {
                lsx_writeb(ft, p->bit.shreg);
                p->bytes_written++;
        }
        sox_debug("cvsd: min slope %f, max slope %f",
               p->com.v_min, p->com.v_max);

        return (SOX_SUCCESS);
}

/* ---------------------------------------------------------------------- */

int sox_cvsdstopread(sox_format_t * ft)
{
        priv_t *p = (priv_t *) ft->priv;

        sox_debug("cvsd: min value %f, max value %f",
               p->com.v_min, p->com.v_max);

        return(SOX_SUCCESS);
}

/* ---------------------------------------------------------------------- */

sox_size_t sox_cvsdread(sox_format_t * ft, sox_sample_t *buf, sox_size_t nsamp)
{
        priv_t *p = (priv_t *) ft->priv;
        sox_size_t done = 0;
        float oval;

        while (done < nsamp) {
                if (!p->bit.cnt) {
                        if (lsx_read_b_buf(ft, &(p->bit.shreg), 1) != 1)
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
                                          CVSD_DEC_FILTERLEN);
                        sox_debug_more("input %d %f\n", debug_count, p->com.mla_int);
                        sox_debug_more("recon %d %f\n", debug_count, oval);
                        debug_count++;

                        if (oval > p->com.v_max)
                                p->com.v_max = oval;
                        if (oval < p->com.v_min)
                                p->com.v_min = oval;
                        *buf++ = (oval * ((float)SOX_SAMPLE_MAX));
                        done++;
                }
                p->com.phase &= 3;
        }
        return done;
}

/* ---------------------------------------------------------------------- */

sox_size_t sox_cvsdwrite(sox_format_t * ft, const sox_sample_t *buf, sox_size_t nsamp)
{
        priv_t *p = (priv_t *) ft->priv;
        sox_size_t done = 0;
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
                                ((float)SOX_SAMPLE_MAX);
                        done++;
                }
                p->com.phase &= 3;
                /* insert input filter here! */
                inval = float_conv(p->c.enc.input_filter,
                                   (p->cvsd_rate < 24000) ?
                                   (enc_filter_16[(p->com.phase >= 2)]) :
                                   (enc_filter_32[p->com.phase]),
                                   CVSD_ENC_FILTERLEN);
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
                        lsx_writeb(ft, p->bit.shreg);
                        p->bytes_written++;
                        p->bit.shreg = p->bit.cnt = 0;
                        p->bit.mask = 1;
                } else
                        p->bit.mask <<= 1;
                p->com.phase += p->com.phase_inc;
                sox_debug_more("input %d %f\n", debug_count, inval);
                sox_debug_more("recon %d %f\n", debug_count, p->c.enc.recon_int);
                debug_count++;
        }
}

/* ---------------------------------------------------------------------- */
/*
 * DVMS file header
 */

/* FIXME: eliminate these 4 functions */

static uint32_t get32_le(unsigned char **p)
{
  uint32_t val = (((*p)[3]) << 24) | (((*p)[2]) << 16) |
          (((*p)[1]) << 8) | (**p);
  (*p) += 4;
  return val;
}

static uint16_t get16_le(unsigned char **p)
{
  unsigned val = (((*p)[1]) << 8) | (**p);
  (*p) += 2;
  return val;
}

static void put32_le(unsigned char **p, uint32_t val)
{
  *(*p)++ = val & 0xff;
  *(*p)++ = (val >> 8) & 0xff;
  *(*p)++ = (val >> 16) & 0xff;
  *(*p)++ = (val >> 24) & 0xff;
}

static void put16_le(unsigned char **p, unsigned val)
{
  *(*p)++ = val & 0xff;
  *(*p)++ = (val >> 8) & 0xff;
}

struct dvms_header {
        char          Filename[14];
        unsigned      Id;
        unsigned      State;
        time_t        Unixtime;
        unsigned      Usender;
        unsigned      Ureceiver;
        sox_size_t     Length;
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

static int dvms_read_header(sox_format_t * ft, struct dvms_header *hdr)
{
        unsigned char hdrbuf[DVMS_HEADER_LEN];
        unsigned char *pch = hdrbuf;
        int i;
        unsigned sum;

        if (lsx_readbuf(ft, hdrbuf, sizeof(hdrbuf)) != sizeof(hdrbuf))
        {
                return (SOX_EOF);
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
                sox_report("DVMS header checksum error, read %u, calculated %u",
                     hdr->Crc, sum);
                return (SOX_EOF);
        }
        return (SOX_SUCCESS);
}

/* ---------------------------------------------------------------------- */

/*
 * note! file must be seekable
 */
static int dvms_write_header(sox_format_t * ft, struct dvms_header *hdr)
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
        put32_le(&pch, (unsigned)hdr->Unixtime);
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
        if (lsx_seeki(ft, 0, SEEK_SET) < 0)
        {
                sox_report("seek failed\n: %s",strerror(errno));
                return (SOX_EOF);
        }
        if (lsx_writebuf(ft, hdrbuf, sizeof(hdrbuf)) != sizeof(hdrbuf))
        {
                sox_report("%s",strerror(errno));
                return (SOX_EOF);
        }
        return (SOX_SUCCESS);
}

/* ---------------------------------------------------------------------- */

static void make_dvms_hdr(sox_format_t * ft, struct dvms_header *hdr)
{
        priv_t *p = (priv_t *) ft->priv;
        size_t len;
        char * comment = sox_cat_comments(ft->oob.comments);

        memset(hdr->Filename, 0, sizeof(hdr->Filename));
        len = strlen(ft->filename);
        if (len >= sizeof(hdr->Filename))
                len = sizeof(hdr->Filename)-1;
        memcpy(hdr->Filename, ft->filename, len);
        hdr->Id = hdr->State = 0;
        hdr->Unixtime = sox_globals.repeatable? 0 : time(NULL);
        hdr->Usender = hdr->Ureceiver = 0;
        hdr->Length = p->bytes_written;
        hdr->Srate = p->cvsd_rate/100;
        hdr->Days = hdr->Custom1 = hdr->Custom2 = 0;
        memset(hdr->Info, 0, sizeof(hdr->Info));
        len = strlen(comment);
        if (len >= sizeof(hdr->Info))
                len = sizeof(hdr->Info)-1;
        memcpy(hdr->Info, comment, len);
        memset(hdr->extend, 0, sizeof(hdr->extend));
        free(comment);
}

/* ---------------------------------------------------------------------- */

int sox_dvmsstartread(sox_format_t * ft)
{
        struct dvms_header hdr;
        int rc;

        rc = dvms_read_header(ft, &hdr);
        if (rc){
            lsx_fail_errno(ft,SOX_EHDR,"unable to read DVMS header");
            return rc;
        }

        sox_debug("DVMS header of source file \"%s\":", ft->filename);
        sox_debug("  filename  \"%.14s\"", hdr.Filename);
        sox_debug("  id        0x%x", hdr.Id);
        sox_debug("  state     0x%x", hdr.State);
        sox_debug("  time      %s", ctime(&hdr.Unixtime)); /* ctime generates lf */
        sox_debug("  usender   %u", hdr.Usender);
        sox_debug("  ureceiver %u", hdr.Ureceiver);
        sox_debug("  length    %u", hdr.Length);
        sox_debug("  srate     %u", hdr.Srate);
        sox_debug("  days      %u", hdr.Days);
        sox_debug("  custom1   %u", hdr.Custom1);
        sox_debug("  custom2   %u", hdr.Custom2);
        sox_debug("  info      \"%.16s\"", hdr.Info);
        ft->signal.rate = (hdr.Srate < 240) ? 16000 : 32000;
        sox_debug("DVMS rate %dbit/s using %gbit/s deviation %g%%",
               hdr.Srate*100, ft->signal.rate,
               ((ft->signal.rate - hdr.Srate*100) * 100) / ft->signal.rate);
        rc = sox_cvsdstartread(ft);
        if (rc)
            return rc;

        return(SOX_SUCCESS);
}

/* ---------------------------------------------------------------------- */

int sox_dvmsstartwrite(sox_format_t * ft)
{
        struct dvms_header hdr;
        int rc;

        rc = sox_cvsdstartwrite(ft);
        if (rc)
            return rc;

        make_dvms_hdr(ft, &hdr);
        rc = dvms_write_header(ft, &hdr);
        if (rc){
                lsx_fail_errno(ft,rc,"cannot write DVMS header");
            return rc;
        }

        if (!ft->seekable)
               sox_warn("Length in output .DVMS header will wrong since can't seek to fix it");

        return(SOX_SUCCESS);
}

/* ---------------------------------------------------------------------- */

int sox_dvmsstopwrite(sox_format_t * ft)
{
        struct dvms_header hdr;
        int rc;

        sox_cvsdstopwrite(ft);
        if (!ft->seekable)
        {
            sox_warn("File not seekable");
            return (SOX_EOF);
        }
        if (lsx_seeki(ft, 0, 0) != 0)
        {
                lsx_fail_errno(ft,errno,"Can't rewind output file to rewrite DVMS header.");
                return(SOX_EOF);
        }
        make_dvms_hdr(ft, &hdr);
        rc = dvms_write_header(ft, &hdr);
        if(rc){
            lsx_fail_errno(ft,rc,"cannot write DVMS header");
            return rc;
        }
        return rc;
}

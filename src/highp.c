/*
 * Sound Tools High-Pass effect file.
 *
 * (C) 2000 Chris Bagwell <cbagwell@sprynet.com>
 * See License file for further copyright information.
 *
 * Algorithm:  Recursive single pole high-pass filter
 *
 * Reference: The Scientist and Engineer's Guide to Digital Processing
 *
 *      output[N] = A0 * input[N] + A1 * input[N-1] + B1 * output[N-1]
 *
 *      X  = exp(-2.0 * pi * Fc)
 *      A0 = (1 + X) / 2
 *      A1 = -(1 + X) / 2
 *      B1 = X
 *      Fc = cutoff freq / sample rate
 *
 * Mimics an RC high-pass filter:
 *
 *        || C
 *    ----||--------->
 *        ||    |
 *              <
 *              > R
 *              <
 *              |
 *              V
 *
 */

#include <math.h>
#include "st_i.h"

static st_effect_t st_highp_effect;

/* Private data for Highpass effect */
typedef struct highpstuff {
        float   cutoff;
        double  A0, A1, B1;
        double  inm1, outm1;
} *highp_t;

/*
 * Process options
 */
static int st_highp_getopts(eff_t effp, int n, char **argv) 
{
        highp_t highp = (highp_t) effp->priv;

        if ((n < 1) || !sscanf(argv[0], "%f", &highp->cutoff))
        {
                st_fail(st_highp_effect.usage);
                return (ST_EOF);
        }
        return (ST_SUCCESS);
}

/*
 * Prepare processing.
 */
static int st_highp_start(eff_t effp)
{
        highp_t highp = (highp_t) effp->priv;
        if (highp->cutoff > effp->ininfo.rate/2)
        {
                st_fail("Highpass: cutoff must be < sample rate / 2 (Nyquest rate)");
                return (ST_EOF);
        }

        highp->B1 = exp((-2.0 * M_PI * (highp->cutoff / effp->ininfo.rate)));
        highp->A0 = (1 + highp->B1) / 2;
        highp->A1 = (-1 * (1 + highp->B1)) / 2;
        highp->inm1 = 0.0;
        highp->outm1 = 0.0;

        if (effp->globalinfo->octave_plot_effect)
        {
          printf(
            "title('SoX effect: %s cutoff=%g (rate=%u)')\n"
            "xlabel('Frequency (Hz)')\n"
            "ylabel('Amplitude Response (dB)')\n"
            "Fs=%u;minF=10;maxF=Fs/2;\n"
            "axis([minF maxF -95 5])\n"
            "sweepF=logspace(log10(minF),log10(maxF),200);\n"
            "grid on\n"
            "[h,w]=freqz([%f %f],[1 %f],sweepF,Fs);\n"
            "semilogx(w,20*log10(h),'b')\n"
            "pause\n"
            , effp->name, highp->cutoff
            , effp->ininfo.rate, effp->ininfo.rate
            , highp->A0, highp->A1, -highp->B1
            );
          exit(0);
        }
        return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
static int st_highp_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf, 
                  st_size_t *isamp, st_size_t *osamp)
{
        highp_t highp = (highp_t) effp->priv;
        int len, done;
        double d;
        st_sample_t l;

        len = ((*isamp > *osamp) ? *osamp : *isamp);

        for(done = 0; done < len; done++) {
                l = *ibuf++;
                d = highp->A0 * l + 
                    highp->A1 * highp->inm1 + 
                    highp->B1 * highp->outm1;
                ST_SAMPLE_CLIP_COUNT(d, effp->clippedCount);
                highp->inm1 = l;
                highp->outm1 = d;
                *obuf++ = d;
        }
        *isamp = len;
        *osamp = len;
        return (ST_SUCCESS);
}

static st_effect_t st_highp_effect = {
  "highp",
  "Usage: highp cutoff",
  0,
  st_highp_getopts,
  st_highp_start,
  st_highp_flow,
  st_effect_nothing_drain,
  st_effect_nothing
};

const st_effect_t *st_highp_effect_fn(void)
{
    return &st_highp_effect;
}

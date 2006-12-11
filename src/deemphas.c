/*
 * July 5, 1991
 *
 * Deemphases Filter
 *
 * Fixed deemphasis filter for processing pre-emphasized audio cd samples
 * 09/02/98 (c) Heiko Eissfeldt
 * License: LGPL (Lesser Gnu Public License)
 *
 * This implements the inverse filter of the optional pre-emphasis stage as
 * defined by ISO 908 (describing the audio cd format).
 *
 * Background:
 * In the early days of audio cds, there were recording problems
 * with noise (for example in classical recordings). The high dynamics
 * of audio cds exposed these recording errors a lot.
 *
 * The commonly used solution at that time was to 'pre-emphasize' the
 * trebles to have a better signal-noise-ratio. That is trebles were
 * amplified before recording, so that they would give a stronger
 * signal compared to the underlying (tape)noise.
 *
 * For that purpose the audio signal was prefiltered with the following
 * frequency response (simple first order filter):
 *
 * V (in dB)
 * ^
 * |
 * |                         _________________
 * |                        /
 * |                       / |
 * |     20 dB / decade ->/  |
 * |                     /   |
 * |____________________/_ _ |_ _ _ _ _ _ _ _ _ _ _ _ _ lg f
 * |0 dB                |    |
 * |                    |    |
 * |                    |    |
 *                 3.1KHz    ca. 10KHz
 *
 * So the recorded audio signal has amplified trebles compared to the
 * original.
 * HiFi cd players do correct this by applying an inverse filter
 * automatically, the cd-rom drives or cd burners used by digital
 * sampling programs (like cdda2wav) however do not.
 *
 * So, this is what this effect does.
 *
 * Here is the gnuplot file for the frequency response
   of the deemphasis. The error is below +-0.1dB

-------- Start of gnuplot file ---------------------
# first define the ideal filter. We use the tenfold sampling frequency.
T=1./441000.
OmegaU=1./15E-6
OmegaL=15./50.*OmegaU
V0=OmegaL/OmegaU
H0=V0-1.
B=V0*tan(OmegaU*T/2.)
# the coefficients follow
a1=(B - 1.)/(B + 1.)
b0=(1.0 + (1.0 - a1) * H0/2.)
b1=(a1 + (a1 - 1.0) * H0/2.)
# helper variables
D=b1/b0
o=2*pi*T
H2(f)=b0*sqrt((1+2*cos(f*o)*D+D*D)/(1+2*cos(f*o)*a1+a1*a1))
#
# now approximate the ideal curve with a fitted one for sampling
frequency
# of 44100 Hz. Fitting parameters are
# amplification at high frequencies V02
# and tau of the upper edge frequency OmegaU2 = 2 *pi * f(upper)
T2=1./44100.
V02=0.3365
OmegaU2=1./19E-6
B2=V02*tan(OmegaU2*T2/2.)
# the coefficients follow
a12=(B2 - 1.)/(B2 + 1.)
b02=(1.0 + (1.0 - a12) * (V02-1.)/2.)
b12=(a12 + (a12 - 1.0) * (V02-1.)/2.)
# helper variables
D2=b12/b02
o2=2*pi*T2
H(f)=b02*sqrt((1+2*cos(f*o2)*D2+D2*D2)/(1+2*cos(f*o2)*a12+a12*a12))
# plot best, real, ideal, level with halved attenuation,
#      level at full attentuation, 10fold magnified error
set logscale x
set grid xtics ytics mxtics mytics
plot [f=1000:20000] [-12:2] 20*log10(H(f)),20*log10(H2(f)),
20*log10(OmegaL/(2*
pi*f)), 0.5*20*log10(V0), 20*log10(V0), 200*log10(H(f)/H2(f))
pause -1 "Hit return to continue"
-------- End of gnuplot file ---------------------

 */

/*
 * adapted from Sound Tools skeleton effect file.
 */

#include <math.h>
#include "st_i.h"

static st_effect_t st_deemph_effect ;

/* Private data for deemph file */
typedef struct deemphstuff {
     st_sample_t lastin;
     double      lastout;
} *deemph_t;

assert_static(sizeof(struct deemphstuff) <= ST_MAX_EFFECT_PRIVSIZE, 
    /* else */ deemph_PRIVSIZE_too_big);

/* filter coefficients */
#define a1      -0.62786881719628784282
#define b0      0.45995451989513153057
#define b1      -0.08782333709141937339

/*
 * Prepare processing.
 * Do all initializations.
 */
static int st_deemph_start(eff_t effp)
{
     /* check the input format */

     /* This used to check the input file sample encoding method and size
      * but these are irrelevant as effects always work with the ST internal
      * long-integer format regardless of the input format.
      *   The only parameter that is important for the deemph effect is
      * sampling rate as this has been harded coded into the pre-calculated
      * filter coefficients.
      */
     if (effp->ininfo.rate != 44100)
     {
          st_fail("The deemphasis effect works only with audio-CD-like samples.\nThe input format however has %d Hz sample rate.",
            effp->ininfo.rate);
          return (ST_EOF);
     }
     else
     {
          deemph_t deemph = (deemph_t) effp->priv;

          deemph->lastin = 0;
          deemph->lastout = 0.0;
     }
     if (effp->globalinfo.octave_plot_effect)
     {
       printf(
         "title('SoX effect: %s (rate=%u)')\n"
         "xlabel('Frequency (Hz)')\n"
         "ylabel('Amplitude Response (dB)')\n"
         "Fs=%u;minF=10;maxF=Fs/2;\n"
         "axis([minF maxF -25 25])\n"
         "sweepF=logspace(log10(minF),log10(maxF),200);\n"
         "grid on\n"
         "[h,w]=freqz([%f %f],[1 %f],sweepF,Fs);\n"
         "semilogx(w,20*log10(h),'b')\n"
         "pause\n"
         , effp->name
         , effp->ininfo.rate, effp->ininfo.rate
         , b0, b1, a1
         );
       exit(0);
     }
     return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

static int st_deemph_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf, 
                   st_size_t *isamp, st_size_t *osamp)
{
     deemph_t deemph = (deemph_t) effp->priv;
     int len, done;

     len = ((*isamp > *osamp) ? *osamp : *isamp);
     for(done = len; done; done--) {
          deemph->lastout = *ibuf * b0 +
                         deemph->lastin * b1 -
                         deemph->lastout * a1;
          deemph->lastin = *ibuf++;
          *obuf++ = deemph->lastout > 0.0 ?
                    deemph->lastout + 0.5 :
                    deemph->lastout - 0.5;
     }
     *isamp = *osamp = len;
     return (ST_SUCCESS);
}

static st_effect_t st_deemph_effect = {
  "deemph",
  "Usage: Deemphasis filtering effect takes no options",
  0,
  st_effect_nothing_getopts,
  st_deemph_start,
  st_deemph_flow,
  st_effect_nothing_drain,
  st_effect_nothing
};

const st_effect_t *st_deemph_effect_fn(void)
{
    return &st_deemph_effect;
}

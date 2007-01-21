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


/* filter coefficients */
p->a1 = -0.62786881719628784282;
p->b0 =  0.45995451989513153057;
p->b1 = -0.08782333709141937339;


/* The sample-rate must be 44100 as this has been harded coded into the
 * pre-calculated filter coefficients.
 */
if (effp->ininfo.rate != 44100) {
  st_fail("Sample rate must be 44100 (audio-CD)");
  return ST_EOF;
}

# 15/50us EIAJ de-emphasis filter for CD/DAT
#
# 09/02/98 (c) Heiko Eissfeldt
#
# 18/03/07 robs@users.sourceforge.net: changed to biquad for better accuracy.
#
# License: LGPL (Lesser Gnu Public License)
#
# This implements the inverse filter of the optional pre-emphasis stage
# as defined by ISO 908 (describing the audio cd format).
#
# Background: In the early days of audio cds, there were recording
# problems with noise (for example in classical recordings). The high
# dynamics of audio cds exposed these recording errors a lot.
#
# The commonly used solution at that time was to 'pre-emphasize' the
# trebles to have a better signal-noise-ratio. That is trebles were
# amplified before recording, so that they would give a stronger signal
# compared to the underlying (tape) noise.
#
# For that purpose the audio signal was prefiltered with the following
# frequency response (simple first order filter):
#
# V (in dB)
# ^
# |
# |~10dB                    _________________
# |                        /
# |                       / |
# |      20dB / decade ->/  |
# |                     /   |
# |____________________/_ _ |_ _ _ _ _ _ _ _ _ Frequency
# |0 dB                |    |
# |                    |    |
# |                    |    |
#                 3.1kHz    ~10kHz
#
# So the recorded audio signal has amplified trebles compared to the
# original.  HiFi cd players do correct this by applying an inverse
# filter automatically, the cd-rom drives or cd burners used by digital
# sampling programs (like cdda2wav) however do not.
#
# So, this is what this effect does.
#
# This is the gnuplot file for the frequency response of the deemphasis.
#
# The absolute error is <=0.04dB up to ~12kHz, and <=0.06dB up to 20kHz.

# First define the ideal filter:

# Filter parameters
T=1./441000.          # we use the tenfold sampling frequency
OmegaU=1./15e-6
OmegaL=15./50.*OmegaU

# Calculate filter coefficients
V0=OmegaL/OmegaU
H0=V0 - 1.
B=V0*tan(OmegaU*T/2.)
a1=(B - 1.)/(B + 1.)
b0=(1.0 + (1.0 - a1) * H0/2.)
b1=(a1 + (a1 - 1.0) * H0/2.)

# helper variables
D=b1/b0
o=2*pi*T

# Ideal transfer function
Hi(f)=b0*sqrt((1 + 2*cos(f*o)*D + D*D)/(1 + 2*cos(f*o)*a1 + a1*a1))

# Now use a biquad (RBJ high shelf) with sampling frequency of 44100 Hz
# to approximate the ideal curve:

# Filter parameters
m_t=1./44100.
m_gain=-9.477
m_slope=.4845
m_f0=5283

# Calculate filter coefficients
m_A=exp(m_gain/40.*log(10.))
m_w0=2.*pi*m_f0*m_t
m_alpha=sin(m_w0)/2.*sqrt((m_A+1./m_A)*(1./m_slope-1.)+2.)
m_b0=m_A*((m_A+1.)+(m_A-1.)*cos(m_w0)+2.*sqrt(m_A)*m_alpha)
m_b1=-2.*m_A*((m_A-1.)+(m_A+1.)*cos(m_w0))
m_b2=m_A*((m_A+1.)+(m_A-1.)*cos(m_w0)-2.*sqrt(m_A)*m_alpha)
m_a0=(m_A+1.)-(m_A-1.)*cos(m_w0)+2.*sqrt(m_A)*m_alpha
m_a1=2.*((m_A-1.)-(m_A+1.)*cos(m_w0))
m_a2=(m_A+1.)-(m_A-1.)*cos(m_w0)-2.*sqrt(m_A)*m_alpha
m_b2=m_b2/m_a0
m_b1=m_b1/m_a0
m_b0=m_b0/m_a0
m_a2=m_a2/m_a0
m_a1=m_a1/m_a0

# helper variables
m_o=2*pi*m_t

# Best fit transfer function
Hb(f)=sqrt(\
  (m_b0*m_b0 + m_b1*m_b1 + m_b2*m_b2 +\
    2.*(m_b0*m_b1 + m_b1*m_b2)*cos(f*m_o) +\
    2.*(m_b0*m_b2)* cos(2.*f*m_o)) /\
  (1. + m_a1*m_a1 + m_a2*m_a2 +\
    2.*(m_a1 + m_a1*m_a2)*cos(f*m_o) +\
    2.*m_a2*cos(2.*f*m_o)))

# plot real, best, ideal, level with halved attenuation,
#   level at full attentuation, 10fold magnified error
set logscale x
set grid xtics ytics mxtics mytics
set key left bottom
plot [f=1000:20000] [-12:2] \
20*log10(Hi(f)),\
20*log10(Hb(f)),\
20*log10(OmegaL/(2* pi*f)),\
0.5*20*log10(V0),\
20*log10(V0),\
200*log10(Hb(f)/Hi(f))

pause -1 "Hit return to continue"

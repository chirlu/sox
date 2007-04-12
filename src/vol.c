/*
 * (c) 20/03/2000 Fabien COELHO <fabien@coelho.net> for sox.
 *
 * Change volume of sound file, with basic linear amplitude formula.
 * Beware of saturations! clipping is checked and reported.
 * Cannot handle different number of channels.
 * Cannot handle rate change.
 */
#define vol_usage \
  "Usage: vol gain[[ ]type [limitergain]]\n" \
  "\t(default type=amplitude: 1 is constant, < 0 change phase;\n" \
  "\ttype=power 1 is constant; type=dB: 0 is constant, +6 doubles ampl.)\n" \
  "\tThe peak limiter has a gain much less than 1 (e.g. 0.05 or 0.02) and is\n" \
  "\tonly used on peaks (to prevent clipping); default is no limiter."

#include "sox_i.h"

#include <math.h>   /* exp(), sqrt() */

#define LOG_10_20 ((double)(0.1151292546497022842009e0))

typedef struct {
    double gain; /* amplitude gain. */
    
    sox_bool uselimiter;
    double limiterthreshhold;
    double limitergain;
    int limited; /* number of limited values to report. */
    int totalprocessed;
} * vol_t;

enum {VOL_amplitude, VOL_dB, VOL_power};

static enum_item const vol_types[] = {
  ENUM_ITEM(VOL_,amplitude)
  ENUM_ITEM(VOL_,dB)
  ENUM_ITEM(VOL_,power)
  {0, 0}};

/*
 * Process options: gain (float) type (amplitude, power, dB)
 */
static int getopts(eff_t effp, int n, char **argv) 
{
  vol_t vol = (vol_t) effp->priv; 
  char string[11];
  char * q = string;
  char dummy;          /* To check for extraneous chars. */
  unsigned have_type;

  vol->gain = 1;       /* Default is no change. */
  vol->uselimiter = sox_false; /* Default is no limiter. */
  
  if (!n || (have_type = sscanf(argv[0], "%lf %10s %c", &vol->gain, string, &dummy) - 1) > 1) {
    sox_fail(effp->h->usage);
    return SOX_EOF;
  }
  ++argv, --n;

  if (!have_type && n) {
    ++have_type;
    q = *argv;
    ++argv, --n;
  }

  if (have_type) {
    enum_item const * p = find_enum_text(q, vol_types);
    if (!p) {
      sox_fail(effp->h->usage);
      return SOX_EOF;
    }
    switch (p->value) {
      case VOL_dB: vol->gain = exp(vol->gain*LOG_10_20); break;
      case VOL_power: /* power to amplitude, keep phase change */
        vol->gain = vol->gain > 0 ? sqrt(vol->gain) : -sqrt(-vol->gain);
        break;
    }
  }

  if (n) {
    if (fabs(vol->gain) < 1 || sscanf(*argv, "%lf %c", &vol->limitergain, &dummy) != 1 || vol->limitergain <= 0 || vol->limitergain >= 1) {
      sox_fail(effp->h->usage);
      return SOX_EOF;                  
    }
    
    vol->uselimiter = sox_true;
    /* The following equation is derived so that there is no 
     * discontinuity in output amplitudes */
    /* and a SOX_SAMPLE_MAX input always maps to a SOX_SAMPLE_MAX output 
     * when the limiter is activated. */
    /* (NOTE: There **WILL** be a discontinuity in the slope 
     * of the output amplitudes when using the limiter.) */
    vol->limiterthreshhold = SOX_SAMPLE_MAX * (1.0 - vol->limitergain) / (fabs(vol->gain) - vol->limitergain);
  }
  sox_debug("mult=%g limit=%g", vol->gain, vol->limitergain);
  return SOX_SUCCESS;
}

/*
 * Start processing
 */
static int start(eff_t effp)
{
    vol_t vol = (vol_t) effp->priv;
    
    if (vol->gain == 1)
      return SOX_EFF_NULL;

    if (effp->outinfo.channels != effp->ininfo.channels) {
        sox_fail("vol cannot handle different channels (in %d, out %d)"
             " use avg or pan", effp->ininfo.channels, effp->outinfo.channels);
        return SOX_EOF;
    }

    if (effp->outinfo.rate != effp->ininfo.rate) {
        sox_fail("vol cannot handle different rates (in %ld, out %ld)"
             " use resample", effp->ininfo.rate, effp->outinfo.rate);
        return SOX_EOF;
    }

    vol->limited = 0;
    vol->totalprocessed = 0;

    return SOX_SUCCESS;
}

/*
 * Process data.
 */
static int flow(eff_t effp, const sox_ssample_t *ibuf, sox_ssample_t *obuf, 
                sox_size_t *isamp, sox_size_t *osamp)
{
    vol_t vol = (vol_t) effp->priv;
    register double gain = vol->gain;
    register double limiterthreshhold = vol->limiterthreshhold;
    register double sample;
    register sox_size_t len;
    
    len = min(*osamp, *isamp);

    /* report back dealt with amount. */
    *isamp = len; *osamp = len;
    
    if (vol->uselimiter)
    {
        vol->totalprocessed += len;
        
        for (;len>0; len--)
            {
                sample = *ibuf++;
                
                if (sample > limiterthreshhold)
                {
                        sample =  (SOX_SAMPLE_MAX - vol->limitergain * (SOX_SAMPLE_MAX - sample));
                        vol->limited++;
                }
                else if (sample < -limiterthreshhold)
                {
                        sample = -(SOX_SAMPLE_MAX - vol->limitergain * (SOX_SAMPLE_MAX + sample));
                        /* FIXME: MIN is (-MAX)-1 so need to make sure we
                         * don't go over that.  Probably could do this
                         * check inside the above equation but I didn't
                         * think it thru.
                         */
                        if (sample < SOX_SAMPLE_MIN)
                            sample = SOX_SAMPLE_MIN;
                        vol->limited++;
                } else
                        sample = gain * sample;

                SOX_SAMPLE_CLIP_COUNT(sample, effp->clips);
               *obuf++ = sample;
            }
    }
    else
    {
        /* quite basic, with clipping */
        for (;len>0; len--)
        {
                sample = gain * *ibuf++;
                SOX_SAMPLE_CLIP_COUNT(sample, effp->clips);
                *obuf++ = sample;
        }
    }
    return SOX_SUCCESS;
}

static int stop(eff_t effp)
{
  vol_t vol = (vol_t) effp->priv;
  if (vol->limited) {
    sox_warn("limited %d values (%d percent).", 
         vol->limited, (int) (vol->limited * 100.0 / vol->totalprocessed));
  }
  return SOX_SUCCESS;
}

sox_effect_t const * sox_vol_effect_fn(void)
{
  static sox_effect_t driver = {
    "vol", vol_usage, SOX_EFF_MCHAN, getopts, start, flow, 0, stop, 0
  };
  return &driver;
}

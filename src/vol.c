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

#include "st_i.h"

#include <math.h>   /* exp(), sqrt() */

#define LOG_10_20 ((double)(0.1151292546497022842009e0))

typedef struct {
    double gain; /* amplitude gain. */
    
    st_bool uselimiter;
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
  vol->uselimiter = st_false; /* Default is no limiter. */
  
  if (!n || (have_type = sscanf(argv[0], "%lf %10s %c", &vol->gain, string, &dummy) - 1) > 1) {
    st_fail(effp->h->usage);
    return ST_EOF;
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
      st_fail(effp->h->usage);
      return ST_EOF;
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
      st_fail(effp->h->usage);
      return ST_EOF;                  
    }
    
    vol->uselimiter = st_true;
    /* The following equation is derived so that there is no 
     * discontinuity in output amplitudes */
    /* and a ST_SAMPLE_MAX input always maps to a ST_SAMPLE_MAX output 
     * when the limiter is activated. */
    /* (NOTE: There **WILL** be a discontinuity in the slope 
     * of the output amplitudes when using the limiter.) */
    vol->limiterthreshhold = ST_SAMPLE_MAX * (1.0 - vol->limitergain) / (fabs(vol->gain) - vol->limitergain);
  }
  st_debug("mult=%g limit=%g", vol->gain, vol->limitergain);
  return ST_SUCCESS;
}

/*
 * Start processing
 */
static int start(eff_t effp)
{
    vol_t vol = (vol_t) effp->priv;
    
    if (vol->gain == 1)
      return ST_EFF_NULL;

    if (effp->outinfo.channels != effp->ininfo.channels) {
        st_fail("vol cannot handle different channels (in %d, out %d)"
             " use avg or pan", effp->ininfo.channels, effp->outinfo.channels);
        return ST_EOF;
    }

    if (effp->outinfo.rate != effp->ininfo.rate) {
        st_fail("vol cannot handle different rates (in %ld, out %ld)"
             " use resample", effp->ininfo.rate, effp->outinfo.rate);
        return ST_EOF;
    }

    vol->limited = 0;
    vol->totalprocessed = 0;

    return ST_SUCCESS;
}

/*
 * Process data.
 */
static int flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf, 
                st_size_t *isamp, st_size_t *osamp)
{
    vol_t vol = (vol_t) effp->priv;
    register double gain = vol->gain;
    register double limiterthreshhold = vol->limiterthreshhold;
    register double sample;
    register st_size_t len;
    
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
                        sample =  (ST_SAMPLE_MAX - vol->limitergain * (ST_SAMPLE_MAX - sample));
                        vol->limited++;
                }
                else if (sample < -limiterthreshhold)
                {
                        sample = -(ST_SAMPLE_MAX - vol->limitergain * (ST_SAMPLE_MAX + sample));
                        /* FIXME: MIN is (-MAX)-1 so need to make sure we
                         * don't go over that.  Probably could do this
                         * check inside the above equation but I didn't
                         * think it thru.
                         */
                        if (sample < ST_SAMPLE_MIN)
                            sample = ST_SAMPLE_MIN;
                        vol->limited++;
                } else
                        sample = gain * sample;

                ST_SAMPLE_CLIP_COUNT(sample, effp->clips);
               *obuf++ = sample;
            }
    }
    else
    {
        /* quite basic, with clipping */
        for (;len>0; len--)
        {
                sample = gain * *ibuf++;
                ST_SAMPLE_CLIP_COUNT(sample, effp->clips);
                *obuf++ = sample;
        }
    }
    return ST_SUCCESS;
}

static int stop(eff_t effp)
{
  vol_t vol = (vol_t) effp->priv;
  if (vol->limited) {
    st_warn("limited %d values (%d percent).", 
         vol->limited, (int) (vol->limited * 100.0 / vol->totalprocessed));
  }
  return ST_SUCCESS;
}

st_effect_t const * st_vol_effect_fn(void)
{
  static st_effect_t driver = {
    "vol", vol_usage, ST_EFF_MCHAN, getopts, start, flow, 0, stop, 0
  };
  return &driver;
}

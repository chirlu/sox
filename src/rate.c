/*
 * Sound Tools rate change effect.
 *
 * Now obsolete, and implemented by resample. Name retained for
 * backwards compatibility.
 */

#include "st_i.h"
 
int st_resample_getopts(eff_t effp, int n, char **argv);
int st_resample_start(eff_t effp);
int st_resample_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf, 
                     st_size_t *isamp, st_size_t *osamp);
int st_resample_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_resample_stop(eff_t effp);

static st_effect_t st_rate_effect = {
  "rate",
  "Usage: Rate effect takes no options",
  ST_EFF_RATE | ST_EFF_DEPRECATED,
  st_resample_getopts,
  st_resample_start,
  st_resample_flow,
  st_resample_drain,
  st_resample_stop,
  st_effect_nothing
};

const st_effect_t *st_rate_effect_fn(void)
{
    return &st_rate_effect;
}

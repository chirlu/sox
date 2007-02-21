/*
 * libSoX rate change effect.
 *
 * Now obsolete, and implemented by resample. Name retained for
 * backwards compatibility.
 */

#include "sox_i.h"
 
int sox_resample_getopts(eff_t effp, int n, char **argv);
int sox_resample_start(eff_t effp);
int sox_resample_flow(eff_t effp, const sox_sample_t *ibuf, sox_sample_t *obuf, 
                     sox_size_t *isamp, sox_size_t *osamp);
int sox_resample_drain(eff_t effp, sox_sample_t *obuf, sox_size_t *osamp);
int sox_resample_stop(eff_t effp);

static sox_effect_t sox_rate_effect = {
  "rate",
  "Usage: Rate effect takes no options",
  SOX_EFF_RATE | SOX_EFF_DEPRECATED,
  sox_resample_getopts,
  sox_resample_start,
  sox_resample_flow,
  sox_resample_drain,
  sox_resample_stop,
  sox_effect_nothing
};

const sox_effect_t *sox_rate_effect_fn(void)
{
    return &sox_rate_effect;
}

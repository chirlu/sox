/*
 * libSoX rate change effect.
 *
 * Now obsolete, and implemented by resample. Name retained for
 * backwards compatibility.
 */

#include "sox_i.h"
 
int sox_resample_getopts(sox_effect_t effp, int n, char **argv);
int sox_resample_start(sox_effect_t effp);
int sox_resample_flow(sox_effect_t effp, const sox_ssample_t *ibuf, sox_ssample_t *obuf, 
                     sox_size_t *isamp, sox_size_t *osamp);
int sox_resample_drain(sox_effect_t effp, sox_ssample_t *obuf, sox_size_t *osamp);
int sox_resample_stop(sox_effect_t effp);

static sox_effect_handler_t sox_rate_effect = {
  "rate",
  "Usage: Rate effect takes no options",
  SOX_EFF_RATE | SOX_EFF_DEPRECATED,
  sox_resample_getopts,
  sox_resample_start,
  sox_resample_flow,
  sox_resample_drain,
  sox_resample_stop,
  NULL
};

const sox_effect_handler_t *sox_rate_effect_fn(void)
{
    return &sox_rate_effect;
}

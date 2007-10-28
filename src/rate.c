/*
 * libSoX rate change effect.
 *
 * Now obsolete, and implemented by resample. Name retained for
 * backwards compatibility.
 */

#include "sox_i.h"
 
sox_effect_handler_t const * sox_rate_effect_fn(void)
{
  static sox_effect_handler_t handler;
  handler = *sox_resample_effect_fn();
  handler.name = "rate";
  handler.usage = handler.getopts = NULL;
  handler.flags |= SOX_EFF_DEPRECATED;
  return &handler;
}

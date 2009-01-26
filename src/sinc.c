/* Effect: sinc filters     Copyright (c) 2008-9 robs@users.sourceforge.net
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "sox_i.h"
#include "dft_filter.h"
#include "getopt.h"
#include <string.h>

typedef struct {
  dft_filter_priv_t base;
  double att, phase, freq0, freq1, tbw0, tbw1;
  int num_taps[2];
} priv_t;

static int create(sox_effect_t * effp, int argc, char * * argv)
{
  priv_t * p = (priv_t *)effp->priv;
  dft_filter_priv_t * b = &p->base;
  char * parse_ptr = argv[0];
  int i = 0;

  b->filter_ptr = &b->filter;
  p->phase = 50;
  p->att = 120;
  while (i < 2) {
    int c = 1;
    while (c && (c = getopt(argc, argv, "+a:p:t:n:MIL")) != -1) switch (c) {
      GETOPT_NUMERIC('a', att,  40 , 200)
      GETOPT_NUMERIC('p', phase, 0, 100);
      GETOPT_NUMERIC('n', num_taps[1], 10, 32767);
      case 't': p->tbw1 = lsx_parse_frequency(optarg, &parse_ptr);
        if (p->tbw1 < 1 || *parse_ptr) return lsx_usage(effp);
        break;
      case 'M': p->phase =  0; break;
      case 'I': p->phase = 25; break;
      case 'L': p->phase = 50; break;
      default: c = 0;
    }
    if (!i++ && !(p->tbw1 && p->num_taps[1])) {
      p->tbw0 = p->tbw1, p->num_taps[0] = p->num_taps[1];
      if (optind < argc) {
        parse_ptr = argv[optind++];
        if (*parse_ptr != '-')
          p->freq0 = lsx_parse_frequency(parse_ptr, &parse_ptr);
        if (*parse_ptr == '-')
          p->freq1 = lsx_parse_frequency(parse_ptr+1, &parse_ptr);
      }
    }
  }
  return optind != argc || p->freq0 < 0 || p->freq1 < 0 || *parse_ptr ?
      lsx_usage(effp) : SOX_SUCCESS;
}

static void invert(double * h, int n)
{
  int i;
  for (i = 0; i < n; ++i)
    h[i] = -h[i];
  h[(n - 1) / 2] += 1;
}

static double * lpf(sox_effect_t * effp, double Fc, double tbw, int * num_taps, double att, double phase)
{
  double Fn = effp->in_signal.rate * .5;
  if (!Fc) {
    *num_taps = 0;
    return NULL;
  }
  if (phase != 0 && phase != 50 && phase != 100)
    att *= 34./33; /* negate degradation with intermediate phase */
  *num_taps = *num_taps? *num_taps |= 1 :
      min(32767, lsx_lpf_num_taps(att, (tbw? tbw / Fn : .05) * .5, 0));
  lsx_report("num taps = %i", *num_taps);
  return lsx_make_lpf(*num_taps, Fc / Fn, lsx_kaiser_beta(att), 1.);
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  dft_filter_filter_t * f = p->base.filter_ptr;

  if (!f->num_taps) {
    double * h[2];
    int i, longer;
    h[0] = lpf(effp, p->freq0, p->tbw0, &p->num_taps[0], p->att, p->phase);
    h[1] = lpf(effp, p->freq1, p->tbw1, &p->num_taps[1], p->att, p->phase);
    if (h[0]) invert(h[0], p->num_taps[0]);
    longer = p->num_taps[1] > p->num_taps[0];
    f->num_taps = p->num_taps[longer];
    if (h[0] && h[1]) {
      for (i = 0; i < p->num_taps[!longer]; ++i)
        h[longer][i + (f->num_taps - p->num_taps[!longer])/2] += h[!longer][i];
      free(h[!longer]);
      if (p->freq0 < p->freq1)
        invert(h[longer], f->num_taps);
    }
    if (p->phase != 50)
      lsx_fir_to_phase(&h[longer], &f->num_taps, &f->post_peak, p->phase);
    else f->post_peak = f->num_taps / 2;
    lsx_debug("%i %i %g%%", f->num_taps, f->post_peak,
        100 - 100. * f->post_peak / (f->num_taps - 1));
    if (effp->global_info->plot != sox_plot_off) {
      char title[100];
      sprintf(title, "SoX effect: sinc filter freq=%g-%g", p->freq0, p->freq1);
      lsx_plot_fir(h[longer], f->num_taps, effp->in_signal.rate,
          effp->global_info->plot, title, -p->att - 20, 10.);
      return SOX_EOF;
    }
    f->dft_length = lsx_set_dft_length(f->num_taps);
    f->coefs = lsx_calloc(f->dft_length, sizeof(*f->coefs));
    for (i = 0; i < f->num_taps; ++i)
      f->coefs[(i + f->dft_length - f->num_taps + 1) & (f->dft_length - 1)] = h[longer][i] / f->dft_length * 2;
    free(h[longer]);
    lsx_safe_rdft(f->dft_length, 1, f->coefs);
  }
  return sox_dft_filter_effect_fn()->start(effp);
}

sox_effect_handler_t const * sox_sinc_effect_fn(void)
{
  static sox_effect_handler_t handler;
  handler = *sox_dft_filter_effect_fn();
  handler.name = "sinc";
  handler.usage = "[-a att] [-p phase|-M|-I|-L] [-t tbw|-n taps] freq1[-freq2 [-t ...|-n ...]]";
  handler.getopts = create;
  handler.start = start;
  handler.priv_size = sizeof(priv_t);
  return &handler;
}

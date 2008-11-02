/* Effect: dither/noise-shape     Copyright (c) 2008 robs@users.sourceforge.net
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
#include "getopt.h"

#define N 9
#define CONVOLVE _ _ _ _ _ _ _ _ _
static double coefs[][N] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0},
  {2.033, -2.165, 1.959, -1.590, .6149, 0, 0, 0, 0},
  {2.412, -3.370, 3.937, -4.174, 3.353, -2.205, 1.281, -.569, .0847},
  {1.662, -1.263, .4827, -.2913, .1268, -.1124, .03252, -.01265, -.03524},
  {2.847, -4.685, 6.214, -7.184, 6.639, -5.032, 3.263, -1.632, .4191},
};

#define PREC effp->out_signal.precision

#define RAND_CONST 140359821
static int32_t rand_ = ~RAND_CONST;
#define RAND_ (rand_ = rand_ * RAND_CONST + 1)

typedef enum {Pdf_rectangular, Pdf_triangular, Pdf_gaussian} pdf_type_t;
static lsx_enum_item const pdf_types[] = {
  LSX_ENUM_ITEM(Pdf_,rectangular)
  LSX_ENUM_ITEM(Pdf_,triangular)
  LSX_ENUM_ITEM(Pdf_,gaussian)
  {0, 0}};

typedef enum {Shape_none, Shape_lipshitz, Shape_f_weighted,
  Shape_modified_e_weighted, Shape_improved_e_weighted} filter_type_t;
static lsx_enum_item const filter_types[] = {
  LSX_ENUM_ITEM(Shape_,none)
  LSX_ENUM_ITEM(Shape_,lipshitz)
  {"f-weighted", Shape_f_weighted},
  {"modified-e-weighted", Shape_modified_e_weighted},
  {"improved-e-weighted", Shape_improved_e_weighted},
  {0, 0}};

typedef struct {
  pdf_type_t    pdf;
  filter_type_t  filter;
  double        am0, am1, depth;
  double        previous_errors[N * 2];
  size_t        pos;
} priv_t;

static int getopts(sox_effect_t * effp, int argc, char * * argv)
{
  priv_t * p = (priv_t *)effp->priv;
  int c;

  p->pdf = Pdf_triangular;
  while ((c = getopt(argc, argv, "+rtsf:")) != -1) switch (c) {
    case 'r': p->pdf = Pdf_rectangular; break;
    case 't': p->pdf = Pdf_triangular ; break;
    case 's': p->filter = Shape_improved_e_weighted; break;
    case 'f': p->filter = lsx_enum_option(c, filter_types);   break;
    default: lsx_fail("invalid option `-%c'", optopt); return lsx_usage(effp);
  }
  argc -= optind, argv += optind;
  p->depth = 1;
  do {NUMERIC_PARAMETER(depth, .5, 4)} while (0);
  return argc? lsx_usage(effp) : SOX_SUCCESS;
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;

  if (PREC >= 24)
    return SOX_EFF_NULL;   /* Dithering not needed at this resolution */

  if (p->filter &&
      (effp->in_signal.rate < 44100 || effp->in_signal.rate > 48000)) {
    lsx_fail("noise shaping not supported with this sample rate");
    return SOX_EOF;
  }
  p->am1 = p->depth / (1 << PREC);
  p->am0 = (p->pdf == Pdf_triangular) * p->am1;
  lsx_report("pdf=%s filter=%s depth=%g", lsx_find_enum_value(p->pdf, pdf_types)->text, lsx_find_enum_value(p->filter, filter_types)->text, p->depth);
  return SOX_SUCCESS;
}

static int flow(sox_effect_t * effp, const sox_sample_t * ibuf,
    sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t len = *isamp = *osamp = min(*isamp, *osamp);
  int dummy = 0;

  if (p->filter) while (len--) {
    #define _ d += coefs[p->filter][j] * p->previous_errors[p->pos + j], ++j;
    double r = p->am0 * RAND_ + p->am1 * RAND_;
    double error, d = *ibuf++;
    int j = 0;
    CONVOLVE
    *obuf = SOX_ROUND_CLIP_COUNT(d + r, dummy);
    error = d - ((((*obuf++^(1<<31))+(1<<(31-PREC)))&(-1<<(32-PREC)))^(1<<31));
    p->pos = p->pos? p->pos - 1 : p->pos - 1 + N;
    p->previous_errors[p->pos + N] = p->previous_errors[p->pos] = error;
  }
  else while (len--) {
    double d = *ibuf++ + p->am0 * RAND_ + p->am1 * RAND_;
    *obuf++ = SOX_ROUND_CLIP_COUNT(d, dummy);
  }
  return SOX_SUCCESS;
}

sox_effect_handler_t const * sox_dither_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "dither", "[-r|-t] [-s] [depth]",
    SOX_EFF_GETOPT | SOX_EFF_PREC, getopts, start, flow, 0, 0, 0, sizeof(priv_t)
  };
  return &handler;
}

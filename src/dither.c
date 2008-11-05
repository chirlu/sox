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

#ifdef NDEBUG /* Enable assert always. */
#undef NDEBUG /* Must undef above assert.h or other that might include it. */
#endif

#include "sox_i.h"
#include "getopt.h"
#include <assert.h>

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

typedef enum {
  Shape_none, Shape_lipshitz, Shape_f_weighted, Shape_modified_e_weighted,
  Shape_improved_e_weighted, Shape_gesemann, Shape_shibata, Shape_low_shibata, Shape_high_shibata
} filter_name_t;
static lsx_enum_item const filter_names[] = {
  LSX_ENUM_ITEM(Shape_,none)
  LSX_ENUM_ITEM(Shape_,lipshitz)
  {"f-weighted", Shape_f_weighted},
  {"modified-e-weighted", Shape_modified_e_weighted},
  {"improved-e-weighted", Shape_improved_e_weighted},
  LSX_ENUM_ITEM(Shape_,gesemann)
  LSX_ENUM_ITEM(Shape_,shibata)
  {"low-shibata", Shape_low_shibata},
  {"high-shibata", Shape_high_shibata},
  {0, 0}};

typedef struct {
  sox_rate_t  rate;
  enum {fir, iir} type ;
  size_t len;
  double const * coefs;
  filter_name_t name;
} filter_t;

static double const lip44[] = {2.033, -2.165, 1.959, -1.590, .6149};
static double const fwe44[] = {2.412, -3.370, 3.937, -4.174, 3.353, -2.205, 1.281, -.569, .0847};
static double const mew44[] = {1.662, -1.263, .4827, -.2913, .1268, -.1124, .03252, -.01265, -.03524};
static double const iew44[] = {2.847, -4.685, 6.214, -7.184, 6.639, -5.032, 3.263, -1.632, .4191};
static double const ges44[] = {2.2061, -.4706, -.2534, -.6214, 1.0587, .0676, -.6054, -.2738};
static double const ges48[] = {2.2374, -.7339, -.1251, -.6033, .903, .0116, -.5853, -.2571};

static double const shi48[] = {
  2.8720729351043701172,  -5.0413231849670410156,   6.2442994117736816406,  -5.8483986854553222656,
  3.7067542076110839844,  -1.0495119094848632812,  -1.1830236911773681641,   2.1126792430877685547,
 -1.9094531536102294922,   0.99913084506988525391, -0.17090806365013122559, -0.32615602016448974609,
  0.39127644896507263184, -0.26876461505889892578,  0.097676105797290802002,-0.023473845794796943665,
}; /* 48k, N=16, amp=18 */

static double const shi44[] = {
  2.6773197650909423828,  -4.8308925628662109375,   6.570110321044921875,   -7.4572014808654785156,
  6.7263274192810058594,  -4.8481650352478027344,   2.0412089824676513672,   0.7006359100341796875,
 -2.9537565708160400391,   4.0800385475158691406,  -4.1845216751098632812,   3.3311812877655029297,
 -2.1179926395416259766,   0.879302978515625,      -0.031759146600961685181,-0.42382788658142089844,
  0.47882103919982910156, -0.35490813851356506348,  0.17496839165687561035, -0.060908168554306030273,
}; /* 44.1k, N=20, amp=27 */

static double const shi38[] = {
  1.6335992813110351562,  -2.2615492343902587891,   2.4077029228210449219,  -2.6341717243194580078,
  2.1440362930297851562,  -1.8153258562088012695,   1.0816224813461303711,  -0.70302653312683105469,
  0.15991993248462677002,  0.041549518704414367676,-0.29416576027870178223,  0.2518316805362701416,
 -0.27766478061676025391,  0.15785403549671173096, -0.10165894031524658203,  0.016833892092108726501,
}; /* 37.8k, N=16 */

static double const shi32[] = {
  0.82901298999786376953, -0.98922657966613769531,  0.59825712442398071289, -1.0028809309005737305,
  0.59938216209411621094, -0.79502451419830322266,  0.42723315954208374023, -0.54492527246475219727,
  0.30792605876922607422, -0.36871799826622009277,  0.18792048096656799316, -0.2261127084493637085,
  0.10573341697454452515, -0.11435490846633911133,  0.038800679147243499756,-0.040842197835445404053,
}; /* 32k, N=16 */

static double const shi22[] = {
  0.065229974687099456787,-0.54981261491775512695, -0.40278548002243041992, -0.31783768534660339355,
 -0.28201797604560852051, -0.16985194385051727295, -0.15433363616466522217, -0.12507140636444091797,
 -0.08903945237398147583, -0.064410120248794555664,-0.047146003693342208862,-0.032805237919092178345,
 -0.028495194390416145325,-0.011695005930960178375,-0.011831838637590408325,
}; /* 22.05k, N=15 */

static double const shl48[] = {
  2.3925774097442626953,  -3.4350297451019287109,   3.1853709220886230469,  -1.8117271661758422852,
 -0.20124770700931549072,  1.4759907722473144531,  -1.7210904359817504883,   0.97746700048446655273,
 -0.13790138065814971924, -0.38185903429985046387,  0.27421241998672485352,  0.066584214568138122559,
 -0.35223302245140075684,  0.37672343850135803223, -0.23964276909828186035,  0.068674825131893157959,
}; /* 48k, N=16, amp=10 */

static double const shl44[] = {
  2.0833916664123535156,  -3.0418450832366943359,   3.2047898769378662109,  -2.7571926116943359375,
  1.4978630542755126953,  -0.3427594602108001709,  -0.71733748912811279297,  1.0737057924270629883,
 -1.0225815773010253906,   0.56649994850158691406, -0.20968692004680633545, -0.065378531813621520996,
  0.10322438180446624756, -0.067442022264003753662,-0.00495197344571352005,
}; /* 44.1k, N=15, amp=9 */

static double const shh44[] = {
   3.0259189605712890625, -6.0268716812133789062,   9.195003509521484375,  -11.824929237365722656,
  12.767142295837402344, -11.917946815490722656,    9.1739168167114257812,  -5.3712320327758789062,
   1.1393624544143676758,  2.4484779834747314453,  -4.9719839096069335938,   6.0392003059387207031,
  -5.9359521865844726562,  4.903278350830078125,   -3.5527443885803222656,   2.1909697055816650391,
  -1.1672389507293701172,  0.4903914332389831543,  -0.16519790887832641602,  0.023217858746647834778,
}; /* 44.1k, N=20 */

static const filter_t filters[] = {
  {44100, fir,  5, lip44, Shape_lipshitz},
  {46000, fir,  9, fwe44, Shape_f_weighted},
  {46000, fir,  9, mew44, Shape_modified_e_weighted},
  {46000, fir,  9, iew44, Shape_improved_e_weighted},
  {48000, iir,  4, ges48, Shape_gesemann},
  {44100, iir,  4, ges44, Shape_gesemann},
  {48000, fir, 16, shi48, Shape_shibata},
  {44100, fir, 20, shi44, Shape_shibata},
  {37800, fir, 16, shi38, Shape_shibata},
  {32000, fir, 16, shi32, Shape_shibata},
  {22050, fir, 15, shi22, Shape_shibata},
  {48000, fir, 16, shl48, Shape_low_shibata},
  {44100, fir, 15, shl44, Shape_low_shibata},
  {44100, fir, 20, shh44, Shape_high_shibata},
  {    0, fir,  0,  NULL, Shape_none},
};

#define MAX_N 30

typedef struct {
  pdf_type_t    pdf;
  filter_name_t filter_name;
  double        am0, am1, depth;
  double        previous_errors[MAX_N * 2];
  double        previous_outputs[MAX_N * 2];
  size_t        pos;
  double const * coefs;
  int           (*flow)(sox_effect_t *, const sox_sample_t *, sox_sample_t *, size_t *, size_t *);
} priv_t;

#define CONVOLVE _ _ _ _
#define NAME flow_iir_4
#define N 4
#include "dither_iir.h"
#define CONVOLVE _ _ _ _ _
#define NAME flow_fir_5
#define N 5
#include "dither_fir.h"
#define CONVOLVE _ _ _ _ _ _ _ _ _
#define NAME flow_fir_9
#define N 9
#include "dither_fir.h"
#define CONVOLVE _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
#define NAME flow_fir_15
#define N 15
#include "dither_fir.h"
#define CONVOLVE _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
#define NAME flow_fir_16
#define N 16
#include "dither_fir.h"
#define CONVOLVE _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
#define NAME flow_fir_20
#define N 20
#include "dither_fir.h"

static int flow_no_shape(sox_effect_t * effp, const sox_sample_t * ibuf,
    sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;

  size_t len = *isamp = *osamp = min(*isamp, *osamp);
  int dummy = 0;

  while (len--) {
    double d = *ibuf++ + p->am0 * RAND_ + p->am1 * RAND_;
    *obuf++ = SOX_ROUND_CLIP_COUNT(d, dummy);
  }
  return SOX_SUCCESS;
}

static int getopts(sox_effect_t * effp, int argc, char * * argv)
{
  priv_t * p = (priv_t *)effp->priv;
  int c;

  p->pdf = Pdf_triangular;
  while ((c = getopt(argc, argv, "+rtsf:")) != -1) switch (c) {
    case 'r': p->pdf = Pdf_rectangular; break;
    case 't': p->pdf = Pdf_triangular ; break;
    case 's': p->filter_name = Shape_shibata; break;
    case 'f':
      p->filter_name = lsx_enum_option(c, filter_names);
      if (p->filter_name == INT_MAX)
        return SOX_EOF;
      break;
    default: lsx_fail("invalid option `-%c'", optopt); return lsx_usage(effp);
  }
  argc -= optind, argv += optind;
  p->depth = 1;
  do {NUMERIC_PARAMETER(depth, .5, 1)} while (0);
  return argc? lsx_usage(effp) : SOX_SUCCESS;
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;

  if (PREC >= 24)
    return SOX_EFF_NULL;   /* Dithering not needed at this resolution */

  if (!p->filter_name)
    p->flow = flow_no_shape;
  else {
    filter_t const * f;

    for (f = filters; f->len && (f->name != p->filter_name || fabs(effp->in_signal.rate - f->rate) / f->rate > .05); ++f);
    if (!f->len) {
      lsx_fail("no `%s' filter is available for rate %g", lsx_find_enum_value(p->filter_name, filter_names)->text, effp->in_signal.rate);
      return SOX_EOF;
    }
    assert(f->len <= MAX_N);
    if (f->type == fir) switch(f->len) {
      case  5: p->flow = flow_fir_5 ; break;
      case  9: p->flow = flow_fir_9 ; break;
      case 15: p->flow = flow_fir_15; break;
      case 16: p->flow = flow_fir_16; break;
      case 20: p->flow = flow_fir_20; break;
      default: assert(sox_false);
    } else switch(f->len) {
      case  4: p->flow = flow_iir_4 ; break;
      default: assert(sox_false);
    }
    p->coefs = f->coefs;
  }
  p->am1 = p->depth / (1 << PREC);
  p->am0 = (p->pdf == Pdf_triangular) * p->am1;
  lsx_debug("pdf=%s filter=%s depth=%g", lsx_find_enum_value(p->pdf, pdf_types)->text, lsx_find_enum_value(p->filter_name, filter_names)->text, p->depth);
  return SOX_SUCCESS;
}

static int flow(sox_effect_t * effp, const sox_sample_t * ibuf,
    sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  return p->flow(effp, ibuf, obuf, isamp, osamp);
}

sox_effect_handler_t const * sox_dither_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "dither", "[-r|-t] [-s|-f filter] [depth]"
    "\n  -r       Rectangular PDF"
    "\n  -t       Triangular PDF (default)"
    "\n  -s       Shape noise (with shibata filter)"
    "\n  -f name  Set shaping filter to one of: lipshitz, f-weighted,"
    "\n           modified-e-weighted, improved-e-weighted, gesemann,"
    "\n           shibata, low-shibata, high-shibata."
    "\n  depth    Noise depth; 0.5 to 1; default 1",
    SOX_EFF_GETOPT | SOX_EFF_PREC, getopts, start, flow, 0, 0, 0, sizeof(priv_t)
  };
  return &handler;
}

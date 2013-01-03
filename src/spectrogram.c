/* libSoX effect: Spectrogram       (c) 2008-9 robs@users.sourceforge.net
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
#include "fft4g.h"
#include <assert.h>
#include <math.h>
#ifdef HAVE_LIBPNG_PNG_H
#include <libpng/png.h>
#else
#include <png.h>
#endif
#include <zlib.h>

#define MAX_FFT_SIZE 4096
#define is_p2(x) !(x & (x - 1))

#define MAX_X_SIZE 200000

typedef enum {Window_Hann, Window_Hamming, Window_Bartlett, Window_Rectangular, Window_Kaiser} win_type_t;
static lsx_enum_item const window_options[] = {
  LSX_ENUM_ITEM(Window_,Hann)
  LSX_ENUM_ITEM(Window_,Hamming)
  LSX_ENUM_ITEM(Window_,Bartlett)
  LSX_ENUM_ITEM(Window_,Rectangular)
  LSX_ENUM_ITEM(Window_,Kaiser)
  {0, 0}};

typedef struct {
  /* Parameters */
  double     pixels_per_sec, duration, start_time,  window_adjust;
  int        x_size0, y_size, Y_size, dB_range, gain, spectrum_points, perm;
  sox_bool   monochrome, light_background, high_colour, slack_overlap, no_axes;
  sox_bool   raw, alt_palette, truncate;
  win_type_t win_type;
  char const * out_name, * title, * comment;

  /* Shared work area */
  double     * shared, * * shared_ptr;

  /* Per-channel work area */
  int        WORK;  /* Start of work area is marked by this dummy variable. */
  uint64_t   skip;
  int        dft_size, step_size, block_steps, block_num, rows, cols, read;
  int        x_size, end, end_min, last_end;
  sox_bool   truncated;
  double     buf[MAX_FFT_SIZE], dft_buf[MAX_FFT_SIZE], window[MAX_FFT_SIZE];
  double     block_norm, max, magnitudes[(MAX_FFT_SIZE>>1) + 1];
  float      * dBfs;
} priv_t;

#define secs(cols) \
  ((double)(cols) * p->step_size * p->block_steps / effp->in_signal.rate)

static unsigned char const alt_palette[] =
  "\0\0\0\0\0\3\0\1\5\0\1\10\0\1\12\0\1\13\0\1\16\1\2\20\1\2\22\1\2\25\1\2\26"
  "\1\2\30\1\3\33\1\3\35\1\3\37\1\3\40\1\3\"\1\3$\1\3%\1\3'\1\3(\1\3*\1\3,\1"
  "\3.\1\3/\1\3""0\1\3""2\1\3""4\2\3""6\4\3""8\5\3""9\7\3;\11\3=\13\3?\16\3"
  "A\17\2B\21\2D\23\2F\25\2H\27\2J\30\2K\32\2M\35\2O\40\2Q$\2S(\2U+\2W0\2Z3"
  "\2\\7\2_;\2a>\2cB\2eE\2hI\2jM\2lQ\2nU\2pZ\2r_\2tc\2uh\2vl\2xp\3zu\3|z\3}"
  "~\3~\203\3\200\207\3\202\214\3\204\220\3\205\223\3\203\226\3\200\230\3~\233"
  "\3|\236\3z\240\3x\243\3u\246\3s\251\3q\253\3o\256\3m\261\3j\263\3h\266\3"
  "f\272\3b\274\3^\300\3Z\303\3V\307\3R\312\3N\315\3J\321\3F\324\3C\327\3>\333"
  "\3:\336\3""6\342\3""2\344\3/\346\7-\350\15,\352\21+\354\27*\355\33)\356\40"
  "(\360&'\362*&\364/$\3654#\3669#\370>!\372C\40\374I\40\374O\"\374V&\374]*"
  "\374d,\374k0\374r3\374z7\375\201;\375\210>\375\217B\375\226E\375\236I\375"
  "\245M\375\254P\375\261T\375\267X\375\274\\\375\301a\375\306e\375\313i\375"
  "\320m\376\325q\376\332v\376\337z\376\344~\376\351\202\376\356\206\376\363"
  "\213\375\365\217\374\366\223\373\367\230\372\367\234\371\370\241\370\371"
  "\245\367\371\252\366\372\256\365\372\263\364\373\267\363\374\274\361\375"
  "\300\360\375\305\360\376\311\357\376\314\357\376\317\360\376\321\360\376"
  "\324\360\376\326\360\376\330\360\376\332\361\377\335\361\377\337\361\377"
  "\341\361\377\344\361\377\346\362\377\350\362\377\353";
#define alt_palette_len ((array_length(alt_palette) - 1) / 3)

static int getopts(sox_effect_t * effp, int argc, char **argv)
{
  priv_t * p = (priv_t *)effp->priv;
  uint64_t duration;
  char const * next;
  int c;
  lsx_getopt_t optstate;
  lsx_getopt_init(argc, argv, "+S:d:x:X:y:Y:z:Z:q:p:W:w:st:c:AarmlhTo:", NULL, lsx_getopt_flag_none, 1, &optstate);

  p->dB_range = 120, p->spectrum_points = 249, p->perm = 1; /* Non-0 defaults */
  p->out_name = "spectrogram.png", p->comment = "Created by SoX";

  while ((c = lsx_getopt(&optstate)) != -1) switch (c) {
    GETOPT_NUMERIC(optstate, 'x', x_size0       , 100, MAX_X_SIZE)
    GETOPT_NUMERIC(optstate, 'X', pixels_per_sec,  1 , 5000)
    GETOPT_NUMERIC(optstate, 'y', y_size        , 64 , 1200)
    GETOPT_NUMERIC(optstate, 'Y', Y_size        , 130, MAX_FFT_SIZE / 2 + 2)
    GETOPT_NUMERIC(optstate, 'z', dB_range      , 20 , 180)
    GETOPT_NUMERIC(optstate, 'Z', gain          ,-100, 100)
    GETOPT_NUMERIC(optstate, 'q', spectrum_points, 0 , p->spectrum_points)
    GETOPT_NUMERIC(optstate, 'p', perm          ,  1 , 6)
    GETOPT_NUMERIC(optstate, 'W', window_adjust , -10, 10)
    case 'w': p->win_type = lsx_enum_option(c, optstate.arg, window_options);   break;
    case 's': p->slack_overlap    = sox_true;   break;
    case 'A': p->alt_palette      = sox_true;   break;
    case 'a': p->no_axes          = sox_true;   break;
    case 'r': p->raw              = sox_true;   break;
    case 'm': p->monochrome       = sox_true;   break;
    case 'l': p->light_background = sox_true;   break;
    case 'h': p->high_colour      = sox_true;   break;
    case 'T': p->truncate         = sox_true;   break;
    case 't': p->title            = optstate.arg; break;
    case 'c': p->comment          = optstate.arg; break;
    case 'o': p->out_name         = optstate.arg; break;
    case 'S': next = lsx_parsesamples(1e5, optstate.arg, &duration, 't');
      if (next && !*next) {p->start_time = duration * 1e-5; break;}
      return lsx_usage(effp);
    case 'd': next = lsx_parsesamples(1e5, optstate.arg, &duration, 't');
      if (next && !*next) {p->duration = duration * 1e-5; break;}
      return lsx_usage(effp);
    default: lsx_fail("invalid option `-%c'", optstate.opt); return lsx_usage(effp);
  }
  if (!!p->x_size0 + !!p->pixels_per_sec + !!p->duration > 2) {
    lsx_fail("only two of -x, -X, -d may be given");
    return SOX_EOF;
  }
  if (p->y_size && p->Y_size) {
    lsx_fail("only one of -y, -Y may be given");
    return SOX_EOF;
  }
  p->gain = -p->gain;
  --p->perm;
  p->spectrum_points += 2;
  if (p->alt_palette)
    p->spectrum_points = min(p->spectrum_points, (int)alt_palette_len);
  p->shared_ptr = &p->shared;
  return optstate.ind !=argc || p->win_type == INT_MAX? lsx_usage(effp) : SOX_SUCCESS;
}

static double make_window(priv_t * p, int end)
{
  double sum = 0, * w = end < 0? p->window : p->window + end;
  int i, n = p->dft_size - abs(end);

  if (end) memset(p->window, 0, sizeof(p->window));
  for (i = 0; i < n; ++i) w[i] = 1;
  switch (p->win_type) {
    case Window_Hann: lsx_apply_hann(w, n); break;
    case Window_Hamming: lsx_apply_hamming(w, n); break;
    case Window_Bartlett: lsx_apply_bartlett(w, n); break;
    case Window_Rectangular: break;
    default: lsx_apply_kaiser(w, n, lsx_kaiser_beta(
        (p->dB_range + p->gain) * (1.1 + p->window_adjust / 50)));
  }
  for (i = 0; i < p->dft_size; ++i) sum += p->window[i];
  for (i = 0; i < p->dft_size; ++i) p->window[i] *= 2 / sum
    * sqr((double)n / p->dft_size);    /* empirical small window adjustment */
  return sum;
}

static double * rdft_init(int n)
{
  double * q = lsx_malloc(2 * (n / 2 + 1) * n * sizeof(*q)), * p = q;
  int i, j;
  for (j = 0; j <= n / 2; ++j) for (i = 0; i < n; ++i)
    *p++ = cos(2 * M_PI * j * i / n), *p++ = sin(2 * M_PI * j * i / n);
  return q;
}

#define _ re += in[i] * *q++, im += in[i++] * *q++,
static void rdft_p(double const * q, double const * in, double * out, int n)
{
  int i, j;
  for (j = 0; j <= n / 2; ++j) {
    double re = 0, im = 0;
    for (i = 0; i < (n & ~7);) _ _ _ _ _ _ _ _ 0;
    while (i < n) _ 0;
    *out++ += re * re + im * im;
  }
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  double actual, duration = p->duration, pixels_per_sec = p->pixels_per_sec;

  memset(&p->WORK, 0, sizeof(*p) - field_offset(priv_t, WORK));

  p->skip = p->start_time * effp->in_signal.rate + .5;
  p->x_size = p->x_size0;
  while (sox_true) {
    if (!pixels_per_sec && p->x_size && duration)
      pixels_per_sec = min(5000, p->x_size / duration);
    else if (!p->x_size && pixels_per_sec && duration)
      p->x_size = min(MAX_X_SIZE, (int)(pixels_per_sec * duration + .5));
    if (!duration && effp->in_signal.length != SOX_UNKNOWN_LEN) {
      duration = effp->in_signal.length / (effp->in_signal.rate * effp->in_signal.channels);
      duration -= p->start_time;
      if (duration <= 0)
        duration = 1;
      continue;
    } else if (!p->x_size) {
      p->x_size = 800;
      continue;
    } else if (!pixels_per_sec) {
      pixels_per_sec = 100;
      continue;
    }
    break;
  }

  if (p->y_size) {
    p->dft_size = 2 * (p->y_size - 1);
    if (!is_p2(p->dft_size) && !effp->flow)
      p->shared = rdft_init(p->dft_size);
  } else {
   int y = max(32, (p->Y_size? p->Y_size : 550) / effp->in_signal.channels - 2);
   for (p->dft_size = 128; p->dft_size <= y; p->dft_size <<= 1);
  }
  if (is_p2(p->dft_size) && !effp->flow)
    lsx_safe_rdft(p->dft_size, 1, p->dft_buf);
  lsx_debug("duration=%g x_size=%i pixels_per_sec=%g dft_size=%i", duration, p->x_size, pixels_per_sec, p->dft_size);

  p->end = p->dft_size;
  p->rows = (p->dft_size >> 1) + 1;
  actual = make_window(p, p->last_end = 0);
  lsx_debug("window_density=%g", actual / p->dft_size);
  p->step_size = (p->slack_overlap? sqrt(actual * p->dft_size) : actual) + .5;
  p->block_steps = effp->in_signal.rate / pixels_per_sec;
  p->step_size = p->block_steps / ceil((double)p->block_steps / p->step_size) +.5;
  p->block_steps = floor((double)p->block_steps / p->step_size +.5);
  p->block_norm = 1. / p->block_steps;
  actual = effp->in_signal.rate / p->step_size / p->block_steps;
  if (!effp->flow && actual != pixels_per_sec)
    lsx_report("actual pixels/s = %g", actual);
  lsx_debug("step_size=%i block_steps=%i", p->step_size, p->block_steps);
  p->max = -p->dB_range;
  p->read = (p->step_size - p->dft_size) / 2;
  return SOX_SUCCESS;
}

static int do_column(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  int i;

  if (p->cols == p->x_size) {
    p->truncated = sox_true;
    if (!effp->flow)
      lsx_report("PNG truncated at %g seconds", secs(p->cols));
    return p->truncate? SOX_EOF : SOX_SUCCESS;
  }
  ++p->cols;
  p->dBfs = lsx_realloc(p->dBfs, p->cols * p->rows * sizeof(*p->dBfs));
  for (i = 0; i < p->rows; ++i) {
    double dBfs = 10 * log10(p->magnitudes[i] * p->block_norm);
    p->dBfs[(p->cols - 1) * p->rows + i] = dBfs + p->gain;
    p->max = max(dBfs, p->max);
  }
  memset(p->magnitudes, 0, p->rows * sizeof(*p->magnitudes));
  p->block_num = 0;
  return SOX_SUCCESS;
}

static int flow(sox_effect_t * effp,
    const sox_sample_t * ibuf, sox_sample_t * obuf,
    size_t * isamp, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t len = *isamp = *osamp = min(*isamp, *osamp);
  int i;

  memcpy(obuf, ibuf, len * sizeof(*obuf)); /* Pass on audio unaffected */

  if (p->skip) {
    if (p->skip >= len) {
      p->skip -= len;
      return SOX_SUCCESS;
    }
    ibuf += p->skip;
    len -= p->skip;
    p->skip = 0;
  }
  while (!p->truncated) {
    if (p->read == p->step_size) {
      memmove(p->buf, p->buf + p->step_size,
          (p->dft_size - p->step_size) * sizeof(*p->buf));
      p->read = 0;
    }
    for (; len && p->read < p->step_size; --len, ++p->read, --p->end)
      p->buf[p->dft_size - p->step_size + p->read] =
        SOX_SAMPLE_TO_FLOAT_64BIT(*ibuf++,);
    if (p->read != p->step_size)
      break;

    if ((p->end = max(p->end, p->end_min)) != p->last_end)
      make_window(p, p->last_end = p->end);
    for (i = 0; i < p->dft_size; ++i) p->dft_buf[i] = p->buf[i] * p->window[i];
    if (is_p2(p->dft_size)) {
      lsx_safe_rdft(p->dft_size, 1, p->dft_buf);
      p->magnitudes[0] += sqr(p->dft_buf[0]);
      for (i = 1; i < p->dft_size >> 1; ++i)
        p->magnitudes[i] += sqr(p->dft_buf[2*i]) + sqr(p->dft_buf[2*i+1]);
      p->magnitudes[p->dft_size >> 1] += sqr(p->dft_buf[1]);
    }
    else rdft_p(*p->shared_ptr, p->dft_buf, p->magnitudes, p->dft_size);
    if (++p->block_num == p->block_steps && do_column(effp) == SOX_EOF)
      return SOX_EOF;
  }
  return SOX_SUCCESS;
}

static int drain(sox_effect_t * effp, sox_sample_t * obuf_, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;

  if (!p->truncated) {
    sox_sample_t * ibuf = lsx_calloc(p->dft_size, sizeof(*ibuf));
    sox_sample_t * obuf = lsx_calloc(p->dft_size, sizeof(*obuf));
    size_t isamp = (p->dft_size - p->step_size) / 2;
    int left_over = (isamp + p->read) % p->step_size;

    if (left_over >= p->step_size >> 1)
      isamp += p->step_size - left_over;
    lsx_debug("cols=%i left=%i end=%i", p->cols, p->read, p->end);
    p->end = 0, p->end_min = -p->dft_size;
    if (flow(effp, ibuf, obuf, &isamp, &isamp) == SOX_SUCCESS && p->block_num) {
      p->block_norm *= (double)p->block_steps / p->block_num;
      do_column(effp);
    }
    lsx_debug("flushed cols=%i left=%i end=%i", p->cols, p->read, p->end);
    free(obuf);
    free(ibuf);
  }
  (void)obuf_, *osamp = 0;
  return SOX_SUCCESS;
}

enum {Background, Text, Labels, Grid, fixed_palette};

static unsigned colour(priv_t const * p, double x)
{
  unsigned c = x < -p->dB_range? 0 : x >= 0? p->spectrum_points - 1 :
      1 + (1 + x / p->dB_range) * (p->spectrum_points - 2);
  return fixed_palette + c;
}

static void make_palette(priv_t const * p, png_color * palette)
{
  int i;

  if (p->light_background) {
    memcpy(palette++, (p->monochrome)? "\337\337\337":"\335\330\320", (size_t)3);
    memcpy(palette++, "\0\0\0"      , (size_t)3);
    memcpy(palette++, "\077\077\077", (size_t)3);
    memcpy(palette++, "\077\077\077", (size_t)3);
  } else {
    memcpy(palette++, "\0\0\0"      , (size_t)3);
    memcpy(palette++, "\377\377\377", (size_t)3);
    memcpy(palette++, "\277\277\277", (size_t)3);
    memcpy(palette++, "\177\177\177", (size_t)3);
  }
  for (i = 0; i < p->spectrum_points; ++i) {
    double c[3], x = (double)i / (p->spectrum_points - 1);
    int at = p->light_background? p->spectrum_points - 1 - i : i;
    if (p->monochrome) {
      c[2] = c[1] = c[0] = x;
      if (p->high_colour) {
        c[(1 + p->perm) % 3] = x < .4? 0 : 5 / 3. * (x - .4);
        if (p->perm < 3)
          c[(2 + p->perm) % 3] = x < .4? 0 : 5 / 3. * (x - .4);
      }
      palette[at].red  = .5 + 255 * c[0];
      palette[at].green= .5 + 255 * c[1];
      palette[at].blue = .5 + 255 * c[2];
      continue;
    }
    if (p->high_colour) {
      static const int states[3][7] = {
        {4,5,0,0,2,1,1}, {0,0,2,1,1,3,2}, {4,1,1,3,0,0,2}};
      int j, phase_num = min(7 * x, 6);
      for (j = 0; j < 3; ++j) switch (states[j][phase_num]) {
        case 0: c[j] = 0; break;
        case 1: c[j] = 1; break;
        case 2: c[j] = sin((7 * x - phase_num) * M_PI / 2); break;
        case 3: c[j] = cos((7 * x - phase_num) * M_PI / 2); break;
        case 4: c[j] =      7 * x - phase_num;  break;
        case 5: c[j] = 1 - (7 * x - phase_num); break;
      }
    } else if (p->alt_palette) {
      int n = (double)i / (p->spectrum_points - 1) * (alt_palette_len - 1) + .5;
      c[0] = alt_palette[3 * n + 0] / 255.;
      c[1] = alt_palette[3 * n + 1] / 255.;
      c[2] = alt_palette[3 * n + 2] / 255.;
    } else {
      if      (x < .13) c[0] = 0;
      else if (x < .73) c[0] = 1  * sin((x - .13) / .60 * M_PI / 2);
      else              c[0] = 1;
      if      (x < .60) c[1] = 0;
      else if (x < .91) c[1] = 1  * sin((x - .60) / .31 * M_PI / 2);
      else              c[1] = 1;
      if      (x < .60) c[2] = .5 * sin((x - .00) / .60 * M_PI);
      else if (x < .78) c[2] = 0;
      else              c[2] =          (x - .78) / .22;
    }
    palette[at].red  = .5 + 255 * c[p->perm % 3];
    palette[at].green= .5 + 255 * c[(1 + p->perm + (p->perm % 2)) % 3];
    palette[at].blue = .5 + 255 * c[(2 + p->perm - (p->perm % 2)) % 3];
  }
}

static const Bytef fixed[] =
  "x\332eT\241\266\2450\fDVV>Y\371$re%2\237\200|2\22YY\211D\"+\337'<y\345\312"
  "\375\fd\345f\222\224\313\236\235{\270\344L\247a\232\4\246\351\201d\230\222"
  "\304D\364^ \352\362S\"m\347\311\237\237\27\64K\243\2302\265\35\v\371<\363y"
  "\354_\226g\354\214)e \2458\341\17\20J4\215[z<\271\277\367\0\63\64@\177\330c"
  "\227\204 Ir.\5$U\200\260\224\326S\17\200=\\k\20QA\334%\342\20*\303P\234\211"
  "\366\36#\370R\276_\316s-\345\222Dlz\363my*;\217\373\346z\267\343\236\364\246"
  "\236\365\2419\305p\333\267\23(\207\265\333\233\325Y\342\243\265\357\262\215"
  "\263t\271$\276\226ea\271.\367&\320\347\202_\234\27\377\345\222\253?\3422\364"
  "\207y\256\236\229\331\33\f\376\227\266\"\356\253j\366\363\347\334US\34]\371?"
  "\255\371\336\372z\265v\34\226\247\32\324\217\334\337\317U4\16\316{N\370\31"
  "\365\357iL\231y\33y\264\211D7\337\4\244\261\220D\346\1\261\357\355>\3\342"
  "\223\363\0\303\277\f[\214A,p\34`\255\355\364\37\372\224\342\277\f\207\255\36"
  "_V\7\34\241^\316W\257\177\b\242\300\34\f\276\33?/9_\331f\346\36\25Y)\2301"
  "\257\2414|\35\365\237\3424k\3\244\3\242\261\6\b\275>z$\370\215:\270\363w\36/"
  "\265kF\v\20o6\242\301\364\336\27\325\257\321\364fs\231\215G\32=\257\305Di"
  "\304^\177\304R\364Q=\225\373\33\320\375\375\372\200\337\37\374}\334\337\20"
  "\364\310(]\304\267b\177\326Yrj\312\277\373\233\37\340";
static unsigned char * font;
#define font_x 5
#define font_y 12
#define font_X (font_x + 1)

#define pixel(x,y) pixels[(y) * cols + (x)]
#define print_at(x,y,c,t) print_at_(pixels,cols,x,y,c,t,0)
#define print_up(x,y,c,t) print_at_(pixels,cols,x,y,c,t,1)

static void print_at_(png_byte * pixels, int cols, int x, int y, int c, char const * text, int orientation)
{
  for (;*text; ++text) {
    int pos = ((*text < ' ' || *text > '~'? '~' + 1 : *text) - ' ') * font_y;
    int i, j;
    for (i = 0; i < font_y; ++i) {
      unsigned line = font[pos++];
      for (j = 0; j < font_x; ++j, line <<= 1)
        if (line & 0x80) switch (orientation) {
          case 0: pixel(x + j, y - i) = c; break;
          case 1: pixel(x + i, y + j) = c; break;
        }
    }
    switch (orientation) {
      case 0: x += font_X; break;
      case 1: y += font_X; break;
    }
  }
}

static int axis(double to, int max_steps, double * limit, char * * prefix)
{
  double scale = 1, step = max(1, 10 * to);
  int i, prefix_num = 0;
  if (max_steps) {
    double try, log_10 = HUGE_VAL, min_step = (to *= 10) / max_steps;
    for (i = 5; i; i >>= 1) if ((try = ceil(log10(min_step * i))) <= log_10)
      step = pow(10., log_10 = try) / i, log_10 -= i > 1;
    prefix_num = floor(log_10 / 3);
    scale = pow(10., -3. * prefix_num);
  }
  *prefix = "pnum-kMGTPE" + prefix_num + (prefix_num? 4 : 11);
  *limit = to * scale;
  return step * scale + .5;
}

#define below 48
#define left 58
#define between 37
#define spectrum_width 14
#define right 35

static int stop(sox_effect_t * effp)
{
  priv_t *    p        = (priv_t *) effp->priv;
  FILE *      file     = fopen(p->out_name, "wb");
  uLong       font_len = 96 * font_y;
  int         chans    = effp->in_signal.channels;
  int         c_rows   = p->rows * chans + chans - 1;
  int         rows     = p->raw? c_rows : below + c_rows + 30 + 20 * !!p->title;
  int         cols     = p->raw? p->cols : left + p->cols + between + spectrum_width + right;
  png_byte *  pixels   = lsx_malloc(cols * rows * sizeof(*pixels));
  png_bytepp  png_rows = lsx_malloc(rows * sizeof(*png_rows));
  png_structp png      = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0,0);
  png_infop   png_info = png_create_info_struct(png);
  png_color   palette[256];
  int         i, j, k, base, step, tick_len = 3 - p->no_axes;
  char        text[200], * prefix;
  double      limit;

  free(p->shared);
  if (!file) {
    lsx_fail("failed to create `%s': %s", p->out_name, strerror(errno));
    goto error;
  }
  lsx_debug("signal-max=%g", p->max);
  font = lsx_malloc(font_len);
  assert(uncompress(font, &font_len, fixed, sizeof(fixed)-1) == Z_OK);
  make_palette(p, palette);
  memset(pixels, Background, cols * rows * sizeof(*pixels));
  png_init_io(png, file);
  png_set_PLTE(png, png_info, palette, fixed_palette + p->spectrum_points);
  png_set_IHDR(png, png_info, (png_uint_32)cols, (png_uint_32)rows, 8,
      PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
      PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  for (j = 0; j < rows; ++j)               /* Put (0,0) at bottom-left of PNG */
    png_rows[rows - 1 - j] = (png_bytep)(pixels + j * cols);

  /* Spectrogram */
  for (k = 0; k < chans; ++k) {
    priv_t * q = (priv_t *)(effp - effp->flow + k)->priv;
    base = !p->raw * below + (chans - 1 - k) * (p->rows + 1);
    for (j = 0; j < p->rows; ++j) {
      for (i = 0; i < p->cols; ++i)
        pixel(!p->raw * left + i, base + j) = colour(p, q->dBfs[i*p->rows + j]);
      if (!p->raw && !p->no_axes)                                 /* Y-axis lines */
        pixel(left - 1, base + j) = pixel(left + p->cols, base + j) = Grid;
    }
    if (!p->raw && !p->no_axes) for (i = -1; i <= p->cols; ++i)   /* X-axis lines */
      pixel(left + i, base - 1) = pixel(left + i, base + p->rows) = Grid;
  }

  if (!p->raw) {
    if (p->title && (i = (int)strlen(p->title) * font_X) < cols + 1) /* Title */
      print_at((cols - i) / 2, rows - font_y, Text, p->title);

    if ((int)strlen(p->comment) * font_X < cols + 1)     /* Footer comment */
      print_at(1, font_y, Text, p->comment);

    /* X-axis */
    step = axis(secs(p->cols), p->cols / (font_X * 9 / 2), &limit, &prefix);
    sprintf(text, "Time (%.1ss)", prefix);               /* Axis label */
    print_at(left + (p->cols - font_X * (int)strlen(text)) / 2, 24, Text, text);
    for (i = 0; i <= limit; i += step) {
      int y, x = limit? (double)i / limit * p->cols + .5 : 0;
      for (y = 0; y < tick_len; ++y)                     /* Ticks */
        pixel(left-1+x, below-1-y) = pixel(left-1+x, below+c_rows+y) = Grid;
      if (step == 5 && (i%10))
        continue;
      sprintf(text, "%g", .1 * i);                       /* Tick labels */
      x = left + x - 3 * strlen(text);
      print_at(x, below - 6, Labels, text);
      print_at(x, below + c_rows + 14, Labels, text);
    }

    /* Y-axis */
    step = axis(effp->in_signal.rate / 2,
        (p->rows - 1) / ((font_y * 3 + 1) >> 1), &limit, &prefix);
    sprintf(text, "Frequency (%.1sHz)", prefix);         /* Axis label */
    print_up(10, below + (c_rows - font_X * (int)strlen(text)) / 2, Text, text);
    for (k = 0; k < chans; ++k) {
      base = below + k * (p->rows + 1);
      for (i = 0; i <= limit; i += step) {
        int x, y = limit? (double)i / limit * (p->rows - 1) + .5 : 0;
        for (x = 0; x < tick_len; ++x)                   /* Ticks */
          pixel(left-1-x, base+y) = pixel(left+p->cols+x, base+y) = Grid;
        if ((step == 5 && (i%10)) || (!i && k && chans > 1))
          continue;
        sprintf(text, i?"%5g":"   DC", .1 * i);          /* Tick labels */
        print_at(left - 4 - font_X * 5, base + y + 5, Labels, text);
        sprintf(text, i?"%g":"DC", .1 * i);
        print_at(left + p->cols + 6, base + y + 5, Labels, text);
      }
    }

    /* Z-axis */
    k = min(400, c_rows);
    base = below + (c_rows - k) / 2;
    print_at(cols - right - 2 - font_X, base - 13, Text, "dBFS");/* Axis label */
    for (j = 0; j < k; ++j) {                            /* Spectrum */
      png_byte b = colour(p, p->dB_range * (j / (k - 1.) - 1));
      for (i = 0; i < spectrum_width; ++i)
        pixel(cols - right - 1 - i, base + j) = b;
    }
    step = 10 * ceil(p->dB_range / 10. * (font_y + 2) / (k - 1));
    for (i = 0; i <= p->dB_range; i += step) {           /* (Tick) labels */
      int y = (double)i / p->dB_range * (k - 1) + .5;
      sprintf(text, "%+i", i - p->gain - p->dB_range);
      print_at(cols - right + 1, base + y + 5, Labels, text);
    }
  }
  free(font);
  png_set_rows(png, png_info, png_rows);
  png_write_png(png, png_info, PNG_TRANSFORM_IDENTITY, NULL);
  fclose(file);
error: png_destroy_write_struct(&png, &png_info);
  free(png_rows);
  free(pixels);
  free(p->dBfs);
  return SOX_SUCCESS;
}

static int end(sox_effect_t * effp) {return effp->flow? SOX_SUCCESS:stop(effp);}

sox_effect_handler_t const * lsx_spectrogram_effect_fn(void)
{
  static sox_effect_handler_t handler = {"spectrogram", 0, SOX_EFF_MODIFY,
    getopts, start, flow, drain, end, 0, sizeof(priv_t)};
  static char const * lines[] = {
    "[options]",
    "\t-x num\tX-axis size in pixels; default derived or 800",
    "\t-X num\tX-axis pixels/second; default derived or 100",
    "\t-y num\tY-axis size in pixels (per channel); slow if not 1 + 2^n",
    "\t-Y num\tY-height total (i.e. not per channel); default 550",
    "\t-z num\tZ-axis range in dB; default 120",
    "\t-Z num\tZ-axis maximum in dBFS; default 0",
    "\t-q num\tZ-axis quantisation (0 - 249); default 249",
    "\t-w name\tWindow: Hann (default), Hamming, Bartlett, Rectangular, Kaiser",
    "\t-W num\tWindow adjust parameter (-10 - 10); applies only to Kaiser",
    "\t-s\tSlack overlap of windows",
    "\t-a\tSuppress axis lines",
    "\t-r\tRaw spectrogram; no axes or legends",
    "\t-l\tLight background",
    "\t-m\tMonochrome",
    "\t-h\tHigh colour",
    "\t-p num\tPermute colours (1 - 6); default 1",
    "\t-A\tAlternative, inferior, fixed colour-set (for compatibility only)",
    "\t-t text\tTitle text",
    "\t-c text\tComment text",
    "\t-o text\tOutput file name; default `spectrogram.png'",
    "\t-d time\tAudio duration to fit to X-axis; e.g. 1:00, 48",
    "\t-S time\tStart the spectrogram at the given time through the input",
  };
  static char * usage;
  handler.usage = lsx_usage_lines(&usage, lines, array_length(lines));
  return &handler;
}

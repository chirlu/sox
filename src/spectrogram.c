/* libSoX effect: Spectrogram       (c) 2008 robs@users.sourceforge.net
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
#include "getopt.h"
#include <assert.h>
#include <math.h>
#include <png.h>

#define malloc              lsx_malloc
#define calloc              lsx_calloc
#define realloc             lsx_realloc
#define FROM_SOX            SOX_SAMPLE_TO_FLOAT_64BIT
#define DFT_BASE_SIZE       512
#define MAX_DFT_SIZE_SHIFT  3
#define MAX_DFT_SIZE        (DFT_BASE_SIZE << MAX_DFT_SIZE_SHIFT)
#define MAX_COLS            999 /* Also max seconds */

static void apply_hann(double h[], const int num_points)
{
  int i, m = num_points - 1;
  for (i = 0; i < num_points; ++i) {
    double x = 2 * M_PI * i / m;
    h[i] *= .5 - .5 * cos(x);
  }
}

static void apply_hamming(double h[], const int num_points)
{
  int i, m = num_points - 1;
  for (i = 0; i < num_points; ++i) {
    double x = 2 * M_PI * i / m;
    h[i] *= .53836 - .46164 * cos(x);
  }
}

static void apply_bartlett(double h[], const int num_points)
{
  int i, m = num_points - 1;
  for (i = 0; i < num_points; ++i) {
    h[i] *= 2. / m * (m / 2. - fabs(i - m / 2.));
  }
}

static double kaiser_beta(double att)
{
  if (att > 100  ) return .1117 * att - 1.11;
  if (att > 50   ) return .1102 * (att - 8.7);
  if (att > 20.96) return .58417 * pow(att -20.96, .4) + .07886 * (att - 20.96);
  return 0;
}

static void apply_kaiser(double h[], const int num_points, double beta)
{
  int i, m = num_points - 1;
  for (i = 0; i <= m; ++i) {
    double x = 2. * i / m - 1;
    h[i] *= bessel_I_0(beta * sqrt(1 - x * x)) / bessel_I_0(beta);
  }
}

typedef enum {Window_Hann, Window_Hamming, Window_Bartlett, Window_Rectangular, Window_Kaiser} win_type_t;
static enum_item const window_options[] = {
  ENUM_ITEM(Window_,Hann)
  ENUM_ITEM(Window_,Hamming)
  ENUM_ITEM(Window_,Bartlett)
  ENUM_ITEM(Window_,Rectangular)
  ENUM_ITEM(Window_,Kaiser)
  {0, 0}};

typedef struct {
  int        y_size, dft_size, rows, cols, step_size, block_steps, block_num;
  double     buf[MAX_DFT_SIZE];
  double     dft_buf[MAX_DFT_SIZE];
  double     window[MAX_DFT_SIZE];
  int        bit_rev_table[100];  /* For Ooura fft */
  double     sin_cos_table[dft_sc_len(MAX_DFT_SIZE)];  /* ditto */
  double     magnitudes[(MAX_DFT_SIZE >> 1) + 1];
  float      * dBfs;
  double     pixels_per_sec, block_norm, max;
  int        dB_range, gain, style, read, end, end_min, last_end;
  char const * out_name, * title, * comment;
  sox_bool   no_axes;
  win_type_t win_type;
} priv_t;

#define GETOPT_NUMERIC(ch, name, min, max) case ch:{ \
  char * end_ptr; \
  double d = strtod(optarg, &end_ptr); \
  if (end_ptr == optarg || d < min || d > max || *end_ptr != '\0') {\
    sox_fail("parameter `%s' must be between %g and %g", #name, (double)min, (double)max); \
    return lsx_usage(effp); \
  } \
  p->name = d; \
  break; \
}

static int enum_option(int c, enum_item const * items)
{
  enum_item const * p = find_enum_text(optarg, items);
  if (p == NULL) {
    unsigned len = 1;
    char * set = lsx_malloc(len);
    *set = 0;
    for (p = items; p->text; ++p) {
      set = lsx_realloc(set, len += 2 + strlen(p->text));
      strcat(set, ", "); strcat(set, p->text);
    }
    sox_fail("-%c: '%s' is not one of: %s.", c, optarg, set + 2);
    free(set);
    return INT_MAX;
  }
  return p->value;
}

static int getopts(sox_effect_t * effp, int argc, char **argv)
{
  priv_t * p = (priv_t *)effp->priv;
  int c, callers_optind = optind, callers_opterr = opterr;

  assert(array_length(p->bit_rev_table) >= (size_t)dft_br_len(MAX_DFT_SIZE));

  --argv, ++argc, optind = 1, opterr = 0;                /* re-jig for getopt */
  p->pixels_per_sec = 100, p->y_size = 2, p->dB_range = 120;/* non-0 defaults */
  p->style = 1, p->out_name = "spectrogram.png", p->comment = "Created by SoX";

  while ((c = getopt(argc, argv, "+x:y:z:g:s:w:t:c:o:a")) != -1) switch (c) {
    GETOPT_NUMERIC('x', pixels_per_sec,  1 , 5000)
    GETOPT_NUMERIC('y', y_size        ,  1 , 1 + MAX_DFT_SIZE_SHIFT)
    GETOPT_NUMERIC('z', dB_range      , 40 , 180)
    GETOPT_NUMERIC('g', gain          ,-100, 100)
    GETOPT_NUMERIC('s', style         ,  1 , 4)
    case 'w': p->win_type = enum_option(c, window_options);   break;
    case 't': p->title    = optarg;   break;
    case 'c': p->comment  = optarg;   break;
    case 'o': p->out_name = optarg;   break;
    case 'a': p->no_axes  = sox_true; break;
    default: sox_fail("unknown option `-%c'", optopt); return lsx_usage(effp);
  }
  p->y_size -= 1, p->style -= 1;
  argc -= optind, optind = callers_optind, opterr = callers_opterr;
  return argc || p->win_type == INT_MAX? lsx_usage(effp) : SOX_SUCCESS;
}

static int make_window(priv_t * p, int end)
{
  double sum = 0, * w = end < 0? p->window : p->window + end;
  int i, n = p->dft_size - abs(end);

  if (end) memset(p->window, 0, sizeof(p->window));
  for (i = 0; i < n; ++i) w[i] = 1;
  switch (p->win_type) {
    case Window_Hann: apply_hann(w, n); break;
    case Window_Hamming: apply_hamming(w, n); break;
    case Window_Bartlett: apply_bartlett(w, n); break;
    case Window_Rectangular: break;
    default: apply_kaiser(w, n, kaiser_beta(p->dB_range + 10.));
  }
  for (i = 0; i < p->dft_size; ++i) sum += p->window[i];
  for (i = 0; i < p->dft_size; ++i) p->window[i] *= 2 / sum
    * sqr((double)n / p->dft_size);    /* empirical small window adjustment */
  return sum + .5;
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  double actual;

  if (effp->in_signal.channels != 1) {
    sox_fail("only 1 channel is supported");
    return SOX_EOF;
  }
  p->end = p->dft_size = DFT_BASE_SIZE << p->y_size;
  p->rows = (p->dft_size >> 1) + 1;
  p->step_size = make_window(p, p->last_end = 0);
  p->block_steps = effp->in_signal.rate / p->pixels_per_sec;
  p->step_size = p->block_steps / ceil((double)p->block_steps / p->step_size) +.5;
  p->block_steps = floor((double)p->block_steps / p->step_size +.5);
  p->block_norm = 1. / p->block_steps;
  actual = effp->in_signal.rate / p->step_size / p->block_steps;
  if (actual != p->pixels_per_sec)
    sox_report("actual pixels/s = %g", actual);
  sox_debug("step_size=%i block_steps=%i", p->step_size, p->block_steps);
  p->max = -p->dB_range;
  p->read = (p->step_size - p->dft_size) / 2;
  return SOX_SUCCESS;
}

static int do_column(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  int i;

  if (p->cols == MAX_COLS) {
    sox_warn("PNG truncated at %g seconds", (double)MAX_COLS * p->step_size * p->block_steps / effp->in_signal.rate);
    return SOX_EOF;
  }
  ++p->cols;
  p->dBfs = realloc(p->dBfs, p->cols * p->rows * sizeof(*p->dBfs));
  for (i = 0; i < p->rows; ++i) {
    double dBfs = 10 * log10(p->magnitudes[i] * p->block_norm);
    p->dBfs[(p->cols - 1) * p->rows + i] = dBfs + p->gain;
    if (dBfs > p->max)
      p->max = dBfs;
  }
  memset(p->magnitudes, 0, p->rows * sizeof(*p->magnitudes));
  p->block_num = 0;
  return SOX_SUCCESS;
}

static int flow(sox_effect_t * effp,
    const sox_sample_t * ibuf, sox_sample_t * obuf,
    sox_size_t * isamp, sox_size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  sox_size_t len = min(*isamp, *osamp), dummy = 0; /* No need to clip count */
  int i;

  memcpy(obuf, ibuf, len * sizeof(*obuf)); /* Pass on audio unaffected */
  *isamp = *osamp = len;

  while (sox_true) {
    if (p->read == p->step_size) {
      memmove(p->buf, p->buf + p->step_size,
          (p->dft_size - p->step_size) * sizeof(*p->buf));
      p->read = 0;
    }
    for (; len && p->read < p->step_size; --len, ++p->read, --p->end)
      p->buf[p->dft_size - p->step_size + p->read] = FROM_SOX(*ibuf++, dummy);
    if (p->read != p->step_size)
      break;

    if ((p->end = max(p->end, p->end_min)) != p->last_end)
      make_window(p, p->last_end = p->end);
    for (i = 0; i < p->dft_size; ++i) p->dft_buf[i] = p->buf[i] * p->window[i];
    rdft(p->dft_size, 1, p->dft_buf, p->bit_rev_table, p->sin_cos_table);
    p->magnitudes[0] += sqr(p->dft_buf[0]);
    for (i = 1; i < p->dft_size >> 1; ++i)
      p->magnitudes[i] += sqr(p->dft_buf[2*i]) + sqr(p->dft_buf[2*i+1]);
    p->magnitudes[p->dft_size >> 1] += sqr(p->dft_buf[1]);

    if (++p->block_num == p->block_steps && do_column(effp) == SOX_EOF)
      return SOX_EOF;
  }
  return SOX_SUCCESS;
}

static int drain(sox_effect_t * effp, sox_sample_t * obuf_, sox_size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;

  if (p->cols != MAX_COLS) {
    sox_sample_t * ibuf = calloc(p->dft_size, sizeof(*ibuf));
    sox_sample_t * obuf = calloc(p->dft_size, sizeof(*obuf));
    sox_size_t isamp = (p->dft_size - p->step_size) / 2;
    int left_over = (isamp + p->read) % p->step_size;

    if (left_over >= p->step_size >> 1)
      isamp += p->step_size - left_over;
    sox_debug("cols=%i left=%i end=%i", p->cols, p->read, p->end);
    p->end = 0;
    p->end_min = -p->dft_size;
    if (flow(effp, ibuf, obuf, &isamp, &isamp) == SOX_SUCCESS && p->block_num) {
      p->block_norm *= (double)p->block_steps / p->block_num;
      do_column(effp);
    }
    sox_debug("flushed cols=%i left=%i end=%i", p->cols, p->read, p->end);
    free(obuf);
    free(ibuf);
  }
  (void)obuf_, *osamp = 0;
  return SOX_SUCCESS;
}

#define spectrum_points (1 + 250)
enum {Background = spectrum_points, Text, Labels, Grid, palette_size};

static unsigned colour(double x, int range)
{
  x = range_limit(x, -range, 0);
  return (1 + x / range) * (spectrum_points - 1) + .5;
}

static void make_palette(png_byte * palette, int style)
{
  int i, j, at;
  for (i = j = 0; j < spectrum_points; i += 3, ++j) {
    double r, g, b, x = (double)j / (spectrum_points - 1);
    if (style > 1)
      palette[i+2] = palette[i+1] = palette[i+0] = style == 2? j : 255 - j;
    else {
      at = style? (spectrum_points - 1) * 3 - i : i;
      if      (x < .16) r = 0;
      else if (x < .72) r = .99 *           sin((x - .16) / .56 * M_PI / 2);
      else              r = .01 *               (x - .72) / .28          + .99;
      if      (x < .52) g = 0;
      else if (x < .86) g = .99 * .5 * (1 - cos((x - .52) / .34 * M_PI));
      else              g = .01 *               (x - .86) / .14          + .99;
      if      (x < .34) b = .5  *           sin((x - .00) / .34 * M_PI / 2);
      else if (x < .63) b = .5  * .5 * (1 + cos((x - .34) / .29 * M_PI));
      else              b =            (1 - cos((x - .63) / .37 * M_PI / 2));
      palette[at + 0] = r * 255 + .5;
      palette[at + 1] = g * 255 + .5;
      palette[at + 2] = b * 255 + .5;
    }
  }
  i = spectrum_points * 3;
  switch (style) {
    case 0: case 2:
      memcpy(palette + i, "\0\0\0"      , 3), i+= 3;
      memcpy(palette + i, "\377\377\377", 3), i+= 3;
      memcpy(palette + i, "\277\277\277", 3), i+= 3;
      memcpy(palette + i, "\177\177\177", 3), i+= 3;
      return;
    case 1:  memcpy(palette + i, "\335\330\320", 3), i+= 3; break;
    default: memcpy(palette + i, "\337\337\337", 3), i+= 3;
  }
  memcpy(palette + i, "\0\0\0"      , 3), i+= 3;
  memcpy(palette + i, "\077\077\077", 3), i+= 3;
  memcpy(palette + i, "\077\077\077", 3), i+= 3;
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
    int pos = (*text - ' ') * font_y;
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

static int axis(double to, int max_steps, int * limit, char * * prefix)
{
  double scale = 1, step = 1;
  int i, prefix_num = 0;
  if (max_steps) {
    double try, log_10 = HUGE_VAL, min_step = (to *= 10) / max_steps;
    for (i = 5; i; i >>= 1) if ((try = ceil(log10(min_step * i))) <= log_10)
      step = pow(10., log_10 = try) / i, log_10 -= i > 1;
    prefix_num = floor(log_10 / 3);
    scale = pow(10., -3. * prefix_num);
  }
  *prefix = "pnum-kMGTPE" + prefix_num + (prefix_num? 4 : 11);
  *limit = to * scale + .5;
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
  FILE *      file     = fopen(p->out_name, "w");
  uLong       font_len = 96 * font_y;
  int         rows     = below + p->rows + 30 + 20 * !!p->title;
  int         cols     = left + p->cols + between + spectrum_width + right;
  sox_bool    do_frame = !p->no_axes;
  png_byte *  pixels   = malloc(cols * rows * sizeof(*pixels));
  png_bytepp  png_rows = malloc(rows * sizeof(*png_rows));
  png_structp png      = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0,0);
  png_infop   png_info = png_create_info_struct(png);
  png_byte    palette[palette_size * 3];
  int         i, j, step, limit, tick_len = 2 + do_frame;
  char        text[200], * prefix;

  if (!file) {
    sox_fail("failed to create `%s' :(", p->out_name);
    png_destroy_write_struct(&png, &png_info);
    free(png_rows);
    free(pixels);
    free(p->dBfs);
    return SOX_EOF;
  }
  sox_debug("signal-max=%g", p->max);
  font = malloc(font_len);
  assert(uncompress(font, &font_len, fixed, sizeof(fixed)-1) == Z_OK);
  make_palette(palette, p->style);
  memset(pixels, Background, cols * rows * sizeof(*pixels));
  png_init_io(png, file);
  png_set_PLTE(png, png_info, (png_colorp)palette, palette_size);
  png_set_IHDR(png, png_info, (size_t)cols, (size_t)rows, 8,
      PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
      PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  for (j = 0; j < rows; ++j)
    png_rows[rows - 1 - j] = (png_bytep)(pixels + j * cols);

  if (p->title && (i = (int)strlen(p->title) * font_X) < cols + 1) /* Title */
    print_at((cols - i) / 2, rows - font_y, Text, p->title);
  if ((int)strlen(p->comment) * font_X < cols + 1)     /* Footer comment */
    print_at(1, font_y, Text, p->comment);

  /* Spectrogram */
  for (j = 0; j < p->rows; ++j) {
    for (i = 0; i < p->cols; ++i)
      pixel(left + i, below + j) = colour(p->dBfs[i*p->rows + j], p->dB_range);
    if (do_frame)                                      /* Frame verticals */
      pixel(left - 1, below + j) = pixel(left + p->cols,below + j) = Grid;
  }
  if (do_frame) for (i = -1; i <= p->cols; ++i)        /* Frame horizontals */
    pixel(left + i, below - 1) = pixel(left + i, below + p->rows) = Grid;

  /* X-axis */
  step = axis((double)p->cols * p->step_size * p->block_steps / effp->in_signal.rate, p->cols / (font_X * 9 / 2), &limit, &prefix);
  sprintf(text, "Time (%.1ss)", prefix);               /* Axis label */
  print_at(left + (p->cols - font_X * (int)strlen(text)) / 2, 24, Text, text);
  for (i = 0; i <= limit; i += step) {
    int y, x = limit? (double)i / limit * p->cols + .5 : 0;
    for (y = 0; y < tick_len; ++y)                     /* Ticks */
      pixel(left - 1 + x, below - 1 - y) = pixel(left - 1 + x, below + p->rows + y) = Grid;
    if (step == 5 && (i%10))
      continue;
    sprintf(text, "%g", .1 * i);                       /* Tick labels */
    x = left + x - 3 * strlen(text);
    print_at(x, below - 6, Labels, text);
    print_at(x, below + p->rows + 14, Labels, text);
  }

  /* Y-axis */
  step = axis(effp->in_signal.rate / 2, (p->rows - 1) / ((font_y * 3 + 1) >> 1), &limit, &prefix);
  sprintf(text, "Frequency (%.1sHz)", prefix);         /* Axis label */
  print_up(10, below + (p->rows - font_X * (int)strlen(text)) / 2, Text, text);
  for (i = 0; i <= limit; i += step) {
    int x, y = limit? (double)i / limit * (p->rows - 1) + .5 : 0;
    for (x = 0; x < tick_len; ++x)                     /* Ticks */
      pixel(left - 1 - x, below + y) = pixel(left + p->cols + x, below + y) = Grid;
    if (step == 5 && (i%10))
      continue;
    sprintf(text, i?"%5g":"   DC", .1 * i);            /* Tick labels */
    print_at(left - 4 - font_X * 5, below + y + 5, Labels, text);
    sprintf(text, i?"%g":"DC", .1 * i);
    print_at(left + p->cols + 6, below + y + 5, Labels, text);
  }

  /* Z-axis */
  print_at(cols - right - 2 - font_X, below - 13, Text, "dBFS");/* Axis label */
  for (j = 0; j < p->rows; ++j) {                      /* Spectrum */
    png_byte b = colour(p->dB_range * (j / (p->rows - 1.) - 1), p->dB_range);
    for (i = 0; i < spectrum_width; ++i)
      pixel(cols - right - 1 - i, below + j) = b;
  }
  for (i = 0; i <= p->dB_range; i += 10) {             /* (Tick) labels */
    int y = (double)i / p->dB_range * (p->rows - 1) + .5;
    sprintf(text, "%+i", i - p->gain - p->dB_range);
    print_at(cols - right + 1, below + y + 5, Labels, text);
  }

  free(font);
  png_set_rows(png, png_info, png_rows);
  png_write_png(png, png_info, PNG_TRANSFORM_IDENTITY, NULL);
  png_destroy_write_struct(&png, &png_info);
  free(png_rows);
  free(pixels);
  fclose(file);
  free(p->dBfs);
  return SOX_SUCCESS;
}

sox_effect_handler_t const * sox_spectrogram_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "spectrogram", "[options]\n"
      "  -x num    X-axis pixels/second, default 100\n"
      "  -y num    Y-axis resolution (1 - 4), default 2\n"
      "  -z num    Z-axis (colour) range in dB, default 120\n"
      "  -g num    Apply num dB gain; to shift Z-axis\n"
      "  -w name   Window: Hann (default), Hamming, Bartlett, Rectangular, Kaiser\n"
      "  -s num    Display style (1 - 4), default 1\n"
      "  -a        Toggle the presence of the axes' lines\n"
      "  -t text   Title text\n"
      "  -c text   Comment text\n"
      "  -o text   Name of the spectrogram output file, default `spectrogram.png'\n",
    0, getopts, start, flow, drain, stop, NULL, sizeof(priv_t)};
  return &handler;
}

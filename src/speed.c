/*
 * (c) Fabien Coelho <fabien@coelho.net> for sox, 03/2000.
 *
 * see sox copyright.
 *
 * speed up or down the sound, like a tape.
 * basically it's like resampling without resampling;-)
 * this could be done just by changing the rate of the file?
 * I don't know whether any rate is admissible.
 * implemented with a slow automaton, but it's easier than one more buffer.
 * I think it is especially inefficient.
 */

#include "st_i.h"

#include <math.h> /* pow */
#include <string.h>

/* type used for computations.
 */
#ifndef SPEED_FLOAT
#define SPEED_FLOAT float
#define SPEED_FLOAT_SCAN "%f"
#endif

/* constants
 */
#define FOUR               ((SPEED_FLOAT)(4.0e0))
#define ONE                ((SPEED_FLOAT)(1.0e0))
#define HALF               ((SPEED_FLOAT)(0.5e0))
#define ONESIXTH           ((SPEED_FLOAT)(1.0e0/6.0e0))
#define ZERO               ((SPEED_FLOAT)(0.0e0))

#define SPEED_USAGE "speed [-c] factor (default 1.0, <1 slows, -c: factor in cent)"

/* automaton status
 */
typedef enum { sp_input, sp_transfer, sp_compute } buffer_state_t;

/* internal structure
 */
typedef struct
{
    /* options
     */
    SPEED_FLOAT factor;   /* speed factor. */

    /* internals.
     */
    int clipped;          /* number of clipped values to report */

    SPEED_FLOAT rate;     /* rate of buffer sweep */

    int compression;      /* integer compression of the signal. */
    int index;            /* how much of the input buffer is filled */
    st_sample_t *ibuf;    /* small internal input buffer for compression */

    SPEED_FLOAT cbuf[4];  /* computation buffer for interpolation */
    SPEED_FLOAT frac;     /* current index position in cbuf */
    int icbuf;            /* available position in cbuf */

    buffer_state_t state; /* automaton status */

} * speed_t;

/*
static void debug(char * where, speed_t s)
{
    fprintf(stderr, "%s: f=%f r=%f comp=%d i=%d ic=%d frac=%f state=%d v=%f\n",
            where, s->factor, s->rate, s->compression, s->index,
            s->icbuf, s->frac, s->state, s->cbuf[0]);
}
*/

/* compute f(x) with a cubic interpolation...
 */
static SPEED_FLOAT cub(
  SPEED_FLOAT fm1, /* f(-1) */
  SPEED_FLOAT f0,  /* f(0)  */
  SPEED_FLOAT f1,  /* f(1)  */
  SPEED_FLOAT f2,  /* f(2)  */
  SPEED_FLOAT x)   /* 0.0 <= x < 1.0 */
{
    /* a x^3 + b x^2 + c x + d */
    register SPEED_FLOAT a, b, c, d;

    d = f0;
    b = HALF * (f1+fm1) - f0;
    a = ONESIXTH * (f2-f1+fm1-f0-FOUR*b);
    c = f1 - a - b - d;
    
    return ((a * x + b) * x + c) * x + d;
}

/* clip if necessary, and report. */
static st_sample_t clip(speed_t speed, SPEED_FLOAT v)
{
    if (v < ST_SAMPLE_MIN)
    {
        speed->clipped++;
        return ST_SAMPLE_MIN;
    }
    else if (v > ST_SAMPLE_MAX)
    {
        speed->clipped++;
        return ST_SAMPLE_MAX;
    }
    else
        return (st_sample_t) v;
}

/* get options. */
int st_speed_getopts(eff_t effp, int n, char **argv)
{
    speed_t speed = (speed_t) effp->priv;
    int cent = 0;

    speed->factor = ONE; /* default */

    if (n>0 && !strcmp(argv[0], "-c"))
    {
        cent = 1;
        argv++; n--;
    }

    if (n && (!sscanf(argv[0], SPEED_FLOAT_SCAN, &speed->factor) ||
              (cent==0 && speed->factor<=ZERO)))
    {
        printf("n = %d cent = %d speed = %f\n",n,cent,speed->factor);
        st_fail(SPEED_USAGE);
        return ST_EOF;
    }
    else if (cent != 0) /* CONST==2**(1/1200) */
    {
        speed->factor = pow((double)1.00057778950655, speed->factor);
        /* fprintf(stderr, "Speed factor: %f\n", speed->factor);*/
    }

    return ST_SUCCESS;
}

/* start processing. */
int st_speed_start(eff_t effp)
{
    speed_t speed = (speed_t) effp->priv;
    speed->clipped = 0;

    if (speed->factor >= ONE)
    {
        speed->compression = (int) speed->factor; /* floor */
        speed->rate = speed->factor / speed->compression;
    }
    else
    {
        speed->compression = 1;
        speed->rate = speed->factor;
    }

    speed->ibuf   = (st_sample_t *) malloc(speed->compression*
                                           sizeof(st_sample_t));
    speed->index  = 0;

    speed->state = sp_input;
    speed->cbuf[0] = ZERO; /* default previous value for interpolation */
    speed->icbuf = 1;
    speed->frac = ZERO;

    if (!speed->ibuf) {
        st_fail("malloc failed");
        return ST_EOF;
    }

    return ST_SUCCESS;
}

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

/* transfer input buffer to computation buffer.
 */
static void transfer(speed_t speed)
{
    register int i;
    register SPEED_FLOAT s = ZERO;

    for (i=0; i<speed->index; i++)
        s += (SPEED_FLOAT) speed->ibuf[i];
    
    speed->cbuf[speed->icbuf++] = s / ((SPEED_FLOAT) speed->index);
    
    if (speed->icbuf == 4)
        speed->state = sp_compute;
    else
        speed->state = sp_input;
    
    speed->index = 0;
}

/* interpolate values
 */
static st_size_t compute(speed_t speed, st_sample_t *obuf, st_size_t olen)
{
    st_size_t i;

    for(i = 0;
        i<olen && speed->frac < ONE;
        i++, speed->frac += speed->rate)
        obuf[i] = clip(speed, 
                       cub(speed->cbuf[0], speed->cbuf[1],
                           speed->cbuf[2], speed->cbuf[3], 
                           speed->frac));
    
    if (speed->frac >= ONE)
    {
        speed->frac -= ONE;
        speed->cbuf[0] = speed->cbuf[1];
        speed->cbuf[1] = speed->cbuf[2];
        speed->cbuf[2] = speed->cbuf[3];
        speed->icbuf = 3;
        speed->state = sp_input;
    }

    return i; /* number of data out */
}

/* handle a flow.
 */
int st_speed_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                  st_size_t *isamp, st_size_t *osamp)
{
    speed_t speed;
    register st_size_t len, iindex, oindex;

    speed = (speed_t) effp->priv;

    len = MIN(*isamp, *osamp);
    iindex = 0;
    oindex = 0;

    while (iindex<len && oindex<len)
    {
        /* store to input buffer. */
        if (speed->state==sp_input)
        {
            speed->ibuf[speed->index++] = ibuf[iindex++];
            if (speed->index==speed->compression)
                speed->state = sp_transfer;
        }

        /* transfer to compute buffer. */
        if (speed->state==sp_transfer)
            transfer(speed);

        /* compute interpolation. */
        if (speed->state==sp_compute)
            oindex += compute(speed, obuf+oindex, len-oindex);
    }

    *isamp = iindex;
    *osamp = oindex;

    return ST_SUCCESS;
}

/* end of stuff. 
 */
int st_speed_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
    speed_t speed = (speed_t) effp->priv;
    st_size_t i, oindex;

    transfer(speed);

    /* fix up trail by emptying cbuf */
    for (oindex=0, i=0; i<2 && oindex<*osamp;)
    {
        if (speed->state==sp_input)
        {
          speed->ibuf[speed->index++] = ZERO;
          i++;
          if (speed->index==speed->compression)
             speed->state = sp_transfer;
      }

      /* transfer to compute buffer. */
      if (speed->state==sp_transfer)
          transfer(speed);

      /* compute interpolation. */
      if (speed->state==sp_compute)
          oindex += compute(speed, obuf+oindex, *osamp-oindex);
    }

    *osamp = oindex; /* report how much was generated. */

    return ST_SUCCESS;
}

/* stop processing. report overflows. 
 */
int st_speed_stop(eff_t effp)
{
    speed_t speed = (speed_t) effp->priv;

    if (speed->clipped) 
        st_warn("SPEED: %d values clipped...", speed->clipped);

    free(speed->ibuf);
    
    return ST_SUCCESS;
}

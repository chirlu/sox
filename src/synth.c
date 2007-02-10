/*
 * synth - Synthesizer Effect.  
 *
 * Written by Carsten Borchardt Jan 2001
 * Version 0.1
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * The authors are not responsible for 
 * the consequences of using this software.
 */

#include <signal.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "synth.h"

static st_effect_t st_synth_effect;

#define PCOUNT 5

#define SYNTH_SINE       0
#define SYNTH_SQUARE     1
#define SYNTH_SAWTOOTH   2
#define SYNTH_TRIANGLE   3
#define SYNTH_TRAPEZIUM  4
#define SYNTH_TRAPETZ    SYNTH_TRAPEZIUM /* Deprecated name for trapezium */
#define SYNTH_WHITENOISE 5
#define SYNTH_NOISE      SYNTH_WHITENOISE /* Just a handy alias */
#define SYNTH_PINKNOISE  6
#define SYNTH_BROWNNOISE 7
#define SYNTH_EXP        8

#define SYNTH_CREATE    0x000
#define SYNTH_MIX       0x100
#define SYNTH_AMOD      0x200
#define SYNTH_FMOD      0x400

enum_item const synth_type[] = {
  ENUM_ITEM(SYNTH_,SINE      )
  ENUM_ITEM(SYNTH_,SQUARE    )
  ENUM_ITEM(SYNTH_,SAWTOOTH  )
  ENUM_ITEM(SYNTH_,TRIANGLE  )
  ENUM_ITEM(SYNTH_,TRAPEZIUM )
  ENUM_ITEM(SYNTH_,TRAPETZ   )
  ENUM_ITEM(SYNTH_,WHITENOISE)
  ENUM_ITEM(SYNTH_,NOISE     )
  ENUM_ITEM(SYNTH_,PINKNOISE )
  ENUM_ITEM(SYNTH_,BROWNNOISE)
  ENUM_ITEM(SYNTH_,EXP       )
  {0, 0}};

enum_item const combine_type[] = {
  ENUM_ITEM(SYNTH_,CREATE)
  ENUM_ITEM(SYNTH_,MIX   )
  ENUM_ITEM(SYNTH_,AMOD  )
  ENUM_ITEM(SYNTH_,FMOD  )
  {0, 0}};

/* do not ask me for the colored noise, i copied the 
 * algorithm somewhere...
 */
#define BROWNNOISE_FAC  (500.0/32768.0)
#define PINKNOISE_FAC   (5000.0/32768.0)
#define LOG_10_20     0.1151292546497022842009e0

#define MAXCHAN 4


/******************************************************************************
 * start of pink noise generator stuff
 * algorithm stolen from:
 * Author: Phil Burk, http://www.softsynth.com
 */

  
/* Calculate pseudo-random 32 bit number based on linear congruential method. */
static unsigned long GenerateRandomNumber( void )
{
        static unsigned long randSeed = 22222;  /* Change this for different random sequences. */
        randSeed = (randSeed * 196314165) + 907633515;
        return randSeed;
}

#define PINK_MAX_RANDOM_ROWS   (30)
#define PINK_RANDOM_BITS       (24)
#define PINK_RANDOM_SHIFT      ((sizeof(long)*8)-PINK_RANDOM_BITS)

typedef struct{
    long      pink_Rows[PINK_MAX_RANDOM_ROWS];
    long      pink_RunningSum;   /* Used to optimize summing of generators. */
    int       pink_Index;        /* Incremented each sample. */
    int       pink_IndexMask;    /* Index wrapped by ANDing with this mask. */
    float     pink_Scalar;       /* Used to scale within range of -1.0 to +1.0 */
} PinkNoise;

/* Setup PinkNoise structure for N rows of generators. */
static void InitializePinkNoise( PinkNoise *pink, int numRows )
{
        int i;
        long pmax;
        pink->pink_Index = 0;
        pink->pink_IndexMask = (1<<numRows) - 1;
/* Calculate maximum possible signed random value. Extra 1 for white noise always added. */
        pmax = (numRows + 1) * (1<<(PINK_RANDOM_BITS-1));
        pink->pink_Scalar = 1.0f / pmax;
/* Initialize rows. */
        for( i=0; i<numRows; i++ ) pink->pink_Rows[i] = 0;
        pink->pink_RunningSum = 0;
}

/* Generate Pink noise values between -1.0 and +1.0 */
static float GeneratePinkNoise( PinkNoise *pink )
{
        long newRandom;
        long sum;
        float output;

/* Increment and mask index. */
        pink->pink_Index = (pink->pink_Index + 1) & pink->pink_IndexMask;

/* If index is zero, don't update any random values. */
        if( pink->pink_Index != 0 )
        {
        /* Determine how many trailing zeros in PinkIndex. */
        /* This algorithm will hang if n==0 so test first. */
                int numZeros = 0;
                int n = pink->pink_Index;
                while( (n & 1) == 0 )
                {
                        n = n >> 1;
                        numZeros++;
                }

        /* Replace the indexed ROWS random value.
         * Subtract and add back to RunningSum instead of adding all the random
         * values together. Only one changes each time.
         */
                pink->pink_RunningSum -= pink->pink_Rows[numZeros];
                newRandom = ((long)GenerateRandomNumber()) >> PINK_RANDOM_SHIFT;
                pink->pink_RunningSum += newRandom;
                pink->pink_Rows[numZeros] = newRandom;
        }
        
/* Add extra white noise value. */
        newRandom = ((long)GenerateRandomNumber()) >> PINK_RANDOM_SHIFT;
        sum = pink->pink_RunningSum + newRandom;

/* Scale to range of -1.0 to 0.9999. */
        output = pink->pink_Scalar * sum;

        return output;
}

/**************** end of pink noise stuff */



/* Private data for the synthesizer */
typedef struct synthstuff {
    /* options */
    char *length_str;
    int type[MAXCHAN];
    int mix[MAXCHAN];
    double freq[MAXCHAN];
    double freq2[MAXCHAN];
    double par[MAXCHAN][5];

    /* internal stuff */
    st_sample_t max;
    st_size_t samples_done;
    int rate;
    st_size_t length; /* length in number of samples */
    double h[MAXCHAN]; /* store values necessary for  creation */
    PinkNoise pinkn[MAXCHAN];
} *synth_t;


/* a note is given as an int,
 * 0   => 440 Hz = A
 * >0  => number of half notes 'up', 
 * <0  => number of half notes down,
 * example 12 => A of next octave, 880Hz
 *
 * calculated by freq = 440Hz * 2**(note/12)
 */
static double calc_note_freq(double note){
    return (440.0 * pow(2.0,note/12.0));
}


/* read string 's' and convert to frequency
 * 's' can be a positive number which is the frequency in Hz
 * if 's' starts with a hash '%' and a following number the corresponding
 * note is calculated
 * return -1 on error
 */ 
static double StringToFreq(char *s, char **h){
    double f;

    if(*s=='%'){
        f = strtod(s+1,h);
        if ( *h == s+1 ){ 
            /* error*/
            return -1.0;
        }
        f=calc_note_freq(f);
    }else{
        f=strtod(s,h);
        if(*h==s){
            return -1.0;
        }
    }
    if( f < 0.0 )
        return -1.0;
    return f;
}



static void parmcopy(synth_t sy, int s, int d){
    int i;
    sy->freq[d]=sy->freq[s];
    sy->freq2[d]=sy->freq2[s];
    sy->type[d]=sy->type[s];
    sy->mix[d]=sy->mix[s];
    for(i=0;i<PCOUNT;i++){
        sy->par[d][i]=sy->par[s][i];
    }
}


/*
 * Process options
 *
 * Don't do initialization now.
 * The 'info' fields are not yet filled in.
 */
int st_synth_getopts(eff_t effp, int n, char **argv) 
{
    int argn;
    char *hlp;
    int i;
    int c;
    synth_t synth = (synth_t) effp->priv;
    

    /* set default parameters */
    synth->length = 0; /* use length of input file */
    synth->length_str = 0;
    for(c=0;c<MAXCHAN;c++){
        synth->freq[c] = 440.0;
        synth->freq2[c] = 440.0;
        synth->type[c]=SYNTH_SINE; 
        synth->mix[c] = SYNTH_CREATE;
    
        for(i=0;i<PCOUNT;i++)
            synth->par[c][i]= -1.0;
        
        synth->par[c][0]= 0.0; /* offset */
        synth->par[c][1]= 0.0; /* phase */;
    }

    argn=0;
    if ( n<0){
        st_fail(st_synth_effect.usage);
        return(ST_EOF);
    }
    if(n==0){
        /* no arg, use default*/
        return(ST_SUCCESS);
    }
    

    /* read length if given ( if first par starts with digit )*/
    if( isdigit((int)argv[argn][0]) || argv[argn][0] == '.') {
        synth->length_str = (char *)xmalloc(strlen(argv[argn])+1);
        strcpy(synth->length_str,argv[argn]);
        /* Do a dummy parse of to see if it will fail */
        if (st_parsesamples(0, synth->length_str, &synth->length, 't') == NULL)
        {
            st_fail(st_synth_effect.usage);
            return (ST_EOF);
        }
        argn++;
    }
    /* for one or more channel */
    /* type [combine] [f1[-f2]] [p0] [p1] [p2] [p3] [p4] */
    for (c = 0; c < MAXCHAN && n > argn; c++) {
      enum_item const * p = find_enum_text(argv[argn], synth_type);
      if (p == NULL) {
        st_fail("no type given");
        return ST_EOF;
      }
      synth->type[c] = p->value;
      if (++argn == n) break;

      /* maybe there is a combine-type in next arg */
      p = find_enum_text(argv[argn], combine_type);
      if (p != NULL) {
        synth->mix[c] = p->value;
        if (++argn == n) break;
      }

      /* read frequencies if given */
      if (isdigit((int)argv[argn][0]) || argv[argn][0] == '%') {
        synth->freq2[c] = synth->freq[c] = StringToFreq(argv[argn], &hlp);
        if (synth->freq[c] < 0) {
          st_fail("invalid freq");
          return ST_EOF;
        }
        if (*hlp == '-') { /* freq2 given? */
          char * hlp2;
          synth->freq2[c] = StringToFreq(hlp + 1, &hlp2);
          if (synth->freq2[c] < 0) {
            st_fail("invalid freq2");
            return ST_EOF;
          }
          if (synth->length_str == NULL) {
            st_fail("length must be given when using freq2");
            return ST_EOF;
          }
        }
        if (++argn == n) break;
      }

      /* read rest of parameters */
      for (i = 0; argn < n && isdigit((int)argv[argn][0]); ++i, ++argn) {
        if (i == PCOUNT) {
          st_fail("too many parameters");
          return ST_EOF;
        }
        synth->par[c][i] = strtod(argv[argn], &hlp);
        if (hlp == argv[argn]) {
          st_fail("parameter error");
          return ST_EOF;
        }
      }
      if (argn == n) break;
    }

    /* make some intelligent parameter initialization for channels
     * where no parameters were given
     *
     * - if only parms for one channel were given, copy to other channels
     * - if parm for 2 channels were given, copy to channel 1->3, 2->4
     * - if parm for 3 channels were given, copy 2->4
     */
    if(c == 0 || c >= MAXCHAN){
        for(c=1;c<MAXCHAN;c++)
            parmcopy(synth,0,c);
    }else if(c == 1){
        parmcopy(synth,0,2);
        parmcopy(synth,1,3);
    }else if(c == 2){
        parmcopy(synth,1,3);
    }

    return (ST_SUCCESS);
}


/*
 * Prepare processing.
 * Do all initializations.
 */
int st_synth_start(eff_t effp)
{
    int i;
    int c;
    synth_t synth = (synth_t) effp->priv;
    int shift_for_max = (4 - min(effp->outinfo.size, 4)) << 3;

    synth->max = (ST_SAMPLE_MAX >> shift_for_max) << shift_for_max;

    if (synth->length_str)
    {
        if (st_parsesamples(effp->ininfo.rate, synth->length_str,
                            &synth->length, 't') == NULL)
        {
            st_fail(st_synth_effect.usage);
            return(ST_EOF);
        }
    }

    synth->samples_done=0;
    synth->rate = effp->ininfo.rate;
    
    for(i=0;i< MAXCHAN; i++){
        synth->h[i]=0.0;
    }

    /* parameter adjustment for all channels */
    for(c=0;c<MAXCHAN;c++){
        /* adjust parameter 0 - 100% to 0..1 */
        for(i=0;i<PCOUNT;i++){
            synth->par[c][i] /= 100.0;
        }
    
        /* give parameters nice defaults for the different 'type' */
    
        switch(synth->type[c]){
            case SYNTH_SINE:
                break;
            case SYNTH_SQUARE:
                /* p2 is pulse width */
                if(synth->par[c][2] < 0.0){
                    synth->par[c][2] = 0.5; /* default to 50% duty cycle */
                }
                break;
            case SYNTH_TRIANGLE:
                /* p2 is position of maximum*/
                if(synth->par[c][2] < 0.0){
                    /* default : 0 */
                    synth->par[c][2]=0.5;
                }
                break;
            case SYNTH_SAWTOOTH:
                /* no parameters, use TRIANGLE to create no-default-sawtooth */
                break;
            case SYNTH_TRAPETZ:
                /* p2 is length of rising slope,
                 * p3 position where falling slope begins
                 * p4 position of end of falling slope
                 */
                if(synth->par[c][2] < 0.0 ){
                    synth->par[c][2]= 0.1;
                    synth->par[c][3]= 0.5;
                    synth->par[c][4]= 0.6;
                }else if(synth->par[c][3] < 0.0){
                    /* try a symetric waveform
                     */
                    if(synth->par[c][2] <= 0.5){
                        synth->par[c][3] = (1.0-2.0*synth->par[c][2])/2.0;
                        synth->par[c][4] = synth->par[c][3] + synth->par[c][2];
                    }else{
                        /* symetric is not possible, fall back to asymetrical 
                         * triangle
                         */
                        synth->par[c][3]=synth->par[c][2];
                        synth->par[c][4]=1.0;
                    }
                }else if(synth->par[c][4] < 0.0){
                    /* simple falling slope to the end */
                    synth->par[c][4]=1.0;
                }
                break;
            case SYNTH_PINKNOISE:
                /* Initialize pink noise signals with different numbers of rows. */
                InitializePinkNoise( &(synth->pinkn[c]),10+2*c);
                break;
            case SYNTH_EXP:
                /* p2 is position of maximum*/
                if (synth->par[c][2] < 0)
                  synth->par[c][2] = 0.5;

                /* p2 is amplitude */
                if (synth->par[c][3] < 0)
                  synth->par[c][3] = 1;
                break;

            default:
                break;
        }

        st_debug("type=%i, mix=%i, length=%u, f1=%g, f2=%g",
                synth->type[c], synth->mix[c], 
                synth->length, synth->freq[c], synth->freq2[c]);
        st_debug("p0=%g, p1=%g, p2=%g, p3=%g, p4=%g",
                synth->par[c][0], synth->par[c][1],
                synth->par[c][2], synth->par[c][3], synth->par[c][4]);
    }
    st_debug("inchan=%i, rate=%i", (int)effp->ininfo.channels,synth->rate);
    return (ST_SUCCESS);
}



static st_sample_t do_synth(st_sample_t iv, synth_t synth, int c){
    st_sample_t ov=iv;
    double r=0.0; /* -1 .. +1 */
    double f;
    double om;
    double sd;
    double move;
    double t,dt ;

    if(synth->length<=0){
        /* there is no way to change the freq. without knowing the length
         * use startfreq all the time ...
         */
        f = synth->freq[c];
    }else{
        f = synth->freq[c] * 
            exp( (log(synth->freq2[c])-log(synth->freq[c]))* 
                 synth->samples_done/synth->length );
    }
    om = 1.0 / f; /* periodendauer inn sec */
    t = synth->samples_done / (double)synth->rate; /* zeit seit start in sec */
    dt = t - synth->h[c]; /* seit seitdem letzte periode um war. */
    if( dt < om){
        /* wir sind noch in der periode.. */
    }else{
        /* schon in naechste periode */
        synth->h[c]+=om;
        dt=t-synth->h[c];
    }
    sd= dt/om; /* position in der aktuellen periode; 0<= sd < 1*/
    sd = fmod(sd+synth->par[c][1],1.0); /* phase einbauen */


    switch(synth->type[c]){
        case SYNTH_SINE:
            r = sin(2.0 * M_PI * sd);
            break;
        case SYNTH_SQUARE:
            /* |_______           | +1
             * |       |          |
             * |_______|__________|  0
             * |       |          |
             * |       |__________| -1
             * |                  |
             * 0       p2          1
             */
            if(sd < synth->par[c][2]){
                r = -1.0;
            }else{
                r = +1.0;
            }
            break;
        case SYNTH_SAWTOOTH:
            /* |           __| +1
             * |        __/  |
             * |_______/_____|  0
             * |  __/        |
             * |_/           | -1
             * |             |
             * 0             1
             */
            r = -1.0 + 2.0 * sd;
            break;
        case SYNTH_TRIANGLE:
            /* |    _    | +1
             * |   / \   |
             * |__/___\__|  0
             * | /     \ |
             * |/       \| -1
             * |         |
             * 0   p2    1
             */

            if( sd < synth->par[c][2]){ /* in rising Part of period */
                r = -1.0 + 2.0 * sd / synth->par[c][2];
            }else{    /* falling part */
                r = 1.0 - 2.0 *
                    (sd-synth->par[c][2])/(1-synth->par[c][2]);
            }
            break;
        case SYNTH_TRAPETZ:
            /* |    ______             |+1
             * |   /      \            |
             * |__/________\___________| 0
             * | /          \          |
             * |/            \_________|-1
             * |                       |
             * 0   p2    p3   p4       1
             */
            if( sd < synth->par[c][2]){ /* in rising part of period */
                r = -1.0 + 2.0 * sd / synth->par[c][2];
            }else if( sd < synth->par[c][3]){ /* in constant Part of period */
                r=1.0;
            }else if( sd < synth->par[c][4] ){ /* falling part */
                r = 1.0 - 2.0 *
                    (sd - synth->par[c][3])/(synth->par[c][4]-synth->par[c][3]);
            }else{
                r = -1.0;
            }
            break;

        case SYNTH_EXP:
            /* |             |              | +1
             * |            | |             |
             * |          _|   |_           | 0
             * |       __-       -__        |
             * |____---             ---____ | f(p3) 
             * |                            |
             * 0             p2             1
             */
            move=exp( - synth->par[c][3] * LOG_10_20 * 100.0 ); /* 0 ..  1 */
            if ( sd < synth->par[c][2] ) {
                r = move * exp(sd * log(1.0/move)/synth->par[c][2]);
            }else{
                r = move * 
                    exp( (1-sd)*log(1.0/move)/
                         (1.0-synth->par[c][2]));
            }

            /* r in 0 .. 1 */
            r = r * 2.0 - 1.0; /* -1 .. +1 */
            break;
        case SYNTH_WHITENOISE:
            r= 2.0* rand()/(double)RAND_MAX - 1.0;
            break;
        case SYNTH_PINKNOISE:
            r = GeneratePinkNoise( &(synth->pinkn[c]) );
            break;
        case SYNTH_BROWNNOISE:
            /* no idea if this algorithm is good enough.. */
            move = 2.0* rand()/(double)RAND_MAX - 1.0;
            move *= BROWNNOISE_FAC;
            synth->h[c] += move;
            if ((synth->h[c]) > 1.0)
                synth->h[c] -= 2.0*move;
            if ((synth->h[c]) < -1.0)
                synth->h[c] += 2.0*move;
            r=synth->h[c];
            break;
        default:
            st_warn("synth: internal error 1");
            break;
    }

    /* add offset, but prevent clipping */
    om = fabs(synth->par[c][0]);
    if( om <= 1.0 ){
        r *= 1.0 - om; /* reduce amp, prevent clipping */
        r += om;
    }


    switch(synth->mix[c]){
        case SYNTH_CREATE:
            ov = synth->max * r;
            break;
        case SYNTH_MIX:
            ov = iv/2 + r*synth->max/2;
            break;
        case SYNTH_AMOD:
            ov = (st_sample_t)(0.5*(r+1.0)*(double)iv);
            break;
        case SYNTH_FMOD:
            ov = iv * r ;
            break;
        default:
            st_fail("synth: internal error 2");
            break;
    }

    return ov;
}



/*
 * Processed signed long samples from ibuf to obuf.
 */
int st_synth_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf, 
                  st_size_t *isamp, st_size_t *osamp)
{
    synth_t synth = (synth_t) effp->priv;
    int len; /* number of input samples */
    int done = 0;
    int c;
    int chan=effp->ininfo.channels;
    int result = ST_SUCCESS;

    if(chan > MAXCHAN ){
        st_fail("synth: can not operate with more than %d channels",MAXCHAN);
        return(ST_EOF);
    }

    len = ((*isamp > *osamp) ? *osamp : *isamp) / chan;

    while (done < len && result == ST_SUCCESS)
    {
        for(c=0;c<chan;c++){
            /* each channel is independent, but the algorithm is the same */

            obuf[c] = do_synth(ibuf[c],synth,c);
        }
        ibuf+=chan;
        obuf+=chan;
        ++done;
        synth->samples_done++;
        if (synth->length > 0 && synth->samples_done == synth->length)
        {
            result = ST_EOF;
        }
    }
    *isamp = *osamp = done * chan;
    return result;
}

static st_effect_t st_synth_effect = {
  "synth",
  "Usage: synth [len] {[type] [combine] [freq[-freq2]] [off] [ph] [p1] [p2] [p3]}",
  ST_EFF_MCHAN,
  st_synth_getopts,
  st_synth_start,
  st_synth_flow,
  st_effect_nothing_drain,
  st_effect_nothing,
  st_effect_nothing
};

const st_effect_t *st_synth_effect_fn(void)
{
    return &st_synth_effect;
}
/*-------------------------------------------------------------- end of file */


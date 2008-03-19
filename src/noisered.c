/*
 * noiseprof - Noise Profiling Effect. 
 *
 * Written by Ian Turner (vectro@vectro.org)
 *
 * Copyright 1999 Ian Turner
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Authors are not responsible for the consequences of using this software.
 */

#include "noisered.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

typedef struct chandata {
    float *window;
    float *lastwindow;
    float *noisegate;
    float *smoothing;
} chandata_t;

/* Holds profile information */
typedef struct reddata {
    char* profile_filename;
    float threshold;

    chandata_t *chandata;
    sox_size_t bufdata;
} * reddata_t;

/*
 * Get the options. Default file is stdin (if the audio
 * input file isn't coming from there, of course!)
 */
static int sox_noisered_getopts(sox_effect_t * effp, int argc, char **argv)
{
  reddata_t p = (reddata_t) effp->priv;

  if (argc > 0) {
    p->profile_filename = argv[0];
    ++argv;
    --argc;
  }

  p->threshold = 0.5;
  do {     /* break-able block */
    NUMERIC_PARAMETER(threshold, 0, 1);
  } while (0);

  return argc? lsx_usage(effp) : SOX_SUCCESS;
}

/*
 * Prepare processing.
 * Do all initializations.
 */
static int sox_noisered_start(sox_effect_t * effp)
{
    reddata_t data = (reddata_t) effp->priv;
    sox_size_t fchannels = 0;
    sox_size_t channels = effp->in_signal.channels;
    sox_size_t i;
    FILE* ifp;

    data->chandata = (chandata_t*)xcalloc(channels, sizeof(*(data->chandata)));
    data->bufdata = 0;
    for (i = 0; i < channels; i ++) {
        data->chandata[i].noisegate = (float*)xcalloc(FREQCOUNT, sizeof(float));
        data->chandata[i].smoothing = (float*)xcalloc(FREQCOUNT, sizeof(float));
        data->chandata[i].lastwindow = NULL;
    }

    /* Here we actually open the input file. */
    if (!data->profile_filename || !strcmp(data->profile_filename, "-")) {
      if (effp->global_info->global_info->stdin_in_use_by) {
        sox_fail("stdin already in use by '%s'", effp->global_info->global_info->stdin_in_use_by);
        return SOX_EOF;
      }
      effp->global_info->global_info->stdin_in_use_by = effp->handler.name;
      ifp = stdin;
    }
    else if ((ifp = fopen(data->profile_filename, "r")) == NULL) {
        sox_fail("Couldn't open profile file %s: %s",
                data->profile_filename, strerror(errno));
        return SOX_EOF;
    }

    while (1) {
        sox_size_t i1;
        float f1;
        if (2 != fscanf(ifp, " Channel %u: %f", &i1, &f1))
            break;
        if (i1 != fchannels) {
            sox_fail("noisered: Got channel %d, expected channel %d.",
                    i1, fchannels);
            return SOX_EOF;
        }

        data->chandata[fchannels].noisegate[0] = f1;
        for (i = 1; i < FREQCOUNT; i ++) {
            if (1 != fscanf(ifp, ", %f", &f1)) {
                sox_fail("noisered: Not enough datums for channel %d "
                        "(expected %d, got %d)", fchannels, FREQCOUNT, i);
                return SOX_EOF;
            }
            data->chandata[fchannels].noisegate[i] = f1;
        }
        fchannels ++;
    }
    if (fchannels != channels) {
        sox_fail("noisered: channel mismatch: %d in input, %d in profile.",
                channels, fchannels);
        return SOX_EOF;
    }
    if (ifp != stdin)
      fclose(ifp);

    return (SOX_SUCCESS);
}

/* Mangle a single window. Each output sample (except the first and last
 * half-window) is the result of two distinct calls to this function, 
 * due to overlapping windows. */
static void reduce_noise(chandata_t* chan, float* window, double level)
{
    float *inr, *ini, *outr, *outi, *power;
    float *smoothing = chan->smoothing;
    int i;

    inr = (float*)xcalloc(WINDOWSIZE * 5, sizeof(float));
    ini = inr + WINDOWSIZE;
    outr = ini + WINDOWSIZE;
    outi = outr + WINDOWSIZE;
    power = outi + WINDOWSIZE;
    
    for (i = 0; i < FREQCOUNT; i ++)
        assert(smoothing[i] >= 0 && smoothing[i] <= 1);

    memcpy(inr, window, WINDOWSIZE*sizeof(float));

    FFT(WINDOWSIZE, 0, inr, NULL, outr, outi);

    memcpy(inr, window, WINDOWSIZE*sizeof(float));
    WindowFunc(HANNING, WINDOWSIZE, inr);
    PowerSpectrum(WINDOWSIZE, inr, power);

    for (i = 0; i < FREQCOUNT; i ++) {
        float smooth;
        float plog;
        plog = log(power[i]);
        if (power[i] != 0 && plog < chan->noisegate[i] + level*8.0)
            smooth = 0.0;
        else
            smooth = 1.0;
        
        smoothing[i] = smooth * 0.5 + smoothing[i] * 0.5;
    }
    
    /* Audacity says this code will eliminate tinkle bells.
     * I have no idea what that means. */
    for (i = 2; i < FREQCOUNT - 2; i ++) {
        if (smoothing[i]>=0.5 &&
            smoothing[i]<=0.55 &&
            smoothing[i-1]<0.1 &&
            smoothing[i-2]<0.1 &&
            smoothing[i+1]<0.1 &&
            smoothing[i+2]<0.1)
            smoothing[i] = 0.0;
    }
    
    outr[0] *= smoothing[0];
    outi[0] *= smoothing[0];
    outr[FREQCOUNT-1] *= smoothing[FREQCOUNT-1];
    outi[FREQCOUNT-1] *= smoothing[FREQCOUNT-1];
    
    for (i = 1; i < FREQCOUNT-1; i ++) {
        int j = WINDOWSIZE - i;
        float smooth = smoothing[i];
        
        outr[i] *= smooth;
        outi[i] *= smooth;
        outr[j] *= smooth;
        outi[j] *= smooth;
    }
    
    FFT(WINDOWSIZE, 1, outr, outi, inr, ini);
    WindowFunc(HANNING, WINDOWSIZE, inr);
    
    memcpy(window, inr, WINDOWSIZE*sizeof(float));

    for (i = 0; i < FREQCOUNT; i ++)
        assert(smoothing[i] >= 0 && smoothing[i] <= 1);

    free(inr);
}

/* Do window management once we have a complete window, including mangling
 * the current window. */
static int process_window(sox_effect_t * effp, reddata_t data, unsigned chan_num, unsigned num_chans,
                          sox_sample_t *obuf, unsigned len) {
    int j;
    float* nextwindow;
    int use = min(len, WINDOWSIZE)-min(len,(WINDOWSIZE/2));
    chandata_t *chan = &(data->chandata[chan_num]);
    int first = (chan->lastwindow == NULL);

    if ((nextwindow = (float*)xcalloc(WINDOWSIZE, sizeof(float))) == NULL)
        return SOX_EOF;
    
    memcpy(nextwindow, chan->window+WINDOWSIZE/2,
           sizeof(float)*(WINDOWSIZE/2));

    reduce_noise(chan, chan->window, data->threshold);
    if (!first) {
        for (j = 0; j < use; j ++) {
            float s = chan->window[j] + chan->lastwindow[WINDOWSIZE/2 + j];
            obuf[chan_num + num_chans * j] =
                SOX_FLOAT_32BIT_TO_SAMPLE(s, effp->clips);
        }
        free(chan->lastwindow);
    } else {
        for (j = 0; j < use; j ++) {
            assert(chan->window[j] >= -1 && chan->window[j] <= 1);
            obuf[chan_num + num_chans * j] =
                SOX_FLOAT_32BIT_TO_SAMPLE(chan->window[j], effp->clips);
        }
    }
    chan->lastwindow = chan->window;
    chan->window = nextwindow;
    
    return use;
}

/*
 * Read in windows, and call process_window once we get a whole one.
 */
static int sox_noisered_flow(sox_effect_t * effp, const sox_sample_t *ibuf, sox_sample_t *obuf, 
                    sox_size_t *isamp, sox_size_t *osamp)
{
    reddata_t data = (reddata_t) effp->priv;
    sox_size_t samp = min(*isamp, *osamp);
    sox_size_t tracks = effp->in_signal.channels;
    sox_size_t track_samples = samp / tracks;
    sox_size_t ncopy = min(track_samples, WINDOWSIZE-data->bufdata);
    sox_size_t whole_window = (ncopy + data->bufdata == WINDOWSIZE);
    int oldbuf = data->bufdata;
    sox_size_t i;

    /* FIXME: Make this automatic for all effects */
    assert(effp->in_signal.channels == effp->out_signal.channels);

    if (whole_window)
        data->bufdata = WINDOWSIZE/2;
    else
        data->bufdata += ncopy;

    /* Reduce noise on every channel. */
    for (i = 0; i < tracks; i ++) {
        chandata_t* chan = &(data->chandata[i]);
        sox_size_t j;

        if (chan->window == NULL)
            chan->window = (float*)xcalloc(WINDOWSIZE, sizeof(float));
        
        for (j = 0; j < ncopy; j ++)
            chan->window[oldbuf + j] =
                SOX_SAMPLE_TO_FLOAT_32BIT(ibuf[i + tracks * j], effp->clips);

        if (!whole_window)
            continue;
        else
            process_window(effp, data, i, tracks, obuf, oldbuf + ncopy);
    }
    
    *isamp = tracks*ncopy;
    if (whole_window)
        *osamp = tracks*(WINDOWSIZE/2);
    else
        *osamp = 0;

    return SOX_SUCCESS;
}

/*
 * We have up to half a window left to dump.
 */

static int sox_noisered_drain(sox_effect_t * effp, sox_sample_t *obuf, sox_size_t *osamp)
{
    reddata_t data = (reddata_t)effp->priv;
    unsigned i;
    unsigned tracks = effp->in_signal.channels;
    for (i = 0; i < tracks; i ++)
        *osamp = process_window(effp, data, i, tracks, obuf, data->bufdata);

    /* FIXME: This is very picky.  osamp needs to be big enough to get all
     * remaining data or it will be discarded.
     */
    return (SOX_EOF);
}

/*
 * Clean up.
 */
static int sox_noisered_stop(sox_effect_t * effp)
{
    reddata_t data = (reddata_t) effp->priv;
    sox_size_t i;

    for (i = 0; i < effp->in_signal.channels; i ++) {
        chandata_t* chan = &(data->chandata[i]);
        free(chan->lastwindow);
        free(chan->window);
        free(chan->smoothing);
        free(chan->noisegate);
    }
    
    free(data->chandata);

    return (SOX_SUCCESS);
}

static sox_effect_handler_t sox_noisered_effect = {
  "noisered",
  "[profile-file [amount]]",
  SOX_EFF_MCHAN|SOX_EFF_LENGTH,
  sox_noisered_getopts,
  sox_noisered_start,
  sox_noisered_flow,
  sox_noisered_drain,
  sox_noisered_stop,
  NULL
};

const sox_effect_handler_t *sox_noisered_effect_fn(void)
{
    return &sox_noisered_effect;
}

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

static st_effect_t st_noisered_effect;

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
    st_size_t bufdata;
} * reddata_t;

/*
 * Get the options. Filename is mandatory, though a reasonable default would
 * be stdin (if the input file isn't coming from there, of course!)
 */
static int st_noisered_getopts(eff_t effp, int n, char **argv) 
{
    reddata_t data = (reddata_t) effp->priv;

    if (n > 2 || n < 1) {
            st_fail(st_noisered_effect.usage);
            return (ST_EOF);
    }
    data->threshold = 0.5;
    data->profile_filename = argv[0];
    if (n == 2)
    {
        data->threshold = atof(argv[1]);

        if (data->threshold > 1)
        {
            data->threshold = 1;
        } else if (data->threshold < 0)
        {
            data->threshold = 0;
        }
    }
    return (ST_SUCCESS);
}

/*
 * Prepare processing.
 * Do all initializations.
 */
static int st_noisered_start(eff_t effp)
{
    reddata_t data = (reddata_t) effp->priv;
    int fchannels = 0;
    int channels = effp->ininfo.channels;
    int i;
    FILE* ifp;

    data->chandata = (chandata_t*)xcalloc(channels, sizeof(*(data->chandata)));
    data->bufdata = 0;
    for (i = 0; i < channels; i ++) {
        data->chandata[i].noisegate = (float*)xcalloc(FREQCOUNT, sizeof(float));
        data->chandata[i].smoothing = (float*)xcalloc(FREQCOUNT, sizeof(float));
        data->chandata[i].lastwindow = NULL;
    }

    /* Here we actually open the input file. */
    if (strcmp(data->profile_filename, "-") == 0)
      ifp = stdin;
    else
      ifp = fopen(data->profile_filename, "r");
    if (ifp == NULL) {
        st_fail("Couldn't open profile file %s: %s",
                data->profile_filename, strerror(errno));            
        return ST_EOF;
    }

    while (1) {
        int i1;
        float f1;
        if (2 != fscanf(ifp, " Channel %d: %f", &i1, &f1))
            break;
        if (i1 != fchannels) {
            st_fail("noisered: Got channel %d, expected channel %d.",
                    i1, fchannels);
            return ST_EOF;
        }

        data->chandata[fchannels].noisegate[0] = f1;
        for (i = 1; i < FREQCOUNT; i ++) {
            if (1 != fscanf(ifp, ", %f", &f1)) {
                st_fail("noisered: Not enough datums for channel %d "
                        "(expected %d, got %d)", fchannels, FREQCOUNT, i);
                return ST_EOF;
            }
            data->chandata[fchannels].noisegate[i] = f1;
        }
        fchannels ++;
    }
    if (fchannels != channels) {
        st_fail("noisered: channel mismatch: %d in input, %d in profile.",
                channels, fchannels);
        return ST_EOF;
    }
    if (strcmp(data->profile_filename, "-") != 0)
      fclose(ifp);

    return (ST_SUCCESS);
}

/* Mangle a single window. Each output sample (except the first and last
 * half-window) is the result of two distinct calls to this function, 
 * due to overlapping windows. */
static void reduce_noise(chandata_t* chan, float* window, float level)
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
static int process_window(eff_t effp, reddata_t data, int chan_num, int num_chans,
                          st_sample_t *obuf, int len) {
    int j;
    float* nextwindow;
    int use = min(len, WINDOWSIZE)-min(len,(WINDOWSIZE/2));
    chandata_t *chan = &(data->chandata[chan_num]);
    int first = (chan->lastwindow == NULL);

    if ((nextwindow = (float*)xcalloc(WINDOWSIZE, sizeof(float))) == NULL)
        return ST_EOF;
    
    memcpy(nextwindow, chan->window+WINDOWSIZE/2,
           sizeof(float)*(WINDOWSIZE/2));

    reduce_noise(chan, chan->window, data->threshold);
    if (!first) {
        for (j = 0; j < use; j ++) {
            float s = chan->window[j] + chan->lastwindow[WINDOWSIZE/2 + j];
            obuf[chan_num + num_chans * j] =
                ST_FLOAT_DWORD_TO_SAMPLE(s, effp->clips);
        }
        free(chan->lastwindow);
    } else {
        for (j = 0; j < use; j ++) {
            assert(chan->window[j] >= -1 && chan->window[j] <= 1);
            obuf[chan_num + num_chans * j] =
                ST_FLOAT_DWORD_TO_SAMPLE(chan->window[j], effp->clips);
        }
    }
    chan->lastwindow = chan->window;
    chan->window = nextwindow;
    
    return use;
}

/*
 * Read in windows, and call process_window once we get a whole one.
 */
static int st_noisered_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf, 
                    st_size_t *isamp, st_size_t *osamp)
{
    reddata_t data = (reddata_t) effp->priv;
    st_size_t samp = min(*isamp, *osamp);
    st_size_t tracks = effp->ininfo.channels;
    st_size_t track_samples = samp / tracks;
    st_size_t ncopy = min(track_samples, WINDOWSIZE-data->bufdata);
    st_size_t whole_window = (ncopy + data->bufdata == WINDOWSIZE);
    int oldbuf = data->bufdata;
    st_size_t i;

    assert(effp->ininfo.channels == effp->outinfo.channels);

    if (whole_window)
        data->bufdata = WINDOWSIZE/2;
    else
        data->bufdata += ncopy;

    /* Reduce noise on every channel. */
    for (i = 0; i < tracks; i ++) {
        chandata_t* chan = &(data->chandata[i]);
        st_size_t j;

        if (chan->window == NULL)
            chan->window = (float*)xcalloc(WINDOWSIZE, sizeof(float));
        
        for (j = 0; j < ncopy; j ++)
            chan->window[oldbuf + j] =
                ST_SAMPLE_TO_FLOAT_DWORD(ibuf[i + tracks * j], effp->clips);

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

    return ST_SUCCESS;
}

/*
 * We have up to half a window left to dump.
 */

static int st_noisered_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
    reddata_t data = (reddata_t)effp->priv;
    int i;
    int tracks = effp->ininfo.channels;
    for (i = 0; i < tracks; i ++)
        *osamp = process_window(effp, data, i, tracks, obuf, data->bufdata);

    /* FIXME: This is very picky.  osamp needs to be big enough to get all
     * remaining data or it will be discarded.
     */
    return (ST_EOF);
}

/*
 * Clean up.
 */
static int st_noisered_stop(eff_t effp)
{
    reddata_t data = (reddata_t) effp->priv;
    st_size_t i;

    for (i = 0; i < effp->ininfo.channels; i ++) {
        chandata_t* chan = &(data->chandata[i]);
        free(chan->lastwindow);
        free(chan->window);
        free(chan->smoothing);
        free(chan->noisegate);
    }
    
    free(data->chandata);

    return (ST_SUCCESS);
}

static st_effect_t st_noisered_effect = {
  "noisered",
  "Usage: noiseprof profile-file [threshold]",
  ST_EFF_MCHAN,
  st_noisered_getopts,
  st_noisered_start,
  st_noisered_flow,
  st_noisered_drain,
  st_noisered_stop,
  st_effect_nothing
};

const st_effect_t *st_noisered_effect_fn(void)
{
    return &st_noisered_effect;
}

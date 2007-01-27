/*
 * noiseprof - Noise Profiling Effect. 
 *
 * Written by Ian Turner (vectro@vectro.org)
 *
 * Copyright 1999 Ian Turner and others
 * This file is part of SoX.

 * SoX is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include "noisered.h"

#include <assert.h>
#include <string.h>
#include <errno.h>

static st_effect_t st_noiseprof_effect;

typedef struct chandata {
    float *sum;
    int   *profilecount;

    float *window;
} chandata_t;

typedef struct profdata {
    char* output_filename;
    FILE* output_file;

    chandata_t *chandata;
    st_size_t bufdata;
} * profdata_t;

/*
 * Get the filename, if any. We don't open it until st_noiseprof_start.
 */
static int st_noiseprof_getopts(eff_t effp, int n, char **argv) 
{
    profdata_t data = (profdata_t) effp->priv;

    if (n == 1) {
        data->output_filename = argv[0];
    } else if (n > 1) {
        st_fail(st_noiseprof_effect.usage);
        return (ST_EOF);
    }

    return (ST_SUCCESS);
}

/*
 * Prepare processing.
 * Do all initializations.
 */
static int st_noiseprof_start(eff_t effp)
{
    profdata_t data = (profdata_t) effp->priv;
    int channels = effp->ininfo.channels;
    int i;
   
    if (data->output_filename != NULL) {
        if (strcmp(data->output_filename, "-") != 0)
          data->output_file = fopen(data->output_filename, "w");
        else
          data->output_file = stdout;
        if (data->output_file == NULL) {
            st_fail("Couldn't open output file %s: %s",
                    data->output_filename, strerror(errno));            
        }
    } else {
        /* FIXME: We should detect output to stdout, and redirect to stderr. */
        data->output_file = stderr;
    }

    data->chandata = (chandata_t*)xcalloc(channels, sizeof(*(data->chandata)));
    data->bufdata = 0;
    for (i = 0; i < channels; i ++) {
        data->chandata[i].sum = (float*)xcalloc(FREQCOUNT, sizeof(float));
        data->chandata[i].profilecount = (int*)xcalloc(FREQCOUNT, sizeof(int));
        data->chandata[i].window = (float*)xcalloc(WINDOWSIZE, sizeof(float));
    }

    return ST_SUCCESS;
}

/* Collect statistics from the complete window on channel chan. */
static void collect_data(chandata_t* chan) {
    float *out = (float*)xcalloc(FREQCOUNT, sizeof(float));
    int i;

    PowerSpectrum(WINDOWSIZE, chan->window, out);

    for (i = 0; i < FREQCOUNT; i ++) {
        if (out[i] > 0) {
            float value = log(out[i]);
            chan->sum[i] += value;
            chan->profilecount[i] ++;
        }
    }

    free(out);
}

/*
 * Grab what we can from ibuf, and process if we have a whole window.
 */
static int st_noiseprof_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf, 
                    st_size_t *isamp, st_size_t *osamp)
{
    profdata_t data = (profdata_t) effp->priv;
    int samp = min(*isamp, *osamp);
    int tracks = effp->ininfo.channels;
    st_size_t track_samples = samp / tracks;
    int ncopy = 0;
    int i;

    assert(effp->ininfo.channels == effp->outinfo.channels);

    /* How many samples per track to analyze? */
    ncopy = min(track_samples, WINDOWSIZE-data->bufdata);

    /* Collect data for every channel. */
    for (i = 0; i < tracks; i ++) {
        chandata_t* chan = &(data->chandata[i]);
        int j;
        for (j = 0; j < ncopy; j ++) {
            chan->window[j+data->bufdata] =
                ST_SAMPLE_TO_FLOAT_DWORD(ibuf[i+j*tracks], effp->clips);
        }
        if (ncopy + data->bufdata == WINDOWSIZE)
            collect_data(chan);
    }

    data->bufdata += ncopy;
    assert(data->bufdata <= WINDOWSIZE);
    if (data->bufdata == WINDOWSIZE)
        data->bufdata = 0;
        
    memcpy(obuf, ibuf, ncopy*tracks);
    *isamp = *osamp = ncopy*tracks;

    return (ST_SUCCESS);
}

/*
 * Finish off the last window.
 */

static int st_noiseprof_drain(eff_t effp, st_sample_t *obuf UNUSED, st_size_t *osamp)
{
    profdata_t data = (profdata_t) effp->priv;
    int tracks = effp->ininfo.channels;
    int i;

    *osamp = 0;

    if (data->bufdata == 0) {
        return ST_EOF;
    }

    for (i = 0; i < tracks; i ++) {
        int j;
        for (j = data->bufdata+1; j < WINDOWSIZE; j ++) {
            data->chandata[i].window[j] = 0;
        }
        collect_data(&(data->chandata[i]));
    }

    if (data->bufdata == WINDOWSIZE || data->bufdata == 0)
        return ST_EOF;
    else
        return ST_SUCCESS;
}

/*
 * Print profile and clean up.
 */
static int st_noiseprof_stop(eff_t effp)
{
    profdata_t data = (profdata_t) effp->priv;
    st_size_t i;

    for (i = 0; i < effp->ininfo.channels; i ++) {
        int j;
        chandata_t* chan = &(data->chandata[i]);

        fprintf(data->output_file, "Channel %d: ", i);

        for (j = 0; j < FREQCOUNT; j ++) {
            fprintf(data->output_file, "%s%f", j == 0 ? "" : ", ",
                    chan->sum[j] / chan->profilecount[j]);
        }
        fprintf(data->output_file, "\n");

        free(chan->sum);
        free(chan->profilecount);
    }

    free(data->chandata);

    if (data->output_file != stderr && data->output_file != stdout)
        fclose(data->output_file);
    
    return (ST_SUCCESS);
}

static st_effect_t st_noiseprof_effect = {
  "noiseprof",
  "Usage: noiseprof [filename]",
  ST_EFF_MCHAN | ST_EFF_REPORT,
  st_noiseprof_getopts,
  st_noiseprof_start,
  st_noiseprof_flow,
  st_noiseprof_drain,
  st_noiseprof_stop,
  st_effect_nothing
};

const st_effect_t *st_noiseprof_effect_fn(void)
{
    return &st_noiseprof_effect;
}

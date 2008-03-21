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

typedef struct chandata {
    float *sum;
    int   *profilecount;

    float *window;
} chandata_t;

typedef struct profdata {
    char* output_filename;
    FILE* output_file;

    chandata_t *chandata;
    sox_size_t bufdata;
} * profdata_t;

/*
 * Get the filename, if any. We don't open it until sox_noiseprof_start.
 */
static int sox_noiseprof_getopts(sox_effect_t * effp, int n, char **argv) 
{
    profdata_t data = (profdata_t) effp->priv;

    if (n == 1) {
        data->output_filename = argv[0];
    } else if (n > 1)
      return lsx_usage(effp);

    return (SOX_SUCCESS);
}

/*
 * Prepare processing.
 * Do all initializations.
 */
static int sox_noiseprof_start(sox_effect_t * effp)
{
  profdata_t data = (profdata_t) effp->priv;
  unsigned channels = effp->in_signal.channels;
  unsigned i;
   
  /* Note: don't fall back to stderr if stdout is unavailable
   * since we already use stderr for diagnostics. */
  if (!data->output_filename || !strcmp(data->output_filename, "-")) {
    if (effp->global_info->global_info->stdout_in_use_by) {
      sox_fail("stdout already in use by '%s'", effp->global_info->global_info->stdout_in_use_by);
      return SOX_EOF;
    }
    effp->global_info->global_info->stdout_in_use_by = effp->handler.name;
    data->output_file = stdout;
  }
  else if ((data->output_file = fopen(data->output_filename, "w")) == NULL) {
    sox_fail("Couldn't open profile file %s: %s", data->output_filename, strerror(errno));
    return SOX_EOF;
  }

  data->chandata = (chandata_t*)lsx_calloc(channels, sizeof(*(data->chandata)));
  data->bufdata = 0;
  for (i = 0; i < channels; i ++) {
    data->chandata[i].sum = (float*)lsx_calloc(FREQCOUNT, sizeof(float));
    data->chandata[i].profilecount = (int*)lsx_calloc(FREQCOUNT, sizeof(int));
    data->chandata[i].window = (float*)lsx_calloc(WINDOWSIZE, sizeof(float));
  }

  return SOX_SUCCESS;
}

/* Collect statistics from the complete window on channel chan. */
static void collect_data(chandata_t* chan) {
    float *out = (float*)lsx_calloc(FREQCOUNT, sizeof(float));
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
static int sox_noiseprof_flow(sox_effect_t * effp, const sox_sample_t *ibuf, sox_sample_t *obuf, 
                    sox_size_t *isamp, sox_size_t *osamp)
{
    profdata_t data = (profdata_t) effp->priv;
    sox_size_t samp = min(*isamp, *osamp);
    sox_size_t tracks = effp->in_signal.channels;
    sox_size_t track_samples = samp / tracks;
    int ncopy = 0;
    sox_size_t i;

    /* FIXME: Make this automatic for all effects */
    assert(effp->in_signal.channels == effp->out_signal.channels);

    /* How many samples per track to analyze? */
    ncopy = min(track_samples, WINDOWSIZE-data->bufdata);

    /* Collect data for every channel. */
    for (i = 0; i < tracks; i ++) {
        chandata_t* chan = &(data->chandata[i]);
        int j;
        for (j = 0; j < ncopy; j ++) {
            chan->window[j+data->bufdata] =
                SOX_SAMPLE_TO_FLOAT_32BIT(ibuf[i+j*tracks], effp->clips);
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

    return (SOX_SUCCESS);
}

/*
 * Finish off the last window.
 */

static int sox_noiseprof_drain(sox_effect_t * effp, sox_sample_t *obuf UNUSED, sox_size_t *osamp)
{
    profdata_t data = (profdata_t) effp->priv;
    int tracks = effp->in_signal.channels;
    int i;

    *osamp = 0;

    if (data->bufdata == 0) {
        return SOX_EOF;
    }

    for (i = 0; i < tracks; i ++) {
        int j;
        for (j = data->bufdata+1; j < WINDOWSIZE; j ++) {
            data->chandata[i].window[j] = 0;
        }
        collect_data(&(data->chandata[i]));
    }

    if (data->bufdata == WINDOWSIZE || data->bufdata == 0)
        return SOX_EOF;
    else
        return SOX_SUCCESS;
}

/*
 * Print profile and clean up.
 */
static int sox_noiseprof_stop(sox_effect_t * effp)
{
    profdata_t data = (profdata_t) effp->priv;
    sox_size_t i;

    for (i = 0; i < effp->in_signal.channels; i ++) {
        int j;
        chandata_t* chan = &(data->chandata[i]);

        fprintf(data->output_file, "Channel %d: ", i);

        for (j = 0; j < FREQCOUNT; j ++) {
            double r = chan->profilecount[j] != 0 ? 
                    chan->sum[j] / chan->profilecount[j] : 0;
            fprintf(data->output_file, "%s%f", j == 0 ? "" : ", ", r);
        }
        fprintf(data->output_file, "\n");

        free(chan->sum);
        free(chan->profilecount);
    }

    free(data->chandata);

    if (data->output_file != stdout)
        fclose(data->output_file);
    
    return (SOX_SUCCESS);
}

static sox_effect_handler_t sox_noiseprof_effect = {
  "noiseprof",
  "[profile-file]",
  SOX_EFF_MCHAN,
  sox_noiseprof_getopts,
  sox_noiseprof_start,
  sox_noiseprof_flow,
  sox_noiseprof_drain,
  sox_noiseprof_stop,
  NULL
};

const sox_effect_handler_t *sox_noiseprof_effect_fn(void)
{
    return &sox_noiseprof_effect;
}
